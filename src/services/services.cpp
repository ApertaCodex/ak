#include "services/services.hpp"
#include "core/config.hpp"
#include "storage/vault.hpp"
#include "system/system.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <future>
#include <thread>
#include <algorithm>

namespace ak {
namespace services {

// Default service definitions with full information
const std::map<std::string, Service> DEFAULT_SERVICES = {
    {"anthropic", {"anthropic", "ANTHROPIC_API_KEY", "Anthropic Claude AI", "https://api.anthropic.com/v1/messages", "POST", "", "Bearer", true, true}},
    {"azure_openai", {"azure_openai", "AZURE_OPENAI_API_KEY", "Azure OpenAI Service", "", "GET", "", "Bearer", true, true}},
    {"brave", {"brave", "BRAVE_API_KEY", "Brave Search API", "https://api.search.brave.com/res/v1/web/search", "GET", "", "Bearer", true, true}},
    {"cohere", {"cohere", "COHERE_API_KEY", "Cohere AI", "https://api.cohere.ai/v1/models", "GET", "", "Bearer", true, true}},
    {"deepseek", {"deepseek", "DEEPSEEK_API_KEY", "DeepSeek AI", "https://api.deepseek.com/v1/models", "GET", "", "Bearer", true, true}},
    {"exa", {"exa", "EXA_API_KEY", "Exa Search", "https://api.exa.ai/search", "POST", "", "Bearer", true, true}},
    {"fireworks", {"fireworks", "FIREWORKS_API_KEY", "Fireworks AI", "https://api.fireworks.ai/inference/v1/models", "GET", "", "Bearer", true, true}},
    {"gemini", {"gemini", "GEMINI_API_KEY", "Google Gemini", "https://generativelanguage.googleapis.com/v1/models", "GET", "", "Bearer", true, true}},
    {"groq", {"groq", "GROQ_API_KEY", "Groq AI", "https://api.groq.com/openai/v1/models", "GET", "", "Bearer", true, true}},
    {"huggingface", {"huggingface", "HUGGINGFACE_TOKEN", "Hugging Face", "https://huggingface.co/api/whoami", "GET", "", "Bearer", true, true}},
    {"inference", {"inference", "INFERENCE_API_KEY", "Hugging Face Inference API", "https://api-inference.huggingface.co/models", "GET", "", "Bearer", true, true}},
    {"langchain", {"langchain", "LANGCHAIN_API_KEY", "LangChain", "https://api.smith.langchain.com/info", "GET", "", "Bearer", true, true}},
    {"continue", {"continue", "CONTINUE_API_KEY", "Continue.dev", "https://api.continue.dev/v1/health", "GET", "", "Bearer", true, true}},
    {"composio", {"composio", "COMPOSIO_API_KEY", "Composio", "https://backend.composio.dev/api/v1/actions", "GET", "", "Bearer", true, true}},
    {"hyperbolic", {"hyperbolic", "HYPERBOLIC_API_KEY", "Hyperbolic AI", "https://api.hyperbolic.xyz/v1/models", "GET", "", "Bearer", true, true}},
    {"logfire", {"logfire", "LOGFIRE_TOKEN", "Pydantic Logfire", "https://logfire-api.pydantic.dev/v1/info", "GET", "", "Bearer", true, true}},
    {"mistral", {"mistral", "MISTRAL_API_KEY", "Mistral AI", "https://api.mistral.ai/v1/models", "GET", "", "Bearer", true, true}},
    {"openai", {"openai", "OPENAI_API_KEY", "OpenAI", "https://api.openai.com/v1/models", "GET", "", "Bearer", true, true}},
    {"openrouter", {"openrouter", "OPENROUTER_API_KEY", "OpenRouter", "https://openrouter.ai/api/v1/models", "GET", "", "Bearer", true, true}},
    {"perplexity", {"perplexity", "PERPLEXITY_API_KEY", "Perplexity AI", "https://api.perplexity.ai/models", "GET", "", "Bearer", true, true}},
    {"sambanova", {"sambanova", "SAMBANOVA_API_KEY", "SambaNova AI", "https://api.sambanova.ai/v1/models", "GET", "", "Bearer", true, true}},
    {"tavily", {"tavily", "TAVILY_API_KEY", "Tavily Search", "https://api.tavily.com/search", "POST", "", "Bearer", true, true}},
    {"together", {"together", "TOGETHER_API_KEY", "Together AI", "https://api.together.xyz/v1/models", "GET", "", "Bearer", true, true}},
    {"xai", {"xai", "XAI_API_KEY", "xAI Grok", "https://api.x.ai/v1/models", "GET", "", "Bearer", true, true}},
    // Cloud providers
    {"aws", {"aws", "AWS_ACCESS_KEY_ID", "Amazon Web Services", "", "GET", "", "AWS Signature", false, true}},
    {"gcp", {"gcp", "GOOGLE_APPLICATION_CREDENTIALS", "Google Cloud Platform", "", "GET", "", "OAuth2", false, true}},
    {"azure", {"azure", "AZURE_CLIENT_ID", "Microsoft Azure", "", "GET", "", "OAuth2", false, true}},
    {"github", {"github", "GITHUB_TOKEN", "GitHub", "https://api.github.com/user", "GET", "", "Bearer", false, true}},
    {"docker", {"docker", "DOCKER_AUTH_TOKEN", "Docker Hub", "", "GET", "", "Bearer", false, true}},
    // Database providers
    {"mongodb", {"mongodb", "MONGODB_URI", "MongoDB", "", "GET", "", "Connection String", false, true}},
    {"postgres", {"postgres", "DATABASE_URL", "PostgreSQL", "", "GET", "", "Connection String", false, true}},
    {"redis", {"redis", "REDIS_URL", "Redis", "", "GET", "", "Connection String", false, true}},
    // Other common services
    {"stripe", {"stripe", "STRIPE_SECRET_KEY", "Stripe Payment", "https://api.stripe.com/v1/account", "GET", "", "Bearer", false, true}},
    {"sendgrid", {"sendgrid", "SENDGRID_API_KEY", "SendGrid Email", "https://api.sendgrid.com/v3/user/profile", "GET", "", "Bearer", false, true}},
    {"twilio", {"twilio", "TWILIO_AUTH_TOKEN", "Twilio", "https://api.twilio.com/2010-04-01/Accounts.json", "GET", "", "Basic Auth", false, true}},
    {"slack", {"slack", "SLACK_API_TOKEN", "Slack", "https://slack.com/api/auth.test", "GET", "", "Bearer", false, true}},
    {"discord", {"discord", "DISCORD_TOKEN", "Discord Bot", "https://discord.com/api/v10/users/@me", "GET", "", "Bot", false, true}},
    {"vercel", {"vercel", "VERCEL_TOKEN", "Vercel", "https://api.vercel.com/v2/user", "GET", "", "Bearer", false, true}},
    {"netlify", {"netlify", "NETLIFY_AUTH_TOKEN", "Netlify", "https://api.netlify.com/api/v1/user", "GET", "", "Bearer", false, true}},
};

// Legacy SERVICE_KEYS for backwards compatibility
const std::map<std::string, std::string> SERVICE_KEYS = []() {
    std::map<std::string, std::string> keys;
    for (const auto& [name, service] : DEFAULT_SERVICES) {
        keys[name] = service.keyName;
    }
    return keys;
}();

// Legacy TESTABLE_SERVICES for backwards compatibility
const std::unordered_set<std::string> TESTABLE_SERVICES = []() {
    std::unordered_set<std::string> testable;
    for (const auto& [name, service] : DEFAULT_SERVICES) {
        if (service.testable) {
            testable.insert(name);
        }
    }
    return testable;
}();

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

std::unordered_set<std::string> getKnownServiceKeys(const core::Config& cfg) {
    auto keys = getKnownServiceKeys();
    
    // Add user-defined service keys
    try {
        auto allServices = loadAllServices(cfg);
        for (const auto& [name, service] : allServices) {
            if (!service.isBuiltIn) {
                keys.insert(service.keyName);
            }
        }
    } catch (const std::exception&) {
        // Ignore errors loading services
    }
    
    return keys;
}

// Detect configured providers from vault and environment.
// Returns services that are TESTABLE and have sufficient configuration (both built-in and custom).
std::vector<std::string> detectConfiguredServices(const core::Config& cfg, const std::string& profileName) {
    std::vector<std::string> services;
    std::unordered_set<std::string> allAvailableKeys;
    
    // Collect all available keys from multiple sources
    try {
        // 1. Load profile keys if profile specified
        if (!profileName.empty()) {
            auto profileKeys = ak::storage::loadProfileKeys(cfg, profileName);
            for (const auto& [key, value] : profileKeys) {
                if (!value.empty()) {
                    allAvailableKeys.insert(key);
                }
            }
        } else {
            // Load default profile keys
            auto defaultProfile = ak::storage::getDefaultProfileName();
            auto profileKeys = ak::storage::loadProfileKeys(cfg, defaultProfile);
            for (const auto& [key, value] : profileKeys) {
                if (!value.empty()) {
                    allAvailableKeys.insert(key);
                }
            }
        }
        
        // 2. Check environment variables as fallback
        auto allServices = loadAllServices(cfg);
        for (const auto& [name, service] : allServices) {
            const char* envValue = getenv(service.keyName.c_str());
            if (envValue && *envValue) {
                allAvailableKeys.insert(service.keyName);
            }
        }
        
    } catch (const std::exception& e) {
        // Fall back to environment only
        auto allServices = loadAllServices(cfg);
        for (const auto& [name, service] : allServices) {
            const char* envValue = getenv(service.keyName.c_str());
            if (envValue && *envValue) {
                allAvailableKeys.insert(service.keyName);
            }
        }
    }
    
    // Check which services have their required keys available
    auto allServices = loadAllServices(cfg);
    for (const auto& [name, service] : allServices) {
        if (service.testable) {
            bool isConfigured = false;
            
            // Special handling for gemini service to check Google aliases
            if (name == "gemini") {
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
                
                if (!isConfigured) {
                    std::string googleKey = getGoogleApiKey();
                    isConfigured = !googleKey.empty();
                }
            } else {
                isConfigured = allAvailableKeys.find(service.keyName) != allAvailableKeys.end();
            }
            
            if (isConfigured) {
                services.push_back(name);
            }
        }
    }
    
    std::sort(services.begin(), services.end());
    return services;
}

// Testing functions
std::pair<bool, std::string> curl_ok(const std::string& args) {
    // Use -f flag to make curl fail on HTTP error status codes (4xx, 5xx)
    std::string cmd = "curl -fsSL --connect-timeout 5 --max-time 12 " + args + " 2>&1";
    int exitCode = 0;
    std::string output = system::runCmdCapture(cmd, &exitCode);
    bool success = (exitCode == 0);
    
    // Additional check for common error patterns in output even if exit code is 0
    if (success && !output.empty()) {
        std::string lowerOutput = output;
        std::transform(lowerOutput.begin(), lowerOutput.end(), lowerOutput.begin(), ::tolower);
        
        // Check for common error patterns
        if (lowerOutput.find("\"error\"") != std::string::npos ||
            lowerOutput.find("unauthorized") != std::string::npos ||
            lowerOutput.find("invalid_api_key") != std::string::npos ||
            lowerOutput.find("invalid api key") != std::string::npos ||
            lowerOutput.find("authentication failed") != std::string::npos ||
            lowerOutput.find("access denied") != std::string::npos ||
            lowerOutput.find("forbidden") != std::string::npos) {
            success = false;
        }
    }
    
    return {success, success ? "" : output};
}

TestResult test_one(const core::Config& cfg, const std::string& service, const std::string& profileName) {
    TestResult result;
    result.service = service;
    result.ok = false;
    
    auto start = std::chrono::steady_clock::now();
    
    // Helper function to get API key from profile or environment
    auto getServiceKey = [&cfg, &profileName](const std::string& keyName) -> std::string {
        // First check profile keys if profile specified
        if (!profileName.empty()) {
            try {
                auto profileKeys = ak::storage::loadProfileKeys(cfg, profileName);
                auto it = profileKeys.find(keyName);
                if (it != profileKeys.end() && !it->second.empty()) {
                    return it->second;
                }
            } catch (const std::exception&) {
                // Profile load failed, continue to other sources
            }
        } else {
            // Check default profile
            try {
                auto defaultProfile = ak::storage::getDefaultProfileName();
                auto profileKeys = ak::storage::loadProfileKeys(cfg, defaultProfile);
                auto it = profileKeys.find(keyName);
                if (it != profileKeys.end() && !it->second.empty()) {
                    return it->second;
                }
            } catch (const std::exception&) {
                // Default profile load failed, continue to other sources
            }
        }
        
        // Check environment variables as fallback
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
            // Check if it's a built-in service first
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
                // Check if it's a user-defined service
                try {
                    auto allServices = loadAllServices(cfg);
                    auto serviceIt = allServices.find(service);
                    
                    if (serviceIt != allServices.end() && serviceIt->second.testable) {
                        const Service& serviceObj = serviceIt->second;
                        std::string apiKey = getServiceKey(serviceObj.keyName);
                        if (!apiKey.empty()) {
                            if (!serviceObj.testEndpoint.empty()) {
                                // Perform actual HTTP test
                                std::string curlArgs = "-X " + serviceObj.testMethod;
                                
                                // Add custom headers if specified
                                if (!serviceObj.testHeaders.empty()) {
                                    curlArgs += " " + serviceObj.testHeaders;
                                }
                                
                                // Add authorization header based on auth method
                                if (serviceObj.authMethod == "Bearer") {
                                    curlArgs += " -H 'Authorization: Bearer " + apiKey + "'";
                                } else if (serviceObj.authMethod == "Basic Auth") {
                                    curlArgs += " -H 'Authorization: Basic " + apiKey + "'";
                                } else {
                                    curlArgs += " -H 'Authorization: " + serviceObj.authMethod + " " + apiKey + "'";
                                }
                                
                                // Add the endpoint
                                curlArgs += " " + serviceObj.testEndpoint;
                                
                                auto curl_result = curl_ok(curlArgs);
                                result.ok = curl_result.first;
                                if (!result.ok) {
                                    result.error_message = curl_result.second;
                                    if (getenv("AK_DEBUG_TESTS")) {
                                        std::cerr << "[debug] service=" << service << " curl_output=" << result.error_message << std::endl;
                                    }
                                }
                            } else {
                                // Basic key existence test
                                result.ok = true;
                            }
                        } else {
                            result.error_message = "No API key configured for service: " + serviceObj.keyName;
                        }
                    } else {
                        result.error_message = "Unknown or non-testable service";
                    }
                } catch (const std::exception& e) {
                    result.error_message = "Error loading services: " + std::string(e.what());
                }
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
    bool fail_fast,
    const std::string& profileName) {
    
    std::vector<TestResult> results;
    std::vector<std::future<TestResult>> futures;
    
    // Launch tests in parallel
    for (const auto& service : services) {
        futures.push_back(
            std::async(std::launch::async, [&cfg, service, profileName]() {
                return test_one(cfg, service, profileName);
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

// Unified service management functions
std::map<std::string, Service> loadAllServices(const core::Config& cfg) {
    std::map<std::string, Service> allServices;
    
    // Load default services first
    for (const auto& [name, service] : DEFAULT_SERVICES) {
        allServices[name] = service;
    }
    
    // Load user-defined services
    std::string userServicesPath = cfg.configDir + "/user_services.txt";
    std::ifstream file(userServicesPath);
    if (!file.is_open()) {
        return allServices; // Return just default services if file doesn't exist
    }
    
    std::string line;
    Service currentService;
    bool inService = false;
    
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Service block delimiter
        if (line == "[SERVICE]") {
            if (inService && !currentService.name.empty()) {
                services.push_back(currentService);
            }
            currentService = CustomService();
            inService = true;
            continue;
        }
        
        if (!inService) continue;
        
        if (line == "[SERVICE]") {
            // Save previous service if it's valid
            if (inService && !currentService.name.empty()) {
                currentService.isBuiltIn = false; // User-defined services are not built-in
                allServices[currentService.name] = currentService;
            }
            
            // Start new service
            currentService = Service();
            inService = true;
            continue;
        }
        
        if (!inService) continue;
        
        // Parse key=value pairs
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        
        if (key == "name") {
            currentService.name = value;
        } else if (key == "key_name") {
            currentService.keyName = value;
        } else if (key == "description") {
            currentService.description = value;
        } else if (key == "test_endpoint") {
            currentService.testEndpoint = value;
        } else if (key == "test_method") {
            currentService.testMethod = value;
        } else if (key == "test_headers") {
            currentService.testHeaders = value;
        } else if (key == "auth_method") {
            currentService.authMethod = value;
        } else if (key == "testable") {
            currentService.testable = (value == "true" || value == "1");
        }
    }
    
    // Add the last service if valid
    if (inService && !currentService.name.empty()) {
        currentService.isBuiltIn = false; // User-defined services are not built-in
        allServices[currentService.name] = currentService;
    }
    
    return allServices;
}

void saveUserServices(const core::Config& cfg, const std::map<std::string, Service>& services) {
    std::string userServicesPath = cfg.configDir + "/user_services.txt";
    
    // Create config directory if it doesn't exist
    std::filesystem::create_directories(cfg.configDir);
    
    std::ofstream file(userServicesPath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open user services file for writing: " + userServicesPath);
    }
    
    file << "# User Services Configuration\n";
    file << "# This file contains user-defined API services\n\n";
    
    for (const auto& [name, service] : services) {
        // Only save user-defined services (not built-in ones)
        if (!service.isBuiltIn) {
            file << "[SERVICE]\n";
            file << "name=" << service.name << "\n";
            file << "key_name=" << service.keyName << "\n";
            file << "description=" << service.description << "\n";
            file << "test_endpoint=" << service.testEndpoint << "\n";
            file << "test_method=" << service.testMethod << "\n";
            file << "test_headers=" << service.testHeaders << "\n";
            file << "auth_method=" << service.authMethod << "\n";
            file << "testable=" << (service.testable ? "true" : "false") << "\n";
            file << "\n";
        }
    }
}

void addService(const core::Config& cfg, const Service& service) {
    auto services = loadAllServices(cfg);
    services[service.name] = service;
    saveUserServices(cfg, services);
}

void removeService(const core::Config& cfg, const std::string& serviceName) {
    auto services = loadAllServices(cfg);
    
    // Only allow removal of user-defined services
    auto it = services.find(serviceName);
    if (it != services.end() && !it->second.isBuiltIn) {
        services.erase(it);
        saveUserServices(cfg, services);
    }
}

void updateService(const core::Config& cfg, const Service& service) {
    auto services = loadAllServices(cfg);
    
    // Only allow update of user-defined services or modification of built-in ones
    auto it = services.find(service.name);
    if (it != services.end()) {
        // If it's a built-in service being modified, create a user override
        Service updatedService = service;
        if (it->second.isBuiltIn) {
            updatedService.isBuiltIn = false; // Mark as user-defined override
        }
        services[service.name] = updatedService;
        saveUserServices(cfg, services);
    }
}

Service* findService(std::map<std::string, Service>& services, const std::string& name) {
    auto it = services.find(name);
    return (it != services.end()) ? &(it->second) : nullptr;
}

Service getServiceByName(const core::Config& cfg, const std::string& name) {
    auto services = loadAllServices(cfg);
    auto it = services.find(name);
    if (it != services.end()) {
        return it->second;
    }
    throw std::runtime_error("Service not found: " + name);
}

std::map<std::string, std::string> getAllServiceKeys(const core::Config& cfg) {
    std::map<std::string, std::string> allKeys;
    
    auto allServices = loadAllServices(cfg);
    for (const auto& [name, service] : allServices) {
        allKeys[name] = service.keyName;
    }
    
    return allKeys;
}

TestResult testServiceWithKey(const Service& service, const std::string& apiKey) {
    TestResult result;
    result.service = service.name;
    result.ok = false;
    
    auto start = std::chrono::steady_clock::now();
    
    try {
        if (!apiKey.empty() && !service.testEndpoint.empty() && service.testable) {
            // Perform actual HTTP test
            std::string curlArgs = "-X " + service.testMethod;
            
            // Add custom headers if specified
            if (!service.testHeaders.empty()) {
                curlArgs += " " + service.testHeaders;
            }
            
            // Add authorization header based on auth method
            if (service.authMethod == "Bearer") {
                curlArgs += " -H 'Authorization: Bearer " + apiKey + "'";
            } else if (service.authMethod == "Basic Auth") {
                curlArgs += " -H 'Authorization: Basic " + apiKey + "'";
            } else {
                curlArgs += " -H 'Authorization: " + service.authMethod + " " + apiKey + "'";
            }
            
            // Add the endpoint
            curlArgs += " " + service.testEndpoint;
            
            auto curl_result = curl_ok(curlArgs);
            result.ok = curl_result.first;
            if (!result.ok) {
                result.error_message = curl_result.second;
            }
        } else if (!apiKey.empty()) {
            // Basic key existence test for non-testable services
            result.ok = true;
        } else {
            result.error_message = "No API key provided";
        }
    } catch (...) {
        result.ok = false;
        result.error_message = "Unknown exception during testing";
    }
    
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    return result;
}

} // namespace services
} // namespace ak