#include "Session.hpp"
#include "Utils.hpp"
#include "Logger.hpp"

// Static member initialization
std::map<std::string, Session> Session::_sessions;

Session::Session() : _maxAge(3600), _isValid(false) {
    _createdAt = _lastAccessed = time(NULL);
}

Session::Session(const std::string& sessionId) 
    : _sessionId(sessionId), _maxAge(3600), _isValid(true) {
    _createdAt = _lastAccessed = time(NULL);
}

Session::Session(const Session& other) {
    *this = other;
}

Session& Session::operator=(const Session& other) {
    if (this != &other) {
        _sessionId = other._sessionId;
        _data = other._data;
        _createdAt = other._createdAt;
        _lastAccessed = other._lastAccessed;
        _maxAge = other._maxAge;
        _isValid = other._isValid;
    }
    return *this;
}

Session::~Session() {
}

// Getters
const std::string& Session::getSessionId() const { return _sessionId; }
time_t Session::getCreatedAt() const { return _createdAt; }
time_t Session::getLastAccessed() const { return _lastAccessed; }
int Session::getMaxAge() const { return _maxAge; }
bool Session::isValid() const { return _isValid && !isExpired(); }

// Session data management
void Session::set(const std::string& key, const std::string& value) {
    _data[key] = value;
    touch();
}

std::string Session::get(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = _data.find(key);
    return (it != _data.end()) ? it->second : "";
}

bool Session::has(const std::string& key) const {
    return _data.find(key) != _data.end();
}

void Session::remove(const std::string& key) {
    _data.erase(key);
    touch();
}

void Session::clear() {
    _data.clear();
    touch();
}

// Session lifecycle
void Session::touch() {
    _lastAccessed = time(NULL);
}

bool Session::isExpired() const {
    return (time(NULL) - _lastAccessed) > _maxAge;
}

void Session::destroy() {
    _isValid = false;
    _data.clear();
}

// Cookie integration
Cookie Session::createSessionCookie() const {
    Cookie cookie("SESSIONID", _sessionId);
    cookie.setPath("/");
    cookie.setHttpOnly(true);
    cookie.setMaxAge(_maxAge);
    return cookie;
}

// Static session management
Session* Session::getSession(const std::string& sessionId) {
    std::map<std::string, Session>::iterator it = _sessions.find(sessionId);
    if (it != _sessions.end() && it->second.isValid()) {
        it->second.touch();
        return &it->second;
    }
    return NULL;
}

Session* Session::createSession() {
    std::string sessionId = _generateSessionId();
    Session session(sessionId);
    _sessions[sessionId] = session;
    Logger::debug("Created new session: " + sessionId);
    return &_sessions[sessionId];
}

void Session::destroySession(const std::string& sessionId) {
    std::map<std::string, Session>::iterator it = _sessions.find(sessionId);
    if (it != _sessions.end()) {
        it->second.destroy();
        _sessions.erase(it);
        Logger::debug("Destroyed session: " + sessionId);
    }
}

void Session::cleanupExpiredSessions() {
    std::vector<std::string> expiredSessions;
    
    for (std::map<std::string, Session>::iterator it = _sessions.begin(); 
         it != _sessions.end(); ++it) {
        if (it->second.isExpired()) {
            expiredSessions.push_back(it->first);
        }
    }
    
    for (size_t i = 0; i < expiredSessions.size(); ++i) {
        destroySession(expiredSessions[i]);
    }
    
    if (!expiredSessions.empty()) {
        Logger::debug("Cleaned up " + Utils::intToString(expiredSessions.size()) + " expired sessions");
    }
}

size_t Session::getSessionCount() {
    return _sessions.size();
}

std::string Session::_generateSessionId() {
    // Generate a random session ID
    std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string sessionId;
    
    srand(time(NULL) + rand());
    for (int i = 0; i < 32; ++i) {
        sessionId += chars[rand() % chars.length()];
    }
    
    return sessionId;
}
