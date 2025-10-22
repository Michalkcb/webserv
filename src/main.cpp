#include "webserv.hpp"
#include "Server.hpp"
#include "Logger.hpp"
#include "Utils.hpp"

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [configuration_file]" << std::endl;
    std::cout << "  configuration_file: Path to server configuration file (optional)" << std::endl;
    std::cout << "                     Default: ./config/default.conf" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string configFile = "./config/default.conf";
    
    // Parse command line arguments
    if (argc > 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    if (argc == 2) {
        configFile = argv[1];
    }
    
    // Ustaw poziom logowania: domyślnie INFO, z możliwością nadpisania
    // przez zmienną środowiskową WEBSERV_LOG_LEVEL (DEBUG|INFO|WARN|ERROR).
    {
        const char* lv = getenv("WEBSERV_LOG_LEVEL");
        if (lv) {
            if (strcmp(lv, "DEBUG") == 0)      Logger::setLevel(Logger::DEBUG);
            else if (strcmp(lv, "INFO") == 0)  Logger::setLevel(Logger::INFO);
            else if (strcmp(lv, "WARN") == 0)  Logger::setLevel(Logger::WARN);
            else if (strcmp(lv, "ERROR") == 0) Logger::setLevel(Logger::ERROR);
            else                                 Logger::setLevel(Logger::INFO);
        } else {
            Logger::setLevel(Logger::INFO);
        }
    }
    
    try {
        Logger::info("=== Webserv HTTP Server ===");
        Logger::info("Version: 1.0");
        Logger::info("Configuration file: " + configFile);
        
        // Create and start server
        Server server(configFile);
        server.start();
        server.run();
        
    } catch (const std::exception& e) {
        Logger::error("Server error: " + std::string(e.what()));
        return 1;
    }
    
    Logger::info("Server shutdown complete");
    return 0;
}
