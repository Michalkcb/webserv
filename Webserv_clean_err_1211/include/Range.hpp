#ifndef RANGE_HPP
#define RANGE_HPP

#include "webserv.hpp"

struct ByteRange {
    size_t start;
    size_t end;
    bool isValid;
    
    ByteRange() : start(0), end(0), isValid(false) {}
    ByteRange(size_t s, size_t e) : start(s), end(e), isValid(true) {}
};

class Range {
private:
    std::vector<ByteRange> _ranges;
    size_t _contentLength;

public:
    Range();
    Range(const std::string& rangeHeader, size_t contentLength);
    Range(const Range& other);
    Range& operator=(const Range& other);
    ~Range();

    // Range parsing
    bool parseRangeHeader(const std::string& rangeHeader, size_t contentLength);
    
    // Range validation
    bool isValid() const;
    bool isSingleRange() const;
    bool isMultiRange() const;
    
    // Range access
    const std::vector<ByteRange>& getRanges() const;
    ByteRange getFirstRange() const;
    size_t getTotalRanges() const;
    
    // Content generation
    std::string extractRange(const std::string& content, const ByteRange& range) const;
    std::string generateMultipartBody(const std::string& content, const std::string& contentType) const;
    
    // Response headers
    std::string generateContentRangeHeader(const ByteRange& range) const;
    std::string generateContentLengthHeader(const ByteRange& range) const;
    
    // Static utilities
    static bool isRangeRequest(const std::string& rangeHeader);
    static std::string generateBoundary();
};

#endif
