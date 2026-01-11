#ifndef SERVERMANAGER_HPP
#define SERVERMANAGER_HPP

#include <vector>
#include <map>
#include <set>
#include <poll.h>
#include "Server.hpp"
#include "Config.hpp"

class ServerManager {
private:
    std::vector<Server*> servers;
    std::vector<struct pollfd> poll_fds;
    std::map<int, int> fd_to_server;  // Maps fd to server index
    std::set<int> server_fds;         // Track which fds are server sockets
    
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
    int findServerByFd(int fd) const;
};

#endif