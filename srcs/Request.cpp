#include "../includes/Request.hpp"
#include <sstream>
#include <iostream>

Request::Request() {}

void Request::parse(const std::string& raw_request) {
    std::istringstream stream(raw_request);
    std::string line;
    
    // Parse request line: GET /index.html HTTP/1.1
    std::getline(stream, line);
    std::istringstream request_line(line);
    request_line >> method >> path >> version;
    
    std::cout << "Method: " << method << std::endl;
    std::cout << "Path: " << path << std::endl;
    std::cout << "Version: " << version << std::endl;
    
    // Parse headers
    while (std::getline(stream, line) && line != "\r") {
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 2); // Skip ": "
            // Remove \r if present
            if (!value.empty() && value[value.length()-1] == '\r')
                value.erase(value.length()-1);
            headers[key] = value;
        }
    }
    
    // The rest is the body (if any)
    std::string remaining;
    while (std::getline(stream, line)) {
        body += line + "\n";
    }
}

std::string Request::getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = headers.find(key);
    if (it != headers.end())
        return it->second;
    return "";
}