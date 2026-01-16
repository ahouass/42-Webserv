#include "ServerManager.hpp"
#include "Response.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <cstring>
#include <fcntl.h>
#include <errno.h>

ServerManager::ServerManager() {}

ServerManager::~ServerManager() {
    stop();
}

bool ServerManager::initServers(const std::vector<ServerConfig>& configs) {
    std::cout << "=== Initializing Servers ===" << std::endl;
    
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
    
    std::cout << "[" << server->getConfig().server_name << ":" << server->getPort() << "] New connection" << std::endl;
    
    // Add client to poll
    addPollFd(client_fd, POLLIN);
    fd_to_server[client_fd] = server_index;
    
    // Initialize client state for incremental parsing
    ClientState state;
    state.server_index = server_index;
    client_states[client_fd] = state;
}

void ServerManager::handleClientRequest(int client_fd) {
    // Read available data from socket
    char buffer[8192];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    
    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available right now, will try again later
            return;
        }
        std::cerr << "Read error on client fd " << client_fd << std::endl;
        closeClient(client_fd);
        return;
    }
    
    if (bytes_read == 0) {
        // Client closed connection
        closeClient(client_fd);
        return;
    }
    
    buffer[bytes_read] = '\0';
    
    // Get client state
    std::map<int, ClientState>::iterator it = client_states.find(client_fd);
    if (it == client_states.end()) {
        std::cerr << "No state found for client fd " << client_fd << std::endl;
        closeClient(client_fd);
        return;
    }
    
    ClientState& state = it->second;
    Request& req = state.request;
    
    // Append received data to request
    req.appendData(std::string(buffer, bytes_read));
    
    // Try to parse headers if not done yet
    if (!req.isHeadersComplete()) {
        if (!req.parseHeaders()) {
            // Headers not complete yet, wait for more data
            return;
        }
        
        // Check body size limit early
        Server* server = servers[state.server_index];
        size_t max_size = server->getConfig().client_max_body_size;
        if (req.getContentLength() > max_size) {
            // Send 413 and close
            Response res;
            res.setStatus(413, "Payload Too Large");
            res.setHeader("Content-Type", "text/html");
            res.setHeader("Connection", "close");
            res.setBody("<html><body><h1>413 Payload Too Large</h1></body></html>");
            
            std::string response_str = res.toString();
            send(client_fd, response_str.c_str(), response_str.length(), 0);
            closeClient(client_fd);
            return;
        }
    }
    
    // Check if request is complete (headers + full body)
    if (!req.isComplete()) {
        // Still waiting for body data
        return;
    }
    
    // Request is complete, process it
    
    // Get the original server (based on which port received the connection)
    Server* original_server = servers[state.server_index];
    int port = original_server->getPort();
    
    // Check Host header for HTTP/1.1 compliance
    std::string host_header = req.getHeader("Host");
    if (host_header.empty() && req.getVersion() == "HTTP/1.1") {
        // HTTP/1.1 requires Host header
        Response res;
        res.setStatus(400, "Bad Request");
        res.setHeader("Content-Type", "text/html");
        res.setHeader("Connection", "close");
        res.setBody("<html><body><h1>400 Bad Request</h1><p>Missing Host header</p></body></html>");
        
        std::string response_str = res.toString();
        send(client_fd, response_str.c_str(), response_str.length(), 0);
        closeClient(client_fd);
        return;
    }
    
    // Find the correct server based on Host header (virtual hosting)
    int server_index = state.server_index;  // Default to original
    if (!host_header.empty()) {
        int matched = findServerByHost(host_header, port);
        if (matched != -1) {
            server_index = matched;
        }
    }
    
    Server* server = servers[server_index];
    std::cout << "[" << server->getConfig().server_name << ":" << server->getPort() 
              << "] " << req.getMethod() << " " << req.getPath() << std::endl;
    
    // Let the server handle the complete request (use pre-parsed request)
    server->handleClient(client_fd, req);
    
    // Close connection after response
    closeClient(client_fd);
}

void ServerManager::closeClient(int client_fd) {
    removePollFd(client_fd);
    fd_to_server.erase(client_fd);
    client_states.erase(client_fd);
    close(client_fd);
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
    client_states.clear();
    
    // Delete all servers
    for (size_t i = 0; i < servers.size(); i++) {
        delete servers[i];
    }
    servers.clear();
}

// Extract hostname from Host header (removes port if present)
// e.g., "localhost:8080" -> "localhost"
std::string ServerManager::extractHostname(const std::string& host) const {
    size_t colon_pos = host.find(':');
    if (colon_pos != std::string::npos) {
        return host.substr(0, colon_pos);
    }
    return host;
}

// Find the best matching server based on Host header and port
// Returns server index, or -1 if no match (should use default)
int ServerManager::findServerByHost(const std::string& host, int port) const {
    std::string hostname = extractHostname(host);
    int first_match_on_port = -1;
    
    for (size_t i = 0; i < servers.size(); i++) {
        const ServerConfig& config = servers[i]->getConfig();
        
        // Check if this server listens on the same port
        if (config.port == port) {
            // Remember first server on this port as fallback
            if (first_match_on_port == -1) {
                first_match_on_port = i;
            }
            
            // Check if server_name matches
            if (config.server_name == hostname) {
                return i;
            }
        }
    }
    
    // No exact match, use first server on this port as default
    return first_match_on_port;
}


