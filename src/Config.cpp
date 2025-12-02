#include "Config.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include <set>

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
    // Validation: ensure there are no ambiguous server blocks listening on the
    // same host:port without distinct server_name values. If two server blocks
    // bind to the same listen address:port and either one omits a server_name
    // (acts as a catch-all) or they share any server_name, the configuration
    // is ambiguous and we treat it as an error.
    for (size_t i = 0; i < _servers.size(); ++i) {
        for (size_t j = i + 1; j < _servers.size(); ++j) {
            if (_servers[i].host == _servers[j].host && _servers[i].port == _servers[j].port) {
                const std::vector<std::string>& a = _servers[i].serverNames;
                const std::vector<std::string>& b = _servers[j].serverNames;
                bool aEmpty = a.empty();
                bool bEmpty = b.empty();
                if (aEmpty || bEmpty) {
                    throw std::runtime_error("Ambiguous server blocks: multiple servers listen on "
                                             + _servers[i].host + ":" + Utils::intToString(_servers[i].port)
                                             + " where at least one server has no server_name. Please provide distinct server_name values.");
                }
                std::set<std::string> aset(a.begin(), a.end());
                for (size_t k = 0; k < b.size(); ++k) {
                    if (aset.find(b[k]) != aset.end()) {
                        throw std::runtime_error("Conflicting server_name '" + b[k] + "' on the same listen "
                                                 + _servers[i].host + ":" + Utils::intToString(_servers[i].port)
                                                 + ". Provide unique server_name values per server block.");
                    }
                }
            }
        }
    }
    // Debug: log parsed server blocks and their locations
    for (size_t i = 0; i < _servers.size(); ++i) {
        const ServerBlock& s = _servers[i];
        std::string names;
        for (size_t j = 0; j < s.serverNames.size(); ++j) { if (j) names += ","; names += s.serverNames[j]; }
        Logger::debug("Parsed server: host='" + s.host + "' port=" + Utils::intToString(s.port) + " names='" + names + "' root='" + s.root + "'");
        for (size_t k = 0; k < s.locations.size(); ++k) {
            Logger::debug("  location: path='" + s.locations[k].getPath() + "' root='" + s.locations[k].getRoot() + "'");
        }
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
            server.cgiTimeoutSeconds = 10;
            
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
            // Default root for a location inherits server.root but maps
            // the location path into the filesystem when the location
            // root wasn't explicitly provided. For example, a
            // `location /private {}` with server root `./www` should
            // map to `./www/private` by default so that getFullPath()
            // resolves URIs like `/private/secret.txt` ->
            // `./www/private/secret.txt`.
            if (locationPath != "/") {
                std::string locRoot = server.root;
                // remove trailing slash from server.root if present
                if (!locRoot.empty() && locRoot[locRoot.size()-1] == '/') locRoot.erase(locRoot.size()-1);
                // locationPath begins with '/', so concatenate directly
                locRoot += locationPath;
                location.setRoot(locRoot);
            } else {
                location.setRoot(server.root);
            }
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
        } else if (directive == "cgi_path") {
            if (!values.empty()) server.cgiPath = values[0];
        } else if (directive == "cgi_timeout") {
            if (!values.empty()) {
                server.cgiTimeoutSeconds = Utils::stringToInt(values[0]);
            }
        } else if (directive == "cgi_ext" || directive == "cgi_extension") {
            if (!values.empty()) server.cgiExtension = values[0];
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
        // Propagate server-level CGI settings to default location
        if (!server.cgiPath.empty()) defaultLocation.setCgiPath(server.cgiPath);
        if (!server.cgiExtension.empty()) defaultLocation.setCgiExtension(server.cgiExtension);
        defaultLocation.setCgiTimeout(server.cgiTimeoutSeconds);
        server.locations.push_back(defaultLocation);
    }

    // Ensure there is a /cgi-bin location to serve scripts placed under /cgi-bin
    bool hasCgiBin = false;
    for (size_t i = 0; i < server.locations.size(); ++i) {
        if (server.locations[i].getPath() == "/cgi-bin") { hasCgiBin = true; break; }
    }
    if (!hasCgiBin) {
        Location cgiLocation("/cgi-bin");
        // Default root for cgi-bin is server.root + "/cgi-bin"
        std::string cgiRoot = server.root;
        if (!cgiRoot.empty() && cgiRoot[cgiRoot.length()-1] == '/') cgiRoot.erase(cgiRoot.length()-1);
        cgiRoot += "/cgi-bin";
        cgiLocation.setRoot(cgiRoot);
        cgiLocation.addAllowedMethod("GET");
        cgiLocation.addAllowedMethod("POST");
        cgiLocation.setAutoindex(false);
        if (!server.cgiPath.empty()) cgiLocation.setCgiPath(server.cgiPath);
        if (!server.cgiExtension.empty()) cgiLocation.setCgiExtension(server.cgiExtension);
        cgiLocation.setCgiTimeout(server.cgiTimeoutSeconds);
        server.locations.push_back(cgiLocation);
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
            if (!values.empty()) {
                // Support both forms:
                //   return 301 http://example.com/;
                //   return http://example.com/;
                std::string url;
                int status = 302;
                if (values.size() >= 2 && Utils::isNumber(values[0])) {
                    status = Utils::stringToInt(values[0]);
                    url = values[1];
                } else {
                    url = values[0];
                }
                location.setRedirect(url);
                location.setRedirectStatus(status);
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
        } else if (directive == "cgi_timeout") {
            if (!values.empty()) {
                location.setCgiTimeout(Utils::stringToInt(values[0]));
            }
        } else if (directive == "deny") {
            if (!values.empty()) {
                // Support only `deny all;` for now
                if (values[0] == "all") {
                    location.setDenyAll(true);
                }
            }
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

const Config::ServerBlock* Config::findServerByPortAndName(int port, const std::string& serverName) const {
    std::string nameLower = Utils::toLowerCase(serverName);

    // First pass: try to match server_name among servers that listen on the port
    if (!nameLower.empty()) {
        for (size_t i = 0; i < _servers.size(); ++i) {
            if (_servers[i].port == port) {
                const std::vector<std::string>& names = _servers[i].serverNames;
                for (size_t j = 0; j < names.size(); ++j) {
                    if (Utils::toLowerCase(names[j]) == nameLower) {
                        return &_servers[i];
                    }
                }
            }
        }
    }

    // Second pass: return first server listening on the port
    for (size_t i = 0; i < _servers.size(); ++i) {
        if (_servers[i].port == port) return &_servers[i];
    }

    // Fallback to first server in config
    return _servers.empty() ? NULL : &_servers[0];
}
