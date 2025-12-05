/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGI.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mbany <mbany@student.42warsaw.pl>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/06 00:25:13 by mbany             #+#    #+#             */
/*   Updated: 2025/12/06 00:25:14 by mbany            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */



#include "CGI.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include <cstring>
#include <cctype>
#include <fstream>
#include <dirent.h>

// Allocation counter for CGI objects to disambiguate reused addresses
static unsigned long g_cgiAllocCounter = 0;

CGI::CGI() : _pid(-1), _inputFd(-1), _outputFd(-1), _isRunning(false), _finalized(false),
             _startTime(0), _lastOutputTime(0), _totalBytesRead(0), _execId(0), _allocId(++g_cgiAllocCounter), _owner(NULL) {}

CGI::CGI(const std::string& cgiPath) : _cgiPath(cgiPath), _pid(-1), _inputFd(-1), _outputFd(-1), _isRunning(false), _finalized(false),
             _startTime(0), _lastOutputTime(0), _totalBytesRead(0), _execId(0), _allocId(++g_cgiAllocCounter), _owner(NULL) {}


// Owner management
void CGI::setOwner(void* owner) {
    _owner = owner;
    // Registry audit disabled: emit to Logger instead of creating a file.
    std::ostringstream ss;
    ss << "SET_OWNER ts=" << (unsigned long)time(NULL)*1000UL << " cgi_ptr=" << (void*)this << " owner=" << owner
       << " pid=" << _pid << " exec=" << _execId << " alloc=" << _allocId;
    Logger::debug(ss.str());
}

void* CGI::getOwner() const {
    return _owner;
}

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
    // Disabled: avoid creating per-CGI env dumps on disk. Emit a short debug line instead.
    std::ostringstream ss;
    ss << "CGI_ENV_DUMP_DISABLED pid=" << pid << " vars=" << env.size();
    Logger::debug(ss.str());
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

    // For mapped .bla, we'll chdir into handler directory and exec basename
    std::string handlerPath = _cgiPath;
    if (isMappedBla && !handlerPath.empty() && !Utils::fileExists(handlerPath)) {
        Logger::error("CGI handler not found: " + handlerPath);
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
    // Do not call setpgid; not required by subject
        close(inPipe[1]); close(outPipe[0]);
        dup2(inPipe[0],  STDIN_FILENO);
        dup2(outPipe[1], STDOUT_FILENO);
        // Redirect stderr to a per-CGI file in /tmp so exec/ runtime errors
        // from the child are captured for debugging instead of being discarded.
        {
            // Use time-based suffix (or /dev/urandom) instead of getpid() to avoid using getpid()
            // Diagnostic file creation for CGI stderr disabled: redirect to /dev/null
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull != -1) { dup2(devnull, STDERR_FILENO); close(devnull); }
        }

        // Set correct working directory for CGI (subject requirement):
        // - for mapped .bla: use directory of handler (handlerAbs)
        // - otherwise: use directory of scriptPath
        {
            std::string work = (isMappedBla ? handlerPath : scriptPath);
            // extract dirname: everything before last '/'
            std::string::size_type slash = work.rfind('/');
            std::string workDir = (slash == std::string::npos) ? std::string(".") : work.substr(0, slash);
            if (!workDir.empty()) {
                if (chdir(workDir.c_str()) == -1) {
                    // Cannot chdir to intended working directory; exit child
                    _exit(126);
                }
            }
        }

        std::vector<char*> argv;
        if (isMappedBla) {
            // exec basename of handler within its directory
            std::string::size_type slash = handlerPath.rfind('/');
            std::string handlerBase = (slash == std::string::npos) ? handlerPath : handlerPath.substr(slash + 1);
            // Use strdup to allocate stable, independent C-strings for execve
            argv.push_back(strdup(handlerBase.c_str()));
            // Pass the target script path as first argument to the handler
            argv.push_back(strdup(scriptPath.c_str()));
        } else {
            std::string interp = getCgiInterpreter(scriptPath);
            // compute script basename (used when we've chdir'd into the script dir)
            std::string::size_type slash = scriptPath.rfind('/');
            std::string scriptBase = (slash == std::string::npos) ? scriptPath : scriptPath.substr(slash + 1);
                if (!interp.empty()) {
                // When invoking an interpreter, pass the script basename since
                // we've chdir'ed into the script's directory above.
                // Allocate stable C-strings with strdup to avoid any lifetime
                // or allocator aliasing issues observed in production.
                argv.push_back(strdup(interp.c_str()));
                std::string ext = Utils::getFileExtension(scriptPath);
                    // Ensure environment variables that some interpreter frontends
                    // consult (notably SCRIPT_FILENAME / PATH_TRANSLATED) are
                    // consistent with the chdir() above. We'll update the
                    // envArray entries directly because execve uses envArray
                    // rather than the process environ.
                    std::string newScriptFn = scriptBase;
                    for (size_t ei = 0; envArray[ei]; ++ei) {
                        if (std::strncmp(envArray[ei], "SCRIPT_FILENAME=", 15) == 0) {
                            free(envArray[ei]);
                            std::string nv = std::string("SCRIPT_FILENAME=") + newScriptFn;
                            envArray[ei] = strdup(nv.c_str());
                        }
                        if (std::strncmp(envArray[ei], "PATH_TRANSLATED=", 15) == 0) {
                            free(envArray[ei]);
                            std::string nv2 = std::string("PATH_TRANSLATED=") + newScriptFn;
                            envArray[ei] = strdup(nv2.c_str());
                        }
                    }
                if (ext == "php") {
                    argv.push_back(strdup("-f"));
                    argv.push_back(strdup(scriptBase.c_str()));
                } else {
                    argv.push_back(strdup(scriptBase.c_str()));
                }
            } else {
                // Execute the script directly using the original path
                argv.push_back(strdup(scriptPath.c_str())); // execute script directly
            }
        }
        argv.push_back(NULL);

    // execve program is argv[0]
    execve(argv[0], &argv[0], envArray);
    // If execve returns, it failed. Write an explanatory message to
    // the redirected stderr (we dup2'd STDERR_FILENO earlier) so the
    // per-CGI error log contains the errno and program name.
    // Use write() (allowed) instead of dprintf.
    {
        std::string prog = argv[0] ? argv[0] : "(null)";
        const char* dbg = getenv("WEBSERV_DEBUG");
        std::string msg;
        if (dbg) {
            msg = std::string("execve(") + prog + std::string(") failed: ") + strerror(errno) + std::string("\n");
        } else {
            msg = std::string("execve(") + prog + std::string(") failed\n");
        }
        ssize_t w = write(STDERR_FILENO, msg.c_str(), msg.length()); (void)w;
    }
    _exit(127);
    }

    close(inPipe[0]); close(outPipe[1]);
    _inputFd  = inPipe[1];
    _outputFd = outPipe[0];
    fcntl(_inputFd,  F_SETFL, O_NONBLOCK);
    fcntl(_outputFd, F_SETFL, O_NONBLOCK);

    _isRunning      = true;
    _startTime      = time(NULL);
    // Unique execution id to disambiguate rapid re-use of heap addresses or
    // identical second-resolution start times. Use a process-wide counter.
    static unsigned long g_cgiExecCounter = 0;
    _execId = ++g_cgiExecCounter;
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

    Logger::info("CGI execute(): pid=" + Utils::intToString(_pid) +
                 " mappedBla=" + (isMappedBla ? "true" : "false") +
                 " hasBody=" + (hasBody ? "true" : "false") +
                 " PATH_INFO=" + _env["PATH_INFO"] +
                 " exec=" + Utils::intToString(_execId) + " alloc=" + Utils::intToString(_allocId));

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
    {
        const char* dbg = getenv("WEBSERV_DEBUG");
        std::ostringstream ss;
        ss << "CGI::isRunning() waitpid result=" << result << ", pid=" << _pid;
        if (dbg) ss << ", errno=" << errno;
        Logger::debug(ss.str());
    }
    if (result == _pid) { // child transitioned to a waited state
        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            Logger::debug("CGI::isRunning(): child exited with status=" + Utils::intToString(code));
        } else if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            Logger::debug("CGI::isRunning(): child terminated by signal=" + Utils::intToString(sig));
        } else {
            Logger::debug("CGI::isRunning(): child changed state (status=" + Utils::intToString(status) + ")");
        }
        const_cast<CGI*>(this)->_isRunning = false;
        return false;
    }
    if (result == -1) { // error => treat as finished
        const char* dbg = getenv("WEBSERV_DEBUG");
        if (dbg) {
            Logger::debug(std::string("CGI::isRunning(): waitpid error: ") + std::string(strerror(errno)));
        } else {
            Logger::debug("CGI::isRunning(): waitpid error");
        }
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
        if (n == -1) {
            // Do not branch on errno here; caller will retry on future POLLOUT
            break;
        }
        if (n == 0) {
            // treat like temporarily full; retry later
            break;
        }
        // hard error (do not inspect errno here per evaluation rules)
        Logger::error("CGI::writeToInput() error: write() failed");
        break;
    }

    if (total > 0) {
        Logger::debug("CGI::writeToInput() wrote " + Utils::intToString((int)total) + " bytes to fd=" + Utils::intToString(_inputFd) + " pid=" + Utils::intToString(_pid));
        return (ssize_t)total;
    }
    return -1;
}
// ...existing code...

ssize_t CGI::readFromOutput(char* buffer, size_t size) {
    if (_outputFd == -1) return -1;
    // Debug: log low-level read attempt on CGI output fd
    Logger::debug("CGI::readFromOutput() about to read fd=" + Utils::intToString(_outputFd) + ", size=" + Utils::intToString((int)size) + " pid=" + Utils::intToString(_pid));
    ssize_t bytesRead = read(_outputFd, buffer, size);
    if (bytesRead > 0) {
        _lastOutputTime = time(NULL);
        _totalBytesRead += bytesRead;
        Logger::debug("CGI::readFromOutput() read " + Utils::intToString(bytesRead) + " bytes, totalRead=" + Utils::intToString(_totalBytesRead) + " pid=" + Utils::intToString(_pid));
    } else if (bytesRead == 0) {
        Logger::debug("CGI::readFromOutput() returned 0 (EOF) pid=" + Utils::intToString(_pid));
    } else {
        // Do not log strerror(errno) or errno value here: evaluation forbids
        // checking errno after read/write. Use a generic debug message.
        Logger::debug("CGI::readFromOutput() error reading from output fd pid=" + Utils::intToString(_pid));
    }
    return bytesRead;
}

void CGI::terminate() {
    if (_pid != -1 && _isRunning) {
        // Try to kill the entire process group first (negative PID)
        // This ensures that any child processes spawned by the CGI are also terminated
        kill(-_pid, SIGTERM);
        // Give the process a short chance to exit; do not block here.
        // Main loop (Server::run) will detect termination via waitpid(WNOHANG)
        // or CGI::isRunning checks. Send SIGKILL afterwards to ensure termination.
        kill(-_pid, SIGKILL);
        // Also kill the specific process if it's still running
        kill(_pid, SIGKILL);
        // Reap without blocking (best effort)
        waitpid(_pid, NULL, WNOHANG);
        _isRunning = false;
    }
    _cleanup();
}

void CGI::closeInput() {
    if (_inputFd != -1) {
    Logger::debug(std::string("DIAG_CGI_closeInput: before close(fd=") + Utils::intToString(_inputFd) + ")");
    close(_inputFd);
    Logger::debug(std::string("DIAG_CGI_closeInput: after close(fd=") + Utils::intToString(_inputFd) + ")");
    _inputFd = -1;
    }
}

int CGI::waitForCompletion() {
    if (_pid == -1) return -1;

    int status;
    // Use non-blocking wait to avoid stalling the main event loop.
    int res = waitpid(_pid, &status, WNOHANG);
    if (res == 0) {
        // Child still running; caller should retry later via poll-driven finalizer.
        return -1;
    }
    if (res == -1) {
        // Error from waitpid: treat as finished but report failure.
        const char* dbg = getenv("WEBSERV_DEBUG");
        if (dbg) {
            Logger::debug(std::string("CGI::waitForCompletion(): waitpid error: ") + std::string(strerror(errno)) + " pid=" + Utils::intToString(_pid));
        } else {
            Logger::debug("CGI::waitForCompletion(): waitpid error pid=" + Utils::intToString(_pid));
        }
        _isRunning = false;
        int saved_pid = _pid;
        _pid = -1;
        (void)saved_pid;
        return -1;
    }

    // res == _pid: child has exited
    _isRunning = false;
    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    // Clear pid now that we've reaped it
    _pid = -1;
    return exitCode;
}

int CGI::getInputFd() const { return _inputFd; }
int CGI::getOutputFd() const { return _outputFd; }
time_t CGI::getStartTime() const { return _startTime; }
time_t CGI::getLastActivityTime() const { return _lastOutputTime; }

int CGI::getPid() const { return _pid; }
unsigned long CGI::getExecId() const { return _execId; }
unsigned long CGI::getAllocId() const { return _allocId; }

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
    // If the script file is executable or starts with a shebang (#!)
    // prefer executing it directly. This allows files that are named
    // with a different extension (e.g. youpi.php) but are actually
    // shell scripts to run without requiring an interpreter binary
    // (or when e.g. php-cgi is not installed).
    if (Utils::fileExists(scriptPath)) {
        // Only allow direct execution when the file starts with a shebang (#!).
        // Treating any executable file as directly runnable caused issues when
        // text scripts (e.g. .php without shebang) were marked executable but
        // have no interpreter declared — execve would fail with ENOEXEC and
        // produce no useful stderr. Prefer shebang detection and otherwise
        // fall back to extension-based interpreter mapping.
        std::ifstream ifs(scriptPath.c_str());
        if (ifs.is_open()) {
            char a = 0, b = 0;
            ifs.get(a);
            ifs.get(b);
            if (a == '#' && b == '!') {
                return ""; // execute directly via shebang
            }
        }
    }

    std::string extension = Utils::getFileExtension(scriptPath);

    if (extension == "php") return "/usr/bin/php-cgi";
    if (extension == "py") return "/usr/bin/python3";
    if (extension == "pl") return "/usr/bin/perl";
    if (extension == "rb") return "/usr/bin/ruby";
    if (extension == "sh") return "/bin/sh";
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
        // Do not block here. Attempt to reap the child without waiting.
        // The main loop will continue to monitor and reap if needed.
        if (_isRunning) {
            kill(-_pid, SIGKILL);  // Force kill the process group
            kill(_pid, SIGKILL);   // Also force kill the specific process
        }
        // Reap if already exited (non-blocking)
        waitpid(_pid, NULL, WNOHANG);
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
