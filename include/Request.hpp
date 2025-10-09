#ifndef REQUEST_HPP
#define REQUEST_HPP

#include "webserv.hpp"

class Request {
public:
    enum ParseState {
        PARSE_REQUEST_LINE,
        PARSE_HEADERS,
        PARSE_BODY,
        PARSE_COMPLETE,
        PARSE_ERROR
    };

private:
    std::string _method;
    std::string _uri;
    std::string _version;
    Headers _headers;
    std::string _body;
    std::string _rawRequest;
    ParseState _state;
    bool _isChunked;
    size_t _contentLength;
    size_t _bodyReceived;
    std::string _remainingData;
    // Per-request state for chunked parsing
    std::string _chunkBuffer;
    size_t _expectedChunkSize;
    bool _readingChunkSize;
    time_t _chunkStartTime;  // Track when chunked parsing began for timeout detection

    void _parseRequestLine(const std::string& line);
    void _parseHeader(const std::string& line);
    void _parseChunkedBody(const std::string& data);
    bool _isValidMethod(const std::string& method) const;
    bool _isValidUri(const std::string& uri) const;
    bool _isValidVersion(const std::string& version) const;

public:
    Request();
    Request(const Request& other);
    Request& operator=(const Request& other);
    ~Request();

    // Parsing methods
    ParseState parse(const std::string& data);
    void reset();
    bool isComplete() const;
    bool hasError() const;
    void clearBody();
    void removeHeader(const std::string& name);
    void finalizeBody();

    // Getters
    const std::string& getMethod() const;
    const std::string& getUri() const;
    const std::string& getVersion() const;
    const Headers& getHeaders() const;
    const std::string& getBody() const;
    const std::string& getRawRequest() const;
    ParseState getState() const;
    std::string getHeader(const std::string& name) const;
    bool hasHeader(const std::string& name) const;
    size_t getContentLength() const;
    bool isChunked() const;
    bool isStreamingMode() const;
    bool hasChunkedTimeout(int timeoutSeconds = 60) const;  // Check if chunked upload has timed out

    // Setters
    void setMethod(const std::string& method);
    void setUri(const std::string& uri);
    void setVersion(const std::string& version);
    void setHeader(const std::string& name, const std::string& value);
    void setBody(const std::string& body);

    // Utility methods
    std::string getQueryString() const;
    std::string getPath() const;
    std::map<std::string, std::string> getQueryParams() const;

};

#endif
