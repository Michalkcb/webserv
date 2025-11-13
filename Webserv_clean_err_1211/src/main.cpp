#include "webserv.hpp"
#include "Server.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "CGI.hpp"
#include <vector>

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
    
    // Ustaw poziom logowania: domyślnie INFO (bez użycia getenv, by zachować zgodność z en.subject)
    Logger::setLevel(Logger::INFO);
    
    try {
        Logger::info("=== Webserv HTTP Server ===");
        Logger::info("Version: 1.0");
        Logger::info("Configuration file: " + configFile);

        // Startup check: resolve interpreter paths for common script extensions
        {
            CGI tmpCgi; // use getCgiInterpreter helper
            std::vector<std::string> exts;
            exts.push_back("php");
            exts.push_back("py");
            exts.push_back("pl");
            exts.push_back("rb");
            exts.push_back("sh");
            exts.push_back("bla");
            for (size_t i = 0; i < exts.size(); ++i) {
                const std::string& e = exts[i];
                std::string dummy = std::string("dummy.") + e;
                std::string interp = tmpCgi.getCgiInterpreter(dummy);
                if (interp.empty()) {
                    Logger::info(std::string("Interpreter for .") + e + " => (execute directly / shebang or no interpreter)");
                } else {
                    bool found = Utils::fileExists(interp);
                    Logger::info(std::string("Interpreter for .") + e + " => " + interp + (found ? " (found)" : " (MISSING)"));
                }
            }
        }
        
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
