#include "Cookie.hpp"
#include "Utils.hpp"
#include "Logger.hpp"

Cookie::Cookie() : _maxAge(-1), _secure(false), _httpOnly(false) {
}

Cookie::Cookie(const std::string& name, const std::string& value) 
    : _name(name), _value(value), _maxAge(-1), _secure(false), _httpOnly(false) {
}

Cookie::Cookie(const Cookie& other) {
    *this = other;
}

Cookie& Cookie::operator=(const Cookie& other) {
    if (this != &other) {
        _name = other._name;
        _value = other._value;
        _domain = other._domain;
        _path = other._path;
        _expires = other._expires;
        _maxAge = other._maxAge;
        _secure = other._secure;
        _httpOnly = other._httpOnly;
        _sameSite = other._sameSite;
    }
    return *this;
}

Cookie::~Cookie() {
}

// Getters
const std::string& Cookie::getName() const { return _name; }
const std::string& Cookie::getValue() const { return _value; }
const std::string& Cookie::getDomain() const { return _domain; }
const std::string& Cookie::getPath() const { return _path; }
const std::string& Cookie::getExpires() const { return _expires; }
int Cookie::getMaxAge() const { return _maxAge; }
bool Cookie::isSecure() const { return _secure; }
bool Cookie::isHttpOnly() const { return _httpOnly; }
const std::string& Cookie::getSameSite() const { return _sameSite; }

// Setters
void Cookie::setName(const std::string& name) { _name = name; }
void Cookie::setValue(const std::string& value) { _value = value; }
void Cookie::setDomain(const std::string& domain) { _domain = domain; }
void Cookie::setPath(const std::string& path) { _path = path; }
void Cookie::setExpires(const std::string& expires) { _expires = expires; }
void Cookie::setMaxAge(int maxAge) { _maxAge = maxAge; }
void Cookie::setSecure(bool secure) { _secure = secure; }
void Cookie::setHttpOnly(bool httpOnly) { _httpOnly = httpOnly; }
void Cookie::setSameSite(const std::string& sameSite) { _sameSite = sameSite; }

std::string Cookie::toString() const {
    if (_name.empty()) return "";
    
    std::string cookieStr = _name + "=" + _value;
    
    if (!_domain.empty()) cookieStr += "; Domain=" + _domain;
    if (!_path.empty()) cookieStr += "; Path=" + _path;
    if (!_expires.empty()) cookieStr += "; Expires=" + _expires;
    if (_maxAge >= 0) cookieStr += "; Max-Age=" + Utils::intToString(_maxAge);
    if (_secure) cookieStr += "; Secure";
    if (_httpOnly) cookieStr += "; HttpOnly";
    if (!_sameSite.empty()) cookieStr += "; SameSite=" + _sameSite;
    
    return cookieStr;
}

bool Cookie::isValid() const {
    return !_name.empty() && !_value.empty();
}

Cookie Cookie::parseCookieHeader(const std::string& cookieHeader) {
    Cookie cookie;
    
    std::vector<std::string> parts = Utils::split(cookieHeader, ";");
    if (parts.empty()) return cookie;
    
    // Parse name=value pair
    std::vector<std::string> nameValue = Utils::split(Utils::trim(parts[0]), "=");
    if (nameValue.size() == 2) {
        cookie.setName(Utils::trim(nameValue[0]));
        cookie.setValue(Utils::trim(nameValue[1]));
    }
    
    // Parse attributes
    for (size_t i = 1; i < parts.size(); ++i) {
        std::string part = Utils::trim(parts[i]);
        std::vector<std::string> attr = Utils::split(part, "=");
        
        if (attr.size() == 1) {
            std::string attrName = Utils::toLowerCase(Utils::trim(attr[0]));
            if (attrName == "secure") cookie.setSecure(true);
            else if (attrName == "httponly") cookie.setHttpOnly(true);
        } else if (attr.size() == 2) {
            std::string attrName = Utils::toLowerCase(Utils::trim(attr[0]));
            std::string attrValue = Utils::trim(attr[1]);
            
            if (attrName == "domain") cookie.setDomain(attrValue);
            else if (attrName == "path") cookie.setPath(attrValue);
            else if (attrName == "expires") cookie.setExpires(attrValue);
            else if (attrName == "max-age") cookie.setMaxAge(Utils::stringToInt(attrValue));
            else if (attrName == "samesite") cookie.setSameSite(attrValue);
        }
    }
    
    return cookie;
}

std::map<std::string, std::string> Cookie::parseCookies(const std::string& cookieHeader) {
    std::map<std::string, std::string> cookies;
    
    std::vector<std::string> parts = Utils::split(cookieHeader, ";");
    for (size_t i = 0; i < parts.size(); ++i) {
        std::vector<std::string> nameValue = Utils::split(Utils::trim(parts[i]), "=");
        if (nameValue.size() == 2) {
            cookies[Utils::trim(nameValue[0])] = Utils::trim(nameValue[1]);
        }
    }
    
    return cookies;
}

std::string Cookie::formatSetCookieHeader(const Cookie& cookie) {
    return cookie.toString();
}

std::string Cookie::urlEncode(const std::string& str) {
    return Utils::urlEncode(str);
}

std::string Cookie::urlDecode(const std::string& str) {
    return Utils::urlDecode(str);
}
