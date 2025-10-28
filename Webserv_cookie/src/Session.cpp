#include "Session.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
// For secure random seed fallback
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

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
    // Generate a random session ID using /dev/urandom when possible.
    const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const size_t chars_len = chars.length();
    const int ID_LEN = 32;
    std::string sessionId;
    sessionId.reserve(ID_LEN);

    int fd = open("/dev/urandom", O_RDONLY);
    if (fd != -1) {
        unsigned char buf[ID_LEN];
        ssize_t r = read(fd, buf, ID_LEN);
        close(fd);
        if (r == ID_LEN) {
            for (int i = 0; i < ID_LEN; ++i) {
                sessionId += chars[buf[i] % chars_len];
            }
            return sessionId;
        }
        // fallthrough to fallback if read failed
    }

    // Fallback: deterministic xorshift32 PRNG seeded from time + stack address.
    // Avoid using getpid()/rand()/srand() which are not allowed by the subject.
    unsigned int seed = (unsigned int)time(NULL);
    // mix in some stack address entropy
    // Use pointer value mixed into seed without relying on uintptr_t typedef
    unsigned long adr = (unsigned long)&seed;
    seed ^= (unsigned int)(adr & 0xFFFFFFFFUL);

    // xorshift32
    unsigned int x = seed ? seed : 0xdeadbeefu;
    for (int i = 0; i < ID_LEN; ++i) {
        // advance PRNG
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        sessionId += chars[x % chars_len];
    }
    return sessionId;
}
