#include "Request.hpp"
#include "Utils.hpp"
#include "Logger.hpp"

Request::Request() : _state(PARSE_REQUEST_LINE), _isChunked(false), 
                     _contentLength(0), _bodyReceived(0), _expectedChunkSize(0), _readingChunkSize(true), _chunkStartTime(0) {
}

Request::Request(const Request& other) {
    *this = other;
}

Request& Request::operator=(const Request& other) {
    if (this != &other) {
        _method = other._method;
        _uri = other._uri;
        _version = other._version;
        _headers = other._headers;
        _body = other._body;
        _rawRequest = other._rawRequest;
        _state = other._state;
        _isChunked = other._isChunked;
        _contentLength = other._contentLength;
        _bodyReceived = other._bodyReceived;
        _remainingData = other._remainingData;
    _chunkBuffer = other._chunkBuffer;
    _expectedChunkSize = other._expectedChunkSize;
    _readingChunkSize = other._readingChunkSize;
    _chunkStartTime = other._chunkStartTime;
    }
    return *this;
}

Request::~Request() {
}

Request::ParseState Request::parse(const std::string& data) {
    // Ogranicz rozmiar bufora surowego żądania (np. 64KB), żeby nie pożerać pamięci
    const size_t RAW_CAP = 64 * 1024;
    if (_rawRequest.size() < RAW_CAP) {
        size_t can = RAW_CAP - _rawRequest.size();
        _rawRequest.append(data.c_str(), std::min(can, data.size()));
    }
    std::string buffer = _remainingData + data;
    _remainingData.clear();
    
    if (_state == PARSE_REQUEST_LINE) {
        // Be tolerant: skip any leading empty lines (CRLF or LF) per RFC 7230 3.5
        while (!buffer.empty()) {
            if (buffer.compare(0, 2, "\r\n") == 0) {
                buffer.erase(0, 2);
                continue;
            }
            if (buffer[0] == '\n') {
                buffer.erase(0, 1);
                continue;
            }
            break;
        }
        size_t pos = buffer.find("\r\n");
        if (pos != std::string::npos) {
            try {
                _parseRequestLine(buffer.substr(0, pos));
                _state = PARSE_HEADERS;
                buffer = buffer.substr(pos + 2);
            } catch (const std::exception& e) {
                Logger::error("Failed to parse request line: " + std::string(e.what()));
                _state = PARSE_ERROR;
                return _state;
            }
        } else {
            _remainingData = buffer;
            return _state;
        }
    }
    
    if (_state == PARSE_HEADERS) {
        size_t headersEnd = buffer.find("\r\n\r\n");
        if (headersEnd != std::string::npos) {
            std::string headersSection = buffer.substr(0, headersEnd);
            std::vector<std::string> headerLines = Utils::split(headersSection, "\r\n");
            
            for (size_t i = 0; i < headerLines.size(); ++i) {
                try {
                    _parseHeader(headerLines[i]);
                } catch (const std::exception& e) {
                    Logger::error("Failed to parse header: " + std::string(e.what()));
                    _state = PARSE_ERROR;
                    return _state;
                }
            }
            
            // DEBUG: Log all received headers for troubleshooting
            Logger::debug("=== All HTTP Headers Received ===");
            for (Headers::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
                Logger::debug("Header: '" + it->first + "' = '" + it->second + "'");
            }
            Logger::debug("=== End Headers ===");
            
            // Check for Content-Length or Transfer-Encoding
            if (hasHeader("content-length")) {
                _contentLength = Utils::stringToInt(getHeader("content-length"));
                if (_contentLength > 0) {
                    _state = PARSE_BODY;
                } else {
                    _state = PARSE_COMPLETE;
                }
            } else if (hasHeader("transfer-encoding") && 
                       Utils::toLowerCase(getHeader("transfer-encoding")) == "chunked") {
                _isChunked = true;
                _chunkStartTime = time(NULL);  // Start timing chunked uploads
                _state = PARSE_BODY;
            } else {
                _state = PARSE_COMPLETE;
            }
            
            buffer = buffer.substr(headersEnd + 4);
        } else {
            _remainingData = buffer;
            return _state;
        }
    }
    
    if (_state == PARSE_BODY && !buffer.empty()) {
        // For chunked uploads, treat any arrival of body bytes as activity
        // and reset the inactivity timer to avoid false timeouts during
        // long, legitimate uploads.
        if (_isChunked) {
            _chunkStartTime = time(NULL);
        }
        if (_isChunked) {
            _parseChunkedBody(buffer);
            if (_state == PARSE_COMPLETE) {
                finalizeBody(); // set Content-Length, drop Transfer-Encoding
            }
        } else {
            size_t bytesToRead = std::min(buffer.length(), _contentLength - _bodyReceived);
            _body += buffer.substr(0, bytesToRead);
            _bodyReceived += bytesToRead;

            if (_bodyReceived >= _contentLength) {
                _state = PARSE_COMPLETE;
                finalizeBody(); // normalize headers for CGI
                Logger::debug("Request parsing complete: received " +
                              Utils::intToString(_bodyReceived) + " of " +
                              Utils::intToString(_contentLength) + " bytes");
            }

            if (buffer.length() > bytesToRead) {
                _remainingData = buffer.substr(bytesToRead);
            }
        }
    }
    
    return _state;
}

void Request::discardBodyPrefix(size_t n) {
    if (n == 0) return;
    if (n >= _body.size()) {
        _body.clear();
        _bodyReceived = 0; // reset liczników do aktualnej długości body
        return;
    }
    _body.erase(0, n);
    if (_bodyReceived >= n) _bodyReceived -= n; else _bodyReceived = 0;
}

void Request::_parseRequestLine(const std::string& line) {
    // Be tolerant to multiple spaces/tabs between tokens and trailing spaces
    std::string trimmed = Utils::trim(line);
    if (trimmed.empty()) {
        throw std::runtime_error("Invalid request line format");
    }

    std::istringstream iss(trimmed);
    std::string methodToken, targetToken, versionToken;
    if (!(iss >> methodToken >> targetToken >> versionToken)) {
        throw std::runtime_error("Invalid request line format");
    }

    _method = Utils::toUpperCase(methodToken);

    // Support absolute-form request-target (e.g., GET http://host:port/path HTTP/1.1)
    if (targetToken.compare(0, 7, "http://") == 0 || targetToken.compare(0, 8, "https://") == 0) {
        size_t schemeEnd = targetToken.find("://");
        size_t pathPos = std::string::npos;
        if (schemeEnd != std::string::npos) {
            // Find first '/' after the authority to extract the path
            pathPos = targetToken.find('/', schemeEnd + 3);
        }
        if (pathPos != std::string::npos) {
            targetToken = targetToken.substr(pathPos);
        } else {
            // No explicit path in absolute URI -> use '/'
            targetToken = "/";
        }
    }

    _uri = targetToken;
    _version = versionToken;

    if (!_isValidMethod(_method)) {
        throw std::runtime_error("Invalid HTTP method");
    }
    if (!_isValidUri(_uri)) {
        throw std::runtime_error("Invalid URI");
    }
    if (!_isValidVersion(_version)) {
        throw std::runtime_error("Invalid HTTP version");
    }
}

void Request::_parseHeader(const std::string& line) {
    // Skip empty lines or lines without colons (more tolerant parsing for ubuntu_tester)
    std::string trimmedLine = Utils::trim(line);
    if (trimmedLine.empty()) {
        return; // Skip empty lines
    }
    
    size_t colonPos = line.find(':');
    if (colonPos == std::string::npos) {
        // Log and skip malformed header lines instead of throwing
        Logger::debug("Skipping malformed header line: '" + line + "'");
        return;
    }
    
    std::string name = Utils::trim(line.substr(0, colonPos));
    std::string value = Utils::trim(line.substr(colonPos + 1));
    
    if (name.empty()) {
        Logger::debug("Skipping header with empty name: '" + line + "'");
        return;
    }
    
    _headers[Utils::toLowerCase(name)] = value;
}

void Request::_parseChunkedBody(const std::string& data) {
    // Use a local working buffer constructed from the supplied data.
    // parse() supplies the concatenation of any previous remaining data and the
    // new data chunk, so operate on a local buffer to avoid double-appending
    // into the member _chunkBuffer which previously caused duplication.
    std::string buffer = data;

    while (!buffer.empty()) {
        if (_readingChunkSize) {
            size_t pos = buffer.find("\r\n");
            if (pos == std::string::npos) {
                break; // Wait for more data
            }

            std::string chunkSizeStr = buffer.substr(0, pos);
            _expectedChunkSize = Utils::hexToSize(chunkSizeStr);
            Logger::debug("Chunked parser: found chunk size header '" + chunkSizeStr + "' -> " + Utils::intToString(_expectedChunkSize));
            // Activity observed: reset timer when we successfully parse a chunk size
            _chunkStartTime = time(NULL);
            buffer = buffer.substr(pos + 2);

            if (_expectedChunkSize == 0) {
                // End of chunks
                Logger::debug("Chunked parser: received terminating zero-size chunk");
                _state = PARSE_COMPLETE;
                break;
            }
            _readingChunkSize = false;
        } else {
            if (buffer.length() >= _expectedChunkSize + 2) {
                _body += buffer.substr(0, _expectedChunkSize);
                Logger::debug("Chunked parser: consumed chunk of size " + Utils::intToString(_expectedChunkSize));
                // Activity observed: reset timer when we consume a full chunk
                _chunkStartTime = time(NULL);
                buffer = buffer.substr(_expectedChunkSize + 2); // +2 for \r\n
                _readingChunkSize = true;
            } else {
                break; // Wait for more data
            }
        }
    }

    // any leftover partial data (incomplete chunk size or incomplete chunk data)
    // should be saved for the next parse call
    _remainingData = buffer;
}

void Request::removeHeader(const std::string& name) {
    _headers.erase(Utils::toLowerCase(name));
}

// Normalize headers after body is fully parsed
void Request::finalizeBody() {
    if (_isChunked) {
        // You already decoded into _body in _parseChunkedBody
        removeHeader("transfer-encoding");
        setHeader("content-length", Utils::intToString((int)_body.size()));
    }
}

bool Request::_isValidMethod(const std::string& method) const {
    return (method == "GET" || method == "POST" || method == "DELETE" || 
            method == "PUT" || method == "HEAD" || method == "OPTIONS");
}

bool Request::_isValidUri(const std::string& uri) const {
    return !uri.empty() && uri[0] == '/';
}

bool Request::_isValidVersion(const std::string& version) const {
    return version == "HTTP/1.1" || version == "HTTP/1.0";
}

void Request::reset() {
    _method.clear();
    _uri.clear();
    _version.clear();
    _headers.clear();
    _body.clear();
    _rawRequest.clear();
    _remainingData.clear();
    _state = PARSE_REQUEST_LINE;
    _isChunked = false;
    _contentLength = 0;
    _bodyReceived = 0;
    _chunkBuffer.clear();
    _expectedChunkSize = 0;
    _readingChunkSize = true;
    _chunkStartTime = 0;  // Reset chunk timing
}

bool Request::isComplete() const {
    return _state == PARSE_COMPLETE;
}

bool Request::hasError() const {
    return _state == PARSE_ERROR;
}

// Getters
const std::string& Request::getMethod() const { return _method; }
const std::string& Request::getUri() const { return _uri; }
const std::string& Request::getVersion() const { return _version; }
const Headers& Request::getHeaders() const { return _headers; }
const std::string& Request::getBody() const { return _body; }
const std::string& Request::getRawRequest() const { return _rawRequest; }
Request::ParseState Request::getState() const { return _state; }
size_t Request::getContentLength() const { return _contentLength; }
bool Request::isChunked() const { return _isChunked; }
bool Request::isStreamingMode() const {
    // Streaming mode means the request body is supplied in a streaming fashion
    // e.g. chunked transfer-encoding or when content-length indicates a body
    if (_isChunked) return true;
    if (_contentLength > 0) return true;
    // Some methods imply a possible body even without content-length
    if (_method == "POST" || _method == "PUT") return true;
    return false;
}

std::string Request::getHeader(const std::string& name) const {
    std::string lowerName = Utils::toLowerCase(name);
    Headers::const_iterator it = _headers.find(lowerName);
    return (it != _headers.end()) ? it->second : "";
}

bool Request::hasHeader(const std::string& name) const {
    std::string lowerName = Utils::toLowerCase(name);
    return _headers.find(lowerName) != _headers.end();
}

// Setters
void Request::setMethod(const std::string& method) { _method = method; }
void Request::setUri(const std::string& uri) { _uri = uri; }
void Request::setVersion(const std::string& version) { _version = version; }
void Request::setBody(const std::string& body) { _body = body; }

void Request::setHeader(const std::string& name, const std::string& value) {
    _headers[Utils::toLowerCase(name)] = value;
}

void Request::clearBody() {
    _body.clear();
}

std::string Request::getQueryString() const {
    size_t pos = _uri.find('?');
    return (pos != std::string::npos) ? _uri.substr(pos + 1) : "";
}

std::string Request::getPath() const {
    size_t pos = _uri.find('?');
    return (pos != std::string::npos) ? _uri.substr(0, pos) : _uri;
}

std::map<std::string, std::string> Request::getQueryParams() const {
    std::map<std::string, std::string> params;
    std::string queryString = getQueryString();
    
    if (queryString.empty()) return params;
    
    std::vector<std::string> pairs = Utils::split(queryString, "&");
    for (size_t i = 0; i < pairs.size(); ++i) {
        size_t eqPos = pairs[i].find('=');
        if (eqPos != std::string::npos) {
            std::string key = Utils::urlDecode(pairs[i].substr(0, eqPos));
            std::string value = Utils::urlDecode(pairs[i].substr(eqPos + 1));
            params[key] = value;
        } else {
            params[Utils::urlDecode(pairs[i])] = "";
        }
    }
    
    return params;
}

bool Request::hasChunkedTimeout(int timeoutSeconds) const {
    if (!_isChunked || _chunkStartTime == 0) {
        return false;  // Not a chunked request or timing not started
    }
    
    // Check if chunked upload has been going on for too long without completion
    time_t now = time(NULL);
    time_t elapsed = now - _chunkStartTime;
    
    if (elapsed > timeoutSeconds) {
        Logger::debug("Chunked upload timeout detected: " + Utils::intToString((int)elapsed) + 
                      " seconds elapsed, limit is " + Utils::intToString(timeoutSeconds));
        return true;
    }
    
    return false;
}
