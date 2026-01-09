#include "Server.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

Server::Server() : server_fd(-1), port(8080), root_directory("./www") {}

Server::Server(int p, const std::string& root) 
    : server_fd(-1), port(p), root_directory(root) {}

Server::~Server() {
    stop();
}

bool Server::start() {
    // 1. Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Error: Failed to create socket" << std::endl;
        return false;
    }
    
    // 2. Set socket options (allow address reuse)
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Error: setsockopt failed" << std::endl;
        return false;
    }
    
    // 3. Bind to port
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Error: Bind failed on port " << port << std::endl;
        return false;
    }
    
    // 4. Listen
    if (listen(server_fd, 10) < 0) {
        std::cerr << "Error: Listen failed" << std::endl;
        return false;
    }
    
    std::cout << "Server started on port " << port << std::endl;
    std::cout << "Root directory: " << root_directory << std::endl;
    std::cout << "Waiting for connections..." << std::endl;
    
    return true;
}

void Server::run() {
    while (true) {
        // Accept connection
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            std::cerr << "Error: Accept failed" << std::endl;
            continue;
        }
        
        std::cout << "\n--- New client connected ---" << std::endl;
        
        // Handle the client
        handleClient(client_fd);
        
        // Close connection
        close(client_fd);
        std::cout << "--- Client disconnected ---\n" << std::endl;
    }
}

void Server::handleClient(int client_fd) {
    // Read request
    char buffer[4096] = {0};
    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    
    if (bytes_read <= 0) {
        std::cerr << "Error: Failed to read request" << std::endl;
        return;
    }
    
    // Parse request
    Request req;
    req.parse(std::string(buffer));
    
    // Handle request and create response
    Response res = handleRequest(req);
    
    // Send response
    std::string response_str = res.toString();
    send(client_fd, response_str.c_str(), response_str.length(), 0);
    
    std::cout << "Response sent: " << res.toString().substr(0, 50) << "..." << std::endl;
}

Response Server::handleRequest(const Request& req) {
    Response res;
    
    std::cout << "Method: " << req.getMethod() << std::endl;
    std::cout << "Path: " << req.getPath() << std::endl;
    
    // Only handle GET for now
    if (req.getMethod() != "GET") {
        res.setStatus(501, "Not Implemented");
        res.setHeader("Content-Type", "text/html");
        res.setBody("<h1>501 Not Implemented</h1><p>Only GET method is supported.</p>");
        return res;
    }
    
    // Build file path
    std::string path = req.getPath();
    
    // If path is "/", serve index.html
    if (path == "/") {
        path = "/index.html";
    }
    
    std::string file_path = root_directory + path;
    
    std::cout << "Looking for file: " << file_path << std::endl;
    
    // Check if file exists
    if (!fileExists(file_path)) {
        std::cout << "File not found!" << std::endl;
        return serve404();
    }
    
    // Read file
    std::string content = readFile(file_path);
    if (content.empty()) {
        std::cout << "Error reading file!" << std::endl;
        return serve404();
    }
    
    // Success! Send file
    res.setStatus(200, "OK");
    res.setHeader("Content-Type", Response::getContentType(file_path));
    res.setBody(content);
    
    std::cout << "File served successfully!" << std::endl;
    
    return res;
}

std::string Server::readFile(const std::string& path) {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return buffer.str();
}

bool Server::fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

Response Server::serve404() {
    Response res;
    res.setStatus(404, "Not Found");
    res.setHeader("Content-Type", "text/html");
    
    // Try to serve custom 404 page
    std::string custom_404_path = root_directory + "/404.html";
    if (fileExists(custom_404_path)) {
        std::string content = readFile(custom_404_path);
        res.setBody(content);
    } else {
        // Default 404 page
        res.setBody(
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head><title>404 Not Found</title></head>\n"
            "<body>\n"
            "<h1>404 - Page Not Found</h1>\n"
            "<p>The requested page could not be found.</p>\n"
            "</body>\n"
            "</html>"
        );
    }
    
    return res;
}

void Server::stop() {
    if (server_fd >= 0) {
        close(server_fd);
        server_fd = -1;
        std::cout << "Server stopped" << std::endl;
    }
}