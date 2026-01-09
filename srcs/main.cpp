#include <iostream>
#include "Server.hpp"

int main() {
    // Create server on port 8080, serving files from ./www
    Server server(8080, "./www");
    
    // Start the server
    if (!server.start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }
    
    // Run the server (infinite loop)
    server.run();
    
    return 0;
}