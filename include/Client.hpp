#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "webserv.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "CGI.hpp"
#include "Config.hpp"
#include "Location.hpp"

class Client {
public:
    enum State {
        RECEIVING_REQUEST,
        PROCESSING_REQUEST,
        SENDING_RESPONSE,
        CGI_PROCESSING,
        CGI_SENDING_HEADERS,
        CGI_STREAMING_BODY,
        FINISHED,
        ERROR_STATE
    };

private:
    int _fd;
    State _state;
    Request _request;
    Response _response;
    std::string _receiveBuffer;
    std::string _sendBuffer;
    std::string _cgiOutputBuffer;
    std::string _cgiInputCopy; // preserve original request body sent to CGI (for diagnostics)
    std::string _cgiWriteBuffer;
    time_t _lastActivity;
    CGI* _cgi;
    size_t _cgiBytesSent;
    bool _keepAlive;
    bool _cgiFinishedWaitingForRequest; // true when CGI completed but request still incomplete
    bool _peerClosed; // true when the client has half-closed (read EOF)
    // Tracks whether we've actually sent the CGI response headers to the client.
    // This is distinct from _sendBuffer being non-empty (which may contain other
    // data like an interim 100 Continue). Use this to decide whether we are in
    // true streaming mode for CGI body bytes.
    bool _cgiHeadersSent;
    // Track whether we've already sent an interim 100 Continue for this request
    bool _sent100Continue;
    // When CGI provides Content-Length, track how many body bytes remain to stream.
    // SIZE_MAX (or (size_t)-1) indicates unknown/not set (i.e., deferred mode).
    size_t _cgiBodyRemaining;

public:
    Client();
    Client(int fd);
    Client(const Client& other);
    Client& operator=(const Client& other);
    ~Client();

    // Getters
    int getFd() const;
    State getState() const;
    const Request& getRequest() const;
    const Response& getResponse() const;
    time_t getLastActivity() const;
    bool isKeepAlive() const;
    CGI* getCgi() const;
    bool hasPeerClosed() const;

    // Setters
    void setState(State state);
    void setResponse(const Response& response);
    void setKeepAlive(bool keepAlive);
    void setCgi(CGI* cgi);

    // Request/Response handling
    ssize_t receiveData();
    ssize_t sendData();
    void processRequest(const class Config& config);
    
    // State management
    void updateLastActivity();
    bool hasTimedOut(int timeoutSeconds = 30) const;
    void reset();
    void close();
    void markPeerClosed();

    // Buffer management
    const std::string& getReceiveBuffer() const;
    const std::string& getSendBuffer() const;
    void clearReceiveBuffer();
    void clearSendBuffer();
    void appendToSendBuffer(const std::string& data);
    
    // CGI handling
    void handleCgiInput();
    void handleCgiOutput();
    void finalizeCgiResponse();
    bool isCgiReady() const;
    bool isWaitingForCgiWrite() const;

private:
    // Request handlers
    Response _handleGetRequest(const Config::ServerBlock& serverConfig, const Location* location);
    Response _handlePostRequest(const Config::ServerBlock& serverConfig, const Location* location);
    Response _handlePutRequest(const Config::ServerBlock& serverConfig, const Location* location);
    Response _handleDeleteRequest(const Config::ServerBlock& serverConfig, const Location* location);
    
    // Bonus features
    void _applyBonusFeatures();
    void _applyCookieSupport();
    void _applySessionManagement();
    void _applyCompression();
    void _applyRangeRequests();

    size_t _cgiBodyOffset;
    size_t _stageBodyChunkForCgi(size_t maxBytes);
};

#endif
