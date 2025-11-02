#include "Server.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include <fstream>
#include <ctime>
#include <sstream>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <netdb.h>

Server* Server::instance = NULL;

// Small helper: write a single line to finalize_cgi_debug.log only when
// diagnostics are explicitly enabled via WEBSERV_ENABLE_DIAG=1. This
// mirrors the gated logging helpers in Client.cpp so the server does not
// unconditionally create debug files during normal runs or evaluation.
static void serverDiagFinalLog(const std::string& s) {
    const char* dbg = getenv("WEBSERV_ENABLE_DIAG");
    if (!dbg || dbg[0] != '1') return;
    std::ofstream f("finalize_cgi_debug.log", std::ios::app);
    if (!f.is_open()) return;
    f << s;
}

Server::Server() : _running(false) {
    instance = this;
}

Server::Server(const std::string& configFile) : _running(false) {
    instance = this;
    loadConfig(configFile);
}

// Server is intentionally non-copyable. Copy constructor and assignment
// operator are declared private in the header and not defined here to
// prevent accidental copying of heavy resources (sockets, clients, etc.).

Server::~Server() {
    stop();
}

void Server::loadConfig(const std::string& configFile) {
    _config.loadConfig(configFile);
}

const Config& Server::getConfig() const {
    return _config;
}

void Server::start() {
    Logger::info("Starting webserver...");
    
    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN);
    
    try {
        _setupServerSockets();
        _running = true;
        Logger::info("Server started successfully");
    } catch (const std::exception& e) {
        Logger::error("Failed to start server: " + std::string(e.what()));
        throw;
    }
}

void Server::stop() {
    if (!_running) return;
    
    Logger::info("Stopping server...");
    _running = false;
    _cleanup();
    Logger::info("Server stopped");
}

// THIS IS THE CORRECT LOGIC
// In src/Server.cpp

// In src/Server.cpp

// In src/Server.cpp

void Server::run() {
    // Main server loop
    while (_running) { // Assuming _running is your loop control variable
        _updatePollFds(); // Use your existing function to set up FDs

        if (_pollFds.empty()) {
            // No sockets to monitor, can happen if all connections are closed
            // You might want a small sleep here to prevent a tight loop
            continue;
        }

        Logger::debug("Polling " + Utils::intToString(_pollFds.size()) + " file descriptors...");
        int poll_count = poll(&_pollFds[0], _pollFds.size(), 100); // 100ms timeout for better responsiveness

        if (poll_count < 0) {
            if (errno == EINTR) continue; // Interrupted by a signal, continue looping
            Logger::error("poll() failed: " + std::string(strerror(errno)));
            break; // Exit on critical poll error
        }
        _checkCgiCompletion();
        if (poll_count == 0) {
            // poll() timed out. This is a good place to check for client timeouts.
            _handleTimeout();
            continue;
        }

        // Handle events on all file descriptors
        _handlePollEvents();
        // After handling poll events, process any requested CGI finalizations
        // in a single canonical place to avoid races between different call
        // sites attempting to finalize the same CGI instance.
        for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
            Client* client = it->second;
            if (client->isCgiFinalizeRequested()) {
                // Only finalize if the client still has an active CGI and hasn't
                // already been finalized. This avoids duplicates where the
                // POLLIN path already finalized the CGI earlier in the same
                // loop iteration.
                    if (client->getCgi() && !client->isCgiFinalized()) {
                    std::ostringstream ss;
                    ss << "PROCESS_QUEUE finalize client=" << (void*)client << " fd=" << client->getFd() << " ts=" << (unsigned long)time(NULL)*1000UL << "\n";
                    serverDiagFinalLog(ss.str());
                    client->finalizeCgiResponse();
                }
                client->clearCgiFinalizeRequest();
            }
        }
    }
    _cleanup();
}

// You also need to implement _updatePollFds and _handlePollEvents.
// Here is a complete, working implementation for them that fits your class structure.

void Server::_updatePollFds() {
    _pollFds.clear();
    _pollOwners.clear();

    // 1. Add all listening server sockets
    for (size_t i = 0; i < _serverSockets.size(); ++i) {
        struct pollfd pfd = {_serverSockets[i], POLLIN, 0};
        _pollFds.push_back(pfd);
        _pollOwners.push_back(NULL);
    }

    // 2. Add all client sockets and their CGI pipes
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        Client* client = it->second;

        // Add the client's main socket
    struct pollfd client_pfd = {client->getFd(), POLLIN, 0};
        if (client->getState() == Client::SENDING_RESPONSE || !client->getSendBuffer().empty()) {
            client_pfd.events |= POLLOUT;
        }
    _pollFds.push_back(client_pfd);
    _pollOwners.push_back((void*)client);

        // If client is waiting to write to a CGI, monitor its input pipe for writability
        if (client->isWaitingForCgiWrite()) {
            if (client->getCgi() && client->getCgi()->getInputFd() != -1) {
                struct pollfd cgi_in_pfd = {client->getCgi()->getInputFd(), POLLOUT, 0};
                _pollFds.push_back(cgi_in_pfd);
                _pollOwners.push_back((void*)client);
            }
        }

        // If a CGI is running, monitor its output pipe for readability
        if ((client->getState() == Client::CGI_PROCESSING || client->getState() == Client::CGI_STREAMING_BODY) && client->getCgi()) {
            if (client->getCgi()->getOutputFd() != -1) {
                struct pollfd cgi_out_pfd = {client->getCgi()->getOutputFd(), POLLIN, 0};
                _pollFds.push_back(cgi_out_pfd);
                _pollOwners.push_back((void*)client);
            }
        }
    }
}

void Server::_handlePollEvents() {
    // 1) Server sockets (listening fds)
    for (size_t i = 0; i < _serverSockets.size(); ++i) {
        short rev = _pollFds[i].revents;
        if (rev) {
            Logger::debug("Server socket fd=" + Utils::intToString(_serverSockets[i]) + ", revents=" + Utils::intToString(rev));
        }
        if (rev & POLLIN) {
            _acceptNewConnection(_serverSockets[i]);
        }
    }

    // 2) Client-related fds and CGI pipes. Dispatch using _pollOwners to avoid
    //    fd-number-based misrouting when fds are recycled by the kernel.
    std::vector<int> clients_to_remove;
    for (size_t i = _serverSockets.size(); i < _pollFds.size(); ++i) {
        short revents = _pollFds[i].revents;
        if (revents == 0) continue;
        void* owner = NULL;
        if (i < _pollOwners.size()) owner = _pollOwners[i];
        if (!owner) continue;
        Client* client = (Client*)owner;
        int current_fd = _pollFds[i].fd;

        // Event on client's main socket
        if (current_fd == client->getFd()) {
            if (revents & (POLLHUP | POLLERR)) {
                client->markPeerClosed();
                Logger::debug("Poll revents on client fd=" + Utils::intToString(client->getFd()) + ": HUP/ERR. sendBufferLen=" + Utils::intToString((int)client->getSendBuffer().length()));
                // On HUP/ERR prefer to flush any pending send buffer once.
                if ((revents & POLLOUT) && !client->getSendBuffer().empty()) client->sendData();
                if (client->getSendBuffer().empty()) client->setState(Client::FINISHED);
            } else {
                // Subject requirement: only one read OR one write per client per poll()
                // If both POLLIN and POLLOUT are set, perform a single operation to
                // satisfy the evaluation constraint. We prefer to perform receive
                // first (to progress request parsing) and skip the send in that
                // iteration. This keeps behaviour deterministic and simple.
                bool didOne = false;
                if ((revents & POLLIN) && !didOne) {
                    client->receiveData();
                    client->processRequest(_config);
                    didOne = true;
                }
                if ((revents & POLLOUT) && !didOne) {
                    client->sendData();
                    didOne = true;
                }
            }
            if (client->getState() == Client::FINISHED || client->getState() == Client::ERROR_STATE)
                clients_to_remove.push_back(client->getFd());
            continue;
        }

        // Events on CGI pipes
        if (client->getCgi()) {
            if (current_fd == client->getCgi()->getInputFd() && (revents & POLLOUT)) {
                client->handleCgiInput();
            }
            if (current_fd == client->getCgi()->getOutputFd() && (revents & (POLLIN | POLLHUP | POLLERR))) {
                client->handleCgiOutput();
            }
        }
    }

    // Close clients that reached terminal state
    for (size_t ri = 0; ri < clients_to_remove.size(); ++ri) {
        _closeClient(clients_to_remove[ri]);
    }
}

int Server::_createServerSocket(const std::string& host, int port) {
    struct addrinfo hints; memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    bool anyHost = (host.empty() || host == "0.0.0.0");
    if (anyHost) hints.ai_flags = AI_PASSIVE;

    std::string portStr = Utils::intToString(port);
    struct addrinfo* res = NULL;
    int gaie = getaddrinfo(anyHost ? NULL : host.c_str(), portStr.c_str(), &hints, &res);
    if (gaie != 0 || !res) {
        throw std::runtime_error(std::string("getaddrinfo failed for host '") + (anyHost ? "*" : host) + "': " + gai_strerror(gaie));
    }

    int serverSocket = -1;
    for (struct addrinfo* ai = res; ai; ai = ai->ai_next) {
        serverSocket = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (serverSocket < 0) continue;

        int opt = 1;
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        int rcvbuf = 262144, sndbuf = 262144;
        setsockopt(serverSocket, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
        setsockopt(serverSocket, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

        Utils::setNonBlocking(serverSocket);

        if (bind(serverSocket, ai->ai_addr, ai->ai_addrlen) == 0) {
            if (listen(serverSocket, SOMAXCONN) == 0) {
                break; // success
            }
        }
        close(serverSocket);
        serverSocket = -1;
    }
    freeaddrinfo(res);

    if (serverSocket < 0) {
        throw std::runtime_error("Failed to create/bind/listen server socket");
    }
    return serverSocket;
}

void Server::_acceptNewConnection(int serverSocket) {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    
    int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
    if (clientSocket < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            Logger::error("Failed to accept connection: " + std::string(strerror(errno)));
        }
        return;
    }
    
    if (_clients.size() >= MAX_CLIENTS) {
        Logger::warn("Maximum clients reached, rejecting connection");
        close(clientSocket);
        return;
    }
    
    // Avoid inet_ntop (not in allowed list). Log only fd.
    Logger::info("New connection accepted (fd: " + Utils::intToString(clientSocket) + ")");
    
    // Set client socket to non-blocking
    Utils::setNonBlocking(clientSocket);
    
    // Optimize client socket for better performance
    int rcvbuf = 262144;  // 256KB receive buffer
    int sndbuf = 262144;  // 256KB send buffer
    if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) < 0) {
        Logger::debug("Failed to set client SO_RCVBUF");
    }
    if (setsockopt(clientSocket, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) < 0) {
        Logger::debug("Failed to set client SO_SNDBUF");
    }
    
    // Enable TCP_NODELAY to reduce latency
    int nodelay = 1;
    if (setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) < 0) {
        Logger::debug("Failed to set TCP_NODELAY");
    }

    // Drop readlink-based diagnostics to stay within allowed functions
    
    // Allocate Client on the heap to ensure single owner semantics
    Client* newClient = new Client(clientSocket);
    _clients[clientSocket] = newClient;
}

void Server::_setupServerSockets() {
    const std::vector<Config::ServerBlock>& servers = _config.getServers();
    if (servers.empty()) {
        throw std::runtime_error("No servers configured");
    }

    // Create one listening socket per configured server block.
    // If creation fails for any, clean up and propagate the error.
    std::vector<int> created;
    try {
        for (size_t i = 0; i < servers.size(); ++i) {
            const Config::ServerBlock& sb = servers[i];
            int sock = _createServerSocket(Config::getHost(sb), Config::getPort(sb));
            _serverSockets.push_back(sock);
            created.push_back(sock);
            Logger::info("Listening on " + Config::getHost(sb) + ":" + Utils::intToString(Config::getPort(sb)));
        }
    } catch (...) {
        // cleanup
        for (size_t i = 0; i < created.size(); ++i) close(created[i]);
        _serverSockets.clear();
        throw;
    }
}

void Server::_handleClientRead(int clientFd) {
    std::map<int, Client*>::iterator it = _clients.find(clientFd);
    if (it == _clients.end()) return;
    
    Client* client = it->second;
    ssize_t bytesRead = client->receiveData();
    
    // Close connection if client disconnected or client reached terminal state
    if ((bytesRead == 0) ||
        client->getState() == Client::FINISHED || 
        client->getState() == Client::ERROR_STATE) {
        _closeClient(clientFd);
        return;
    }
    
    client->processRequest(_config);
}

void Server::_handleClientWrite(int clientFd) {
    std::map<int, Client*>::iterator it = _clients.find(clientFd);
    if (it == _clients.end()) return;
    
    Client* client = it->second;
    ssize_t bytesSent = client->sendData();
    
    if (bytesSent < 0 || client->getState() == Client::FINISHED || client->getState() == Client::ERROR_STATE) {
        _closeClient(clientFd);
    }
}

void Server::_closeClient(int clientFd) {
    std::map<int, Client*>::iterator it = _clients.find(clientFd);
    if (it != _clients.end()) {
    Logger::debug("Closing client connection (fd: " + Utils::intToString(clientFd) + ", state=" + Utils::intToString((int)it->second->getState()) + ", lastActivity=" + Utils::intToString((int)it->second->getLastActivity()) + ", sendBufferLen=" + Utils::intToString((int)it->second->getSendBuffer().length()) + ")");
        it->second->close();
        delete it->second;
        _clients.erase(it);
    }
}

void Server::_handleTimeout() {
    std::vector<int> clientsToClose;
    
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        // If the client appears to have timed out, consider closing it.
        // However, avoid closing clients that are actively sending a response
        // with remaining data in their send buffer — closing them causes the
        // client to receive a truncated response (unexpected EOF). Instead,
        // let the normal send loop continue until the send buffer drains or
        // the connection truly becomes idle for a longer period.
        // Use a more generous idle timeout to accommodate slow large uploads
    const int IDLE_TIMEOUT_SECONDS = 600; // allow long interactive pauses
    if (it->second->hasTimedOut(IDLE_TIMEOUT_SECONDS)) {
            // If the client is still streaming a request body (e.g., large POST)
            // and the request isn't complete yet, do not close on idle timeout.
            if (!it->second->getRequest().isComplete() && it->second->getRequest().isStreamingMode()) {
                Logger::debug("Skipping timeout close for client " + Utils::intToString(it->first) + " because it is still uploading request body");
                continue;
            }
            // If the client is in the middle of CGI processing or streaming
            // and the CGI child is still running, do not close the client.
            // Long uploads may complete well before the CGI finishes, so
            // closing here causes truncated responses or false timeouts.
            if ((it->second->getState() == Client::CGI_PROCESSING || it->second->getState() == Client::CGI_STREAMING_BODY) &&
                it->second->getCgi() && it->second->getCgi()->isRunning()) {
                Logger::debug("Skipping timeout close for client " + Utils::intToString(it->first) + " because CGI is running (state=" + Utils::intToString((int)it->second->getState()) + ")");
                continue;
            }

            // Also skip closing if we're currently in SENDING_RESPONSE and there
            // is still data left to send. This prevents premature EOF for
            // large responses (e.g., CGI-generated bodies).
            if (it->second->getState() == Client::SENDING_RESPONSE && !it->second->getSendBuffer().empty()) {
                Logger::debug("Skipping timeout close for client " + Utils::intToString(it->first) + " because it is actively sending response (sendBufferLen=" + Utils::intToString((int)it->second->getSendBuffer().length()) + ")");
                continue;
            }

            clientsToClose.push_back(it->first);
        }
    }
    
    for (size_t i = 0; i < clientsToClose.size(); ++i) {
            Logger::debug("Client " + Utils::intToString(clientsToClose[i]) + " timed out");
        _closeClient(clientsToClose[i]);
    }
}

void Server::_cleanup() {
    // Close all client connections
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        it->second->close();
        delete it->second;
    }
    _clients.clear();
    
    // Close server sockets
    for (size_t i = 0; i < _serverSockets.size(); ++i) {
        close(_serverSockets[i]);
    }
    _serverSockets.clear();
    
    _pollFds.clear();
}

void Server::_checkCgiCompletion() {
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        Client* client = it->second;
        if ((client->getState() == Client::CGI_PROCESSING || client->getState() == Client::CGI_STREAMING_BODY) && client->getCgi()) {
            CGI* cgi = client->getCgi();
            
            // Check if CGI has finished OR appears to have timed out.
            // Only treat as timed out if both the CGI shows inactivity
            // and the client connection itself has been idle for the
            // same timeout period. This avoids finalizing the CGI while
            // the client is still uploading a large request body.
            // Don't finalize a CGI timeout while the client is still in the
            // middle of uploading (CGI_PROCESSING). Only treat as timed out
            // when the CGI timed out and the client is no longer in
            // CGI_PROCESSING (or is otherwise idle).
            bool cgiFinished = cgi->isFinished();
            bool cgiTimedOut = cgi->hasTimedOut(600); // 10 minutes for large uploads
            bool clientIdle = client->hasTimedOut(30);
            time_t now = time(NULL);
            time_t secondsSinceClientActivity = now - client->getLastActivity();

            Logger::debug("Server::_checkCgiCompletion: client=" + Utils::intToString(it->first) + ", cgiFinished=" + std::string(cgiFinished ? "true" : "false") + ", cgiTimedOut=" + std::string(cgiTimedOut ? "true" : "false") + ", clientState=" + Utils::intToString(client->getState()) + ", clientIdle=" + std::string(clientIdle ? "true" : "false") + ", secSinceActivity=" + Utils::intToString((int)secondsSinceClientActivity));

            // Only finalize on CGI timeout if the client has been idle for the
            // configured timeout AND a short grace period has passed since the
            // client's last activity. This avoids races where the client is
            // actively uploading and the CGI appears inactive for an instant.
            // Only finalize if either:
            // - the CGI finished and the client request is complete or the client
            //   is already idle (no more data expected), OR
            // - the CGI timed out and the client has been idle long enough
            //   (to avoid racing with ongoing uploads).
            if (cgiFinished || (cgiTimedOut && clientIdle)) {
                Logger::debug("CGI completion or timeout detected for client " + Utils::intToString(it->first));

                // Nie wykonujemy żadnego read() tutaj: odczyt CGI stdout wyłącznie po POLLIN
                // (obsługiwany w _handlePollEvents -> Client::handleCgiOutput()).

                // Jeśli CGI zakończył się, ale klient wciąż uploaduje request body,
                // odłóż finalizację aby nie przerwać strumienia po stronie klienta.
                if (cgiFinished && !client->getRequest().isComplete()) {
                    Logger::debug("Deferring CGI finalization: client still uploading request body.");
                    continue;
                }

                bool canFinalizeNow = false;
                if (cgiTimedOut && clientIdle) {
                    // Timeout: możemy finalizować natychmiast bez dalszych odczytów
                    canFinalizeNow = true;
                } else if (cgiFinished) {
                    // Finalizacja po normalnym zakończeniu tylko gdy stdout CGI został już zamknięty (EOF przetworzony)
                    if (client->getCgi() && client->getCgi()->getOutputFd() == -1) {
                        canFinalizeNow = true;
                    }
                }

                if (canFinalizeNow) {
                    // Uwaga: finalizeCgiResponse może już być wykonane w ścieżce POLLIN.
                    if (client->getState() != Client::FINISHED && client->getState() != Client::ERROR_STATE) {
                        if (client->getCgi() == NULL) {
                            Logger::debug("Server::_checkCgiCompletion: CGI already finalized elsewhere, skipping finalizeCgiResponse\n");
                        } else {
                Logger::debug("FINALIZE_CALLSITE:server_check client_fd=" + Utils::intToString(client->getFd()));
                                    {
                                        std::ostringstream ss;
                                        unsigned long ts = (unsigned long)time(NULL) * 1000UL;
                                        ss << "CALLSITE:server_check ts=" << ts << " client_socket=" << client->getFd() << " client_ptr=" << (void*)client << "\n";
                                        serverDiagFinalLog(ss.str());
                                    }
                                    // Defer to the centralized finalizer instead of finalizing here.
                                    client->requestCgiFinalize();
                        }
                    }
                }
            }
        }
    }
}

void Server::signalHandler(int signal) {
    if (instance) {
        Logger::info("Received signal " + Utils::intToString(signal) + ", shutting down...");
        instance->_running = false;
    }
}
