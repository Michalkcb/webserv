#ifndef SESSION_HPP
#define SESSION_HPP

#include "webserv.hpp"
#include "Cookie.hpp"

class Session {
private:
    std::string _sessionId;
    std::map<std::string, std::string> _data;
    time_t _createdAt;
    time_t _lastAccessed;
    int _maxAge;
    bool _isValid;

    static std::map<std::string, Session> _sessions;
    static std::string _generateSessionId();

public:
    Session();
    Session(const std::string& sessionId);
    Session(const Session& other);
    Session& operator=(const Session& other);
    ~Session();

    // Getters
    const std::string& getSessionId() const;
    time_t getCreatedAt() const;
    time_t getLastAccessed() const;
    int getMaxAge() const;
    bool isValid() const;

    // Session data management
    void set(const std::string& key, const std::string& value);
    std::string get(const std::string& key) const;
    bool has(const std::string& key) const;
    void remove(const std::string& key);
    void clear();

    // Session lifecycle
    void touch();
    bool isExpired() const;
    void destroy();

    // Cookie integration
    Cookie createSessionCookie() const;

    // Static session management
    static Session* getSession(const std::string& sessionId);
    static Session* createSession();
    static void destroySession(const std::string& sessionId);
    static void cleanupExpiredSessions();
    static size_t getSessionCount();
};

#endif
