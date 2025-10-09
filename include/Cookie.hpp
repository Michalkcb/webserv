#ifndef COOKIE_HPP
#define COOKIE_HPP

#include "webserv.hpp"

class Cookie {
private:
    std::string _name;
    std::string _value;
    std::string _domain;
    std::string _path;
    std::string _expires;
    int _maxAge;
    bool _secure;
    bool _httpOnly;
    std::string _sameSite;

public:
    Cookie();
    Cookie(const std::string& name, const std::string& value);
    Cookie(const Cookie& other);
    Cookie& operator=(const Cookie& other);
    ~Cookie();

    // Getters
    const std::string& getName() const;
    const std::string& getValue() const;
    const std::string& getDomain() const;
    const std::string& getPath() const;
    const std::string& getExpires() const;
    int getMaxAge() const;
    bool isSecure() const;
    bool isHttpOnly() const;
    const std::string& getSameSite() const;

    // Setters
    void setName(const std::string& name);
    void setValue(const std::string& value);
    void setDomain(const std::string& domain);
    void setPath(const std::string& path);
    void setExpires(const std::string& expires);
    void setMaxAge(int maxAge);
    void setSecure(bool secure);
    void setHttpOnly(bool httpOnly);
    void setSameSite(const std::string& sameSite);

    // Cookie parsing and formatting
    std::string toString() const;
    bool isValid() const;

    // Static utility methods
    static Cookie parseCookieHeader(const std::string& cookieHeader);
    static std::map<std::string, std::string> parseCookies(const std::string& cookieHeader);
    static std::string formatSetCookieHeader(const Cookie& cookie);
    static std::string urlEncode(const std::string& str);
    static std::string urlDecode(const std::string& str);
};

#endif
