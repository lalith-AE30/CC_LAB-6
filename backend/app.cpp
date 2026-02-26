#include <iostream>
#include <cstring>
#include <string>
#include <csignal>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main() {
    std::signal(SIGPIPE, SIG_IGN);

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        std::cerr << "ERROR: Failed to get hostname" << std::endl;
        return 1;
    }
    hostname[255] = '\0';
    
    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "ERROR: Failed to create socket" << std::endl;
        return 1;
    }
    
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "ERROR: Failed to set socket options" << std::endl;
        close(server_fd);
        return 1;
    }
    
    // Bind to port 8080
    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        std::cerr << "ERROR: Failed to bind to port 8080" << std::endl;
        close(server_fd);
        return 1;
    }
    
    if (listen(server_fd, 10) < 0) {
        std::cerr << "ERROR: Failed to listen on port 8080" << std::endl;
        close(server_fd);
        return 1;
    }
    
    std::cout << "Server listening on port 8080 (hostname: " << hostname << ")" << std::endl;
    
    // Accept connections in loop
    while(true) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;

        std::string request;
        request.reserve(1024);
        char buffer[1024];
        while (request.find("\r\n\r\n") == std::string::npos && request.size() < 8192) {
            ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
            if (bytes_read > 0) {
                request.append(buffer, static_cast<size_t>(bytes_read));
                continue;
            }
            if (bytes_read == 0) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        
        // Simple HTTP response
        std::string body = "Served by backend: " + std::string(hostname) + "\n";
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/plain\r\n";
        response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        response += "Connection: close\r\n\r\n";
        response += body;
        
        size_t sent = 0;
        while (sent < response.size()) {
            ssize_t n = send(client_fd, response.c_str() + sent, response.size() - sent, MSG_NOSIGNAL);
            if (n > 0) {
                sent += static_cast<size_t>(n);
                continue;
            }
            if (n < 0 && errno == EINTR) {
                continue;
            }
            break;
        }

        shutdown(client_fd, SHUT_WR);
        close(client_fd);
    }
    
    return 0;
}
