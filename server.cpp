#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    std::cout << "Starting server...\n";
    
    // 1. Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Socket creation failed\n";
        return 1;
    }
    
    // 2. Configure server address
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Accept connections from any IP
    server_addr.sin_port = htons(8080);        // Port 8080
    
    // 3. Bind socket to address
    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed\n";
        close(server_fd);
        return 1;
    }
    
    // 4. Listen for incoming connections
    if (listen(server_fd, 5) < 0) {  // Queue up to 5 connections
        std::cerr << "Listen failed\n";
        close(server_fd);
        return 1;
    }
    
    std::cout << "Server listening on port 8080...\n";
    
    // 5. Accept a client connection
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
    
    if (client_fd < 0) {
        std::cerr << "Accept failed\n";
        close(server_fd);
        return 1;
    }
    
    // Print client info
    // char client_ip[INET_ADDRSTRLEN];
    // inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    // std::cout << "Client connected from: " << client_ip << ":" 
    //           << ntohs(client_addr.sin_port) << "\n";
    
    // 6. Receive message from client
    char buffer[1024] = {0};
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received < 0) {
        std::cerr << "Receive failed\n";
    } else if (bytes_received == 0) {
        std::cout << "Client disconnected\n";
    } else {
        buffer[bytes_received] = '\0';  // Null-terminate the string
        std::cout << "Received from client: " << buffer << "\n";
        
        // 7. Send response back to client
        const char* response = "Message received by server!";
        send(client_fd, response, strlen(response), 0);
        std::cout << "Response sent to client\n";
    }
    
    // 8. Close sockets
    close(client_fd);
    close(server_fd);
    std::cout << "Server shutting down...\n";
    
    return 0;
}