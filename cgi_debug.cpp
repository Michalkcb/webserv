#include <iostream>
#include <fstream>
#include <cstdlib>
#include <vector>
#include <chrono>

int main() {
    // Start timing
    auto start = std::chrono::high_resolution_clock::now();

    // Optional: log to file
    std::ofstream log("cgi_debug.log", std::ios::app);
    log << "=== CGI Execution Started ===\n";

    // Print required CGI headers
    std::cout << "Status: 200 OK\r\n";
    std::cout << "Content-Type: text/plain\r\n\r\n";

    // Log environment variables
    const char* env_vars[] = {
        "REQUEST_METHOD", "CONTENT_LENGTH", "CONTENT_TYPE",
        "QUERY_STRING", "SCRIPT_NAME", "PATH_INFO"
    };

    // Read from stdin
    const char* content_length_str = std::getenv("CONTENT_LENGTH");
    size_t content_length = content_length_str ? std::atoi(content_length_str) : 0;

    log << "Expected content length: " << content_length << "\n";

    size_t total_read = 0;
    std::vector<char> buffer(8192);
    while (std::cin.read(buffer.data(), buffer.size())) {
        total_read += buffer.size();
    }
    total_read += std::cin.gcount(); // final chunk
    log << "Actual bytes read from stdin: " << total_read << "\n";

    // Special case for 100MB test
    if (content_length == 100000000) {
        std::cout << "RETURNED_BODY_CONTENT: " << total_read << "\n";
//        std::cout << "RETURNED_BODY: " << "Read " << total_read << " bytes from stdin, expected " << content_length << "\n";
//        std::cout << "Received body of length: " << content_length << "\n";
//        std::cout << "CONTENT_BODY: " << content_length << "\n";
    }
/*/
    for (const char* var : env_vars) {
        const char* val = std::getenv(var);
        std::string output = std::string(var) + ": " + (val ? val : "undefined") + "\n";
        std::cout << output;
        log << output;
    }
//*/
    // End timing
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    log << "Execution time: " << duration_ms << " ms\n";
    log << "=== CGI Execution Finished ===\n\n";

    // Output summary to client
//    std::cout << "CGI debug complete.\n";
//    std::cout << "Read " << total_read << " bytes from stdin.\n";
//    std::cout << "Execution time: " << duration_ms << " ms\n";

    return 0;
}
