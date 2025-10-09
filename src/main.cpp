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
    
    // Set logging level
    Logger::setLevel(Logger::DEBUG);
    
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
