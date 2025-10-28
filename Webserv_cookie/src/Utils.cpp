#include "Utils.hpp"
#include "Logger.hpp"

static inline int hexDigit(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    c |= 0x20;
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    return -1;
}

bool Utils::dechunk(const std::string& in, std::string& out) {
    out.clear();
    size_t i = 0, n = in.size();
    while (i < n) {
        unsigned long size = 0;
        bool sawDigit = false;
        while (i < n) {
            int d = hexDigit(static_cast<unsigned char>(in[i]));
            if (d >= 0) { size = (size << 4) + (unsigned long)d; sawDigit = true; ++i; continue; }
            if (in[i] == ';') { // skip extensions until CRLF/LF
                while (i < n && !(in[i] == '\r' && i + 1 < n && in[i+1] == '\n') && in[i] != '\n') ++i;
                break;
            }
            if (in[i] == '\r' || in[i] == '\n') break;
            return false;
        }
        if (!sawDigit) return false;

        if (i + 1 < n && in[i] == '\r' && in[i+1] == '\n') i += 2;
        else if (i < n && in[i] == '\n') ++i;
        else return false;

        if (size == 0) {
            // consume optional trailers until empty line
            while (i < n) {
                size_t lineStart = i;
                while (i < n && in[i] != '\n') ++i;
                size_t lineLen = (i > lineStart && in[i-1] == '\r') ? (i - lineStart - 1) : (i - lineStart);
                if (i < n) ++i; // consume LF
                if (lineLen == 0) break;
            }
            return true;
        }

        if (i + size > n) return false;
        out.append(in, i, size);
        i += size;

        if (i + 1 < n && in[i] == '\r' && in[i+1] == '\n') i += 2;
        else if (i < n && in[i] == '\n') ++i;
        else return false;
    }
    return true;
}

std::vector<std::string> Utils::split(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);
    
    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }
    tokens.push_back(str.substr(start));
    return tokens;
}

std::string Utils::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string Utils::toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string Utils::toUpperCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

std::string Utils::intToString(int value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

int Utils::stringToInt(const std::string& str) {
    return std::atoi(str.c_str());
}

size_t Utils::stringToSize(const std::string& str) {
    return static_cast<size_t>(std::atol(str.c_str()));
}

bool Utils::isNumber(const std::string& str) {
    if (str.empty()) return false;
    for (size_t i = 0; i < str.length(); ++i) {
        if (!std::isdigit(str[i])) return false;
    }
    return true;
}

std::string Utils::urlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int hex = 0;
            std::stringstream ss;
            ss << std::hex << str.substr(i + 1, 2);
            ss >> hex;
            result += static_cast<char>(hex);
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string Utils::urlEncode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        char c = str[i];
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            result += c;
        } else {
            std::stringstream ss;
            ss << '%' << std::hex << std::uppercase << static_cast<int>(static_cast<unsigned char>(c));
            result += ss.str();
        }
    }
    return result;
}

bool Utils::fileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool Utils::isDirectory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

std::string Utils::getFileExtension(const std::string& filename) {
    size_t pos = filename.find_last_of('.');
    if (pos != std::string::npos && pos != filename.length() - 1) {
        return filename.substr(pos + 1);
    }
    return "";
}

std::string Utils::getMimeType(const std::string& extension) {
    std::string ext = toLowerCase(extension);
    
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css") return "text/css";
    if (ext == "js") return "application/javascript";
    if (ext == "json") return "application/json";
    if (ext == "xml") return "application/xml";
    if (ext == "txt") return "text/plain";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "ico") return "image/x-icon";
    if (ext == "pdf") return "application/pdf";
    if (ext == "zip") return "application/zip";
    if (ext == "mp3") return "audio/mpeg";
    if (ext == "mp4") return "video/mp4";
    if (ext == "avi") return "video/x-msvideo";
    
    return "application/octet-stream";
}

std::string Utils::getStatusMessage(int statusCode) {
    switch (statusCode) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default: return "Unknown";
    }
}

std::string Utils::readFile(const std::string& filename) {
    std::ifstream file(filename.c_str(), std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool Utils::writeFile(const std::string& filename, const std::string& content) {
    std::ofstream file(filename.c_str(), std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file << content;
    return file.good();
}

std::vector<std::string> Utils::getDirectoryListing(const std::string& path) {
    std::vector<std::string> files;
    DIR* dir = opendir(path.c_str());
    if (dir == NULL) {
        return files;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name != "." && name != "..") {
            files.push_back(name);
        }
    }
    closedir(dir);
    
    std::sort(files.begin(), files.end());
    return files;
}

std::string Utils::generateDirectoryListing(const std::string& path, const std::string& uri) {
    std::vector<std::string> files = getDirectoryListing(path);
    std::stringstream html;
    
    html << "<!DOCTYPE html>\n";
    html << "<html><head><title>Index of " << uri << "</title></head>\n";
    html << "<body><h1>Index of " << uri << "</h1>\n";
    html << "<hr><pre>\n";
    
    // Add parent directory link if not root
    if (uri != "/") {
        html << "<a href=\"../\">../</a>\n";
    }
    
    for (size_t i = 0; i < files.size(); ++i) {
        std::string fullPath = path + "/" + files[i];
        std::string displayName = files[i];
        
        if (isDirectory(fullPath)) {
            displayName += "/";
        }
        
        html << "<a href=\"" << urlEncode(files[i]);
        if (isDirectory(fullPath)) html << "/";
        html << "\">" << displayName << "</a>\n";
    }
    
    html << "</pre><hr></body></html>\n";
    return html.str();
}

size_t Utils::hexToSize(const std::string& hex) {
    size_t result = 0;
    std::stringstream ss;
    ss << std::hex << hex;
    ss >> result;
    return result;
}

std::string Utils::getCurrentTime() {
    time_t now = time(0);
    struct tm* timeinfo = gmtime(&now);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", timeinfo);
    return std::string(buffer);
}

void Utils::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        Logger::error("fcntl F_GETFL failed");
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        Logger::error("fcntl F_SETFL failed");
    }
}
