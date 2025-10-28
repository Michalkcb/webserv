#ifndef WEBSERV_HPP
#define WEBSERV_HPP

// C++ Standard Library Headers
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <exception>
#include <iterator>

// C System Headers
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cerrno>
#include <csignal>
#include <cctype>

// Unix/POSIX Headers
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>
#include <poll.h>

// Project Constants
#define BUFFER_SIZE 65536  // 64KB for better performance
#define MAX_CLIENTS 1024
#define MAX_BODY_SIZE 209715200  // 200MB default
#define HTTP_VERSION "HTTP/1.1"
#define SERVER_NAME "webserv/1.0"

// HTTP Status Codes
#define HTTP_OK 200
#define HTTP_CREATED 201
#define HTTP_NO_CONTENT 204
#define HTTP_MOVED_PERMANENTLY 301
#define HTTP_FOUND 302
#define HTTP_BAD_REQUEST 400
#define HTTP_FORBIDDEN 403
#define HTTP_NOT_FOUND 404
#define HTTP_METHOD_NOT_ALLOWED 405
#define HTTP_REQUEST_TIMEOUT 408
#define HTTP_PAYLOAD_TOO_LARGE 413
#define HTTP_INTERNAL_SERVER_ERROR 500
#define HTTP_NOT_IMPLEMENTED 501
#define HTTP_BAD_GATEWAY 502
#define HTTP_SERVICE_UNAVAILABLE 503

// Forward declarations
class Server;
class Client;
class Request;
class Response;
class Config;
class Location;
class CGI;
class Logger;

// Type definitions
typedef std::map<std::string, std::string> Headers;
typedef std::map<int, std::string> StatusCodes;

#endif
