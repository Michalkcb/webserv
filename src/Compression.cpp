#include "Compression.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include <cstring>

Compression::CompressionType Compression::getAcceptedCompression(const std::string& acceptEncoding) {
    if (acceptEncoding.empty()) return NONE;
    
    std::string lowerEncoding = Utils::toLowerCase(acceptEncoding);
    
    if (lowerEncoding.find("gzip") != std::string::npos) {
        return GZIP;
    } else if (lowerEncoding.find("deflate") != std::string::npos) {
        return DEFLATE;
    }
    
    return NONE;
}

std::string Compression::compress(const std::string& data, CompressionType type) {
    switch (type) {
        case GZIP:
            return _simpleGzipCompress(data);
        case DEFLATE:
            return simpleCompress(data);
        case NONE:
        default:
            return data;
    }
}

bool Compression::shouldCompress(const std::string& contentType, size_t contentLength) {
    // Don't compress small files (< 1KB)
    if (contentLength < 1024) return false;
    
    // Don't compress already compressed content
    if (contentType.find("image/") == 0 ||
        contentType.find("video/") == 0 ||
        contentType.find("audio/") == 0 ||
        contentType.find("application/zip") == 0 ||
        contentType.find("application/gzip") == 0) {
        return false;
    }
    
    return _isCompressible(contentType);
}

std::string Compression::getEncodingHeader(CompressionType type) {
    switch (type) {
        case GZIP: return "gzip";
        case DEFLATE: return "deflate";
        case NONE:
        default: return "";
    }
}

bool Compression::_isCompressible(const std::string& contentType) {
    return (contentType.find("text/") == 0 ||
            contentType.find("application/json") == 0 ||
            contentType.find("application/javascript") == 0 ||
            contentType.find("application/xml") == 0 ||
            contentType.find("application/xhtml") == 0);
}

std::string Compression::_simpleGzipCompress(const std::string& data) {
    z_stream zs;                        
    memset(&zs, 0, sizeof(zs));

    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        Logger::debug("Failed to initialize gzip compression");
        return data;
    }

    zs.next_in = (Bytef*)data.data();
    zs.avail_in = data.size();

    int ret;
    char outbuffer[32768];
    std::string compressed;

    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = deflate(&zs, Z_FINISH);

        if (compressed.size() < zs.total_out) {
            compressed.append(outbuffer, zs.total_out - compressed.size());
        }
    } while (ret == Z_OK);

    deflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        Logger::debug("Failed to compress data with gzip");
        return data;
    }

    Logger::debug("Real GZIP compression applied: " + Utils::intToString(data.length()) + " -> " + Utils::intToString(compressed.length()) + " bytes");
    return compressed;
}

std::string Compression::simpleCompress(const std::string& data) {
    // Simple run-length encoding for demonstration
    // This is NOT real compression, just for bonus feature demonstration
    std::string compressed;
    
    if (data.empty()) return compressed;
    
    char current = data[0];
    int count = 1;
    
    for (size_t i = 1; i < data.length(); ++i) {
        if (data[i] == current && count < 255) {
            count++;
        } else {
            compressed += current;
            compressed += static_cast<char>(count);
            current = data[i];
            count = 1;
        }
    }
    
    compressed += current;
    compressed += static_cast<char>(count);
    
    // Only return compressed if it's actually smaller, otherwise add a small marker
    if (compressed.length() < data.length()) {
        return compressed;
    } else {
        // For demo purposes, add a small marker to show compression was attempted
        return "COMP:" + data;
    }
}
