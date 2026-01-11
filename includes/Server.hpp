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
    void handleClient(int client_fd, const Request& req);  // For pre-parsed requests
    
    // NEW: Get config info
    int getPort() const { return config.port; }
    std::string getServerName() const { return config.server_name; }
    const ServerConfig& getConfig() const { return config; }
    
private:
    Response handleRequest(const Request& req);
    
    // Location matching
    const LocationConfig* findLocation(const std::string& path) const;
    bool isMethodAllowed(const std::string& method, const LocationConfig* location) const;
    
    // File operations
    std::string readFile(const std::string& path);
    bool fileExists(const std::string& path);
    bool isDirectory(const std::string& path);
    bool writeFile(const std::string& path, const std::string& content);
    
    // Response builders
    Response serveFile(const std::string& path, const LocationConfig* location);
    Response serveDirectory(const std::string& path, const LocationConfig* location);
    Response serveErrorPage(int code, const std::string& message);
    Response serve200(const std::string& message);
    Response serve204();
    Response serve403();
    Response serve404();
    Response serve405();
    Response serve413();
    Response serve500();
    Response serve201(const std::string& message);
    
    // POST handling
    Response handlePost(const Request& req, const LocationConfig* location);
    Response handleMultipartUpload(const Request& req, const LocationConfig* location);
    Response handleRawUpload(const Request& req, const LocationConfig* location);
    
    // DELETE handling
    Response handleDelete(const Request& req, const LocationConfig* location);
    bool deleteFile(const std::string& path);
    
    // CGI handling
    Response handleCGI(const Request& req, const LocationConfig* location);
    
    // Helper
    std::string buildFilePath(const std::string& uri, const LocationConfig* location);
    std::string getUploadPath(const LocationConfig* location) const;
    std::string extractFilename(const std::string& content_disposition) const;
    std::string generateFilename() const;
};

#endif