#include "Response.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include <sstream> // add

Response::Response() : _statusCode(200), _statusMessage("OK"), _isComplete(false), _bytesSent(0) {
    addDefaultHeaders();
}

Response::Response(int statusCode) : _statusCode(statusCode), _isComplete(false), _bytesSent(0) {
    _statusMessage = Utils::getStatusMessage(statusCode);
    addDefaultHeaders();
}

Response::Response(const Response& other) {
    *this = other;
}

Response& Response::operator=(const Response& other) {
    if (this != &other) {
        _statusCode = other._statusCode;
        _statusMessage = other._statusMessage;
        _headers = other._headers;
        _body = other._body;
        _isComplete = other._isComplete;
        _bytesSent = other._bytesSent;
    }
    return *this;
}

Response::~Response() {
}

void Response::setStatusCode(int statusCode) {
    _statusCode = statusCode;
    _statusMessage = Utils::getStatusMessage(statusCode);
}

void Response::setHeader(const std::string& name, const std::string& value) {
    _headers[name] = value;
}

void Response::setBody(const std::string& body) {
    _body = body;
    setHeader("Content-Length", Utils::intToString(_body.length()));
}

void Response::setBody(const char* data, size_t length) {
    _body.assign(data, length);
    setHeader("Content-Length", Utils::intToString(_body.length()));
}

void Response::appendBody(const std::string& data) {
    _body += data;
    setHeader("Content-Length", Utils::intToString(_body.length()));
}

void Response::setComplete(bool complete) {
    _isComplete = complete;
}

// Cookie support (BONUS)
void Response::setCookie(const Cookie& cookie) {
    if (cookie.isValid()) {
        setHeader("Set-Cookie", cookie.toString());
    }
}

void Response::addCookie(const Cookie& cookie) {
    setCookie(cookie);
}

// Getters
int Response::getStatusCode() const { return _statusCode; }
const std::string& Response::getStatusMessage() const { return _statusMessage; }
const Headers& Response::getHeaders() const { return _headers; }
const std::string& Response::getBody() const { return _body; }
bool Response::isComplete() const { return _isComplete; }
size_t Response::getBytesSent() const { return _bytesSent; }
size_t Response::getContentLength() const { return _body.length(); }

std::string Response::getHeader(const std::string& name) const {
    Headers::const_iterator it = _headers.find(name);
    return (it != _headers.end()) ? it->second : "";
}

bool Response::hasHeader(const std::string& name) const {
    return _headers.find(name) != _headers.end();
}

// Case-insensitive getter helpers. This preserves existing header storage
// (case-sensitive keys), while allowing robust lookups for names like
// "Content-Length" vs "content-length" coming from external modules.
std::string Response::getHeaderCI(const std::string& name) const {
    if (name.empty()) return "";
    // Fast path: exact match
    Headers::const_iterator it = _headers.find(name);
    if (it != _headers.end()) return it->second;
    // Fallback: linear scan with lower-cased compare (header set sizes are small)
    std::string target = Utils::toLowerCase(name);
    for (Headers::const_iterator jt = _headers.begin(); jt != _headers.end(); ++jt) {
        if (Utils::toLowerCase(jt->first) == target) return jt->second;
    }
    return "";
}

bool Response::hasHeaderCI(const std::string& name) const {
    if (name.empty()) return false;
    if (_headers.find(name) != _headers.end()) return true;
    std::string target = Utils::toLowerCase(name);
    for (Headers::const_iterator jt = _headers.begin(); jt != _headers.end(); ++jt) {
        if (Utils::toLowerCase(jt->first) == target) return true;
    }
    return false;
}

// In src/Response.cpp

// ==========================================================
// REPLACE your old toString() function with these TWO functions
// ==========================================================

/**
 * @brief This is the main implementation. It builds the full response string.
 * @param includeBody If true, the response body is included. If false, only
 *                    the status line and headers are returned.
 * @return The formatted HTTP response string.
 */
std::string Response::toString(bool withBody) const {
    std::ostringstream ss;
    ss << "HTTP/1.1" << " " << _statusCode << " " << Utils::getStatusMessage(_statusCode) << "\r\n";

    const std::string cl = getHeader("Content-Length");
    const std::string teLower = Utils::toLowerCase(getHeader("Transfer-Encoding"));
    const bool skipTE = (!cl.empty() || teLower == "identity");

    for (Headers::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
        std::string keyLower = Utils::toLowerCase(it->first);
        if (keyLower == "transfer-encoding" && skipTE) continue;
        ss << it->first << ": " << it->second << "\r\n";
    }

    ss << "\r\n";
    if (withBody) ss << _body;
    return ss.str();
}

void Response::reset() {
    _statusCode = 200;
    _statusMessage = "OK";
    _headers.clear();
    _body.clear();
    _isComplete = false;
    _bytesSent = 0;
    addDefaultHeaders();
}

void Response::addDefaultHeaders() {
    setHeader("Server", SERVER_NAME);
    setHeader("Date", Utils::getCurrentTime());
    // Connection header will be set later based on keep-alive status
}

Response Response::createErrorResponse(int statusCode, const std::string& errorPage) {
    Response response(statusCode);
    
    // Special handling for 405 Method Not Allowed:
    // Some clients/testers don't consume the response body for 405 and immediately
    // issue the next request on the same connection. If we send a body here, those
    // bytes may arrive while the client believes the channel is idle, leading to
    // "unsolicited response on idle HTTP channel" and misaligned parsing for the
    // next request (e.g., HEAD /). To avoid that, return an empty body and an
    // explicit Content-Length: 0 for 405 responses.
    if (statusCode == HTTP_METHOD_NOT_ALLOWED) {
        response.setHeader("Content-Type", "text/plain");
        response.setBody("");               // Sets Content-Length: 0
        response.setComplete(true);
        return response;
    }

    std::string body;
    if (!errorPage.empty() && Utils::fileExists(errorPage)) {
        body = Utils::readFile(errorPage);
        std::string mimeType = Utils::getMimeType(Utils::getFileExtension(errorPage));
        response.setHeader("Content-Type", mimeType);
    } else {
        // Default error page
        std::stringstream html;
        html << "<!DOCTYPE html>\n";
        html << "<html><head><title>" << statusCode << " " << Utils::getStatusMessage(statusCode) << "</title></head>\n";
        html << "<body><h1>" << statusCode << " " << Utils::getStatusMessage(statusCode) << "</h1>\n";
        html << "<hr><p>" << SERVER_NAME << "</p></body></html>\n";
        body = html.str();
        response.setHeader("Content-Type", "text/html");
    }
    
    response.setBody(body);
    response.setComplete(true);
    return response;
}

Response Response::createRedirectResponse(int statusCode, const std::string& location) {
    Response response(statusCode);
    response.setHeader("Location", location);
    
    std::stringstream html;
    html << "<!DOCTYPE html>\n";
    html << "<html><head><title>" << statusCode << " " << Utils::getStatusMessage(statusCode) << "</title></head>\n";
    html << "<body><h1>" << statusCode << " " << Utils::getStatusMessage(statusCode) << "</h1>\n";
    html << "<p>The document has moved <a href=\"" << location << "\">here</a>.</p>\n";
    html << "<hr><p>" << SERVER_NAME << "</p></body></html>\n";
    
    response.setHeader("Content-Type", "text/html");
    response.setBody(html.str());
    response.setComplete(true);
    return response;
}

Response Response::createFileResponse(const std::string& filename, const std::string& mimeType) {
    Response response;
    
    if (!Utils::fileExists(filename)) {
        return createErrorResponse(HTTP_NOT_FOUND);
    }
    
    std::string content = Utils::readFile(filename);
    if (content.empty()) {
        Logger::error("Failed to read file: " + filename);
        return createErrorResponse(HTTP_INTERNAL_SERVER_ERROR);
    }
    
    std::string contentType = mimeType;
    if (contentType.empty()) {
        contentType = Utils::getMimeType(Utils::getFileExtension(filename));
    }
    
    response.setHeader("Content-Type", contentType);
    response.setBody(content);
    response.setComplete(true);
    
    return response;
}

Response Response::createDirectoryListingResponse(const std::string& path, const std::string& uri) {
    Response response;
    
    if (!Utils::isDirectory(path)) {
        return createErrorResponse(HTTP_NOT_FOUND);
    }
    
    std::string html = Utils::generateDirectoryListing(path, uri);
    response.setHeader("Content-Type", "text/html");
    response.setBody(html);
    response.setComplete(true);
    
    return response;
}

void Response::addBytesSent(size_t bytes) {
    _bytesSent += bytes;
}

bool Response::isFullySent() const {
    std::string responseStr = toString();
    return _bytesSent >= responseStr.length();
}
