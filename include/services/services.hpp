#pragma once

#include "core/config.hpp"
#include <string>
#include <vector>
#include <unordered_set>
#include <map>
#include <chrono>

namespace ak {
namespace services {

// Custom service definition
struct CustomService {
    std::string name;
    std::string keyName;
    std::string description;
    std::string testEndpoint;
    std::string testMethod; // GET, POST, etc.
    std::string testHeaders; // Additional headers for testing
    bool testable;
    
    CustomService() : testable(false) {}
    CustomService(const std::string& n, const std::string& k, const std::string& d = "",
                  const std::string& e = "", const std::string& m = "GET",
                  const std::string& h = "", bool t = false)
        : name(n), keyName(k), description(d), testEndpoint(e), testMethod(m), testHeaders(h), testable(t) {}
};

// Service definitions
extern const std::map<std::string, std::string> SERVICE_KEYS;
extern const std::unordered_set<std::string> TESTABLE_SERVICES;

// Service management
std::unordered_set<std::string> getKnownServiceKeys();
std::vector<std::string> detectConfiguredServices(const core::Config& cfg);

// Custom service management
std::vector<CustomService> loadCustomServices(const core::Config& cfg);
void saveCustomServices(const core::Config& cfg, const std::vector<CustomService>& services);
void addCustomService(const core::Config& cfg, const CustomService& service);
void removeCustomService(const core::Config& cfg, const std::string& serviceName);
CustomService* findCustomService(std::vector<CustomService>& services, const std::string& name);
std::map<std::string, std::string> getAllServiceKeys(const core::Config& cfg);

// Test result structure
struct TestResult {
    std::string service;
    bool ok;
    std::chrono::milliseconds duration;
    std::string error_message;
};

// Testing functions
std::pair<bool, std::string> curl_ok(const std::string& args);
TestResult test_one(const core::Config& cfg, const std::string& service);
std::vector<TestResult> run_tests_parallel(
    const core::Config& cfg, 
    const std::vector<std::string>& services, 
    bool fail_fast
);

} // namespace services
} // namespace ak