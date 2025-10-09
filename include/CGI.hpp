#ifndef CGI_HPP
#define CGI_HPP

#include "webserv.hpp"
#include "Request.hpp"
#include "Response.hpp"

class CGI {
private:
    CGI(const CGI&);                 // declared only
    CGI& operator=(const CGI&);      // declared only
    
    std::string _cgiPath;
    std::string _scriptPath;
    std::string _queryString;
    std::map<std::string, std::string> _env;
    pid_t _pid;
    int _inputFd;
    int _outputFd;
    bool _isRunning;
    time_t _startTime;
    time_t _lastOutputTime;
    size_t _totalBytesRead;
    
    void _setupEnvironment(const Request& request);
    char** _createEnvArray() const;
    void _cleanup();

public:
    CGI();
    CGI(const std::string& cgiPath);
    ~CGI();

    // Execute CGI
    bool execute(const Request& request, const std::string& scriptPath);
    bool isRunning() const;
    bool isFinished() const;
    bool hasTimedOut(int timeoutSeconds = 300) const;
    
    // I/O operations
    ssize_t writeToInput(const char* data, size_t len);
    ssize_t readFromOutput(char* buffer, size_t size);
    // Close CGI stdin (signal EOF to the CGI process)
    void closeInput();
    
    // Process management
    void terminate();
    int waitForCompletion();
    
    // File descriptors
    int getInputFd() const;
    int getOutputFd() const;
    time_t getStartTime() const;
    time_t getLastActivityTime() const;
    
    // Generate response from CGI output
    Response parseHeaders(const std::string& headersStr);
    Response generateResponse(const std::string& cgiOutput);
    
    // Static utility methods
    static bool isCgiScript(const std::string& path, const std::string& cgiExtension);
    std::string getCgiInterpreter(const std::string& scriptPath);
};

#endif
