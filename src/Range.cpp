#include "Range.hpp"
#include "Utils.hpp"
#include "Logger.hpp"

Range::Range() : _contentLength(0) {
}

Range::Range(const std::string& rangeHeader, size_t contentLength) 
    : _contentLength(contentLength) {
    parseRangeHeader(rangeHeader, contentLength);
}

Range::Range(const Range& other) {
    *this = other;
}

Range& Range::operator=(const Range& other) {
    if (this != &other) {
        _ranges = other._ranges;
        _contentLength = other._contentLength;
    }
    return *this;
}

Range::~Range() {
}

bool Range::parseRangeHeader(const std::string& rangeHeader, size_t contentLength) {
    _contentLength = contentLength;
    _ranges.clear();
    
    if (rangeHeader.empty() || contentLength == 0) return false;
    
    // Check if it starts with "bytes="
    if (rangeHeader.substr(0, 6) != "bytes=") return false;
    
    std::string ranges = rangeHeader.substr(6);
    std::vector<std::string> rangeSpecs = Utils::split(ranges, ",");
    
    for (size_t i = 0; i < rangeSpecs.size(); ++i) {
        std::string spec = Utils::trim(rangeSpecs[i]);
        size_t dashPos = spec.find('-');
        
        if (dashPos == std::string::npos) continue;
        
        std::string startStr = spec.substr(0, dashPos);
        std::string endStr = spec.substr(dashPos + 1);
        
        ByteRange range;
        
        if (startStr.empty() && !endStr.empty()) {
            // Suffix range: -500 (last 500 bytes)
            size_t suffixLength = Utils::stringToSize(endStr);
            if (suffixLength > 0 && suffixLength <= contentLength) {
                range.start = contentLength - suffixLength;
                range.end = contentLength - 1;
                range.isValid = true;
            }
        } else if (!startStr.empty() && endStr.empty()) {
            // Range from start to end: 500-
            size_t start = Utils::stringToSize(startStr);
            if (start < contentLength) {
                range.start = start;
                range.end = contentLength - 1;
                range.isValid = true;
            }
        } else if (!startStr.empty() && !endStr.empty()) {
            // Complete range: 0-499
            size_t start = Utils::stringToSize(startStr);
            size_t end = Utils::stringToSize(endStr);
            if (start <= end && start < contentLength) {
                range.start = start;
                range.end = (end < contentLength) ? end : contentLength - 1;
                range.isValid = true;
            }
        }
        
        if (range.isValid) {
            _ranges.push_back(range);
        }
    }
    
    return !_ranges.empty();
}

bool Range::isValid() const {
    return !_ranges.empty();
}

bool Range::isSingleRange() const {
    return _ranges.size() == 1;
}

bool Range::isMultiRange() const {
    return _ranges.size() > 1;
}

const std::vector<ByteRange>& Range::getRanges() const {
    return _ranges;
}

ByteRange Range::getFirstRange() const {
    return _ranges.empty() ? ByteRange() : _ranges[0];
}

size_t Range::getTotalRanges() const {
    return _ranges.size();
}

std::string Range::extractRange(const std::string& content, const ByteRange& range) const {
    if (!range.isValid || range.start >= content.length()) {
        return "";
    }
    
    size_t actualEnd = (range.end < content.length()) ? range.end : content.length() - 1;
    size_t length = actualEnd - range.start + 1;
    
    return content.substr(range.start, length);
}

std::string Range::generateMultipartBody(const std::string& content, const std::string& contentType) const {
    if (_ranges.size() <= 1) return "";
    
    std::string boundary = generateBoundary();
    std::string body;
    
    for (size_t i = 0; i < _ranges.size(); ++i) {
        const ByteRange& range = _ranges[i];
        if (!range.isValid) continue;
        
        body += "\r\n--" + boundary + "\r\n";
        body += "Content-Type: " + contentType + "\r\n";
        body += "Content-Range: bytes " + Utils::intToString(range.start) + "-" 
                + Utils::intToString(range.end) + "/" + Utils::intToString(_contentLength) + "\r\n";
        body += "\r\n";
        body += extractRange(content, range);
    }
    
    body += "\r\n--" + boundary + "--\r\n";
    return body;
}

std::string Range::generateContentRangeHeader(const ByteRange& range) const {
    if (!range.isValid) return "";
    
    return "bytes " + Utils::intToString(range.start) + "-" 
           + Utils::intToString(range.end) + "/" + Utils::intToString(_contentLength);
}

std::string Range::generateContentLengthHeader(const ByteRange& range) const {
    if (!range.isValid) return "0";
    
    return Utils::intToString(range.end - range.start + 1);
}

bool Range::isRangeRequest(const std::string& rangeHeader) {
    return !rangeHeader.empty() && rangeHeader.substr(0, 6) == "bytes=";
}

std::string Range::generateBoundary() {
    std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string boundary = "webserv_multipart_";
    
    srand(time(NULL) + rand());
    for (int i = 0; i < 16; ++i) {
        boundary += chars[rand() % chars.length()];
    }
    
    return boundary;
}
