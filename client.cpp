#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    std::cout << "Starting client...\n";
    
    // 1. Create socket
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        std::cerr << "Socket creation failed\n";
        return 1;
    }
    
    // 2. Configure server address to connect to
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);  // Port 8080
    
    // Convert IP address from text to binary
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address / Address not supported\n";
        close(client_fd);
        return 1;
    }
    
    // 3. Connect to server
    std::cout << "Connecting to server at 127.0.0.1:8080...\n";
    if (connect(client_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed\n";
        std::cerr << "Make sure server is running first!\n";
        close(client_fd);
        return 1;
    }
    
    std::cout << "Connected to server!\n";
    
    // 4. Send message to server
    const char* message = "Hello Server! This is Client.";
    int bytes_sent = send(client_fd, message, strlen(message), 0);
    
    if (bytes_sent < 0) {
        std::cerr << "Send failed\n";
        close(client_fd);
        return 1;
    }
    
    std::cout << "Message sent to server: " << message << "\n";
    
    // 5. Receive response from server
    char buffer[1024] = {0};
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received < 0) {
        std::cerr << "Receive failed\n";
    } else if (bytes_received == 0) {
        std::cout << "Server disconnected\n";
    } else {
        buffer[bytes_received] = '\0';  // Null-terminate
        std::cout << "Response from server: " << buffer << "\n";
    }
    
    // 6. Close socket
    close(client_fd);
    std::cout << "Client shutting down...\n";
    
    return 0;
}