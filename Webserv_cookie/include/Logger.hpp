#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "webserv.hpp"

class Logger {
public:
    enum LogLevel {
        DEBUG = 0,
        INFO = 1,
        WARN = 2,
        ERROR = 3
    };

private:
    static LogLevel _level;
    static std::string _getTimestamp();
    static std::string _getLevelString(LogLevel level);

public:
    static void setLevel(LogLevel level);
    static void log(LogLevel level, const std::string& message);
    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warn(const std::string& message);
    static void error(const std::string& message);
};

#endif
