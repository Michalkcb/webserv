#include "Config.hpp"
#include "Utils.hpp"
#include "Logger.hpp"

Config::Config() {
}

Config::Config(const std::string& configFile) : _configFile(configFile) {
    loadConfig(configFile);
}

Config::Config(const Config& other) {
    *this = other;
}

Config& Config::operator=(const Config& other) {
    if (this != &other) {
        _servers = other._servers;
        _configFile = other._configFile;
    }
    return *this;
}

Config::~Config() {
}

void Config::loadConfig(const std::string& filename) {
    _configFile = filename;
    _servers.clear();
    
    if (!Utils::fileExists(filename)) {
        Logger::warn("Config file not found: " + filename + ", using default configuration");
        
        // Create default server configuration
        ServerBlock defaultServer;
        defaultServer.host = "127.0.0.1";
        defaultServer.port = 8080;
        defaultServer.serverNames.push_back("localhost");
        defaultServer.root = "./www";
        defaultServer.index = "index.html";
        defaultServer.maxBodySize = MAX_BODY_SIZE;
        
        // Default location
        Location defaultLocation("/");
        defaultLocation.setRoot("./www");
        defaultLocation.setIndex("index.html");
        defaultLocation.addAllowedMethod("GET");
        defaultLocation.addAllowedMethod("POST");
        defaultLocation.addAllowedMethod("DELETE");
        defaultLocation.setAutoindex(true);
        defaultServer.locations.push_back(defaultLocation);
        
        _servers.push_back(defaultServer);
        return;
    }
    
    try {
        _parseConfigFile(filename);
    } catch (const std::exception& e) {
        Logger::error("Failed to parse config file: " + std::string(e.what()));
        throw;
    }
    
    if (_servers.empty()) {
        throw std::runtime_error("No server blocks found in configuration");
    }
}

void Config::_parseConfigFile(const std::string& filename) {
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + filename);
    }
    
    std::string line;
    while (std::getline(file, line)) {
        line = Utils::trim(line);
        if (line.empty() || line[0] == '#') continue;
        
        if (line.find("server") == 0 && line.find("{") != std::string::npos) {
            ServerBlock server;
            server.host = "127.0.0.1";
            server.port = 8080;
            server.root = "./www";
            server.index = "index.html";
            server.maxBodySize = MAX_BODY_SIZE;
            
            _parseServerBlock(file, server);
            _servers.push_back(server);
        }
    }
}

void Config::_parseServerBlock(std::ifstream& file, ServerBlock& server) {
    std::string line;
    int braceCount = 1;
    
    while (std::getline(file, line) && braceCount > 0) {
        line = Utils::trim(line);
        if (line.empty() || line[0] == '#') continue;
        
        // Handle location blocks separately - don't count their braces at server level
        if (line.find("location") == 0) {
            // Parse location path
            size_t start = line.find_first_of(" \t") + 1;
            size_t end = line.find_first_of(" \t{", start);
            std::string locationPath = line.substr(start, end - start);
            locationPath = Utils::trim(locationPath);
            
            Location location(locationPath);
            location.setRoot(server.root); // Inherit server root
            _parseLocationBlock(file, location);
            server.locations.push_back(location);
            continue;
        }
        
        // Count braces for server-level directives only
        if (line.find("{") != std::string::npos) braceCount++;
        if (line.find("}") != std::string::npos) braceCount--;
        if (braceCount == 0) break;
        
        std::string directive = _parseLine(line);
        std::vector<std::string> values = _parseValues(line);
        
        if (directive == "listen") {
            if (!values.empty()) {
                if (values[0].find(":") != std::string::npos) {
                    std::vector<std::string> hostPort = Utils::split(values[0], ":");
                    server.host = hostPort[0];
                    server.port = Utils::stringToInt(hostPort[1]);
                } else {
                    server.port = Utils::stringToInt(values[0]);
                }
            }
        } else if (directive == "server_name") {
            server.serverNames = values;
        } else if (directive == "root") {
            if (!values.empty()) server.root = values[0];
        } else if (directive == "index") {
            if (!values.empty()) server.index = values[0];
        } else if (directive == "client_max_body_size") {
            if (!values.empty()) {
                std::string sizeStr = values[0];
                size_t multiplier = 1;
                if (sizeStr.find("M") != std::string::npos || sizeStr.find("m") != std::string::npos) {
                    multiplier = 1024 * 1024;
                    sizeStr = sizeStr.substr(0, sizeStr.length() - 1);
                } else if (sizeStr.find("K") != std::string::npos || sizeStr.find("k") != std::string::npos) {
                    multiplier = 1024;
                    sizeStr = sizeStr.substr(0, sizeStr.length() - 1);
                }
                server.maxBodySize = Utils::stringToInt(sizeStr) * multiplier;
            }
        } else if (directive == "error_page") {
            if (values.size() >= 2) {
                int errorCode = Utils::stringToInt(values[0]);
                server.errorPages[errorCode] = values[1];
            }
        }
    }
    
    // Add default location if none specified
    if (server.locations.empty()) {
        Location defaultLocation("/");
        defaultLocation.setRoot(server.root);
        defaultLocation.setIndex(server.index);
        defaultLocation.addAllowedMethod("GET");
        defaultLocation.addAllowedMethod("POST");
        defaultLocation.addAllowedMethod("DELETE");
        server.locations.push_back(defaultLocation);
    }
}

void Config::_parseLocationBlock(std::ifstream& file, Location& location) {
    std::string line;
    int braceCount = 1;
    
    while (std::getline(file, line) && braceCount > 0) {
        line = Utils::trim(line);
        if (line.empty() || line[0] == '#') continue;
        
        if (line.find("{") != std::string::npos) braceCount++;
        if (line.find("}") != std::string::npos) braceCount--;
        if (braceCount == 0) break;
        
        std::string directive = _parseLine(line);
        std::vector<std::string> values = _parseValues(line);
        
        if (directive == "root") {
            if (!values.empty()) location.setRoot(values[0]);
        } else if (directive == "index") {
            if (!values.empty()) location.setIndex(values[0]);
        } else if (directive == "allow_methods" || directive == "methods") {
            location.setAllowedMethods(values);
        } else if (directive == "return") {
            if (values.size() >= 2) {
                location.setRedirect(values[1]);
            }
        } else if (directive == "autoindex") {
            if (!values.empty()) {
                location.setAutoindex(values[0] == "on" || values[0] == "true");
            }
        } else if (directive == "client_max_body_size") {
            if (!values.empty()) {
                std::string sizeStr = values[0];
                size_t multiplier = 1;
                if (sizeStr.find("M") != std::string::npos || sizeStr.find("m") != std::string::npos) {
                    multiplier = 1024 * 1024;
                    sizeStr = sizeStr.substr(0, sizeStr.length() - 1);
                } else if (sizeStr.find("K") != std::string::npos || sizeStr.find("k") != std::string::npos) {
                    multiplier = 1024;
                    sizeStr = sizeStr.substr(0, sizeStr.length() - 1);
                }
                location.setMaxBodySize(Utils::stringToInt(sizeStr) * multiplier);
            }
        } else if (directive == "upload_path") {
            if (!values.empty()) location.setUploadPath(values[0]);
        } else if (directive == "cgi_path") {
            if (!values.empty()) location.setCgiPath(values[0]);
        } else if (directive == "cgi_ext" || directive == "cgi_extension") {
            if (!values.empty()) location.setCgiExtension(values[0]);
        }
    }
}

std::string Config::_parseLine(const std::string& line) {
    size_t end = line.find_first_of(" \t");
    return (end != std::string::npos) ? line.substr(0, end) : line;
}

std::vector<std::string> Config::_parseValues(const std::string& line) {
    size_t start = line.find_first_of(" \t");
    if (start == std::string::npos) return std::vector<std::string>();
    
    std::string valuesPart = Utils::trim(line.substr(start));
    if (valuesPart.empty()) return std::vector<std::string>();
    
    // Remove trailing semicolon if present
    if (!valuesPart.empty() && valuesPart[valuesPart.length() - 1] == ';') {
        valuesPart = valuesPart.substr(0, valuesPart.length() - 1);
    }
    
    return Utils::split(valuesPart, " ");
}

const std::vector<Config::ServerBlock>& Config::getServers() const {
    return _servers;
}

Config::ServerBlock Config::getDefaultServer() const {
    return _servers.empty() ? ServerBlock() : _servers[0];
}

Config::ServerIterator Config::begin() const {
    return ServerIterator(_servers.begin());
}

Config::ServerIterator Config::end() const {
    return ServerIterator(_servers.end());
}

size_t Config::size() const {
    return _servers.size();
}

// Static access methods for ServerBlock
const std::string& Config::getHost(const ServerBlock& server) { return server.host; }
int Config::getPort(const ServerBlock& server) { return server.port; }
const std::vector<std::string>& Config::getServerNames(const ServerBlock& server) { return server.serverNames; }
const std::string& Config::getRoot(const ServerBlock& server) { return server.root; }
const std::string& Config::getIndex(const ServerBlock& server) { return server.index; }
size_t Config::getMaxBodySize(const ServerBlock& server) { return server.maxBodySize; }
const std::map<int, std::string>& Config::getErrorPages(const ServerBlock& server) { return server.errorPages; }
const std::vector<Location>& Config::getLocations(const ServerBlock& server) { return server.locations; }

const Config::ServerBlock* Config::findServer(const std::string& host, int port, const std::string& serverName) const {
    // First pass: exact match
    for (size_t i = 0; i < _servers.size(); ++i) {
        if (_servers[i].host == host && _servers[i].port == port) {
            if (serverName.empty()) {
                return &_servers[i];
            }
            
            const std::vector<std::string>& names = _servers[i].serverNames;
            for (size_t j = 0; j < names.size(); ++j) {
                if (names[j] == serverName) {
                    return &_servers[i];
                }
            }
        }
    }
    
    // Second pass: port match only (default for host:port)
    for (size_t i = 0; i < _servers.size(); ++i) {
        if (_servers[i].port == port) {
            return &_servers[i];
        }
    }
    
    // Return first server as ultimate default
    return _servers.empty() ? NULL : &_servers[0];
}

const Location* Config::findLocation(const ServerBlock& server, const std::string& uri) const {
    const Location* bestMatch = NULL;
    size_t bestMatchLength = 0;
    
    for (size_t i = 0; i < server.locations.size(); ++i) {
        if (server.locations[i].matches(uri)) {
            size_t pathLength = server.locations[i].getPath().length();
            if (pathLength > bestMatchLength) {
                bestMatch = &server.locations[i];
                bestMatchLength = pathLength;
            }
        }
    }
    
    return bestMatch;
}
