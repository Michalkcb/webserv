#ifndef COMPRESSION_HPP
#define COMPRESSION_HPP

#include "webserv.hpp"
#include <zlib.h>

class Compression {
public:
    enum CompressionType {
        NONE,
        GZIP,
        DEFLATE
    };

private:
    static bool _isCompressible(const std::string& contentType);
    static std::string _simpleGzipCompress(const std::string& data);

public:
    // Check if client accepts compression
    static CompressionType getAcceptedCompression(const std::string& acceptEncoding);
    
    // Compress data based on type
    static std::string compress(const std::string& data, CompressionType type);
    
    // Check if content should be compressed
    static bool shouldCompress(const std::string& contentType, size_t contentLength);
    
    // Get compression header value
    static std::string getEncodingHeader(CompressionType type);
    
    // Simple compression implementation (for demonstration)
    static std::string simpleCompress(const std::string& data);
};

#endif
