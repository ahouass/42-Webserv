#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include "Request.hpp"
#include "Response.hpp"
#include "Config.hpp"

class Server {
private:
    int server_fd;
    ServerConfig config;
    
public:
    Server();
    Server(const ServerConfig& cfg);
    ~Server();
    
    bool start();
    void run();  // Keep for single-server mode
    void stop();
    
    // NEW: Expose server_fd for poll()
    int getServerFd() const { return server_fd; }
    
    // NEW: Make handleClient public so ServerManager can call it
    void handleClient(int client_fd);
    
    // NEW: Get config info
    int getPort() const { return config.port; }
    std::string getServerName() const { return config.server_name; }
    
private:
    Response handleRequest(const Request& req);
    
    // Location matching
    const LocationConfig* findLocation(const std::string& path) const;
    bool isMethodAllowed(const std::string& method, const LocationConfig* location) const;
    
    // File operations
    std::string readFile(const std::string& path);
    bool fileExists(const std::string& path);
    bool isDirectory(const std::string& path);
    
    // Response builders
    Response serveFile(const std::string& path, const LocationConfig* location);
    Response serveDirectory(const std::string& path, const LocationConfig* location);
    Response serveErrorPage(int code, const std::string& message);
    Response serve404();
    Response serve405();
    Response serve500();
    
    // Helper
    std::string buildFilePath(const std::string& uri, const LocationConfig* location);
};

#endif