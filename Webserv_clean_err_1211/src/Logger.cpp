#include "Logger.hpp"

Logger::LogLevel Logger::_level = INFO;

void Logger::setLevel(LogLevel level) {
    _level = level;
}

std::string Logger::_getTimestamp() {
    time_t now = time(0);
    char* timeStr = ctime(&now);
    std::string timestamp(timeStr);
    if (!timestamp.empty() && timestamp[timestamp.length() - 1] == '\n') {
        timestamp.erase(timestamp.length() - 1);
    }
    return timestamp;
}

std::string Logger::_getLevelString(LogLevel level) {
    switch (level) {
        case DEBUG: return "DEBUG";
        case INFO:  return "INFO";
        case WARN:  return "WARN";
        case ERROR: return "ERROR";
        default:    return "UNKNOWN";
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level >= _level) {
        std::cerr << "[" << _getTimestamp() << "] " 
                  << _getLevelString(level) << ": " 
                  << message << std::endl;
    }
}

void Logger::debug(const std::string& message) {
    log(DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(INFO, message);
}

void Logger::warn(const std::string& message) {
    log(WARN, message);
}

void Logger::error(const std::string& message) {
    log(ERROR, message);
}
