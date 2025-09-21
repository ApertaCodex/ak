#pragma once

#include "core/config.hpp"
#include "httplib.h"
#include <string>
#include <cstdint>

namespace ak {
namespace http {

/**
 * HTTP Server for AK REST API
 * 
 * Provides a REST API interface to AK functionality, allowing the web interface
 * to communicate directly with the C++ backend without duplicating key management logic.
 */
class Server {
public:
    /**
     * Constructor
     * @param cfg AK configuration object
     */
    explicit Server(const core::Config& cfg);
    
    /**
     * Destructor
     */
    ~Server();
    
    /**
     * Start the HTTP server
     * @param host Host to bind to (default: "127.0.0.1")
     * @param port Port to listen on (default: 8765)
     * @return true if server started successfully, false otherwise
     */
    bool start(const std::string& host = "127.0.0.1", uint16_t port = 8765);
    
    /**
     * Stop the HTTP server
     */
    void stop();
    
    /**
     * Check if server is running
     * @return true if running, false otherwise
     */
    bool isRunning() const;

private:
    const core::Config& config_;
    void* server_impl_; // Forward declaration for httplib::Server
    bool running_;
    
    // Setup HTTP routes
    void setupRoutes();
    
    // Health check endpoint
    void handleHealth(const void* req, void* res);
    
    // Service endpoints
    void handleGetServices(const void* req, void* res);
    void handlePostService(const void* req, void* res);
    void handlePutService(const void* req, void* res);
    void handleDeleteService(const void* req, void* res);
    
    // Key testing endpoints
    void handleTestService(const void* req, void* res);
    void handleTestServices(const void* req, void* res);
    
    // Unmasked key endpoints
    void handleGetKeysUnmasked(const void* req, void* res);
    void handleGetProfileKeysUnmasked(const void* req, void* res);
    
    // Profile endpoints
    void handleGetProfiles(const void* req, void* res);
    void handlePostProfile(const void* req, void* res);
    void handleDeleteProfile(const void* req, void* res);
    
    // Key endpoints for profiles
    void handleGetProfileKeys(const void* req, void* res);
    void handlePostProfileKey(const void* req, void* res);
    void handlePutProfileKey(const void* req, void* res);
    void handleDeleteProfileKey(const void* req, void* res);
    
    // Global key endpoints
    void handleGetKeys(const void* req, void* res);
    void handlePostKey(const void* req, void* res);
    void handlePutKey(const void* req, void* res);
    void handleDeleteKey(const void* req, void* res);
    
    // Utility functions
    std::string jsonError(const std::string& message, int code = 400);
    std::string jsonSuccess(const std::string& message);
    void setCorsHeaders(void* res);
    
    // Request/Response type conversions
    const httplib::Request& getRequest(const void* req) const;
    httplib::Response& getResponse(void* res) const;
};

} // namespace http
} // namespace ak