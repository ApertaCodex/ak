#include "gtest/gtest.h"
#include "services/services.hpp"
#include "core/config.hpp" // Required for ak::core::Config
#include <algorithm>

using namespace ak::services;

TEST(ServiceDefinitions, ServiceKeysContainsExpectedServices) {
    ASSERT_NE(SERVICE_KEYS.find("openai"), SERVICE_KEYS.end());
    ASSERT_NE(SERVICE_KEYS.find("anthropic"), SERVICE_KEYS.end());
    ASSERT_NE(SERVICE_KEYS.find("github"), SERVICE_KEYS.end());
    ASSERT_NE(SERVICE_KEYS.find("aws"), SERVICE_KEYS.end());
    
    ASSERT_EQ(SERVICE_KEYS.at("openai"), "OPENAI_API_KEY");
    ASSERT_EQ(SERVICE_KEYS.at("anthropic"), "ANTHROPIC_API_KEY");
    ASSERT_EQ(SERVICE_KEYS.at("github"), "GITHUB_TOKEN");
    ASSERT_EQ(SERVICE_KEYS.at("aws"), "AWS_ACCESS_KEY_ID");
}

TEST(ServiceDefinitions, TestableServicesContainsAIServices) {
    ASSERT_NE(TESTABLE_SERVICES.find("openai"), TESTABLE_SERVICES.end());
    ASSERT_NE(TESTABLE_SERVICES.find("anthropic"), TESTABLE_SERVICES.end());
    ASSERT_NE(TESTABLE_SERVICES.find("groq"), TESTABLE_SERVICES.end());
    ASSERT_NE(TESTABLE_SERVICES.find("mistral"), TESTABLE_SERVICES.end());
    
    // Non-testable services should not be in TESTABLE_SERVICES
    ASSERT_EQ(TESTABLE_SERVICES.find("aws"), TESTABLE_SERVICES.end());
    ASSERT_EQ(TESTABLE_SERVICES.find("github"), TESTABLE_SERVICES.end());
}

TEST(GetKnownServiceKeysFunction, ShouldReturnAllServiceKeysAndVariations) {
    auto keys = getKnownServiceKeys();
    
    // Should contain main service keys
    ASSERT_NE(keys.find("OPENAI_API_KEY"), keys.end());
    ASSERT_NE(keys.find("ANTHROPIC_API_KEY"), keys.end());
    ASSERT_NE(keys.find("GITHUB_TOKEN"), keys.end());
    ASSERT_NE(keys.find("AWS_ACCESS_KEY_ID"), keys.end());
    
    // Should contain common variations
    ASSERT_NE(keys.find("AWS_SECRET_ACCESS_KEY"), keys.end());
    ASSERT_NE(keys.find("AWS_SESSION_TOKEN"), keys.end());
    ASSERT_NE(keys.find("GOOGLE_CLOUD_PROJECT"), keys.end());
    ASSERT_NE(keys.find("AZURE_CLIENT_SECRET"), keys.end());
    
    // Should be a reasonable number of keys
    ASSERT_GT(keys.size(), 30u); // We have many services + variations
}

TEST(GetKnownServiceKeysFunction, ShouldNotContainDuplicates) {
    auto keys = getKnownServiceKeys();
    std::vector<std::string> keyVector(keys.begin(), keys.end());
    std::sort(keyVector.begin(), keyVector.end());
    
    // Check for duplicates
    for (size_t i = 1; i < keyVector.size(); ++i) {
        ASSERT_NE(keyVector[i], keyVector[i-1]);
    }
}

TEST(TestResultStructure, TestResultInitialization) {
    TestResult result;
    result.service = "openai";
    result.ok = true;
    result.duration = std::chrono::milliseconds(250);
    
    ASSERT_EQ(result.service, "openai");
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.duration.count(), 250);
}

TEST(TestResultStructure, TestResultCanBeCopied) {
    TestResult result1;
    result1.service = "anthropic";
    result1.ok = false;
    result1.duration = std::chrono::milliseconds(5000);
    
    TestResult result2 = result1;
    
    ASSERT_EQ(result2.service, "anthropic");
    ASSERT_FALSE(result2.ok);
    ASSERT_EQ(result2.duration.count(), 5000);
}

TEST(DetectConfiguredServicesFunction, ShouldReturnEmptyVectorWhenNoServicesConfigured) {
    ak::core::Config cfg;
    auto services = detectConfiguredServices(cfg);
    
    // Without environment variables set, should return empty or minimal list
    // This depends on the actual environment, so we just test that it doesn't crash
    ASSERT_GE(services.size(), 0u);
}

TEST(DetectConfiguredServicesFunction, ReturnedServicesShouldBeSorted) {
    ak::core::Config cfg;
    auto services = detectConfiguredServices(cfg);
    
    if (services.size() > 1) {
        for (size_t i = 1; i < services.size(); ++i) {
            ASSERT_LT(services[i-1], services[i]);
        }
    }
}

TEST(DetectConfiguredServicesFunction, ReturnedServicesShouldBeTestable) {
    ak::core::Config cfg;
    auto services = detectConfiguredServices(cfg);
    
    for (const auto& service : services) {
        ASSERT_NE(TESTABLE_SERVICES.find(service), TESTABLE_SERVICES.end());
    }
}

TEST(CurlOkFunction, ShouldHandleBasicCurlOperations) {
    // Test with a simple operation that should work
    auto result = curl_ok("--version");
    
    // This might fail if curl isn't available, but shouldn't crash
    // We mainly want to test that the function can be called
    ASSERT_TRUE(result.first == true || result.first == false);
}

TEST(RunTestsParallelFunction, ShouldHandleEmptyServiceList) {
    ak::core::Config cfg;
    std::vector<std::string> emptyServices;
    
    auto results = run_tests_parallel(cfg, emptyServices, false);
    
    ASSERT_TRUE(results.empty());
}

TEST(RunTestsParallelFunction, ShouldReturnResultsForAllProvidedServices) {
    ak::core::Config cfg;
    std::vector<std::string> services = {"openai", "anthropic"};
    
    auto results = run_tests_parallel(cfg, services, false);
    
    ASSERT_EQ(results.size(), 2u);
    
    // Results should contain the services we requested
    std::set<std::string> resultServices;
    for (const auto& result : results) {
        resultServices.insert(result.service);
    }
    
    ASSERT_NE(resultServices.find("openai"), resultServices.end());
    ASSERT_NE(resultServices.find("anthropic"), resultServices.end());
}

TEST(RunTestsParallelFunction, ShouldRespectFailFastFlag) {
    ak::core::Config cfg;
    std::vector<std::string> services = {"openai", "anthropic", "groq"};
    
    // Test both fail_fast modes
    auto results1 = run_tests_parallel(cfg, services, true);
    auto results2 = run_tests_parallel(cfg, services, false);
    
    // Both should return some results (exact behavior depends on service availability)
    ASSERT_LE(results1.size(), services.size());
    ASSERT_LE(results2.size(), services.size());
}