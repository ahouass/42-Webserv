#include "Config.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cctype>
#include <cstdlib>

Config::Config() {}

std::string Config::trim(const std::string& str) {
    size_t start = 0;
    size_t end = str.length();
    
    while (start < end && std::isspace(str[start]))
        start++;
    while (end > start && std::isspace(str[end - 1]))
        end--;
    
    return str.substr(start, end - start);
}

std::vector<std::string> Config::split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    
    while (std::getline(tokenStream, token, delimiter)) {
        token = trim(token);
        if (!token.empty())
            tokens.push_back(token);
    }
    
    return tokens;
}

size_t Config::parseSize(const std::string& size_str) {
    if (size_str.empty())
        return 0;
    
    size_t multiplier = 1;
    std::string num_str = size_str;
    
    char last_char = size_str[size_str.length() - 1];
    if (last_char == 'K' || last_char == 'k') {
        multiplier = 1024;
        num_str = size_str.substr(0, size_str.length() - 1);
    } else if (last_char == 'M' || last_char == 'm') {
        multiplier = 1024 * 1024;
        num_str = size_str.substr(0, size_str.length() - 1);
    } else if (last_char == 'G' || last_char == 'g') {
        multiplier = 1024 * 1024 * 1024;
        num_str = size_str.substr(0, size_str.length() - 1);
    }
    
    return std::atoi(num_str.c_str()) * multiplier;
}

bool Config::isNumber(const std::string& str) {
    for (size_t i = 0; i < str.length(); i++) {
        if (!std::isdigit(str[i]))
            return false;
    }
    return !str.empty();
}

bool Config::parse(const std::string& filename) {
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open config file: " << filename << std::endl;
        return false;
    }
    
    std::string line;
    ServerConfig* current_server = NULL;
    LocationConfig* current_location = NULL;
    int brace_level = 0;
    bool in_server = false;
    bool in_location = false;
    
    while (std::getline(file, line)) {
        // Remove comments
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        
        line = trim(line);
        if (line.empty())
            continue;
        
        // Remove semicolon at the end
        if (line[line.length() - 1] == ';') {
            line = line.substr(0, line.length() - 1);
            line = trim(line);
        }
        
        // Handle braces
        if (line.find('{') != std::string::npos) {
            brace_level++;
            
            if (line.find("server") == 0) {
                servers.push_back(ServerConfig());
                current_server = &servers.back();
                in_server = true;
            }
            else if (line.find("location") == 0) {
                std::vector<std::string> tokens = split(line, ' ');
                if (tokens.size() >= 2 && current_server) {
                    current_server->locations.push_back(LocationConfig());
                    current_location = &current_server->locations.back();
                    current_location->path = tokens[1];
                    in_location = true;
                }
            }
            continue;
        }
        
        if (line.find('}') != std::string::npos) {
            brace_level--;
            if (in_location) {
                in_location = false;
                current_location = NULL;
            } else if (in_server) {
                in_server = false;
                current_server = NULL;
            }
            continue;
        }
        
        // Parse directives
        std::vector<std::string> tokens = split(line, ' ');
        if (tokens.empty())
            continue;
        
        std::string directive = tokens[0];
        
        // Server-level directives
        if (in_server && !in_location && current_server) {
            if (directive == "listen" && tokens.size() >= 2) {
                current_server->port = std::atoi(tokens[1].c_str());
            }
            else if (directive == "server_name" && tokens.size() >= 2) {
                current_server->server_name = tokens[1];
            }
            else if (directive == "root" && tokens.size() >= 2) {
                current_server->root = tokens[1];
            }
            else if (directive == "index" && tokens.size() >= 2) {
                current_server->index = tokens[1];
            }
            else if (directive == "client_max_body_size" && tokens.size() >= 2) {
                current_server->client_max_body_size = parseSize(tokens[1]);
            }
            else if (directive == "error_page" && tokens.size() >= 3) {
                // error_page 404 /404.html
                // or error_page 500 502 503 /50x.html
                std::string path = tokens[tokens.size() - 1]; // Last token is the path
                for (size_t i = 1; i < tokens.size() - 1; i++) {
                    if (isNumber(tokens[i])) {
                        int code = std::atoi(tokens[i].c_str());
                        current_server->error_pages[code] = path;
                    }
                }
            }
        }
        
        // Location-level directives
        if (in_location && current_location) {
            if (directive == "methods" && tokens.size() >= 2) {
                for (size_t i = 1; i < tokens.size(); i++) {
                    current_location->methods.push_back(tokens[i]);
                }
            }
            else if (directive == "autoindex" && tokens.size() >= 2) {
                current_location->autoindex = (tokens[1] == "on");
            }
            else if (directive == "upload_store" && tokens.size() >= 2) {
                current_location->upload_store = tokens[1];
            }
            else if (directive == "cgi_extension" && tokens.size() >= 2) {
                current_location->cgi_extension = tokens[1];
            }
            else if (directive == "cgi_path" && tokens.size() >= 2) {
                current_location->cgi_path = tokens[1];
            }
            else if (directive == "client_max_body_size" && tokens.size() >= 2) {
                current_location->client_max_body_size = parseSize(tokens[1]);
            }
        }
    }
    
    file.close();
    
    if (servers.empty()) {
        std::cerr << "Error: No server blocks found in config" << std::endl;
        return false;
    }
    
    return true;
}

void Config::print() const {
    std::cout << "\n========== Configuration ==========\n" << std::endl;
    
    for (size_t i = 0; i < servers.size(); i++) {
        const ServerConfig& srv = servers[i];
        std::cout << "Server " << (i + 1) << ":" << std::endl;
        std::cout << "  Port: " << srv.port << std::endl;
        std::cout << "  Server name: " << srv.server_name << std::endl;
        std::cout << "  Root: " << srv.root << std::endl;
        std::cout << "  Index: " << srv.index << std::endl;
        std::cout << "  Max body size: " << srv.client_max_body_size << " bytes" << std::endl;
        
        if (!srv.error_pages.empty()) {
            std::cout << "  Error pages:" << std::endl;
            for (std::map<int, std::string>::const_iterator it = srv.error_pages.begin();
                 it != srv.error_pages.end(); ++it) {
                std::cout << "    " << it->first << " -> " << it->second << std::endl;
            }
        }
        
        if (!srv.locations.empty()) {
            std::cout << "  Locations:" << std::endl;
            for (size_t j = 0; j < srv.locations.size(); j++) {
                const LocationConfig& loc = srv.locations[j];
                std::cout << "    " << loc.path << ":" << std::endl;
                
                if (!loc.methods.empty()) {
                    std::cout << "      Methods: ";
                    for (size_t k = 0; k < loc.methods.size(); k++) {
                        std::cout << loc.methods[k] << " ";
                    }
                    std::cout << std::endl;
                }
                
                std::cout << "      Autoindex: " << (loc.autoindex ? "on" : "off") << std::endl;
                
                if (!loc.upload_store.empty())
                    std::cout << "      Upload store: " << loc.upload_store << std::endl;
                if (!loc.cgi_extension.empty())
                    std::cout << "      CGI extension: " << loc.cgi_extension << std::endl;
                if (!loc.cgi_path.empty())
                    std::cout << "      CGI path: " << loc.cgi_path << std::endl;
                if (loc.client_max_body_size > 0)
                    std::cout << "      Max body size: " << loc.client_max_body_size << " bytes" << std::endl;
            }
        }
        std::cout << std::endl;
    }
    
    std::cout << "===================================\n" << std::endl;
}