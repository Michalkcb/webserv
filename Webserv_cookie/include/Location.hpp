#ifndef LOCATION_HPP
#define LOCATION_HPP

#include "webserv.hpp"

class Location {
private:
    std::string _path;
    std::string _root;
    std::string _index;
    std::string _redirect;
    std::vector<std::string> _allowedMethods;
    bool _autoindex;
    std::string _uploadPath;
    std::string _cgiPath;
    std::string _cgiExtension;
    size_t _maxBodySize;

public:
    Location();
    Location(const std::string& path);
    Location(const Location& other);
    Location& operator=(const Location& other);
    ~Location();

    // Getters
    const std::string& getPath() const;
    const std::string& getRoot() const;
    const std::string& getIndex() const;
    const std::string& getRedirect() const;
    const std::vector<std::string>& getAllowedMethods() const;
    bool getAutoindex() const;
    const std::string& getUploadPath() const;
    const std::string& getCgiPath() const;
    const std::string& getCgiExtension() const;
    size_t getMaxBodySize() const;

    // Setters
    void setPath(const std::string& path);
    void setRoot(const std::string& root);
    void setIndex(const std::string& index);
    void setRedirect(const std::string& redirect);
    void setAllowedMethods(const std::vector<std::string>& methods);
    void addAllowedMethod(const std::string& method);
    void setAutoindex(bool autoindex);
    void setUploadPath(const std::string& uploadPath);
    void setCgiPath(const std::string& cgiPath);
    void setCgiExtension(const std::string& cgiExtension);
    void setMaxBodySize(size_t maxBodySize);

    // Methods
    bool isMethodAllowed(const std::string& method) const;
    bool matches(const std::string& uri) const;
    std::string getFullPath(const std::string& uri) const;
    bool isCgiRequest(const std::string& uri) const;
};

#endif
