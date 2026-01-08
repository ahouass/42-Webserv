#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include "../includes/Request.hpp"
#include "../includes/Response.hpp"

int main() {
    // Create socket (same as before)
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);
    
    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 10);
    
    std::cout << "Server listening on port 8080..." << std::endl;
    
    while (true) {  // Keep accepting connections
        int client_fd = accept(server_fd, NULL, NULL);
        
        // Read request
        char buffer[4096] = {0};
        read(client_fd, buffer, 4096);
        
        // Parse request
        Request req;
        req.parse(std::string(buffer));
        
        // Create response
        Response res;
        res.setStatus(200, "OK");
        res.setHeader("Content-Type", "text/html");
        res.setBody("<h1>Hello from Webserv!</h1>");
        
        // Send response
        std::string response_str = res.toString();
        send(client_fd, response_str.c_str(), response_str.length(), 0);
        
        close(client_fd);
    }
    
    close(server_fd);
    return 0;
}