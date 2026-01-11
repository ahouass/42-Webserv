#ifndef SERVERMANAGER_HPP
#define SERVERMANAGER_HPP

#include <vector>
#include <map>
#include <set>
#include <poll.h>
#include "Server.hpp"
#include "Config.hpp"
#include "Request.hpp"

// Tracks the state of a client connection
struct ClientState {
    Request request;
    int server_index;
    bool response_ready;
    
    ClientState() : server_index(-1), response_ready(false) {}
};

class ServerManager {
private:
    std::vector<Server*> servers;
    std::vector<struct pollfd> poll_fds;
    std::map<int, int> fd_to_server;        // Maps fd to server index
    std::set<int> server_fds;               // Track which fds are server sockets
    std::map<int, ClientState> client_states; // Track partial requests for each client
    
public:
    ServerManager();
    ~ServerManager();
    
    bool initServers(const std::vector<ServerConfig>& configs);
    void run();
    void stop();
    
private:
    void addPollFd(int fd, short events);
    void removePollFd(int fd);
    void handleNewConnection(int server_index);
    void handleClientRequest(int client_fd);
    void closeClient(int client_fd);
    int findServerByFd(int fd) const;
};

#endif