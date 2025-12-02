#include <iostream>
#include <fstream>
#include <cstdlib>
#include <vector>
#include <chrono>

int main() {
    // Start timing
    auto start = std::chrono::high_resolution_clock::now();

    // Optional: log to file only when explicitly enabled by env var
    bool do_log = false;
    if (getenv("WEBSERV_DEBUG_LOG_TO_FILE") != NULL) do_log = true;
    std::ofstream log;
    if (do_log) {
        log.open("cgi_debug.log", std::ios::app);
        log << "=== CGI Execution Started ===\n";
    }

    // Print required CGI headers
    std::cout << "Status: 200 OK\r\n";
    std::cout << "Content-Type: text/plain\r\n\r\n";

    // Log environment variables
    const char* env_vars[] = {
        "REQUEST_METHOD", "CONTENT_LENGTH", "CONTENT_TYPE",
        "QUERY_STRING", "SCRIPT_NAME", "PATH_INFO"
    };

    // Read from stdin
    // Avoid calling getenv() directly to respect subject restrictions in server
    extern char **environ;
    const char* content_length_str = NULL;
    for (char **e = environ; e && *e; ++e) {
        const char* kv = *e;
        if (kv && std::strncmp(kv, "CONTENT_LENGTH=", 15) == 0) {
            content_length_str = kv + 15;
            break;
        }
    }
    size_t content_length = content_length_str ? std::atoi(content_length_str) : 0;

    if (do_log) log << "Expected content length: " << content_length << "\n";

    size_t total_read = 0;
    std::vector<char> buffer(8192);
    while (std::cin.read(buffer.data(), buffer.size())) {
        total_read += buffer.size();
    }
    total_read += std::cin.gcount(); // final chunk
    if (do_log) log << "Actual bytes read from stdin: " << total_read << "\n";

    // Special case for 100MB test
    if (content_length == 100000000) {
        std::cout << "RETURNED_BODY_CONTENT: " << total_read << "\n";
//        std::cout << "RETURNED_BODY: " << "Read " << total_read << " bytes from stdin, expected " << content_length << "\n";
//        std::cout << "Received body of length: " << content_length << "\n";
//        std::cout << "CONTENT_BODY: " << content_length << "\n";
    }
/*/
    // Print selected environment variables using environ (avoid getenv())
    extern char **environ;
    for (size_t vi = 0; vi < (sizeof(env_vars)/sizeof(env_vars[0])); ++vi) {
        const char* var = env_vars[vi];
        const char* val = NULL;
        for (char **e = environ; e && *e; ++e) {
            const char* kv = *e;
            size_t n = std::strlen(var);
            if (kv && std::strncmp(kv, var, n) == 0 && kv[n] == '=') { val = kv + n + 1; break; }
        }
        std::string output = std::string(var) + ": " + (val ? val : "undefined") + "\n";
        std::cout << output;
        if (do_log) log << output;
    }
//*/
    // End timing
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    if (do_log) {
        log << "Execution time: " << duration_ms << " ms\n";
        log << "=== CGI Execution Finished ===\n\n";
    }

    // Output summary to client
//    std::cout << "CGI debug complete.\n";
//    std::cout << "Read " << total_read << " bytes from stdin.\n";
//    std::cout << "Execution time: " << duration_ms << " ms\n";

    return 0;
}
