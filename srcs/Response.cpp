#include "../includes/Response.hpp"
#include <sstream>

Response::Response() : status_code(200), status_message("OK") {}

void Response::setStatus(int code, const std::string& message) {
    status_code = code;
    status_message = message;
}

void Response::setHeader(const std::string& key, const std::string& value) {
    headers[key] = value;
}

void Response::setBody(const std::string& content) {
    body = content;
}

std::string Response::toString() const {
    std::ostringstream response;
    
    // Status line
    response << "HTTP/1.1 " << status_code << " " << status_message << "\r\n";
    
    // Headers
    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end(); ++it) {
        response << it->first << ": " << it->second << "\r\n";
    }
    
    // Empty line
    response << "\r\n";
    
    // Body
    response << body;
    
    return response.str();
}