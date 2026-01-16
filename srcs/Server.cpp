#include "Server.hpp"
#include "CGI.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <ctime>
#include <cstdlib>
#include <cstdio>

Server::Server() : server_fd(-1) {
    // Default config
    config.port = 8080;
    config.root = "./www";
    config.index = "index.html";
}

Server::Server(const ServerConfig& cfg) : server_fd(-1), config(cfg) {}

Server::~Server() {
    stop();
}

bool Server::start() {
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Error: Failed to create socket" << std::endl;
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Error: setsockopt failed" << std::endl;
        return false;
    }
    
    // Bind to port
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config.port);
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Error: Bind failed on port " << config.port << std::endl;
        return false;
    }
    
    // Listen
    if (listen(server_fd, 10) < 0) {
        std::cerr << "Error: Listen failed" << std::endl;
        return false;
    }
    
    std::cout << "\n=== Server Started ===" << std::endl;
    std::cout << "Port: " << config.port << std::endl;
    std::cout << "Root: " << config.root << std::endl;
    std::cout << "Server name: " << config.server_name << std::endl;
    std::cout << "Waiting for connections...\n" << std::endl;
    
    return true;
}

void Server::run() {
    while (true) {
        // Accept connection
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            std::cerr << "Error: Accept failed" << std::endl;
            continue;
        }
        
        std::cout << "--- New client connected ---" << std::endl;
        
        // Handle the client
        handleClient(client_fd);
        
        // Close connection
        close(client_fd);
        std::cout << "--- Client disconnected ---\n" << std::endl;
    }
}

void Server::handleClient(int client_fd) {
    // Read request
    char buffer[8192] = {0};
    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    
    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available yet on non-blocking socket
            return;
        }
        std::cerr << "Error: Failed to read request" << std::endl;
        return;
    }
    
    if (bytes_read == 0) {
        // Client closed connection
        return;
    }
    
    // Parse request
    Request req;
    req.parse(std::string(buffer, bytes_read));
    
    // Handle with parsed request
    handleClient(client_fd, req);
}

void Server::handleClient(int client_fd, const Request& req) {
    // Handle request and create response
    Response res = handleRequest(req);
    
    // Send response
    std::string response_str = res.toString();
    send(client_fd, response_str.c_str(), response_str.length(), 0);
}

Response Server::handleRequest(const Request& req) {
    std::cout << "Method: " << req.getMethod() << std::endl;
    std::cout << "Path: " << req.getPath() << std::endl;
    
    // Find matching location
    const LocationConfig* location = findLocation(req.getPath());
    
    if (location) {
        std::cout << "Matched location: " << location->path << std::endl;
    } else {
        std::cout << "No specific location matched, using default" << std::endl;
    }
    
    // Check if method is allowed
    if (!isMethodAllowed(req.getMethod(), location)) {
        std::cout << "Method not allowed!" << std::endl;
        return serve405();
    }
    
    // Check body size limits for POST
    if (req.getMethod() == "POST") {
        size_t max_size = config.client_max_body_size;
        if (location && location->client_max_body_size > 0) {
            max_size = location->client_max_body_size;
        }
        if (req.getContentLength() > max_size) {
            std::cout << "Request body too large: " << req.getContentLength() 
                      << " > " << max_size << std::endl;
            return serve413();
        }
    }
    
    // Check for CGI request
    if (location && !location->cgi_extension.empty()) {
        if (CGI::isCGIRequest(req.getPath(), location->cgi_extension)) {
            std::cout << "CGI request detected" << std::endl;
            return handleCGI(req, location);
        }
    }
    
    // Handle POST requests (non-CGI)
    if (req.getMethod() == "POST") {
        return handlePost(req, location);
    }
    
    // Handle DELETE requests
    if (req.getMethod() == "DELETE") {
        return handleDelete(req, location);
    }
    
    // Build file path for GET
    std::string file_path = buildFilePath(req.getPath(), location);
    std::cout << "File path: " << file_path << std::endl;
    
    // Check if path exists
    if (!fileExists(file_path)) {
        std::cout << "File not found!" << std::endl;
        return serve404();
    }
    
    // Check if it's a directory
    if (isDirectory(file_path)) {
        return serveDirectory(file_path, location);
    }
    
    // It's a file, serve it
    return serveFile(file_path, location);
}

const LocationConfig* Server::findLocation(const std::string& path) const {
    const LocationConfig* best_match = NULL;
    size_t best_match_len = 0;
    
    // Find the longest matching location path
    for (size_t i = 0; i < config.locations.size(); i++) {
        const LocationConfig& loc = config.locations[i];
        
        // Check if path starts with location path
        if (path.find(loc.path) == 0) {
            size_t loc_len = loc.path.length();
            if (loc_len > best_match_len) {
                best_match = &loc;
                best_match_len = loc_len;
            }
        }
    }
    
    return best_match;
}

bool Server::isMethodAllowed(const std::string& method, const LocationConfig* location) const {
    // If no location specified, allow GET by default
    if (!location) {
        return method == "GET";
    }
    
    // If location has no methods specified, allow all
    if (location->methods.empty()) {
        return true;
    }
    
    // Check if method is in the allowed list
    for (size_t i = 0; i < location->methods.size(); i++) {
        if (location->methods[i] == method) {
            return true;
        }
    }
    
    return false;
}

std::string Server::buildFilePath(const std::string& uri, const LocationConfig* location) {
    (void)location; // Location doesn't override root in current config
    
    std::string base_path = config.root;
    std::string path = uri;
    
    // Build full path - keep the URI path as-is
    // The location path is part of the filesystem structure
    std::string full_path = base_path + path;
    
    return full_path;
}

Response Server::serveFile(const std::string& path, const LocationConfig* location) {
    (void)location; // Unused for now
    
    Response res;
    
    std::string content = readFile(path);
    if (content.empty()) {
        std::cout << "Error reading file" << std::endl;
        return serve500();
    }
    
    res.setStatus(200, "OK");
    res.setHeader("Content-Type", Response::getContentType(path));
    res.setBody(content);
    
    std::cout << "File served successfully!" << std::endl;
    
    return res;
}

Response Server::serveDirectory(const std::string& path, const LocationConfig* location) {
    std::cout << "Path is a directory" << std::endl;
    
    // Try to serve index file
    std::string index_path = path;
    if (index_path[index_path.length() - 1] != '/') {
        index_path += "/";
    }
    index_path += config.index;
    
    if (fileExists(index_path)) {
        std::cout << "Serving index file: " << index_path << std::endl;
        return serveFile(index_path, location);
    }
    
    // If autoindex is enabled, show directory listing
    if (location && location->autoindex) {
        std::cout << "Generating directory listing (autoindex on)" << std::endl;
        
        Response res;
        res.setStatus(200, "OK");
        res.setHeader("Content-Type", "text/html");
        
        // Generate directory listing HTML
        std::ostringstream html;
        html << "<!DOCTYPE html>\n";
        html << "<html>\n<head>\n";
        html << "<title>Index of " << path << "</title>\n";
        html << "<style>\n";
        html << "body { font-family: Arial, sans-serif; margin: 40px; }\n";
        html << "h1 { color: #333; }\n";
        html << "ul { list-style: none; padding: 0; }\n";
        html << "li { padding: 5px; }\n";
        html << "a { text-decoration: none; color: #0066cc; }\n";
        html << "a:hover { text-decoration: underline; }\n";
        html << "</style>\n";
        html << "</head>\n<body>\n";
        html << "<h1>Index of " << path << "</h1>\n";
        html << "<ul>\n";
        
        // Read directory contents
        DIR* dir = opendir(path.c_str());
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                std::string name = entry->d_name;
                
                // Skip . but show ..
                if (name == ".") continue;
                
                html << "<li><a href=\"" << name;
                if (entry->d_type == DT_DIR) {
                    html << "/";
                }
                html << "\">" << name;
                if (entry->d_type == DT_DIR) {
                    html << "/";
                }
                html << "</a></li>\n";
            }
            closedir(dir);
        }
        
        html << "</ul>\n</body>\n</html>";
        
        res.setBody(html.str());
        return res;
    }
    
    // No index file and autoindex disabled = 403 Forbidden
    std::cout << "No index file and autoindex off = 403 Forbidden" << std::endl;
    
    Response res;
    res.setStatus(403, "Forbidden");
    res.setHeader("Content-Type", "text/html");
    res.setBody(
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head><title>403 Forbidden</title></head>\n"
        "<body>\n"
        "<h1>403 Forbidden</h1>\n"
        "<p>Directory listing is disabled.</p>\n"
        "</body>\n"
        "</html>"
    );
    
    return res;
}

Response Server::serveErrorPage(int code, const std::string& message) {
    Response res;
    res.setStatus(code, message);
    res.setHeader("Content-Type", "text/html");
    
    // Check if custom error page exists
    std::map<int, std::string>::const_iterator it = config.error_pages.find(code);
    if (it != config.error_pages.end()) {
        std::string error_page_path = config.root + it->second;
        if (fileExists(error_page_path)) {
            std::string content = readFile(error_page_path);
            res.setBody(content);
            return res;
        }
    }
    
    // Default error page
    std::ostringstream html;
    html << "<!DOCTYPE html>\n";
    html << "<html>\n<head>\n";
    html << "<title>" << code << " " << message << "</title>\n";
    html << "</head>\n<body>\n";
    html << "<h1>" << code << " " << message << "</h1>\n";
    html << "</body>\n</html>";
    
    res.setBody(html.str());
    return res;
}

Response Server::serve404() {
    return serveErrorPage(404, "Not Found");
}

Response Server::serve403() {
    return serveErrorPage(403, "Forbidden");
}

Response Server::serve405() {
    return serveErrorPage(405, "Method Not Allowed");
}

Response Server::serve500() {
    return serveErrorPage(500, "Internal Server Error");
}

Response Server::serve200(const std::string& message) {
    Response res;
    res.setStatus(200, "OK");
    res.setHeader("Content-Type", "application/json");
    
    std::ostringstream json;
    json << "{\"status\":\"success\",\"message\":\"" << message << "\"}";
    res.setBody(json.str());
    
    return res;
}

Response Server::serve204() {
    Response res;
    res.setStatus(204, "No Content");
    // 204 responses must not have a body
    return res;
}

std::string Server::readFile(const std::string& path) {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return buffer.str();
}

bool Server::fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

bool Server::isDirectory(const std::string& path) {
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0) {
        return false;
    }
    return S_ISDIR(buffer.st_mode);
}

bool Server::writeFile(const std::string& path, const std::string& content) {
    std::ofstream file(path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    file.write(content.c_str(), content.length());
    file.close();
    return file.good();
}

Response Server::serve413() {
    return serveErrorPage(413, "Payload Too Large");
}

Response Server::serve201(const std::string& message) {
    Response res;
    res.setStatus(201, "Created");
    res.setHeader("Content-Type", "application/json");
    
    std::ostringstream json;
    json << "{\"status\":\"success\",\"message\":\"" << message << "\"}";
    res.setBody(json.str());
    
    return res;
}

std::string Server::getUploadPath(const LocationConfig* location) const {
    if (location && !location->upload_store.empty()) {
        return location->upload_store;
    }
    return config.root + "/uploads";
}

std::string Server::extractFilename(const std::string& content_disposition) const {
    // Look for filename="something"
    size_t pos = content_disposition.find("filename=\"");
    if (pos == std::string::npos) {
        pos = content_disposition.find("filename=");
        if (pos == std::string::npos) {
            return "";
        }
        pos += 9;
        size_t end = content_disposition.find_first_of(";\r\n ", pos);
        if (end == std::string::npos) {
            return content_disposition.substr(pos);
        }
        return content_disposition.substr(pos, end - pos);
    }
    pos += 10;
    size_t end = content_disposition.find('"', pos);
    if (end == std::string::npos) {
        return "";
    }
    return content_disposition.substr(pos, end - pos);
}

std::string Server::generateFilename() const {
    std::ostringstream oss;
    oss << "upload_" << std::time(NULL) << "_" << (std::rand() % 10000);
    return oss.str();
}

Response Server::handleCGI(const Request& req, const LocationConfig* location) {
    std::cout << "=== CGI Request ===" << std::endl;
    
    // Get the script path
    std::string script_path = CGI::getScriptPath(req.getPath(), config.root, location->cgi_extension);
    std::cout << "Script path: " << script_path << std::endl;
    
    // Check if script exists
    struct stat st;
    if (stat(script_path.c_str(), &st) != 0) {
        std::cout << "CGI script not found: " << script_path << std::endl;
        return serve404();
    }
    
    // Check if script is executable (for direct execution without interpreter)
    if (location->cgi_path.empty() && !(st.st_mode & S_IXUSR)) {
        std::cout << "CGI script not executable: " << script_path << std::endl;
        return serve403();
    }
    
    // Create CGI handler
    CGI cgi;
    
    // Setup CGI from request
    cgi.setupFromRequest(req, script_path, location->cgi_path, config.root, 
                         config.port, config.server_name);
    
    // Set timeout (30 seconds default)
    cgi.setTimeout(30);
    
    // Execute CGI script
    CGIStatus status = cgi.execute();
    
    if (status != CGI_SUCCESS) {
        std::cout << "CGI execution failed with status: " << status << std::endl;
    }
    
    // Build and return response
    return cgi.buildResponse();
}

Response Server::handlePost(const Request& req, const LocationConfig* location) {
    std::cout << "Handling POST request" << std::endl;
    std::cout << "Content-Type: " << req.getHeader("Content-Type") << std::endl;
    std::cout << "Content-Length: " << req.getContentLength() << std::endl;
    std::cout << "Body size: " << req.getBody().length() << std::endl;
    
    // Check if this is a multipart upload
    if (req.isMultipart()) {
        return handleMultipartUpload(req, location);
    }
    
    // Handle raw POST data (application/x-www-form-urlencoded or raw file)
    return handleRawUpload(req, location);
}

Response Server::handleMultipartUpload(const Request& req, const LocationConfig* location) {
    std::cout << "Processing multipart/form-data upload" << std::endl;
    
    // Parse multipart data
    Request& mutable_req = const_cast<Request&>(req);
    if (!mutable_req.parseMultipart()) {
        std::cout << "Failed to parse multipart data" << std::endl;
        
        // Provide more detailed error information
        std::string boundary = req.getBoundary();
        std::ostringstream error_msg;
        error_msg << "{\"status\":\"error\",\"message\":\"Failed to parse multipart data\"";
        if (boundary.empty()) {
            error_msg << ",\"detail\":\"No boundary found in Content-Type header\"";
        } else {
            error_msg << ",\"detail\":\"Boundary parsing failed. Check data format.\"";
            error_msg << ",\"boundary\":\"" << boundary << "\"";
        }
        error_msg << ",\"body_size\":" << req.getBody().length() << "}";
        
        Response res;
        res.setStatus(400, "Bad Request");
        res.setHeader("Content-Type", "application/json");
        res.setBody(error_msg.str());
        return res;
    }
    
    const std::vector<MultipartPart>& parts = req.getParts();
    std::string upload_dir = getUploadPath(location);
    
    // Log parsing summary
    std::cout << "Parsed " << parts.size() << " parts, " << req.getFileCount() 
              << " files, total upload size: " << req.getTotalUploadSize() << " bytes" << std::endl;
    
    // Create upload directory if it doesn't exist
    mkdir(upload_dir.c_str(), 0755);
    
    int files_saved = 0;
    std::vector<std::string> saved_files;
    std::vector<size_t> file_sizes;
    std::vector<std::string> file_types;
    
    for (size_t i = 0; i < parts.size(); i++) {
        const MultipartPart& part = parts[i];
        
        // Only process file uploads
        if (!part.is_file) {
            std::cout << "Skipping non-file field: name=\"" << part.name 
                      << "\" value=\"" << (part.data.length() < 100 ? part.data : part.data.substr(0, 100) + "...") 
                      << "\"" << std::endl;
            continue;
        }
        
        if (part.filename.empty()) {
            std::cout << "Skipping part with empty filename" << std::endl;
            continue;
        }
        
        if (part.data.empty()) {
            std::cout << "Skipping empty file: " << part.filename << std::endl;
            continue;
        }
        
        // Generate unique filename if file already exists
        std::string base_path = upload_dir + "/" + part.filename;
        std::string file_path = base_path;
        int suffix = 1;
        
        struct stat st;
        while (stat(file_path.c_str(), &st) == 0) {
            // File exists, add suffix
            size_t dot_pos = part.filename.find_last_of('.');
            std::ostringstream new_name;
            if (dot_pos != std::string::npos) {
                new_name << upload_dir << "/" << part.filename.substr(0, dot_pos) 
                         << "_" << suffix << part.filename.substr(dot_pos);
            } else {
                new_name << upload_dir << "/" << part.filename << "_" << suffix;
            }
            file_path = new_name.str();
            suffix++;
        }
        
        std::cout << "Saving file: " << part.filename << " (" << part.data.length() << " bytes)"
                  << " type=\"" << part.content_type << "\"" 
                  << " to " << file_path << std::endl;
        
        if (writeFile(file_path, part.data)) {
            files_saved++;
            // Extract just the filename from the path
            size_t last_slash = file_path.find_last_of('/');
            std::string saved_name = (last_slash != std::string::npos) 
                ? file_path.substr(last_slash + 1) : file_path;
            saved_files.push_back(saved_name);
            file_sizes.push_back(part.data.length());
            file_types.push_back(part.content_type);
            std::cout << "File saved successfully as: " << saved_name << std::endl;
        } else {
            std::cout << "Failed to save file: " << part.filename << std::endl;
            return serve500();
        }
    }
    
    if (files_saved == 0) {
        std::cout << "No files were uploaded" << std::endl;
        Response res;
        res.setStatus(400, "Bad Request");
        res.setHeader("Content-Type", "application/json");
        res.setBody("{\"status\":\"error\",\"message\":\"No files found in upload. Make sure the form field is a file input.\"}");
        return res;
    }
    
    // Build success response with detailed file information
    std::ostringstream json;
    json << "{\"status\":\"success\",\"message\":\"" << files_saved << " file(s) uploaded\",\"files\":[";
    for (size_t i = 0; i < saved_files.size(); i++) {
        if (i > 0) json << ",";
        json << "{\"name\":\"" << saved_files[i] << "\""
             << ",\"size\":" << file_sizes[i]
             << ",\"type\":\"" << file_types[i] << "\"}";
    }
    json << "],\"total_size\":" << req.getTotalUploadSize() << "}";
    
    Response res;
    res.setStatus(201, "Created");
    res.setHeader("Content-Type", "application/json");
    res.setBody(json.str());
    return res;
}

Response Server::handleRawUpload(const Request& req, const LocationConfig* location) {
    std::cout << "Processing raw upload" << std::endl;
    
    std::string body = req.getBody();
    if (body.empty()) {
        Response res;
        res.setStatus(400, "Bad Request");
        res.setHeader("Content-Type", "application/json");
        res.setBody("{\"status\":\"error\",\"message\":\"Empty request body\"}");
        return res;
    }
    
    std::string upload_dir = getUploadPath(location);
    mkdir(upload_dir.c_str(), 0755);
    
    // Generate filename based on Content-Type or use generic
    std::string filename = generateFilename();
    std::string ct = req.getHeader("Content-Type");
    
    if (ct.find("text/") != std::string::npos) {
        filename += ".txt";
    } else if (ct.find("application/json") != std::string::npos) {
        filename += ".json";
    } else if (ct.find("image/") != std::string::npos) {
        if (ct.find("jpeg") != std::string::npos || ct.find("jpg") != std::string::npos) {
            filename += ".jpg";
        } else if (ct.find("png") != std::string::npos) {
            filename += ".png";
        } else if (ct.find("gif") != std::string::npos) {
            filename += ".gif";
        }
    } else {
        filename += ".bin";
    }
    
    std::string file_path = upload_dir + "/" + filename;
    std::cout << "Saving raw upload to: " << file_path << std::endl;
    
    if (!writeFile(file_path, body)) {
        std::cout << "Failed to save file!" << std::endl;
        return serve500();
    }
    
    std::cout << "File saved: " << body.length() << " bytes" << std::endl;
    return serve201("File uploaded as " + filename);
}

bool Server::deleteFile(const std::string& path) {
    if (remove(path.c_str()) == 0) {
        return true;
    }
    return false;
}

Response Server::handleDelete(const Request& req, const LocationConfig* location) {
    std::cout << "Handling DELETE request" << std::endl;
    std::cout << "Request path: " << req.getPath() << std::endl;
    
    std::string file_path;
    
    // If location has upload_store, use it as base for DELETE
    if (location && !location->upload_store.empty()) {
        // Extract filename from URI (remove location prefix)
        std::string uri = req.getPath();
        std::string filename;
        
        if (uri.find(location->path) == 0) {
            filename = uri.substr(location->path.length());
            // Remove leading slash if present
            if (!filename.empty() && filename[0] == '/') {
                filename = filename.substr(1);
            }
        } else {
            filename = uri;
        }
        
        file_path = location->upload_store + "/" + filename;
    } else {
        // Fall back to regular file path building
        file_path = buildFilePath(req.getPath(), location);
    }
    
    std::cout << "Delete target: " << file_path << std::endl;
    
    // Check if file exists
    if (!fileExists(file_path)) {
        std::cout << "File not found for deletion: " << file_path << std::endl;
        return serve404();
    }
    
    // Don't allow deleting directories (for safety)
    if (isDirectory(file_path)) {
        std::cout << "Cannot delete directory: " << file_path << std::endl;
        return serve403();
    }
    
    // Security check: ensure we're only deleting within allowed paths
    std::string upload_dir = getUploadPath(location);
    std::string root = config.root;
    
    // Ensure the file is within the server root or upload directory
    bool in_root = (file_path.find(root) == 0);
    bool in_upload = (!upload_dir.empty() && file_path.find(upload_dir) == 0);
    
    if (!in_root && !in_upload) {
        std::cout << "Delete forbidden - file outside allowed paths" << std::endl;
        return serve403();
    }
    
    // Attempt to delete the file
    if (!deleteFile(file_path)) {
        std::cout << "Failed to delete file: " << file_path << std::endl;
        return serve500();
    }
    
    std::cout << "File deleted successfully: " << file_path << std::endl;
    
    // Return 200 with message
    return serve200("File deleted successfully");
}

void Server::stop() {
    if (server_fd >= 0) {
        close(server_fd);
        server_fd = -1;
        std::cout << "Server stopped" << std::endl;
    }
}