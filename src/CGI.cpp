#include "CGI.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <vector>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <dirent.h>

CGI::CGI() : _pid(-1), _inputFd(-1), _outputFd(-1), _isRunning(false), _finalized(false),
             _startTime(0), _lastOutputTime(0), _totalBytesRead(0) {}

CGI::CGI(const std::string& cgiPath) : _cgiPath(cgiPath), _pid(-1), _inputFd(-1), _outputFd(-1), _isRunning(false), _finalized(false),
             _startTime(0), _lastOutputTime(0), _totalBytesRead(0) {}

// Removed copy ctor / operator= definitions to prevent unsafe copying
// ...existing code...

/*/
CGI::CGI(const CGI& other) {
    *this = other;
}

CGI& CGI::operator=(const CGI& other) {
    if (this != &other) {
        _cgiPath = other._cgiPath;
        _scriptPath = other._scriptPath;
        _queryString = other._queryString;
        _env = other._env;
        _pid = other._pid;
        _inputFd = other._inputFd;
        _outputFd = other._outputFd;
        _isRunning = other._isRunning;
        _startTime = other._startTime;
        _lastOutputTime = other._lastOutputTime;
        _totalBytesRead = other._totalBytesRead;
    }
    return *this;
}
//*/

CGI::~CGI() {
    _cleanup();
}

bool CGI::isFinalized() const {
    return _finalized;
}

void CGI::markFinalized() {
    _finalized = true;
}

// Add a small helper to dump env when debugging
static void dumpCgiEnv(pid_t pid, const std::map<std::string,std::string>& env) {
    char path[128];
    std::snprintf(path, sizeof(path), "/tmp/ws_cgi_env_%d.txt", (int)pid);
    std::ofstream ofs(path);
    if (!ofs.is_open()) return;
    for (std::map<std::string,std::string>::const_iterator it = env.begin(); it != env.end(); ++it) {
        ofs << it->first << "=" << it->second << "\n";
    }
}

void CGI::_setupEnvironment(const Request& request) {
    _env.clear();
    _env["REQUEST_METHOD"]    = request.getMethod();
    _env["REQUEST_URI"]       = request.getUri();
    _env["QUERY_STRING"]      = request.getQueryString();
    _env["SERVER_PROTOCOL"]   = "HTTP/1.1";
    _env["GATEWAY_INTERFACE"] = "CGI/1.1";
    _env["SERVER_SOFTWARE"]   = "webserv/1.0";
    _env["SERVER_NAME"]       = "localhost";
    _env["SERVER_PORT"]       = "8080";
    _env["REMOTE_ADDR"]       = "127.0.0.1";
    _env["REMOTE_PORT"]       = "0";

    _env["SCRIPT_NAME"]       = request.getPath();
    _env["PATH_INFO"]         = request.getPath();
    _env["SCRIPT_FILENAME"]   = !_scriptPath.empty() ? _scriptPath : request.getPath();
    _env["PATH_TRANSLATED"]   = _env["SCRIPT_FILENAME"];
    _env["PATH"]              = "/usr/bin:/bin";
    _env["REDIRECT_STATUS"]   = "200";

    std::string te = Utils::toLowerCase(request.getHeader("transfer-encoding"));
    std::string ct = request.getHeader("content-type");
    if (!ct.empty()) _env["CONTENT_TYPE"] = ct;

    if (te.find("chunked") != std::string::npos) {
        _env.erase("CONTENT_LENGTH");
    } else {
        std::string cl = request.getHeader("content-length");
        if (!cl.empty() && Utils::isNumber(cl))
            _env["CONTENT_LENGTH"] = cl;
        else
            _env.erase("CONTENT_LENGTH");
    }

    const Headers& headers = request.getHeaders();
    for (Headers::const_iterator it = headers.begin(); it != headers.end(); ++it) {
        std::string lower = Utils::toLowerCase(it->first);
        if (lower == "content-length" || lower == "content-type") continue;
        std::string name = it->first;
        for (size_t i = 0; i < name.size(); ++i)
            name[i] = (name[i] == '-') ? '_' : std::toupper(static_cast<unsigned char>(name[i]));
        _env["HTTP_" + name] = it->second; // includes HTTP_X_SECRET_HEADER_FOR_TEST
    }
}

bool CGI::execute(const Request& request, const std::string& scriptPath) {
    _scriptPath  = scriptPath;
    _queryString = request.getQueryString();

    std::string ext = Utils::getFileExtension(scriptPath);
    bool isMappedBla = (!ext.empty() && ext == "bla" && !_cgiPath.empty());

    if (!isMappedBla && !Utils::fileExists(scriptPath)) {
        Logger::error("CGI script not found: " + scriptPath);
        return false;
    }

    int inPipe[2], outPipe[2];
    if (pipe(inPipe) == -1 || pipe(outPipe) == -1) {
        Logger::error("CGI: pipe() failed");
        return false;
    }

    // Build environment (already forwards all headers as HTTP_* via _setupEnvironment)
    _setupEnvironment(request);

    if (isMappedBla) {
        // Dla mapowania .bla tester oczekuje, że SCRIPT_FILENAME i PATH_TRANSLATED
        // wskazują na program handlera (cgi_test), a ścieżka pliku .bla zostanie
        // przekazana jako argv[1]. Nazwy skryptu i PATH_INFO pozostają zgodne z żądaniem.
        _env["SCRIPT_FILENAME"] = _cgiPath;   // np. ./cgi_test
        _env["PATH_TRANSLATED"] = _cgiPath;
        _env["SCRIPT_NAME"]     = request.getPath();
        _env["PATH_INFO"]       = request.getPath();
    }

    char** envArray = _createEnvArray();

    std::string handlerAbs = _cgiPath;
    if (!_cgiPath.empty() && _cgiPath[0] != '/') {
        char cwdBuf[512];
        if (getcwd(cwdBuf, sizeof(cwdBuf) - 1))
            handlerAbs = std::string(cwdBuf) + "/" + _cgiPath;
    }
    if (isMappedBla && !handlerAbs.empty() && !Utils::fileExists(handlerAbs)) {
        Logger::error("CGI handler not found: " + handlerAbs);
        close(inPipe[0]); close(inPipe[1]);
        close(outPipe[0]); close(outPipe[1]);
        for (size_t i = 0; envArray[i]; ++i) delete [] envArray[i];
        delete [] envArray;
        return false;
    }

    _pid = fork();
    if (_pid == -1) {
        Logger::error("CGI: fork() failed");
        close(inPipe[0]); close(inPipe[1]);
        close(outPipe[0]); close(outPipe[1]);
        for (size_t i = 0; envArray[i]; ++i) delete [] envArray[i];
        delete [] envArray;
        return false;
    }

    if (_pid == 0) {
        setpgid(0,0);
        close(inPipe[1]); close(outPipe[0]);
        dup2(inPipe[0],  STDIN_FILENO);
        dup2(outPipe[1], STDOUT_FILENO);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull != -1) { dup2(devnull, STDERR_FILENO); close(devnull); }

        std::vector<char*> argv;
        if (isMappedBla) {
            argv.push_back(const_cast<char*>(handlerAbs.c_str()));
            // Pass the target script path as first argument to the handler
            argv.push_back(const_cast<char*>(scriptPath.c_str()));
        } else {
            std::string interp = getCgiInterpreter(scriptPath);
            if (!interp.empty()) {
                argv.push_back(const_cast<char*>(interp.c_str()));
                argv.push_back(const_cast<char*>(scriptPath.c_str())); // use full path
            } else {
                argv.push_back(const_cast<char*>(scriptPath.c_str())); // execute script directly
            }
        }
        argv.push_back(NULL);

        // execve program is argv[0]
        execve(argv[0], &argv[0], envArray);
        _exit(127);
    }

    close(inPipe[0]); close(outPipe[1]);
    _inputFd  = inPipe[1];
    _outputFd = outPipe[0];
    fcntl(_inputFd,  F_SETFL, O_NONBLOCK);
    fcntl(_outputFd, F_SETFL, O_NONBLOCK);

    _isRunning      = true;
    _startTime      = time(NULL);
    _lastOutputTime = _startTime;
    _totalBytesRead = 0;

    dumpCgiEnv(_pid, _env);

    std::string clh = request.getHeader("content-length");
    std::string te  = Utils::toLowerCase(request.getHeader("transfer-encoding"));
    bool hasBody = false;
    if (!clh.empty() && Utils::isNumber(clh) && Utils::stringToInt(clh) > 0)
        hasBody = true;
    else if (te.find("chunked") != std::string::npos || request.getBody().size() > 0)
        hasBody = true;

    if (!hasBody && (request.getMethod() == "GET" || request.getMethod() == "HEAD")) {
        close(_inputFd);
        _inputFd = -1;
    }

    for (size_t i = 0; envArray[i]; ++i) delete [] envArray[i];
    delete [] envArray;

    Logger::debug("CGI execute(): pid=" + Utils::intToString(_pid) +
                  " mappedBla=" + (isMappedBla ? "true" : "false") +
                  " hasBody=" + (hasBody ? "true" : "false") +
                  " PATH_INFO=" + _env["PATH_INFO"]);

    return true;
}

char** CGI::_createEnvArray() const {
    char** envArray = new char*[_env.size() + 1];
    size_t i = 0;
    
    for (std::map<std::string, std::string>::const_iterator it = _env.begin(); 
         it != _env.end(); ++it, ++i) {
        std::string envVar = it->first + "=" + it->second;
        envArray[i] = new char[envVar.length() + 1];
        std::strcpy(envArray[i], envVar.c_str());
    }
    envArray[i] = NULL;
    
    return envArray;
}

bool CGI::isRunning() const {
    if (!_isRunning || _pid == -1)
        return false;

    int status;
    int result = waitpid(_pid, &status, WNOHANG);
    Logger::debug("CGI::isRunning() waitpid result=" + Utils::intToString(result) + ", pid=" + Utils::intToString(_pid) + ", errno=" + Utils::intToString(errno));
    if (result == _pid) { // child transitioned to a waited state
        Logger::debug("CGI::isRunning(): child has exited or changed state (status=" + Utils::intToString(status) + ")");
        const_cast<CGI*>(this)->_isRunning = false;
        return false;
    }
    if (result == -1) { // error => treat as finished
        Logger::debug("CGI::isRunning(): waitpid error: " + std::string(strerror(errno)));
        const_cast<CGI*>(this)->_isRunning = false;
        return false;
    }
    // result == 0 => still running
    return true; // still running
}

bool CGI::isFinished() const {
    // Finished when process no longer running flag has been cleared
    return !_isRunning || !isRunning();
}

bool CGI::hasTimedOut(int timeoutSeconds) const {
    if (!_isRunning) return false;
    // Consider CGI timed out only if it has been idle for more than timeoutSeconds.
    // Use _lastOutputTime as last activity marker; if it's not set, fall back to _startTime.
    time_t lastActivity = _lastOutputTime;
    if (lastActivity == 0) lastActivity = _startTime;
    return (time(NULL) - lastActivity) > timeoutSeconds;
}

// ...existing code...
ssize_t CGI::writeToInput(const char* data, size_t len) {
    if (_inputFd == -1 || len == 0) return 0;

    size_t total = 0;
    for (;;) {
        ssize_t n = ::write(_inputFd, data + total, len - total);
        if (n > 0) {
            total += n;
            _lastOutputTime = time(NULL);
            if (total == len) break;          // all bytes written
            continue;                         // try to push more immediately
        }
        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // pipe full; caller should retry later with the remaining slice
            break;
        }
        if (n == 0) {
            // treat like temporarily full; retry later
            break;
        }
        // hard error
        Logger::error("CGI::writeToInput() error: " + std::string(strerror(errno)));
        break;
    }

    if (total > 0) {
        Logger::debug("CGI::writeToInput() wrote " + Utils::intToString((int)total) + " bytes");
        return (ssize_t)total;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        Logger::debug("CGI::writeToInput() would block (EAGAIN)");
        return -1;
    }
    return -1;
}
// ...existing code...

ssize_t CGI::readFromOutput(char* buffer, size_t size) {
    if (_outputFd == -1) return -1;
    // Debug: log low-level read attempt on CGI output fd
    Logger::debug("CGI::readFromOutput() about to read fd=" + Utils::intToString(_outputFd) + ", size=" + Utils::intToString((int)size));
    ssize_t bytesRead = read(_outputFd, buffer, size);
    if (bytesRead > 0) {
        _lastOutputTime = time(NULL);
        _totalBytesRead += bytesRead;
        Logger::debug("CGI::readFromOutput() read " + Utils::intToString(bytesRead) + " bytes, totalRead=" + Utils::intToString(_totalBytesRead));
    } else if (bytesRead == 0) {
        Logger::debug("CGI::readFromOutput() returned 0 (EOF)");
    } else {
        Logger::debug("CGI::readFromOutput() error: " + std::string(strerror(errno)) + ", errno=" + Utils::intToString(errno));
    }
    return bytesRead;
}

void CGI::terminate() {
    if (_pid != -1 && _isRunning) {
        // Try to kill the entire process group first (negative PID)
        // This ensures that any child processes spawned by the CGI are also terminated
        kill(-_pid, SIGTERM);
        usleep(100000); // 100ms grace period
        kill(-_pid, SIGKILL);
        
        // Also kill the specific process if it's still running
        kill(_pid, SIGKILL);
        waitpid(_pid, NULL, 0);
        _isRunning = false;
    }
    _cleanup();
}

void CGI::closeInput() {
    if (_inputFd != -1) {
    Logger::debug(std::string("DIAG_CGI_closeInput: before close(fd=") + Utils::intToString(_inputFd) + ")");
    // Pre-close snapshot in CGI::closeInput
    {
        static int cgi_pre_snap = 0;
        char pre_path[256];
        snprintf(pre_path, sizeof(pre_path), "/tmp/ws_fds_cgi_close_pre_%d.txt", ++cgi_pre_snap);
        FILE* fpre = fopen(pre_path, "w");
        if (fpre) {
            fprintf(fpre, "CGI pre-close snapshot: inputFd=%d\n", _inputFd);
            DIR* dpre = opendir("/proc/self/fd");
            if (dpre) {
                struct dirent* depre;
                while ((depre = readdir(dpre)) != NULL) {
                    if (depre->d_name[0] == '.') continue;
                    char linkpathpre[256];
                    snprintf(linkpathpre, sizeof(linkpathpre), "/proc/self/fd/%s", depre->d_name);
                    char bufpre[512];
                    ssize_t rpre = readlink(linkpathpre, bufpre, sizeof(bufpre)-1);
                    if (rpre > 0) bufpre[rpre] = '\0'; else strcpy(bufpre, "(unreadable)");
                    fprintf(fpre, "fd=%s -> %s\n", depre->d_name, bufpre);
                }
                closedir(dpre);
            }
            fclose(fpre);
        }
    }
    close(_inputFd);
    Logger::debug(std::string("DIAG_CGI_closeInput: after close(fd=") + Utils::intToString(_inputFd) + ")");
    // Snapshot server fds for forensic analysis (post-close)
    {
        static int cgi_snap = 0;
        char path[256];
        snprintf(path, sizeof(path), "/tmp/ws_fds_cgi_close_%d.txt", ++cgi_snap);
        FILE* fc = fopen(path, "w");
        if (fc) {
            DIR* d = opendir("/proc/self/fd");
            if (d) {
                struct dirent* de;
                while ((de = readdir(d)) != NULL) {
                    if (de->d_name[0] == '.') continue;
                    char linkpath[256];
                    snprintf(linkpath, sizeof(linkpath), "/proc/self/fd/%s", de->d_name);
                    char buf[512];
                    ssize_t r = readlink(linkpath, buf, sizeof(buf)-1);
                    if (r > 0) buf[r] = '\0'; else strcpy(buf, "(unreadable)");
                    fprintf(fc, "fd=%s -> %s\n", de->d_name, buf);
                }
                closedir(d);
            }
            fclose(fc);
        }
    }
    _inputFd = -1;
    }
}

int CGI::waitForCompletion() {
    if (_pid == -1) return -1;
    
    int status;
    waitpid(_pid, &status, 0);
    _isRunning = false;
    
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int CGI::getInputFd() const { return _inputFd; }
int CGI::getOutputFd() const { return _outputFd; }
time_t CGI::getStartTime() const { return _startTime; }
time_t CGI::getLastActivityTime() const { return _lastOutputTime; }

Response CGI::parseHeaders(const std::string& headersStr) {
    Response response;
    std::vector<std::string> lines = Utils::split(headersStr, "\n");
    bool hasStatus = false;

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string line = Utils::trim(lines[i]);
        if (line.empty()) continue;
        size_t c = line.find(':');
        if (c != std::string::npos) {
            std::string name  = Utils::trim(line.substr(0, c));
            std::string value = Utils::trim(line.substr(c + 1));
            if (Utils::toLowerCase(name) == "status") {
                int code = Utils::stringToInt(value);
                if (code < 100 || code > 599) code = 200;
                response.setStatusCode(code);
                hasStatus = true;
            } else {
                response.setHeader(name, value);
            }
        }
    }
    if (!hasStatus)
        response.setStatusCode(HTTP_OK);
    if (!response.hasHeader("Content-Type"))
        response.setHeader("Content-Type", "text/plain");
    response.setComplete(false);
    return response;
}

Response CGI::generateResponse(const std::string& cgiOutput) {
    Logger::debug("CGI::generateResponse bytes=" + Utils::intToString((int)cgiOutput.size()));
    Response r;
    if (cgiOutput.empty()) {
        return Response::createErrorResponse(HTTP_INTERNAL_SERVER_ERROR);
    }

    size_t posCRLF = cgiOutput.find("\r\n\r\n");
    size_t posLF   = cgiOutput.find("\n\n");
    size_t split = std::string::npos;
    size_t sep   = 0;

    if (posCRLF != std::string::npos && (posLF == std::string::npos || posCRLF < posLF)) {
        split = posCRLF; sep = 4;
    } else if (posLF != std::string::npos) {
        split = posLF; sep = 2;
    }

    if (split == std::string::npos) {
        r.setStatusCode(HTTP_OK);
        r.setHeader("Content-Type", "text/plain");
        r.setBody(cgiOutput);
        r.setHeader("Content-Length", Utils::intToString((int)cgiOutput.size()));
        r.setComplete(true);
        return r;
    }

    std::string headersPart = cgiOutput.substr(0, split);
    std::string bodyPart    = cgiOutput.substr(split + sep);

    r = parseHeaders(headersPart);

    if (!r.hasHeader("Content-Length"))
        r.setHeader("Content-Length", Utils::intToString((int)bodyPart.size()));
    if (!r.hasHeader("Content-Type"))
        r.setHeader("Content-Type", "text/plain");

    r.setBody(bodyPart);
    r.setComplete(true);
    return r;
}

bool CGI::isCgiScript(const std::string& path, const std::string& cgiExtension) {
    if (cgiExtension.empty()) return false;
    std::string extension = Utils::getFileExtension(path);
    return extension == cgiExtension;
}

std::string CGI::getCgiInterpreter(const std::string& scriptPath) {
    std::string extension = Utils::getFileExtension(scriptPath);
    
    if (extension == "php") return "/usr/bin/php-cgi";
    if (extension == "py") return "/usr/bin/python3";
    if (extension == "pl") return "/usr/bin/perl";
    if (extension == "rb") return "/usr/bin/ruby";
    if (extension == "bla" && !_cgiPath.empty()) return _cgiPath;
    
    return ""; // No interpreter needed, execute directly
}

void CGI::_cleanup() {
    // CRITICAL FIX: Terminate the CGI process before closing file descriptors
    // This prevents orphaned CGI processes that can interfere with subsequent
    // server runs and cause "bad status code" or "bad cgi returned size body" errors
    if (_pid != -1 && _isRunning) {
        Logger::debug("CGI cleanup: terminating orphaned process " + Utils::intToString(_pid));
        
        // Try to kill the entire process group first (negative PID)
        kill(-_pid, SIGTERM);  // Try graceful termination first
        usleep(100000);  // 100ms grace period for cleanup
        if (_isRunning) {
            kill(-_pid, SIGKILL);  // Force kill the process group
            kill(_pid, SIGKILL);   // Also force kill the specific process
        }
        waitpid(_pid, NULL, 0);  // Reap the zombie process
        _isRunning = false;
        _pid = -1;
    }
    
    if (_inputFd != -1) {
        close(_inputFd);
        _inputFd = -1;
    }
    if (_outputFd != -1) {
        close(_outputFd);
        _outputFd = -1;
    }
}
