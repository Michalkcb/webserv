#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "webserv.hpp"
#include "Location.hpp"

class Config {
public:
    struct ServerBlock {
        std::string host;
        int port;
        std::vector<std::string> serverNames;
        std::string root;
        std::string index;
        size_t maxBodySize;
        std::map<int, std::string> errorPages;
        std::vector<Location> locations;
    };

private:

    std::vector<ServerBlock> _servers;
    std::string _configFile;

    void _parseConfigFile(const std::string& filename);
    void _parseServerBlock(std::ifstream& file, ServerBlock& server);
    void _parseLocationBlock(std::ifstream& file, Location& location);
    std::string _parseLine(const std::string& line);
    std::vector<std::string> _parseValues(const std::string& line);

public:
    Config();
    Config(const std::string& configFile);
    Config(const Config& other);
    Config& operator=(const Config& other);
    ~Config();

    void loadConfig(const std::string& filename);
    const std::vector<ServerBlock>& getServers() const;
    ServerBlock getDefaultServer() const;
    
    // Server block access methods
    class ServerIterator {
    private:
        std::vector<ServerBlock>::const_iterator _it;
    public:
        ServerIterator(std::vector<ServerBlock>::const_iterator it) : _it(it) {}
        const ServerBlock& operator*() const { return *_it; }
        const ServerBlock* operator->() const { return &(*_it); }
        ServerIterator& operator++() { ++_it; return *this; }
        bool operator!=(const ServerIterator& other) const { return _it != other._it; }
        bool operator==(const ServerIterator& other) const { return _it == other._it; }
    };

    ServerIterator begin() const;
    ServerIterator end() const;
    size_t size() const;
    
    // Access server block members
    static const std::string& getHost(const ServerBlock& server);
    static int getPort(const ServerBlock& server);
    static const std::vector<std::string>& getServerNames(const ServerBlock& server);
    static const std::string& getRoot(const ServerBlock& server);
    static const std::string& getIndex(const ServerBlock& server);
    static size_t getMaxBodySize(const ServerBlock& server);
    static const std::map<int, std::string>& getErrorPages(const ServerBlock& server);
    static const std::vector<Location>& getLocations(const ServerBlock& server);
    
    const ServerBlock* findServer(const std::string& host, int port, const std::string& serverName = "") const;
    const Location* findLocation(const ServerBlock& server, const std::string& uri) const;
};

#endif
