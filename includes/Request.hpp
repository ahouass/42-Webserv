#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <map>

class Request {
private:
    std::string method;      // GET, POST, DELETE
    std::string path;        // /index.html
    std::string version;     // HTTP/1.1
    std::map<std::string, std::string> headers;
    std::string body;

public:
    Request();
    void parse(const std::string& raw_request);
    
    std::string getMethod() const { return method; }
    std::string getPath() const { return path; }
    std::string getHeader(const std::string& key) const;
};

#endif