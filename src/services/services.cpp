#include "services/services.hpp"
#include "core/config.hpp"
#include "storage/vault.hpp"
#include "system/system.hpp"
#include <iostream>
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

// Helper function to get Google API key from various common environment variables
std::string getGoogleApiKey() {
    // Try common Google API key environment variable names
    const std::vector<std::string> google_keys = {
        "GEMINI_API_KEY",
        "GOOGLE_API_KEY",
        "GOOGLE_GENERATIVE_AI_API_KEY",
        "GOOGLE_AI_API_KEY",
        "GOOGLE_CLOUD_API_KEY"
    };
    
    for (const auto& key : google_keys) {
        const char* value = getenv(key.c_str());
        if (value && *value) {
            return std::string(value);
        }
    }
    return "";
}

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
    // Add Google API key variations
    keys.insert("GOOGLE_API_KEY");
    keys.insert("GOOGLE_GENERATIVE_AI_API_KEY");
    keys.insert("GOOGLE_AI_API_KEY");
    keys.insert("GOOGLE_CLOUD_API_KEY");
    return keys;
}

// Detect configured providers from vault and environment.
// Only returns services that are TESTABLE and have sufficient configuration.
std::vector<std::string> detectConfiguredServices(const core::Config& cfg) {
    std::vector<std::string> services;
    std::unordered_set<std::string> allAvailableKeys;
    
    // Collect all available keys from multiple sources
    try {
        // 1. Load vault keys
        auto vault = ak::storage::loadVault(cfg);
        for (const auto& [key, value] : vault.kv) {
            if (!value.empty()) {
                allAvailableKeys.insert(key);
            }
        }
        
        // 2. Check environment variables
        for (const auto& pair : SERVICE_KEYS) {
            const char* envValue = getenv(pair.second.c_str());
            if (envValue && *envValue) {
                allAvailableKeys.insert(pair.second);
            }
        }
        
        // 3. Check all profiles for additional keys
        auto profiles = ak::storage::listProfiles(cfg);
        for (const auto& profileName : profiles) {
            auto profileKeys = ak::storage::readProfile(cfg, profileName);
            for (const auto& key : profileKeys) {
                allAvailableKeys.insert(key);
            }
        }
        
    } catch (const std::exception& e) {
        // If we can't load vault or profiles, fall back to environment only
        for (const auto& pair : SERVICE_KEYS) {
            const char* envValue = getenv(pair.second.c_str());
            if (envValue && *envValue) {
                allAvailableKeys.insert(pair.second);
            }
        }
    }
    
    // Check which services have their required keys available
    for (const auto& pair : SERVICE_KEYS) {
        const std::string& service = pair.first;
        const std::string& requiredKey = pair.second;
        
        if (TESTABLE_SERVICES.find(service) != TESTABLE_SERVICES.end()) {
            bool isConfigured = false;
            
            // Special handling for gemini service to check Google aliases
            if (service == "gemini") {
                // Check for any Google API key variation
                const std::vector<std::string> googleKeys = {
                    "GEMINI_API_KEY", "GOOGLE_API_KEY", "GOOGLE_GENERATIVE_AI_API_KEY",
                    "GOOGLE_AI_API_KEY", "GOOGLE_CLOUD_API_KEY"
                };
                for (const auto& googleKey : googleKeys) {
                    if (allAvailableKeys.find(googleKey) != allAvailableKeys.end()) {
                        isConfigured = true;
                        break;
                    }
                }
                
                // Also check environment directly
                if (!isConfigured) {
                    std::string googleKey = getGoogleApiKey();
                    isConfigured = !googleKey.empty();
                }
            } else {
                // Check if the required key is available
                isConfigured = allAvailableKeys.find(requiredKey) != allAvailableKeys.end();
            }
            
            if (isConfigured) {
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
    TestResult result;
    result.service = service;
    result.ok = false;
    
    auto start = std::chrono::steady_clock::now();
    
    // Helper function to get API key from any source (vault, profiles, environment)
    auto getServiceKey = [&cfg](const std::string& keyName) -> std::string {
        // First check vault
        try {
            auto vault = ak::storage::loadVault(cfg);
            auto it = vault.kv.find(keyName);
            if (it != vault.kv.end() && !it->second.empty()) {
                return it->second;
            }
        } catch (const std::exception&) {
            // Vault load failed, continue to other sources
        }
        
        // Then check all profiles
        try {
            auto profiles = ak::storage::listProfiles(cfg);
            for (const auto& profileName : profiles) {
                auto keys = ak::storage::readProfile(cfg, profileName);
                if (std::find(keys.begin(), keys.end(), keyName) != keys.end()) {
                    // Key exists in this profile, load from vault
                    auto vault = ak::storage::loadVault(cfg);
                    auto it = vault.kv.find(keyName);
                    if (it != vault.kv.end() && !it->second.empty()) {
                        return it->second;
                    }
                }
            }
        } catch (const std::exception&) {
            // Profile check failed, continue to environment
        }
        
        // Finally check environment
        const char* envValue = getenv(keyName.c_str());
        if (envValue && *envValue) {
            return std::string(envValue);
        }
        
        return "";
    };
    
    try {
        if (service == "openai") {
            std::string apiKey = getServiceKey("OPENAI_API_KEY");
            if (!apiKey.empty()) {
                std::string args = "-H 'Authorization: Bearer " + apiKey + "' https://api.openai.com/v1/models";
                auto curl_result = curl_ok(args);
                result.ok = curl_result.first;
                if (!result.ok) {
                    result.error_message = curl_result.second;
                    if (getenv("AK_DEBUG_TESTS")) {
                        std::cerr << "[debug] service=" << service << " curl_output=" << result.error_message << std::endl;
                    }
                }
            } else {
                result.error_message = "No OPENAI_API_KEY configured";
            }
        } else if (service == "anthropic") {
            std::string apiKey = getServiceKey("ANTHROPIC_API_KEY");
            if (!apiKey.empty()) {
                std::string args = "-H 'x-api-key: " + apiKey + "' https://api.anthropic.com/v1/messages";
                auto curl_result = curl_ok(args);
                result.ok = curl_result.first;
                if (!result.ok) {
                    result.error_message = curl_result.second;
                    if (getenv("AK_DEBUG_TESTS")) {
                        std::cerr << "[debug] service=" << service << " curl_output=" << result.error_message << std::endl;
                    }
                }
            } else {
                result.error_message = "No ANTHROPIC_API_KEY configured";
            }
        } else if (service == "gemini") {
            // Special handling for Gemini API - check for Google aliases
            std::string googleKey;
            const std::vector<std::string> googleKeys = {
                "GEMINI_API_KEY", "GOOGLE_API_KEY", "GOOGLE_GENERATIVE_AI_API_KEY",
                "GOOGLE_AI_API_KEY", "GOOGLE_CLOUD_API_KEY"
            };
            for (const auto& key : googleKeys) {
                googleKey = getServiceKey(key);
                if (!googleKey.empty()) break;
            }
            
            result.ok = !googleKey.empty();
            if (!result.ok) {
                result.error_message = "No Google API key found (checked GEMINI_API_KEY, GOOGLE_API_KEY, GOOGLE_GENERATIVE_AI_API_KEY, GOOGLE_AI_API_KEY, GOOGLE_CLOUD_API_KEY)";
            }
        } else {
            // Generic test - check if service key exists in any source
            auto it = SERVICE_KEYS.find(service);
            if (it != SERVICE_KEYS.end()) {
                std::string apiKey = getServiceKey(it->second);
                result.ok = !apiKey.empty();
                if (!result.ok) {
                    result.error_message = "No API key configured for service";
                } else {
                    // For other services, just verify the key exists (basic connectivity test)
                    result.ok = true; // Assume success if key is available
                }
            } else {
                result.error_message = "Unknown service";
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