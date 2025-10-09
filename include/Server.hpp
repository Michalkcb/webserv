#ifndef SERVER_HPP
#define SERVER_HPP

#include "webserv.hpp"
#include "Config.hpp"
#include "Client.hpp"

class Server {
private:
    Config _config;
    std::vector<int> _serverSockets;
    std::map<int, Client> _clients;
    std::vector<struct pollfd> _pollFds;
    bool _running;
    
    // Socket management
    int _createServerSocket(const std::string& host, int port);
    void _setupServerSockets();
    void _acceptNewConnection(int serverSocket);
    void _closeClient(int clientFd);
    
    // Poll management
    void _updatePollFds();
    void _handlePollEvents();
    void _handleClientRead(int clientFd);
    void _handleClientWrite(int clientFd);
    void _checkCgiCompletion();
    
    // Request processing
    void _processClientRequest(Client& client);
    Response _handleGetRequest(const Request& request, const Config::ServerBlock& serverConfig, const Location* location);
    Response _handlePostRequest(const Request& request, const Config::ServerBlock& serverConfig, const Location* location);
    Response _handleDeleteRequest(const Request& request, const Config::ServerBlock& serverConfig, const Location* location);
    Response _handleCgiRequest(const Request& request, const Location* location);
    
    // File operations
    Response _serveStaticFile(const std::string& filePath, const Request& request);
    Response _handleFileUpload(const Request& request, const std::string& uploadPath);
    bool _deleteFile(const std::string& filePath);
    
    // Error handling
    Response _generateErrorResponse(int statusCode, const Config::ServerBlock& serverConfig);
    void _handleTimeout();
    void _cleanup();

public:
    Server();
    Server(const std::string& configFile);
    Server(const Server& other);
    Server& operator=(const Server& other);
    ~Server();

    // Server control
    void start();
    void stop();
    void run();
    bool isRunning() const;
    
    // Configuration
    void loadConfig(const std::string& configFile);
    const Config& getConfig() const;
    
    // Signal handling
    static void signalHandler(int signal);
    static Server* instance;
};

#endif
