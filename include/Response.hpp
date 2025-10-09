#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include "webserv.hpp"
#include "Cookie.hpp"

class Response {
private:
    int _statusCode;
    std::string _statusMessage;
    Headers _headers;
    std::string _body;
    bool _isComplete;
    size_t _bytesSent;

public:
    Response();
    Response(int statusCode);
    Response(const Response& other);
    Response& operator=(const Response& other);
    ~Response();

    // Setters
    void setStatusCode(int statusCode);
    void setHeader(const std::string& name, const std::string& value);
    void setBody(const std::string& body);
    void setBody(const char* data, size_t length);
    void appendBody(const std::string& data);
    void setComplete(bool complete);

    // Cookie support (BONUS)
    void setCookie(const Cookie& cookie);
    void addCookie(const Cookie& cookie);

    // Getters
    int getStatusCode() const;
    const std::string& getStatusMessage() const;
    const Headers& getHeaders() const;
    const std::string& getBody() const;
    std::string getHeader(const std::string& name) const;
    bool hasHeader(const std::string& name) const;
    bool isComplete() const;
    size_t getBytesSent() const;
    size_t getContentLength() const;

    // Build response
    std::string toString(bool includeBody = true) const;
    void reset();
    void addDefaultHeaders();

    // Static helper methods
    static Response createErrorResponse(int statusCode, const std::string& errorPage = "");
    static Response createRedirectResponse(int statusCode, const std::string& location);
    static Response createFileResponse(const std::string& filename, const std::string& mimeType = "");
    static Response createDirectoryListingResponse(const std::string& path, const std::string& uri);

    // Send tracking
    void addBytesSent(size_t bytes);
    bool isFullySent() const;
};

#endif
