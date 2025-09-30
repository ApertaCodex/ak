#pragma once

#include "core/config.hpp"
#include <string>
#include <vector>
#include <unordered_set>
#include <map>
#include <chrono>

namespace ak {
namespace services {

// Unified service definition
struct Service {
    std::string name;
    std::string keyName;
    std::string description;
    std::string testEndpoint;
    std::string testMethod; // GET, POST, etc.
    std::string testHeaders; // Additional headers for testing
    std::string authMethod; // Bearer, Basic Auth, etc.
    std::string authLocation; // header, query, body
    std::string authParameter; // header or parameter name
    std::string authPrefix; // prefix applied before token value
    std::string testBody; // Optional JSON/form body payload
    bool testable;
    bool isBuiltIn; // true for predefined services, false for user-created

    Service() : authMethod("Bearer"), authLocation("header"), authParameter("Authorization"), authPrefix(""), testBody(""), testable(false), isBuiltIn(false) {}
    Service(const std::string& n, const std::string& k, const std::string& d = "",
            const std::string& e = "", const std::string& m = "GET",
            const std::string& h = "", const std::string& a = "Bearer",
            bool t = false, bool builtin = false,
            const std::string& location = "header",
            const std::string& parameter = "",
            const std::string& prefix = "",
            const std::string& body = "")
        : name(n), keyName(k), description(d), testEndpoint(e), testMethod(m),
          testHeaders(h), authMethod(a), authLocation(location.empty() ? "header" : location),
          authParameter(parameter), authPrefix(prefix), testBody(body), testable(t), isBuiltIn(builtin)
    {
        if (authParameter.empty()) {
            if (authLocation == "header") {
                authParameter = "Authorization";
            } else {
                authParameter = "api_key";
            }
        }

        if (authPrefix.empty() && authLocation == "header") {
            if (authMethod == "Bearer") {
                authPrefix = "Bearer ";
            } else if (authMethod == "Basic Auth") {
                authPrefix = "Basic ";
            }
        }
    }
};

// Service definitions
extern const std::map<std::string, Service> DEFAULT_SERVICES;
extern const std::map<std::string, std::string> SERVICE_KEYS; // For backwards compatibility
extern const std::unordered_set<std::string> TESTABLE_SERVICES; // For backwards compatibility

// Service management
std::unordered_set<std::string> getKnownServiceKeys();
std::vector<std::string> detectConfiguredServices(const core::Config& cfg, const std::string& profileName);

// Service management
std::map<std::string, Service> loadAllServices(const core::Config& cfg);
void saveUserServices(const core::Config& cfg, const std::map<std::string, Service>& services);
void addService(const core::Config& cfg, const Service& service);
void removeService(const core::Config& cfg, const std::string& serviceName);
void updateService(const core::Config& cfg, const Service& service);
Service* findService(std::map<std::string, Service>& services, const std::string& name);
std::map<std::string, std::string> getAllServiceKeys(const core::Config& cfg);
std::unordered_set<std::string> getKnownServiceKeys(const core::Config& cfg);
Service getServiceByName(const core::Config& cfg, const std::string& name);

// Test result structure
struct TestResult {
    std::string service;
    bool ok;
    std::chrono::milliseconds duration;
    std::string error_message;
    std::string curl_command;
    std::string response_snippet;
    int exit_code = 0;
    int http_status = 0;
};

struct CurlExecResult {
    bool ok = false;
    int exit_code = 0;
    int http_status = 0;
    std::string output;
    std::string command;
};

// Testing functions
CurlExecResult curl_ok(const std::string& args, bool debug = false);
TestResult test_one(const core::Config& cfg, const std::string& service, const std::string& profileName = "", bool debug = false);
std::vector<TestResult> run_tests_parallel(
    const core::Config& cfg,
    const std::vector<std::string>& services,
    bool fail_fast,
    const std::string& profileName = "",
    bool debug = false
);
TestResult testServiceWithKey(const Service& service, const std::string& apiKey);

} // namespace services
} // namespace ak
