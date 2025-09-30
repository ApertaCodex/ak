#include "http/server.hpp"
#include "storage/vault.hpp"
#include "services/services.hpp"
#include "core/config.hpp"
#include "ui/ui.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <exception>
#include <fstream>
#include <iterator>
#include <iomanip>

// Include cpp-httplib header
#include "httplib.h"

namespace {

std::string escapeJson(const std::string& value) {
    std::ostringstream oss;
    for (char c : value) {
        switch (c) {
        case '\\':
            oss << "\\\\";
            break;
        case '\"':
            oss << "\\\"";
            break;
        case '\n':
            oss << "\\n";
            break;
        case '\r':
            oss << "\\r";
            break;
        case '\t':
            oss << "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                oss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<int>(static_cast<unsigned char>(c)) << std::dec;
            } else {
                oss << c;
            }
            break;
        }
    }
    return oss.str();
}

}

namespace ak {
namespace http {

Server::Server(const core::Config& cfg) 
    : config_(cfg), server_impl_(nullptr), running_(false) {
    // Initialize httplib server
    server_impl_ = new httplib::Server();
    setupRoutes();
}

Server::~Server() {
    stop();
    if (server_impl_) {
        delete static_cast<httplib::Server*>(server_impl_);
    }
}

bool Server::start(const std::string& host, uint16_t port) {
    if (running_) {
        return false;
    }
    
    auto* server = static_cast<httplib::Server*>(server_impl_);
    
    std::cout << ui::colorize("🌐 Starting AK HTTP server...", ui::Colors::BRIGHT_CYAN) << "\n";
    std::cout << ui::colorize("   Host: ", ui::Colors::WHITE) << ui::colorize(host, ui::Colors::BRIGHT_WHITE) << "\n";
    std::cout << ui::colorize("   Port: ", ui::Colors::WHITE) << ui::colorize(std::to_string(port), ui::Colors::BRIGHT_WHITE) << "\n";
    std::cout << ui::colorize("   Web Interface: ", ui::Colors::WHITE) << ui::colorize("http://" + host + ":" + std::to_string(port), ui::Colors::BRIGHT_BLUE) << "\n\n";
    
    running_ = true;
    
    // Start server (this will block)
    bool success = server->listen(host.c_str(), port);
    running_ = false;
    
    return success;
}

void Server::stop() {
    if (running_ && server_impl_) {
        static_cast<httplib::Server*>(server_impl_)->stop();
        running_ = false;
        std::cout << ui::colorize("🛑 AK HTTP server stopped", ui::Colors::BRIGHT_RED) << "\n";
    }
}

bool Server::isRunning() const {
    return running_;
}

void Server::setupRoutes() {
    auto* server = static_cast<httplib::Server*>(server_impl_);
    
    // Enable CORS for all routes
    server->set_pre_routing_handler([this](const httplib::Request&, httplib::Response& res) {
        setCorsHeaders(&res);
        return httplib::Server::HandlerResponse::Unhandled;
    });
    
    // Handle OPTIONS requests for CORS preflight
    server->Options(".*", [this](const httplib::Request&, httplib::Response& res) {
        setCorsHeaders(&res);
        return;
    });
    
    // Health check
    server->Get("/api/health", [this](const httplib::Request& req, httplib::Response& res) {
        handleHealth(&req, &res);
    });
    
    // Services endpoints
    server->Get("/api/services", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetServices(&req, &res);
    });
    
    server->Post("/api/services", [this](const httplib::Request& req, httplib::Response& res) {
        handlePostService(&req, &res);
    });
    
    server->Put(R"(/api/services/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        handlePutService(&req, &res);
    });
    
    server->Delete(R"(/api/services/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleDeleteService(&req, &res);
    });
    
    // Key testing endpoints
    server->Post(R"(/api/test/service/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleTestService(&req, &res);
    });
    
    server->Post("/api/test/services", [this](const httplib::Request& req, httplib::Response& res) {
        handleTestServices(&req, &res);
    });
    
    // Unmasked key endpoints
    server->Get("/api/keys/unmasked", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetKeysUnmasked(&req, &res);
    });
    
    server->Get(R"(/api/profiles/([^/]+)/keys/unmasked)", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetProfileKeysUnmasked(&req, &res);
    });
    
    // Profile endpoints
    server->Get("/api/profiles", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetProfiles(&req, &res);
    });
    
    server->Post("/api/profiles", [this](const httplib::Request& req, httplib::Response& res) {
        handlePostProfile(&req, &res);
    });
    
    server->Delete(R"(/api/profiles/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleDeleteProfile(&req, &res);
    });
    
    // Profile-specific key endpoints
    server->Get(R"(/api/profiles/([^/]+)/keys)", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetProfileKeys(&req, &res);
    });
    
    server->Post(R"(/api/profiles/([^/]+)/keys)", [this](const httplib::Request& req, httplib::Response& res) {
        handlePostProfileKey(&req, &res);
    });
    
    server->Put(R"(/api/profiles/([^/]+)/keys/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        handlePutProfileKey(&req, &res);
    });
    
    server->Delete(R"(/api/profiles/([^/]+)/keys/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleDeleteProfileKey(&req, &res);
    });
    
    // Global key endpoints
    server->Get("/api/keys", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetKeys(&req, &res);
    });
    
    server->Post("/api/keys", [this](const httplib::Request& req, httplib::Response& res) {
        handlePostKey(&req, &res);
    });
    
    server->Put(R"(/api/keys/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        handlePutKey(&req, &res);
    });
    
    server->Delete(R"(/api/keys/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleDeleteKey(&req, &res);
    });
    
    // Serve web interface files
    server->Get("/", [](const httplib::Request&, httplib::Response& res) {
        std::ifstream file("web-app.html");
        if (file.good()) {
            std::string content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
            res.set_content(content, "text/html");
        } else {
            res.status = 404;
            res.set_content("Web interface not found. Please ensure web-app.html is in the current directory.", "text/plain");
        }
    });
    
    server->Get("/web-app.html", [](const httplib::Request&, httplib::Response& res) {
        std::ifstream file("web-app.html");
        if (file.good()) {
            std::string content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
            res.set_content(content, "text/html");
        } else {
            res.status = 404;
            res.set_content("Not found", "text/plain");
        }
    });
    
    server->Get("/web-app.js", [](const httplib::Request&, httplib::Response& res) {
        std::ifstream file("web-app.js");
        if (file.good()) {
            std::string content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
            res.set_content(content, "application/javascript");
        } else {
            res.status = 404;
            res.set_content("Not found", "text/plain");
        }
    });
    
    server->Get("/styles.css", [](const httplib::Request&, httplib::Response& res) {
        std::ifstream file("styles.css");
        if (file.good()) {
            std::string content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
            res.set_content(content, "text/css");
        } else {
            res.status = 404;
            res.set_content("Not found", "text/plain");
        }
    });
    
    server->Get("/logo.svg", [](const httplib::Request&, httplib::Response& res) {
        std::ifstream file("logo.svg");
        if (file.good()) {
            std::string content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
            res.set_content(content, "image/svg+xml");
        } else {
            res.status = 404;
            res.set_content("Not found", "text/plain");
        }
    });
    
    server->Get("/logo.png", [](const httplib::Request&, httplib::Response& res) {
        std::ifstream file("logo.png", std::ios::binary);
        if (file.good()) {
            std::string content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
            res.set_content(content, "image/png");
        } else {
            res.status = 404;
            res.set_content("Not found", "text/plain");
        }
    });
    
    // Error handling
    server->set_error_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"error\":\"Not Found\",\"code\":404}", "application/json");
    });
}

void Server::handleHealth(const void* /*req*/, void* res) {
    auto& response = getResponse(res);
    
    std::ostringstream json;
    json << "{"
         << "\"status\":\"ok\","
         << "\"version\":\"" << core::AK_VERSION << "\","
         << "\"backend\":\"" << (config_.gpgAvailable && !config_.forcePlain ? "gpg" : "plain") << "\""
         << "}";
    
    response.set_content(json.str(), "application/json");
}

void Server::handleGetServices(const void* /*req*/, void* res) {
    auto& response = getResponse(res);
    
    try {
        auto allServices = services::loadAllServices(config_);
        
        std::ostringstream json;
        json << "[";
        
        bool first = true;
        for (const auto& [name, service] : allServices) {
            if (!first) json << ",";
            first = false;
            
            json << "{"
                 << "\"name\":\"" << escapeJson(service.name) << "\","
                 << "\"keyName\":\"" << escapeJson(service.keyName) << "\","
                 << "\"description\":\"" << escapeJson(service.description) << "\","
                 << "\"testEndpoint\":\"" << escapeJson(service.testEndpoint) << "\","
                 << "\"testMethod\":\"" << escapeJson(service.testMethod) << "\","
                 << "\"testHeaders\":\"" << escapeJson(service.testHeaders) << "\","
                 << "\"authMethod\":\"" << escapeJson(service.authMethod) << "\","
                 << "\"authLocation\":\"" << escapeJson(service.authLocation) << "\","
                 << "\"authParameter\":\"" << escapeJson(service.authParameter) << "\","
                 << "\"authPrefix\":\"" << escapeJson(service.authPrefix) << "\","
                 << "\"testBody\":\"" << escapeJson(service.testBody) << "\","
                 << "\"testable\":" << (service.testable ? "true" : "false") << ","
                 << "\"isBuiltIn\":" << (service.isBuiltIn ? "true" : "false")
                 << "}";
        }
        
        json << "]";
        response.set_content(json.str(), "application/json");
        
    } catch (const std::exception& e) {
        response.status = 500;
        response.set_content(jsonError("Failed to load services: " + std::string(e.what()), 500), "application/json");
    }
}

void Server::handleGetProfiles(const void* /*req*/, void* res) {
    auto& response = getResponse(res);
    
    try {
        auto profiles = storage::listProfiles(config_);
        
        std::ostringstream json;
        json << "[";
        
        bool first = true;
        for (const auto& profileName : profiles) {
            if (!first) json << ",";
            first = false;
            
            auto keys = storage::readProfile(config_, profileName);
            
            json << "{"
                 << "\"name\":\"" << profileName << "\","
                 << "\"keyCount\":" << keys.size() << ","
                 << "\"keys\":[";
            
            bool firstKey = true;
            for (const auto& key : keys) {
                if (!firstKey) json << ",";
                firstKey = false;
                json << "\"" << key << "\"";
            }
            
            json << "]}";
        }
        
        json << "]";
        response.set_content(json.str(), "application/json");
        
    } catch (const std::exception& e) {
        response.status = 500;
        response.set_content(jsonError("Failed to load profiles: " + std::string(e.what()), 500), "application/json");
    }
}

void Server::handlePostProfile(const void* req, void* res) {
    auto& request = getRequest(req);
    auto& response = getResponse(res);
    
    try {
        // Parse JSON body to get profile name and keys
        std::string body = request.body;
        
        // Simple JSON parsing for profile creation
        // Expected format: {"name": "profile_name", "keys": ["KEY1", "KEY2"]}
        size_t namePos = body.find("\"name\":");
        if (namePos == std::string::npos) {
            response.status = 400;
            response.set_content(jsonError("Missing 'name' field in request"), "application/json");
            return;
        }
        
        size_t nameStart = body.find("\"", namePos + 7) + 1;
        size_t nameEnd = body.find("\"", nameStart);
        std::string profileName = body.substr(nameStart, nameEnd - nameStart);
        
        if (profileName.empty()) {
            response.status = 400;
            response.set_content(jsonError("Profile name cannot be empty"), "application/json");
            return;
        }
        
        // Extract keys if provided, otherwise save all keys
        std::vector<std::string> keys;
        size_t keysPos = body.find("\"keys\":");
        if (keysPos != std::string::npos) {
            // Parse keys array (simplified)
            size_t arrayStart = body.find("[", keysPos);
            size_t arrayEnd = body.find("]", arrayStart);
            std::string keysStr = body.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
            
            // Split by comma and clean up
            std::stringstream ss(keysStr);
            std::string key;
            while (std::getline(ss, key, ',')) {
                // Remove quotes and whitespace
                key.erase(std::remove_if(key.begin(), key.end(), [](char c) {
                    return c == '"' || c == ' ' || c == '\t' || c == '\n';
                }), key.end());
                if (!key.empty()) {
                    keys.push_back(key);
                }
            }
        } else {
            // Save all keys from global vault
            core::KeyStore ks = storage::loadVault(config_);
            for (const auto& pair : ks.kv) {
                keys.push_back(pair.first);
            }
        }
        
        storage::writeProfile(config_, profileName, keys);
        
        response.status = 201;
        response.set_content(jsonSuccess("Profile '" + profileName + "' created with " + std::to_string(keys.size()) + " keys"), "application/json");
        
    } catch (const std::exception& e) {
        response.status = 500;
        response.set_content(jsonError("Failed to create profile: " + std::string(e.what()), 500), "application/json");
    }
}

void Server::handleDeleteProfile(const void* req, void* res) {
    auto& request = getRequest(req);
    auto& response = getResponse(res);
    
    try {
        std::string profileName = request.matches[1];
        
        auto profiles = storage::listProfiles(config_);
        if (std::find(profiles.begin(), profiles.end(), profileName) == profiles.end()) {
            response.status = 404;
            response.set_content(jsonError("Profile '" + profileName + "' not found"), "application/json");
            return;
        }
        
        // Remove profile file
        std::string profilePath = config_.profilesDir + "/" + profileName + ".profile";
        if (std::filesystem::exists(profilePath)) {
            std::filesystem::remove(profilePath);
        }
        
        // Remove profile keys file
        auto keysPath = storage::profileKeysPath(config_, profileName);
        if (std::filesystem::exists(keysPath)) {
            std::filesystem::remove(keysPath);
        }
        
        response.set_content(jsonSuccess("Profile '" + profileName + "' deleted"), "application/json");
        
    } catch (const std::exception& e) {
        response.status = 500;
        response.set_content(jsonError("Failed to delete profile: " + std::string(e.what()), 500), "application/json");
    }
}

void Server::handleGetProfileKeys(const void* req, void* res) {
    auto& request = getRequest(req);
    auto& response = getResponse(res);
    
    try {
        std::string profileName = request.matches[1];
        
        auto profileKeys = storage::loadProfileKeys(config_, profileName);
        auto keyList = storage::readProfile(config_, profileName);
        
        // Fallback to global vault for keys not in profile-specific storage
        core::KeyStore ks = storage::loadVault(config_);
        
        std::ostringstream json;
        json << "{\"profile\":\"" << profileName << "\",\"keys\":[";
        
        bool first = true;
        for (const auto& keyName : keyList) {
            if (!first) json << ",";
            first = false;
            
            std::string value;
            auto pit = profileKeys.find(keyName);
            if (pit != profileKeys.end()) {
                value = pit->second;
            } else {
                auto vit = ks.kv.find(keyName);
                if (vit != ks.kv.end()) {
                    value = vit->second;
                }
            }
            
            json << "{"
                 << "\"name\":\"" << keyName << "\","
                 << "\"value\":\"" << core::maskValue(value) << "\","
                 << "\"masked\":true"
                 << "}";
        }
        
        json << "]}";
        response.set_content(json.str(), "application/json");
        
    } catch (const std::exception& e) {
        response.status = 500;
        response.set_content(jsonError("Failed to load profile keys: " + std::string(e.what()), 500), "application/json");
    }
}

void Server::handlePostProfileKey(const void* req, void* res) {
    auto& request = getRequest(req);
    auto& response = getResponse(res);
    
    try {
        std::string profileName = request.matches[1];
        std::string body = request.body;
        
        // Parse JSON body: {"name": "KEY_NAME", "value": "KEY_VALUE"}
        size_t namePos = body.find("\"name\":");
        size_t valuePos = body.find("\"value\":");
        
        if (namePos == std::string::npos || valuePos == std::string::npos) {
            response.status = 400;
            response.set_content(jsonError("Missing 'name' or 'value' field in request"), "application/json");
            return;
        }
        
        size_t nameStart = body.find("\"", namePos + 7) + 1;
        size_t nameEnd = body.find("\"", nameStart);
        std::string keyName = body.substr(nameStart, nameEnd - nameStart);
        
        size_t valueStart = body.find("\"", valuePos + 8) + 1;
        size_t valueEnd = body.find("\"", valueStart);
        std::string keyValue = body.substr(valueStart, valueEnd - valueStart);
        
        if (keyName.empty() || keyValue.empty()) {
            response.status = 400;
            response.set_content(jsonError("Key name and value cannot be empty"), "application/json");
            return;
        }
        
        // Add to global vault
        core::KeyStore ks = storage::loadVault(config_);
        ks.kv[keyName] = keyValue;
        storage::saveVault(config_, ks);
        
        // Add to profile
        auto profileKeys = storage::readProfile(config_, profileName);
        if (std::find(profileKeys.begin(), profileKeys.end(), keyName) == profileKeys.end()) {
            profileKeys.push_back(keyName);
            storage::writeProfile(config_, profileName, profileKeys);
        }
        
        // Save in profile-specific storage
        auto profileKeyMap = storage::loadProfileKeys(config_, profileName);
        profileKeyMap[keyName] = keyValue;
        storage::saveProfileKeys(config_, profileName, profileKeyMap);
        
        response.status = 201;
        response.set_content(jsonSuccess("Key '" + keyName + "' added to profile '" + profileName + "'"), "application/json");
        
    } catch (const std::exception& e) {
        response.status = 500;
        response.set_content(jsonError("Failed to add key: " + std::string(e.what()), 500), "application/json");
    }
}

void Server::handlePutProfileKey(const void* req, void* res) {
    auto& request = getRequest(req);
    auto& response = getResponse(res);
    
    try {
        std::string profileName = request.matches[1];
        std::string keyName = request.matches[2];
        std::string body = request.body;
        
        // Parse JSON body: {"value": "NEW_KEY_VALUE"}
        size_t valuePos = body.find("\"value\":");
        if (valuePos == std::string::npos) {
            response.status = 400;
            response.set_content(jsonError("Missing 'value' field in request"), "application/json");
            return;
        }
        
        size_t valueStart = body.find("\"", valuePos + 8) + 1;
        size_t valueEnd = body.find("\"", valueStart);
        std::string keyValue = body.substr(valueStart, valueEnd - valueStart);
        
        if (keyValue.empty()) {
            response.status = 400;
            response.set_content(jsonError("Key value cannot be empty"), "application/json");
            return;
        }
        
        // Update global vault
        core::KeyStore ks = storage::loadVault(config_);
        ks.kv[keyName] = keyValue;
        storage::saveVault(config_, ks);
        
        // Update profile-specific storage
        auto profileKeyMap = storage::loadProfileKeys(config_, profileName);
        profileKeyMap[keyName] = keyValue;
        storage::saveProfileKeys(config_, profileName, profileKeyMap);
        
        response.set_content(jsonSuccess("Key '" + keyName + "' updated in profile '" + profileName + "'"), "application/json");
        
    } catch (const std::exception& e) {
        response.status = 500;
        response.set_content(jsonError("Failed to update key: " + std::string(e.what()), 500), "application/json");
    }
}

void Server::handleDeleteProfileKey(const void* req, void* res) {
    auto& request = getRequest(req);
    auto& response = getResponse(res);
    
    try {
        std::string profileName = request.matches[1];
        std::string keyName = request.matches[2];
        
        // Remove from profile
        auto profileKeys = storage::readProfile(config_, profileName);
        auto it = std::find(profileKeys.begin(), profileKeys.end(), keyName);
        if (it != profileKeys.end()) {
            profileKeys.erase(it);
            storage::writeProfile(config_, profileName, profileKeys);
        }
        
        // Remove from profile-specific storage
        auto profileKeyMap = storage::loadProfileKeys(config_, profileName);
        profileKeyMap.erase(keyName);
        storage::saveProfileKeys(config_, profileName, profileKeyMap);
        
        response.set_content(jsonSuccess("Key '" + keyName + "' removed from profile '" + profileName + "'"), "application/json");
        
    } catch (const std::exception& e) {
        response.status = 500;
        response.set_content(jsonError("Failed to delete key: " + std::string(e.what()), 500), "application/json");
    }
}

void Server::handleGetKeys(const void* /*req*/, void* res) {
    auto& response = getResponse(res);
    
    try {
        core::KeyStore ks = storage::loadVault(config_);
        
        std::ostringstream json;
        json << "[";
        
        bool first = true;
        for (const auto& [keyName, keyValue] : ks.kv) {
            if (!first) json << ",";
            first = false;
            
            json << "{"
                 << "\"name\":\"" << keyName << "\","
                 << "\"value\":\"" << core::maskValue(keyValue) << "\","
                 << "\"masked\":true"
                 << "}";
        }
        
        json << "]";
        response.set_content(json.str(), "application/json");
        
    } catch (const std::exception& e) {
        response.status = 500;
        response.set_content(jsonError("Failed to load keys: " + std::string(e.what()), 500), "application/json");
    }
}

void Server::handlePostKey(const void* req, void* res) {
    auto& request = getRequest(req);
    auto& response = getResponse(res);
    
    try {
        std::string body = request.body;
        
        // Parse JSON body: {"name": "KEY_NAME", "value": "KEY_VALUE"}
        size_t namePos = body.find("\"name\":");
        size_t valuePos = body.find("\"value\":");
        
        if (namePos == std::string::npos || valuePos == std::string::npos) {
            response.status = 400;
            response.set_content(jsonError("Missing 'name' or 'value' field in request"), "application/json");
            return;
        }
        
        size_t nameStart = body.find("\"", namePos + 7) + 1;
        size_t nameEnd = body.find("\"", nameStart);
        std::string keyName = body.substr(nameStart, nameEnd - nameStart);
        
        size_t valueStart = body.find("\"", valuePos + 8) + 1;
        size_t valueEnd = body.find("\"", valueStart);
        std::string keyValue = body.substr(valueStart, valueEnd - valueStart);
        
        if (keyName.empty() || keyValue.empty()) {
            response.status = 400;
            response.set_content(jsonError("Key name and value cannot be empty"), "application/json");
            return;
        }
        
        // Add to global vault
        core::KeyStore ks = storage::loadVault(config_);
        bool exists = ks.kv.find(keyName) != ks.kv.end();
        ks.kv[keyName] = keyValue;
        storage::saveVault(config_, ks);
        
        response.status = exists ? 200 : 201;
        response.set_content(jsonSuccess((exists ? "Updated" : "Added") + std::string(" key '") + keyName + "'"), "application/json");
        
    } catch (const std::exception& e) {
        response.status = 500;
        response.set_content(jsonError("Failed to add key: " + std::string(e.what()), 500), "application/json");
    }
}

void Server::handlePutKey(const void* req, void* res) {
    auto& request = getRequest(req);
    auto& response = getResponse(res);
    
    try {
        std::string keyName = request.matches[1];
        std::string body = request.body;
        
        // Parse JSON body: {"value": "NEW_KEY_VALUE"}
        size_t valuePos = body.find("\"value\":");
        if (valuePos == std::string::npos) {
            response.status = 400;
            response.set_content(jsonError("Missing 'value' field in request"), "application/json");
            return;
        }
        
        size_t valueStart = body.find("\"", valuePos + 8) + 1;
        size_t valueEnd = body.find("\"", valueStart);
        std::string keyValue = body.substr(valueStart, valueEnd - valueStart);
        
        if (keyValue.empty()) {
            response.status = 400;
            response.set_content(jsonError("Key value cannot be empty"), "application/json");
            return;
        }
        
        // Update global vault
        core::KeyStore ks = storage::loadVault(config_);
        ks.kv[keyName] = keyValue;
        storage::saveVault(config_, ks);
        
        response.set_content(jsonSuccess("Key '" + keyName + "' updated"), "application/json");
        
    } catch (const std::exception& e) {
        response.status = 500;
        response.set_content(jsonError("Failed to update key: " + std::string(e.what()), 500), "application/json");
    }
}

void Server::handleDeleteKey(const void* req, void* res) {
    auto& request = getRequest(req);
    auto& response = getResponse(res);
    
    try {
        std::string keyName = request.matches[1];
        
        // Remove from global vault
        core::KeyStore ks = storage::loadVault(config_);
        if (ks.kv.erase(keyName) == 0) {
            response.status = 404;
            response.set_content(jsonError("Key '" + keyName + "' not found"), "application/json");
            return;
        }
        
        storage::saveVault(config_, ks);
        response.set_content(jsonSuccess("Key '" + keyName + "' deleted"), "application/json");
        
    } catch (const std::exception& e) {
        response.status = 500;
        response.set_content(jsonError("Failed to delete key: " + std::string(e.what()), 500), "application/json");
    }
}

void Server::handlePostService(const void* req, void* res) {
    auto& request = getRequest(req);
    auto& response = getResponse(res);
    
    try {
        std::string body = request.body;
        
        // Parse JSON body for service creation
        // Expected format: {
        //   "name": "service_name",
        //   "keyName": "API_KEY",
        //   "description": "desc",
        //   "testEndpoint": "url",
        //   "testMethod": "GET",
        //   "authMethod": "Bearer",
        //   "authLocation": "header",
        //   "authParameter": "Authorization",
        //   "authPrefix": "Bearer "
        // }
        size_t namePos = body.find("\"name\":");
        size_t keyPos = body.find("\"keyName\":");
        
        if (namePos == std::string::npos || keyPos == std::string::npos) {
            response.status = 400;
            response.set_content(jsonError("Missing 'name' or 'keyName' field in request"), "application/json");
            return;
        }
        
        auto extractField = [&body](const std::string& fieldName, bool required = true) -> std::string {
            size_t pos = body.find("\"" + fieldName + "\":");
            if (pos == std::string::npos) {
                return required ? "" : "";
            }
            size_t start = body.find("\"", pos + fieldName.length() + 3) + 1;
            size_t end = body.find("\"", start);
            return body.substr(start, end - start);
        };
        
        std::string name = extractField("name");
        std::string keyName = extractField("keyName");
        std::string description = extractField("description", false);
        std::string testEndpoint = extractField("testEndpoint", false);
        std::string testMethod = extractField("testMethod", false);
        std::string authMethod = extractField("authMethod", false);
        std::string authLocation = extractField("authLocation", false);
        std::string authParameter = extractField("authParameter", false);
        std::string authPrefix = extractField("authPrefix", false);
        std::string testBody = extractField("testBody", false);
        
        if (name.empty() || keyName.empty()) {
            response.status = 400;
            response.set_content(jsonError("Service name and keyName cannot be empty"), "application/json");
            return;
        }
        
        // Set defaults
        if (testMethod.empty()) testMethod = "GET";
        if (authMethod.empty()) authMethod = "Bearer";
        if (authLocation.empty()) authLocation = "header";
        
        // Create service
        services::Service service(name, keyName, description, testEndpoint, testMethod, "", authMethod,
                                  !testEndpoint.empty(), false, authLocation, authParameter, authPrefix, testBody);
        services::addService(config_, service);
        
        response.status = 201;
        response.set_content(jsonSuccess("Service '" + name + "' created"), "application/json");
        
    } catch (const std::exception& e) {
        response.status = 500;
        response.set_content(jsonError("Failed to create service: " + std::string(e.what()), 500), "application/json");
    }
}

void Server::handlePutService(const void* req, void* res) {
    auto& request = getRequest(req);
    auto& response = getResponse(res);
    
    try {
        std::string serviceName = request.matches[1];
        std::string body = request.body;
        
        auto extractField = [&body](const std::string& fieldName) -> std::string {
            size_t pos = body.find("\"" + fieldName + "\":");
            if (pos == std::string::npos) return "";
            size_t start = body.find("\"", pos + fieldName.length() + 3) + 1;
            size_t end = body.find("\"", start);
            return body.substr(start, end - start);
        };
        
        // Load existing service or create new one
        auto allServices = services::loadAllServices(config_);
        services::Service service;
        auto it = allServices.find(serviceName);
        if (it != allServices.end()) {
            service = it->second;
        } else {
            service.name = serviceName;
            service.isBuiltIn = false;
        }
        
        // Update fields if provided
        std::string keyName = extractField("keyName");
        if (!keyName.empty()) service.keyName = keyName;
        
        std::string description = extractField("description");
        if (!description.empty()) service.description = description;
        
        std::string testEndpoint = extractField("testEndpoint");
        if (!testEndpoint.empty()) {
            service.testEndpoint = testEndpoint;
            service.testable = true;
        }
        
        std::string testMethod = extractField("testMethod");
        if (!testMethod.empty()) service.testMethod = testMethod;
        
        std::string authMethod = extractField("authMethod");
        if (!authMethod.empty()) service.authMethod = authMethod;

        std::string authLocation = extractField("authLocation");
        if (!authLocation.empty()) service.authLocation = authLocation;

        std::string authParameter = extractField("authParameter");
        if (!authParameter.empty()) service.authParameter = authParameter;

        std::string authPrefix = extractField("authPrefix");
        if (!authPrefix.empty()) service.authPrefix = authPrefix;

        std::string testBody = extractField("testBody");
        if (!testBody.empty()) service.testBody = testBody;
        
        if (service.keyName.empty()) {
            response.status = 400;
            response.set_content(jsonError("Service keyName cannot be empty"), "application/json");
            return;
        }
        
        services::updateService(config_, service);
        response.set_content(jsonSuccess("Service '" + serviceName + "' updated"), "application/json");
        
    } catch (const std::exception& e) {
        response.status = 500;
        response.set_content(jsonError("Failed to update service: " + std::string(e.what()), 500), "application/json");
    }
}

void Server::handleDeleteService(const void* req, void* res) {
    auto& request = getRequest(req);
    auto& response = getResponse(res);
    
    try {
        std::string serviceName = request.matches[1];
        
        // Check if service exists and is not built-in
        auto allServices = services::loadAllServices(config_);
        auto it = allServices.find(serviceName);
        if (it == allServices.end()) {
            response.status = 404;
            response.set_content(jsonError("Service '" + serviceName + "' not found"), "application/json");
            return;
        }
        
        if (it->second.isBuiltIn) {
            response.status = 403;
            response.set_content(jsonError("Cannot delete built-in service '" + serviceName + "'"), "application/json");
            return;
        }
        
        services::removeService(config_, serviceName);
        response.set_content(jsonSuccess("Service '" + serviceName + "' deleted"), "application/json");
        
    } catch (const std::exception& e) {
        response.status = 500;
        response.set_content(jsonError("Failed to delete service: " + std::string(e.what()), 500), "application/json");
    }
}

void Server::handleTestService(const void* req, void* res) {
    auto& request = getRequest(req);
    auto& response = getResponse(res);
    
    try {
        std::string serviceName = request.matches[1];
        std::string body = request.body;
        
        // Parse optional profile parameter
        std::string profileName = "";
        size_t profilePos = body.find("\"profile\":");
        if (profilePos != std::string::npos) {
            size_t start = body.find("\"", profilePos + 10) + 1;
            size_t end = body.find("\"", start);
            profileName = body.substr(start, end - start);
        }
        
        // Test the service
        auto result = services::test_one(config_, serviceName, profileName);
        
        std::ostringstream json;
        json << "{"
             << "\"service\":\"" << result.service << "\","
             << "\"success\":" << (result.ok ? "true" : "false") << ","
             << "\"duration\":" << result.duration.count() << ","
             << "\"message\":\"" << (result.ok ? "Test passed" : result.error_message) << "\""
             << "}";
        
        response.set_content(json.str(), "application/json");
        
    } catch (const std::exception& e) {
        response.status = 500;
        response.set_content(jsonError("Failed to test service: " + std::string(e.what()), 500), "application/json");
    }
}

void Server::handleTestServices(const void* req, void* res) {
    auto& request = getRequest(req);
    auto& response = getResponse(res);
    
    try {
        std::string body = request.body;
        
        // Parse services array and options
        // Expected format: {"services": ["service1", "service2"], "profile": "optional", "failFast": false}
        std::vector<std::string> services;
        std::string profileName = "";
        bool failFast = false;
        
        // Extract services array
        size_t servicesPos = body.find("\"services\":");
        if (servicesPos != std::string::npos) {
            size_t arrayStart = body.find("[", servicesPos);
            size_t arrayEnd = body.find("]", arrayStart);
            std::string servicesStr = body.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
            
            // Parse service names
            std::stringstream ss(servicesStr);
            std::string service;
            while (std::getline(ss, service, ',')) {
                service.erase(std::remove_if(service.begin(), service.end(), [](char c) {
                    return c == '"' || c == ' ' || c == '\t' || c == '\n';
                }), service.end());
                if (!service.empty()) {
                    services.push_back(service);
                }
            }
        }
        
        // Extract optional profile
        size_t profilePos = body.find("\"profile\":");
        if (profilePos != std::string::npos) {
            size_t start = body.find("\"", profilePos + 10) + 1;
            size_t end = body.find("\"", start);
            profileName = body.substr(start, end - start);
        }
        
        // Extract optional failFast
        size_t failFastPos = body.find("\"failFast\":");
        if (failFastPos != std::string::npos) {
            size_t start = failFastPos + 11;
            std::string failFastStr = body.substr(start, 5); // "true" or "false"
            failFast = (failFastStr.find("true") != std::string::npos);
        }
        
        if (services.empty()) {
            // Test all testable services if none specified
            auto allServices = services::loadAllServices(config_);
            for (const auto& [name, service] : allServices) {
                if (service.testable) {
                    services.push_back(name);
                }
            }
        }
        
        // Run tests
        auto results = services::run_tests_parallel(config_, services, failFast, profileName, false);
        
        std::ostringstream json;
        json << "{\"results\":[";
        
        bool first = true;
        for (const auto& result : results) {
            if (!first) json << ",";
            first = false;
            
            json << "{"
                 << "\"service\":\"" << result.service << "\","
                 << "\"success\":" << (result.ok ? "true" : "false") << ","
                 << "\"duration\":" << result.duration.count() << ","
                 << "\"message\":\"" << (result.ok ? "Test passed" : result.error_message) << "\""
                 << "}";
        }
        
        json << "],\"summary\":{";
        int passed = 0, failed = 0;
        for (const auto& result : results) {
            if (result.ok) passed++; else failed++;
        }
        json << "\"total\":" << results.size() << ","
             << "\"passed\":" << passed << ","
             << "\"failed\":" << failed
             << "}}";
        
        response.set_content(json.str(), "application/json");
        
    } catch (const std::exception& e) {
        response.status = 500;
        response.set_content(jsonError("Failed to test services: " + std::string(e.what()), 500), "application/json");
    }
}

void Server::handleGetKeysUnmasked(const void* /*req*/, void* res) {
    auto& response = getResponse(res);
    
    try {
        core::KeyStore ks = storage::loadVault(config_);
        
        std::ostringstream json;
        json << "[";
        
        bool first = true;
        for (const auto& [keyName, keyValue] : ks.kv) {
            if (!first) json << ",";
            first = false;
            
            json << "{"
                 << "\"name\":\"" << keyName << "\","
                 << "\"value\":\"" << keyValue << "\","
                 << "\"masked\":false"
                 << "}";
        }
        
        json << "]";
        response.set_content(json.str(), "application/json");
        
    } catch (const std::exception& e) {
        response.status = 500;
        response.set_content(jsonError("Failed to load unmasked keys: " + std::string(e.what()), 500), "application/json");
    }
}

void Server::handleGetProfileKeysUnmasked(const void* req, void* res) {
    auto& request = getRequest(req);
    auto& response = getResponse(res);
    
    try {
        std::string profileName = request.matches[1];
        
        auto profileKeys = storage::loadProfileKeys(config_, profileName);
        auto keyList = storage::readProfile(config_, profileName);
        
        // Fallback to global vault for keys not in profile-specific storage
        core::KeyStore ks = storage::loadVault(config_);
        
        std::ostringstream json;
        json << "{\"profile\":\"" << profileName << "\",\"keys\":[";
        
        bool first = true;
        for (const auto& keyName : keyList) {
            if (!first) json << ",";
            first = false;
            
            std::string value;
            auto pit = profileKeys.find(keyName);
            if (pit != profileKeys.end()) {
                value = pit->second;
            } else {
                auto vit = ks.kv.find(keyName);
                if (vit != ks.kv.end()) {
                    value = vit->second;
                }
            }
            
            json << "{"
                 << "\"name\":\"" << keyName << "\","
                 << "\"value\":\"" << value << "\","
                 << "\"masked\":false"
                 << "}";
        }
        
        json << "]}";
        response.set_content(json.str(), "application/json");
        
    } catch (const std::exception& e) {
        response.status = 500;
        response.set_content(jsonError("Failed to load unmasked profile keys: " + std::string(e.what()), 500), "application/json");
    }
}

std::string Server::jsonError(const std::string& message, int code) {
    std::ostringstream json;
    json << "{\"error\":\"" << escapeJson(message) << "\",\"code\":" << code << "}";
    return json.str();
}

std::string Server::jsonSuccess(const std::string& message) {
    std::ostringstream json;
    json << "{\"success\":true,\"message\":\"" << escapeJson(message) << "\"}";
    return json.str();
}

void Server::setCorsHeaders(void* res) {
    auto& response = getResponse(res);
    response.set_header("Access-Control-Allow-Origin", "*");
    response.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    response.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

const httplib::Request& Server::getRequest(const void* req) const {
    return *static_cast<const httplib::Request*>(req);
}

httplib::Response& Server::getResponse(void* res) const {
    return *static_cast<httplib::Response*>(res);
}

} // namespace http
} // namespace ak
