#include <iostream>
#include "Server.hpp"
#include "Config.hpp"

int main(int argc, char** argv) {
    // Default config file
    std::string config_file = "config/default.conf";
    
    // Check for command-line argument
    if (argc > 1) {
        config_file = argv[1];
    }
    
    std::cout << "=== Webserv Starting ===" << std::endl;
    std::cout << "Loading config: " << config_file << std::endl << std::endl;
    
    // Parse config file
    Config config;
    if (!config.parse(config_file)) {
        std::cerr << "Failed to parse config file" << std::endl;
        return 1;
    }
    
    // Print parsed config
    config.print();
    
    // Get servers from config
    const std::vector<ServerConfig>& servers = config.getServers();
    
    // For now, just start the first server
    // Later you'll create multiple Server objects for multiple server blocks
    if (servers.empty()) {
        std::cerr << "No servers configured" << std::endl;
        return 1;
    }
    
    const ServerConfig& server_config = servers[0];
    
    // Create server with config values
    Server server(server_config.port, server_config.root);
    
    // Start server
    if (!server.start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }
    
    // Run server (infinite loop)
    server.run();
    
    return 0;
}