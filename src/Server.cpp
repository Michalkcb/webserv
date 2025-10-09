#include "Server.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include <fcntl.h>
#include <netinet/tcp.h>

Server* Server::instance = NULL;

Server::Server() : _running(false) {
    instance = this;
}

Server::Server(const std::string& configFile) : _running(false) {
    instance = this;
    loadConfig(configFile);
}

Server::Server(const Server& other) {
    *this = other;
}

Server& Server::operator=(const Server& other) {
    if (this != &other) {
        _config = other._config;
        _serverSockets = other._serverSockets;
        _clients = other._clients;
        _pollFds = other._pollFds;
        _running = other._running;
    }
    return *this;
}

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
    }
    _cleanup();
}

// You also need to implement _updatePollFds and _handlePollEvents.
// Here is a complete, working implementation for them that fits your class structure.

void Server::_updatePollFds() {
    _pollFds.clear();

    // 1. Add all listening server sockets
    for (size_t i = 0; i < _serverSockets.size(); ++i) {
        struct pollfd pfd = {_serverSockets[i], POLLIN, 0};
        _pollFds.push_back(pfd);
    }

    // 2. Add all client sockets and their CGI pipes
    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        Client& client = it->second;

        // Add the client's main socket
    struct pollfd client_pfd = {client.getFd(), POLLIN, 0};
    if (client.getState() == Client::SENDING_RESPONSE || !client.getSendBuffer().empty()) {
            client_pfd.events |= POLLOUT;
        }
        _pollFds.push_back(client_pfd);

        // If client is waiting to write to a CGI, monitor its input pipe for writability
        if (client.isWaitingForCgiWrite()) {
            if (client.getCgi() && client.getCgi()->getInputFd() != -1) {
                struct pollfd cgi_in_pfd = {client.getCgi()->getInputFd(), POLLOUT, 0};
                _pollFds.push_back(cgi_in_pfd);
            }
        }

        // If a CGI is running, monitor its output pipe for readability
        if ((client.getState() == Client::CGI_PROCESSING || client.getState() == Client::CGI_STREAMING_BODY) && client.getCgi()) {
            if (client.getCgi()->getOutputFd() != -1) {
                struct pollfd cgi_out_pfd = {client.getCgi()->getOutputFd(), POLLIN, 0};
                _pollFds.push_back(cgi_out_pfd);
            }
        }
    }
}

void Server::_handlePollEvents() {
    // Check for new connections on server sockets first
    for (size_t i = 0; i < _serverSockets.size(); ++i) {
        short rev = _pollFds[i].revents;
        if (rev) {
            Logger::debug("Server socket fd=" + Utils::intToString(_serverSockets[i]) + ", revents=" + Utils::intToString(rev));
        }
        if (rev & POLLIN) {
            Logger::debug("POLLIN on server socket fd=" + Utils::intToString(_serverSockets[i]) + ", accepting connection");
            _acceptNewConnection(_serverSockets[i]);
        }
    }

    std::vector<int> clients_to_remove;

    // Check for events on client sockets and CGI pipes
    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        Client& client = it->second;
        int clientFd = it->first;

        // Find the pollfd for the client's main socket, input pipe, and output pipe
        for (size_t i = _serverSockets.size(); i < _pollFds.size(); ++i) {
            int current_fd = _pollFds[i].fd;
            short revents = _pollFds[i].revents;

            if (revents == 0) continue;

            // Event on the client's main socket
            if (current_fd == clientFd) {
                // Handle HUP/ERR carefully: if we still have data to send, try to flush it.
                if (revents & (POLLHUP | POLLERR)) {
                    // Mark peer as closed to stop expecting more reads
                    client.markPeerClosed();
                    Logger::debug("Poll revents on client fd=" + Utils::intToString(clientFd) + ": HUP/ERR. sendBufferLen=" + Utils::intToString((int)client.getSendBuffer().length()));
                    // Still attempt to send any remaining data
                    if (!client.getSendBuffer().empty()) {
                        client.sendData();
                    }
                    // If nothing to send, finish the client
                    if (client.getSendBuffer().empty()) {
                        client.setState(Client::FINISHED);
                    }
                } else {
                    // Handle POLLOUT before POLLIN to avoid a race where we
                    // finish sending a response, reset the client for
                    // keep-alive, and then accidentally clear any already-read
                    // bytes of the next pipelined request within the same
                    // poll iteration. Sending first allows the reset to occur
                    // before we read the next request, preserving correctness.
                    if (revents & POLLOUT) {
                        Logger::debug("POLLOUT on fd=" + Utils::intToString(clientFd) + ", sendBufferLen=" + Utils::intToString((int)client.getSendBuffer().length()));
                        client.sendData();
                    }
                    if (revents & POLLIN)  {
                        Logger::debug("POLLIN on fd=" + Utils::intToString(clientFd));
                        client.receiveData();
                        client.processRequest(_config);
                    }
                }
            }

            // Events on the client's CGI pipes
            if ((client.getState() == Client::CGI_PROCESSING || client.getState() == Client::CGI_STREAMING_BODY) && client.getCgi()) {
                if (current_fd == client.getCgi()->getInputFd() && (revents & POLLOUT)) {
                    client.handleCgiInput();
                }
                if (current_fd == client.getCgi()->getOutputFd() && (revents & POLLIN)) {
                    client.handleCgiOutput();
                }
            }
        }

        // Check if the client's state has changed to finished
        if (client.getState() == Client::FINISHED || client.getState() == Client::ERROR_STATE) {
            clients_to_remove.push_back(clientFd);
        }
    }

    // Safely remove clients after iterating
    for (size_t i = 0; i < clients_to_remove.size(); ++i) {
        _closeClient(clients_to_remove[i]);
    }
}

bool Server::isRunning() const {
    return _running;
}

void Server::_setupServerSockets() {
    for (Config::ServerIterator it = _config.begin(); it != _config.end(); ++it) {
        const Config::ServerBlock& server = *it;
        
        try {
            int serverSocket = _createServerSocket(Config::getHost(server), Config::getPort(server));
            _serverSockets.push_back(serverSocket);
            
            Logger::info("Listening on " + Config::getHost(server) + ":" + Utils::intToString(Config::getPort(server)));
        } catch (const std::exception& e) {
            Logger::error("Failed to create server socket for " + 
                         Config::getHost(server) + ":" + Utils::intToString(Config::getPort(server)) + 
                         ": " + std::string(e.what()));
            throw;
        }
    }
    
    if (_serverSockets.empty()) {
        throw std::runtime_error("No server sockets created");
    }
}

int Server::_createServerSocket(const std::string& host, int port) {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        throw std::runtime_error("Failed to create socket");
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(serverSocket);
        throw std::runtime_error("Failed to set SO_REUSEADDR");
    }
    
    // Optimize socket buffers for better performance
    int rcvbuf = 262144;  // 256KB receive buffer
    int sndbuf = 262144;  // 256KB send buffer
    if (setsockopt(serverSocket, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) < 0) {
        Logger::warn("Failed to set SO_RCVBUF");
    }
    if (setsockopt(serverSocket, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) < 0) {
        Logger::warn("Failed to set SO_SNDBUF");
    }
    
    // Set non-blocking
    Utils::setNonBlocking(serverSocket);
    
    // Bind socket
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    
    if (host == "0.0.0.0" || host.empty()) {
        serverAddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
            close(serverSocket);
            throw std::runtime_error("Invalid host address: " + host);
        }
    }
    
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close(serverSocket);
        throw std::runtime_error("Failed to bind socket to " + host + ":" + Utils::intToString(port));
    }
    
    // Listen
    if (listen(serverSocket, SOMAXCONN) < 0) {
        close(serverSocket);
        throw std::runtime_error("Failed to listen on socket");
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
    
    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
    
    Logger::info("New connection from " + std::string(clientIP) + " (fd: " + Utils::intToString(clientSocket) + ")");
    
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

    // Try to read the kernel-level fd target (/proc/self/fd/<fd>) to capture socket inode like "socket:[12345]"
    char fdPath[64];
    char linkTarget[256];
    snprintf(fdPath, sizeof(fdPath), "/proc/self/fd/%d", clientSocket);
    ssize_t linkLen = readlink(fdPath, linkTarget, sizeof(linkTarget) - 1);
    if (linkLen != -1) {
        linkTarget[linkLen] = '\0';
        Logger::debug(std::string("Accepted fd link: ") + fdPath + " -> " + linkTarget);
    } else {
        Logger::debug(std::string("Accepted fd link: ") + fdPath + " -> (readlink failed: " + std::string(strerror(errno)) + ")");
    }
    
    _clients[clientSocket] = Client(clientSocket);
}

void Server::_handleClientRead(int clientFd) {
    std::map<int, Client>::iterator it = _clients.find(clientFd);
    if (it == _clients.end()) return;
    
    Client& client = it->second;
    ssize_t bytesRead = client.receiveData();
    
    // Close connection if client disconnected or there's a real error
    if ((bytesRead == 0) ||
        (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK) ||
        client.getState() == Client::FINISHED || 
        client.getState() == Client::ERROR_STATE) {
        _closeClient(clientFd);
        return;
    }
    
    client.processRequest(_config);
}

void Server::_handleClientWrite(int clientFd) {
    std::map<int, Client>::iterator it = _clients.find(clientFd);
    if (it == _clients.end()) return;
    
    Client& client = it->second;
    ssize_t bytesSent = client.sendData();
    
    if (bytesSent < 0 || client.getState() == Client::FINISHED || client.getState() == Client::ERROR_STATE) {
        _closeClient(clientFd);
    }
}

void Server::_closeClient(int clientFd) {
    std::map<int, Client>::iterator it = _clients.find(clientFd);
    if (it != _clients.end()) {
    Logger::debug("Closing client connection (fd: " + Utils::intToString(clientFd) + ", state=" + Utils::intToString((int)it->second.getState()) + ", lastActivity=" + Utils::intToString((int)it->second.getLastActivity()) + ", sendBufferLen=" + Utils::intToString((int)it->second.getSendBuffer().length()) + ")");
        it->second.close();
        _clients.erase(it);
    }
}

void Server::_handleTimeout() {
    std::vector<int> clientsToClose;
    
    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        // If the client appears to have timed out, consider closing it.
        // However, avoid closing clients that are actively sending a response
        // with remaining data in their send buffer — closing them causes the
        // client to receive a truncated response (unexpected EOF). Instead,
        // let the normal send loop continue until the send buffer drains or
        // the connection truly becomes idle for a longer period.
        // Use a more generous idle timeout to accommodate slow large uploads
    const int IDLE_TIMEOUT_SECONDS = 600; // allow long interactive pauses
        if (it->second.hasTimedOut(IDLE_TIMEOUT_SECONDS)) {
            // If the client is still streaming a request body (e.g., large POST)
            // and the request isn't complete yet, do not close on idle timeout.
            if (!it->second.getRequest().isComplete() && it->second.getRequest().isStreamingMode()) {
                Logger::debug("Skipping timeout close for client " + Utils::intToString(it->first) + " because it is still uploading request body");
                continue;
            }
            // If the client is in the middle of CGI processing or streaming
            // and the CGI child is still running, do not close the client.
            // Long uploads may complete well before the CGI finishes, so
            // closing here causes truncated responses or false timeouts.
            if ((it->second.getState() == Client::CGI_PROCESSING || it->second.getState() == Client::CGI_STREAMING_BODY) &&
                it->second.getCgi() && it->second.getCgi()->isRunning()) {
                Logger::debug("Skipping timeout close for client " + Utils::intToString(it->first) + " because CGI is running (state=" + Utils::intToString((int)it->second.getState()) + ")");
                continue;
            }

            // Also skip closing if we're currently in SENDING_RESPONSE and there
            // is still data left to send. This prevents premature EOF for
            // large responses (e.g., CGI-generated bodies).
            if (it->second.getState() == Client::SENDING_RESPONSE && !it->second.getSendBuffer().empty()) {
                Logger::debug("Skipping timeout close for client " + Utils::intToString(it->first) + " because it is actively sending response (sendBufferLen=" + Utils::intToString((int)it->second.getSendBuffer().length()) + ")");
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
    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        it->second.close();
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
    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        Client& client = it->second;
        if ((client.getState() == Client::CGI_PROCESSING || client.getState() == Client::CGI_STREAMING_BODY) && client.getCgi()) {
            CGI* cgi = client.getCgi();
            
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
            bool clientIdle = client.hasTimedOut(30);
            time_t now = time(NULL);
            time_t secondsSinceClientActivity = now - client.getLastActivity();

            Logger::debug("Server::_checkCgiCompletion: client=" + Utils::intToString(it->first) + ", cgiFinished=" + std::string(cgiFinished ? "true" : "false") + ", cgiTimedOut=" + std::string(cgiTimedOut ? "true" : "false") + ", clientState=" + Utils::intToString(client.getState()) + ", clientIdle=" + std::string(clientIdle ? "true" : "false") + ", secSinceActivity=" + Utils::intToString((int)secondsSinceClientActivity));

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
                // Read any remaining bytes from CGI
                client.handleCgiOutput();

                // IMPORTANT: If CGI finished but the client request is not complete yet,
                // defer finalization until the upload completes to avoid closing the
                // connection while the client is still writing (broken pipe).
                if (cgiFinished && !client.getRequest().isComplete()) {
                    Logger::debug("Deferring CGI finalization: client still uploading request body.");
                    continue;
                }

                // Finalize the response now.
                if (client.getState() != Client::FINISHED && client.getState() != Client::ERROR_STATE) {
                    client.finalizeCgiResponse();
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
