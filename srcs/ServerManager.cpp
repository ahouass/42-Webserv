#include "ServerManager.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <cstring>
#include <fcntl.h>

ServerManager::ServerManager() {}

ServerManager::~ServerManager() {
    stop();
}

bool ServerManager::initServers(const std::vector<ServerConfig>& configs) {
    std::cout << "\n=== Initializing Servers ===" << std::endl;
    
    // Create and start each server
    for (size_t i = 0; i < configs.size(); i++) {
        Server* server = new Server(configs[i]);
        
        if (!server->start()) {
            std::cerr << "Failed to start server on port " << configs[i].port << std::endl;
            delete server;
            return false;
        }
        
        servers.push_back(server);
        
        // Add server socket to poll
        int server_fd = server->getServerFd();
        addPollFd(server_fd, POLLIN);
        fd_to_server[server_fd] = i;
        server_fds.insert(server_fd);  // Mark this as a server socket
        
        std::cout << "Server " << (i + 1) << " listening on port " 
                  << server->getPort() << std::endl;
    }
    
    std::cout << "\nAll servers started successfully!" << std::endl;
    std::cout << "Waiting for connections...\n" << std::endl;
    
    return true;
}

void ServerManager::run() {
    while (true) {
        // Wait for activity on any socket
        int activity = poll(&poll_fds[0], poll_fds.size(), -1);
        
        if (activity < 0) {
            std::cerr << "poll() error" << std::endl;
            break;
        }
        
        // Check each file descriptor
        for (size_t i = 0; i < poll_fds.size(); i++) {
            if (poll_fds[i].revents & POLLIN) {
                int fd = poll_fds[i].fd;
                
                // Check if this is a server socket (new connection)
                if (server_fds.find(fd) != server_fds.end()) {
                    // This is a server socket - new connection
                    handleNewConnection(fd_to_server[fd]);
                } else {
                    // This is a client socket - handle request
                    handleClientRequest(fd);
                }
            }
        }
    }
}

void ServerManager::handleNewConnection(int server_index) {
    Server* server = servers[server_index];
    int server_fd = server->getServerFd();
    
    // Accept new connection
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
        std::cerr << "Accept failed on server " << (server_index + 1) << std::endl;
        return;
    }
    
    // Set client socket to non-blocking mode
    int flags = fcntl(client_fd, F_GETFL, 0);
    if (flags < 0 || fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        std::cerr << "Failed to set non-blocking mode on client socket" << std::endl;
        close(client_fd);
        return;
    }
    
    std::cout << "--- New connection on server " << (server_index + 1) 
              << " (port " << server->getPort() << ") ---" << std::endl;
    
    // Add client to poll
    addPollFd(client_fd, POLLIN);
    fd_to_server[client_fd] = server_index;
}

void ServerManager::handleClientRequest(int client_fd) {
    int server_index = fd_to_server[client_fd];
    Server* server = servers[server_index];
    
    std::cout << "Handling request on server " << (server_index + 1) 
              << " (port " << server->getPort() << ")" << std::endl;
    
    // Let the server handle the client
    server->handleClient(client_fd);
    
    // Remove client from poll and close
    removePollFd(client_fd);
    fd_to_server.erase(client_fd);
    close(client_fd);
    
    std::cout << "--- Client disconnected ---\n" << std::endl;
}

void ServerManager::addPollFd(int fd, short events) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    poll_fds.push_back(pfd);
}

void ServerManager::removePollFd(int fd) {
    for (size_t i = 0; i < poll_fds.size(); i++) {
        if (poll_fds[i].fd == fd) {
            poll_fds.erase(poll_fds.begin() + i);
            break;
        }
    }
}

void ServerManager::stop() {
    std::cout << "\nStopping all servers..." << std::endl;
    
    // Close all client connections
    for (size_t i = 0; i < poll_fds.size(); i++) {
        close(poll_fds[i].fd);
    }
    poll_fds.clear();
    fd_to_server.clear();
    
    // Delete all servers
    for (size_t i = 0; i < servers.size(); i++) {
        delete servers[i];
    }
    servers.clear();
    
    std::cout << "All servers stopped." << std::endl;
}


