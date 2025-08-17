#pragma once

#include "core/config.hpp"
#include <string>
#include <vector>
#include <unordered_set>
#include <map>
#include <chrono>

namespace ak {
namespace services {

// Service definitions
extern const std::map<std::string, std::string> SERVICE_KEYS;
extern const std::unordered_set<std::string> TESTABLE_SERVICES;

// Service management
std::unordered_set<std::string> getKnownServiceKeys();
std::vector<std::string> detectConfiguredServices(const core::Config& cfg);

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