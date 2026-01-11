#include "Server.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

Server::Server() : server_fd(-1) {
    // Default config
    config.port = 8080;
    config.root = "./www";
    config.index = "index.html";
}

Server::Server(const ServerConfig& cfg) : server_fd(-1), config(cfg) {}

Server::~Server() {
    stop();
}

bool Server::start() {
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Error: Failed to create socket" << std::endl;
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Error: setsockopt failed" << std::endl;
        return false;
    }
    
    // Bind to port
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config.port);
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Error: Bind failed on port " << config.port << std::endl;
        return false;
    }
    
    // Listen
    if (listen(server_fd, 10) < 0) {
        std::cerr << "Error: Listen failed" << std::endl;
        return false;
    }
    
    std::cout << "\n=== Server Started ===" << std::endl;
    std::cout << "Port: " << config.port << std::endl;
    std::cout << "Root: " << config.root << std::endl;
    std::cout << "Server name: " << config.server_name << std::endl;
    std::cout << "Waiting for connections...\n" << std::endl;
    
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
        
        std::cout << "--- New client connected ---" << std::endl;
        
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
    
    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available yet on non-blocking socket
            return;
        }
        std::cerr << "Error: Failed to read request" << std::endl;
        return;
    }
    
    if (bytes_read == 0) {
        // Client closed connection
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
}

Response Server::handleRequest(const Request& req) {
    std::cout << "Method: " << req.getMethod() << std::endl;
    std::cout << "Path: " << req.getPath() << std::endl;
    
    // Find matching location
    const LocationConfig* location = findLocation(req.getPath());
    
    if (location) {
        std::cout << "Matched location: " << location->path << std::endl;
    } else {
        std::cout << "No specific location matched, using default" << std::endl;
    }
    
    // Check if method is allowed
    if (!isMethodAllowed(req.getMethod(), location)) {
        std::cout << "Method not allowed!" << std::endl;
        return serve405();
    }
    
    // Build file path
    std::string file_path = buildFilePath(req.getPath(), location);
    std::cout << "File path: " << file_path << std::endl;
    
    // Check if path exists
    if (!fileExists(file_path)) {
        std::cout << "File not found!" << std::endl;
        return serve404();
    }
    
    // Check if it's a directory
    if (isDirectory(file_path)) {
        return serveDirectory(file_path, location);
    }
    
    // It's a file, serve it
    return serveFile(file_path, location);
}

const LocationConfig* Server::findLocation(const std::string& path) const {
    const LocationConfig* best_match = NULL;
    size_t best_match_len = 0;
    
    // Find the longest matching location path
    for (size_t i = 0; i < config.locations.size(); i++) {
        const LocationConfig& loc = config.locations[i];
        
        // Check if path starts with location path
        if (path.find(loc.path) == 0) {
            size_t loc_len = loc.path.length();
            if (loc_len > best_match_len) {
                best_match = &loc;
                best_match_len = loc_len;
            }
        }
    }
    
    return best_match;
}

bool Server::isMethodAllowed(const std::string& method, const LocationConfig* location) const {
    // If no location specified, allow GET by default
    if (!location) {
        return method == "GET";
    }
    
    // If location has no methods specified, allow all
    if (location->methods.empty()) {
        return true;
    }
    
    // Check if method is in the allowed list
    for (size_t i = 0; i < location->methods.size(); i++) {
        if (location->methods[i] == method) {
            return true;
        }
    }
    
    return false;
}

std::string Server::buildFilePath(const std::string& uri, const LocationConfig* location) {
    std::string base_path = config.root;
    std::string path = uri;
    
    // If location has its own root, use it
    // Note: In your config, locations don't override root, but we can add this feature
    
    // Remove location prefix from path if needed
    if (location && location->path != "/") {
        if (path.find(location->path) == 0) {
            path = path.substr(location->path.length());
            if (path.empty()) {
                path = "/";
            }
        }
    }
    
    // Build full path
    std::string full_path = base_path + path;
    
    return full_path;
}

Response Server::serveFile(const std::string& path, const LocationConfig* location) {
    (void)location; // Unused for now
    
    Response res;
    
    std::string content = readFile(path);
    if (content.empty()) {
        std::cout << "Error reading file" << std::endl;
        return serve500();
    }
    
    res.setStatus(200, "OK");
    res.setHeader("Content-Type", Response::getContentType(path));
    res.setBody(content);
    
    std::cout << "File served successfully!" << std::endl;
    
    return res;
}

Response Server::serveDirectory(const std::string& path, const LocationConfig* location) {
    std::cout << "Path is a directory" << std::endl;
    
    // Try to serve index file
    std::string index_path = path;
    if (index_path[index_path.length() - 1] != '/') {
        index_path += "/";
    }
    index_path += config.index;
    
    if (fileExists(index_path)) {
        std::cout << "Serving index file: " << index_path << std::endl;
        return serveFile(index_path, location);
    }
    
    // If autoindex is enabled, show directory listing
    if (location && location->autoindex) {
        std::cout << "Generating directory listing (autoindex on)" << std::endl;
        
        Response res;
        res.setStatus(200, "OK");
        res.setHeader("Content-Type", "text/html");
        
        // Generate directory listing HTML
        std::ostringstream html;
        html << "<!DOCTYPE html>\n";
        html << "<html>\n<head>\n";
        html << "<title>Index of " << path << "</title>\n";
        html << "<style>\n";
        html << "body { font-family: Arial, sans-serif; margin: 40px; }\n";
        html << "h1 { color: #333; }\n";
        html << "ul { list-style: none; padding: 0; }\n";
        html << "li { padding: 5px; }\n";
        html << "a { text-decoration: none; color: #0066cc; }\n";
        html << "a:hover { text-decoration: underline; }\n";
        html << "</style>\n";
        html << "</head>\n<body>\n";
        html << "<h1>Index of " << path << "</h1>\n";
        html << "<ul>\n";
        
        // Read directory contents
        DIR* dir = opendir(path.c_str());
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                std::string name = entry->d_name;
                
                // Skip . but show ..
                if (name == ".") continue;
                
                html << "<li><a href=\"" << name;
                if (entry->d_type == DT_DIR) {
                    html << "/";
                }
                html << "\">" << name;
                if (entry->d_type == DT_DIR) {
                    html << "/";
                }
                html << "</a></li>\n";
            }
            closedir(dir);
        }
        
        html << "</ul>\n</body>\n</html>";
        
        res.setBody(html.str());
        return res;
    }
    
    // No index file and autoindex disabled = 403 Forbidden
    std::cout << "No index file and autoindex off = 403 Forbidden" << std::endl;
    
    Response res;
    res.setStatus(403, "Forbidden");
    res.setHeader("Content-Type", "text/html");
    res.setBody(
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><title>403 Forbidden</title></head>\n"
        "<body>\n"
        "<h1>403 Forbidden</h1>\n"
        "<p>Directory listing is disabled.</p>\n"
        "</body>\n"
        "</html>"
    );
    
    return res;
}

Response Server::serveErrorPage(int code, const std::string& message) {
    Response res;
    res.setStatus(code, message);
    res.setHeader("Content-Type", "text/html");
    
    // Check if custom error page exists
    std::map<int, std::string>::const_iterator it = config.error_pages.find(code);
    if (it != config.error_pages.end()) {
        std::string error_page_path = config.root + it->second;
        if (fileExists(error_page_path)) {
            std::string content = readFile(error_page_path);
            res.setBody(content);
            return res;
        }
    }
    
    // Default error page
    std::ostringstream html;
    html << "<!DOCTYPE html>\n";
    html << "<html>\n<head>\n";
    html << "<title>" << code << " " << message << "</title>\n";
    html << "</head>\n<body>\n";
    html << "<h1>" << code << " " << message << "</h1>\n";
    html << "</body>\n</html>";
    
    res.setBody(html.str());
    return res;
}

Response Server::serve404() {
    return serveErrorPage(404, "Not Found");
}

Response Server::serve405() {
    return serveErrorPage(405, "Method Not Allowed");
}

Response Server::serve500() {
    return serveErrorPage(500, "Internal Server Error");
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

bool Server::isDirectory(const std::string& path) {
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0) {
        return false;
    }
    return S_ISDIR(buffer.st_mode);
}

void Server::stop() {
    if (server_fd >= 0) {
        close(server_fd);
        server_fd = -1;
        std::cout << "Server stopped" << std::endl;
    }
}