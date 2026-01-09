#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include "Request.hpp"
#include "Response.hpp"

class Server {
private:
    int server_fd;
    int port;
    std::string root_directory;  // Where to look for files (e.g., "./www")
    
public:
    Server();
    Server(int port, const std::string& root);
    ~Server();
    
    bool start();
    void run();
    void stop();
    
private:
    void handleClient(int client_fd);
    Response handleRequest(const Request& req);
    std::string readFile(const std::string& path);
    bool fileExists(const std::string& path);
    Response serve404();
};

#endif