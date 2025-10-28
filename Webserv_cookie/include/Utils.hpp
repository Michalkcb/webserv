#ifndef UTILS_HPP
#define UTILS_HPP

#include "webserv.hpp"

class Utils {
public:
    static std::vector<std::string> split(const std::string& str, const std::string& delimiter);
    static std::string trim(const std::string& str);
    static std::string toLowerCase(const std::string& str);
    static std::string toUpperCase(const std::string& str);
    static std::string intToString(int value);
    static int stringToInt(const std::string& str);
    static size_t stringToSize(const std::string& str);
    static bool isNumber(const std::string& str);
    static std::string urlDecode(const std::string& str);
    static std::string urlEncode(const std::string& str);
    static bool fileExists(const std::string& path);
    static bool isDirectory(const std::string& path);
    static std::string getFileExtension(const std::string& filename);
    static std::string getMimeType(const std::string& extension);
    static std::string getStatusMessage(int statusCode);
    static std::string readFile(const std::string& filename);
    static bool writeFile(const std::string& filename, const std::string& content);
    static std::vector<std::string> getDirectoryListing(const std::string& path);
    static std::string generateDirectoryListing(const std::string& path, const std::string& uri);
    static size_t hexToSize(const std::string& hex);
    static std::string getCurrentTime();
    static void setNonBlocking(int fd);
    static bool dechunk(const std::string& in, std::string& out); // make static
};

#endif
