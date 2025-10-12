#include "Location.hpp"
#include "Utils.hpp"
#include "Logger.hpp"

Location::Location() : _path("/"), _root("./www"), _index("index.html"), 
                       _autoindex(false), _maxBodySize(MAX_BODY_SIZE) {
    _allowedMethods.push_back("GET");
}

Location::Location(const std::string& path) : _path(path), _root("./www"), 
                                              _index("index.html"), _autoindex(false), 
                                              _maxBodySize(MAX_BODY_SIZE) {
    _allowedMethods.push_back("GET");
}

Location::Location(const Location& other) {
    *this = other;
}

Location& Location::operator=(const Location& other) {
    if (this != &other) {
        _path = other._path;
        _root = other._root;
        _index = other._index;
        _redirect = other._redirect;
        _allowedMethods = other._allowedMethods;
        _autoindex = other._autoindex;
        _uploadPath = other._uploadPath;
        _cgiPath = other._cgiPath;
        _cgiExtension = other._cgiExtension;
        _maxBodySize = other._maxBodySize;
    }
    return *this;
}

Location::~Location() {
}

// Getters
const std::string& Location::getPath() const { return _path; }
const std::string& Location::getRoot() const { return _root; }
const std::string& Location::getIndex() const { return _index; }
const std::string& Location::getRedirect() const { return _redirect; }
const std::vector<std::string>& Location::getAllowedMethods() const { return _allowedMethods; }
bool Location::getAutoindex() const { return _autoindex; }
const std::string& Location::getUploadPath() const { return _uploadPath; }
const std::string& Location::getCgiPath() const { return _cgiPath; }
const std::string& Location::getCgiExtension() const { return _cgiExtension; }
size_t Location::getMaxBodySize() const { return _maxBodySize; }

// Setters
void Location::setPath(const std::string& path) { _path = path; }
void Location::setRoot(const std::string& root) { _root = root; }
void Location::setIndex(const std::string& index) { _index = index; }
void Location::setRedirect(const std::string& redirect) { _redirect = redirect; }

void Location::setAllowedMethods(const std::vector<std::string>& methods) {
    _allowedMethods = methods;
}

void Location::addAllowedMethod(const std::string& method) {
    std::string upperMethod = Utils::toUpperCase(method);
    std::vector<std::string>::iterator it = std::find(_allowedMethods.begin(), 
                                                      _allowedMethods.end(), upperMethod);
    if (it == _allowedMethods.end()) {
        _allowedMethods.push_back(upperMethod);
    }
}

void Location::setAutoindex(bool autoindex) { _autoindex = autoindex; }
void Location::setUploadPath(const std::string& uploadPath) { _uploadPath = uploadPath; }
void Location::setCgiPath(const std::string& cgiPath) { _cgiPath = cgiPath; }
void Location::setCgiExtension(const std::string& cgiExtension) { _cgiExtension = cgiExtension; }
void Location::setMaxBodySize(size_t maxBodySize) { _maxBodySize = maxBodySize; }

bool Location::isMethodAllowed(const std::string& method) const {
    // Treat HEAD as GET for permission checks: HEAD should be allowed
    // wherever GET is allowed. Centralizing this here ensures all callers
    // benefit without needing to remember to map HEAD->GET.
    std::string upperMethod = Utils::toUpperCase(method);
    if (upperMethod == "HEAD") upperMethod = "GET";
    return std::find(_allowedMethods.begin(), _allowedMethods.end(), upperMethod) != _allowedMethods.end();
}

bool Location::matches(const std::string& uri) const {
    if (_path == "/") return true;
    
    // Check if URI starts with location path
    if (uri.length() < _path.length()) return false;
    
    std::string uriPrefix = uri.substr(0, _path.length());
    if (uriPrefix != _path) return false;
    
    // Exact match or path ends with '/' or next character is '/'
    return (uri.length() == _path.length() || 
            _path[_path.length() - 1] == '/' || 
            uri[_path.length()] == '/');
}

std::string Location::getFullPath(const std::string& uri) const {
    // Map the request URI to the filesystem by stripping the location path
    // prefix and joining the remainder to the location root. Example:
    //   location /directory { root ./YoupiBanane; }
    //   URI /directory/youpi.bad_extension -> ./YoupiBanane/youpi.bad_extension
    // For the root location '/', we simply join the uri to the root.

    std::string relative = uri;

    if (_path != "/") {
        // Normalize: ensure _path ends with no trailing slash (except for "/")
        std::string normPath = _path;
        if (normPath.size() > 1 && normPath[normPath.size() - 1] == '/') {
            normPath.erase(normPath.size() - 1);
        }

        if (relative.compare(0, normPath.size(), normPath) == 0) {
            relative = relative.substr(normPath.size());
            if (relative.empty()) relative = "/"; // keep a separator
        }
    }

    // Join root and relative
    std::string fullPath = _root;
    bool needSlash = (!_root.empty() && _root[_root.length() - 1] != '/' &&
                      !relative.empty() && relative[0] != '/');
    if (needSlash) fullPath += "/";
    if (!relative.empty() && relative[0] == '/' && !_root.empty() && _root[_root.length() - 1] == '/') {
        fullPath += relative.substr(1);
    } else {
        fullPath += relative;
    }
    return fullPath;
}

bool Location::isCgiRequest(const std::string& uri) const {
    if (_cgiExtension.empty()) {
        return false;
    }
    
    std::string extension = Utils::getFileExtension(uri);
    return extension == _cgiExtension;
}
