#include "services/services.hpp"
#include "core/config.hpp"
#include "system/system.hpp"
#include <chrono>
#include <future>
#include <thread>
#include <algorithm>

namespace ak {
namespace services {

// Service definitions
const std::map<std::string, std::string> SERVICE_KEYS = {
    {"anthropic", "ANTHROPIC_API_KEY"},
    {"azure_openai", "AZURE_OPENAI_API_KEY"},
    {"brave", "BRAVE_API_KEY"},
    {"cohere", "COHERE_API_KEY"},
    {"deepseek", "DEEPSEEK_API_KEY"},
    {"exa", "EXA_API_KEY"},
    {"fireworks", "FIREWORKS_API_KEY"},
    {"gemini", "GEMINI_API_KEY"}, // accepts GOOGLE_* too via env fallback
    {"groq", "GROQ_API_KEY"},
    {"huggingface", "HUGGINGFACE_TOKEN"},
    {"mistral", "MISTRAL_API_KEY"},
    {"openai", "OPENAI_API_KEY"},
    {"openrouter", "OPENROUTER_API_KEY"},
    {"perplexity", "PERPLEXITY_API_KEY"},
    {"sambanova", "SAMBANOVA_API_KEY"},
    {"tavily", "TAVILY_API_KEY"},
    {"together", "TOGETHER_API_KEY"},
    {"xai", "XAI_API_KEY"},
    // Cloud providers
    {"aws", "AWS_ACCESS_KEY_ID"},
    {"gcp", "GOOGLE_APPLICATION_CREDENTIALS"},
    {"azure", "AZURE_CLIENT_ID"},
    {"github", "GITHUB_TOKEN"},
    {"docker", "DOCKER_AUTH_TOKEN"},
    // Database providers
    {"mongodb", "MONGODB_URI"},
    {"postgres", "DATABASE_URL"},
    {"redis", "REDIS_URL"},
    // Other common services
    {"stripe", "STRIPE_SECRET_KEY"},
    {"sendgrid", "SENDGRID_API_KEY"},
    {"twilio", "TWILIO_AUTH_TOKEN"},
    {"slack", "SLACK_API_TOKEN"},
    {"discord", "DISCORD_TOKEN"},
    {"vercel", "VERCEL_TOKEN"},
    {"netlify", "NETLIFY_AUTH_TOKEN"},
};

// Services that have implemented tests in test_one()
const std::unordered_set<std::string> TESTABLE_SERVICES = {
    "anthropic","azure_openai","brave","cohere","deepseek","exa","fireworks",
    "gemini","groq","huggingface","mistral","openai","openrouter",
    "perplexity","sambanova","tavily","together","xai"
};

// Service management
std::unordered_set<std::string> getKnownServiceKeys() {
    std::unordered_set<std::string> keys;
    for (const auto& service : SERVICE_KEYS) {
        keys.insert(service.second);
    }
    // Add common variations
    keys.insert("AWS_SECRET_ACCESS_KEY");
    keys.insert("AWS_SESSION_TOKEN");
    keys.insert("GOOGLE_CLOUD_PROJECT");
    keys.insert("AZURE_CLIENT_SECRET");
    keys.insert("AZURE_TENANT_ID");
    keys.insert("GITHUB_CLIENT_ID");
    keys.insert("GITHUB_CLIENT_SECRET");
    keys.insert("DOCKER_USERNAME");
    keys.insert("DOCKER_PASSWORD");
    keys.insert("STRIPE_PUBLISHABLE_KEY");
    keys.insert("SENDGRID_FROM_EMAIL");
    keys.insert("TWILIO_ACCOUNT_SID");
    keys.insert("SLACK_WEBHOOK_URL");
    keys.insert("DISCORD_CLIENT_ID");
    keys.insert("DISCORD_CLIENT_SECRET");
    return keys;
}

// Detect configured providers from vault and environment.
// Only returns services that are TESTABLE and have sufficient configuration.
std::vector<std::string> detectConfiguredServices(const core::Config& cfg) {
    (void)cfg; // Parameter intentionally unused for now
    std::vector<std::string> services;
    
    // This would normally load the vault, but for now return empty
    // In a full implementation, this would check which services have keys configured
    
    // Placeholder logic - would check vault and environment variables
    for (const auto& pair : SERVICE_KEYS) {
        const std::string& service = pair.first;
        if (TESTABLE_SERVICES.find(service) != TESTABLE_SERVICES.end()) {
            // Check if service is configured (simplified check)
            const char* envValue = getenv(pair.second.c_str());
            if (envValue && *envValue) {
                services.push_back(service);
            }
        }
    }
    
    std::sort(services.begin(), services.end());
    return services;
}

// Testing functions
std::pair<bool, std::string> curl_ok(const std::string& args) {
    std::string cmd = "curl -sS -L --connect-timeout 5 --max-time 12 " + args + " 2>&1";
    int exitCode = 0;
    std::string output = system::runCmdCapture(cmd, &exitCode);
    bool success = (exitCode == 0);
    return {success, success ? "" : output};
}

TestResult test_one(const core::Config& cfg, const std::string& service) {
    (void)cfg; // Parameter intentionally unused for now
    TestResult result;
    result.service = service;
    result.ok = false;
    
    auto start = std::chrono::steady_clock::now();
    
    // Simplified test logic - in a real implementation, this would contain
    // specific API testing logic for each service
    try {
        if (service == "openai") {
            // Example: test OpenAI API
            std::string args = "-H 'Authorization: Bearer test' https://api.openai.com/v1/models";
            auto curl_result = curl_ok(args);
            result.ok = curl_result.first;
            if (!result.ok) {
                result.error_message = curl_result.second;
                // Debug: print captured curl output to stderr when running locally
                if (getenv("AK_DEBUG_TESTS")) {
                    std::cerr << "[debug] service=" << service << " curl_output=" << result.error_message << std::endl;
                }
            }
        } else if (service == "anthropic") {
            // Example: test Anthropic API
            std::string args = "-H 'x-api-key: test' https://api.anthropic.com/v1/messages";
            auto curl_result = curl_ok(args);
            result.ok = curl_result.first;
            if (!result.ok) {
                result.error_message = curl_result.second;
                if (getenv("AK_DEBUG_TESTS")) {
                    std::cerr << "[debug] service=" << service << " curl_output=" << result.error_message << std::endl;
                }
            }
        } else {
            // Generic test - just check if service key exists
            auto it = SERVICE_KEYS.find(service);
            if (it != SERVICE_KEYS.end()) {
                const char* key = getenv(it->second.c_str());
                result.ok = (key && *key);
            }
        }
    } catch (...) {
        result.ok = false;
        result.error_message = "Unknown exception";
    }
    
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    return result;
}

std::vector<TestResult> run_tests_parallel(
    const core::Config& cfg, 
    const std::vector<std::string>& services, 
    bool fail_fast) {
    
    std::vector<TestResult> results;
    std::vector<std::future<TestResult>> futures;
    
    // Launch tests in parallel
    for (const auto& service : services) {
        futures.push_back(
            std::async(std::launch::async, [&cfg, service]() {
                return test_one(cfg, service);
            })
        );
    }
    
    // Collect results
    for (auto& future : futures) {
        TestResult result = future.get();
        results.push_back(result);
        
        if (fail_fast && !result.ok) {
            break;
        }
    }
    
    return results;
}

} // namespace services
} // namespace ak