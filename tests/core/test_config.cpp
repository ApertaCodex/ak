#include "gtest/gtest.h"
#include "core/config.hpp"

using namespace ak::core;

TEST(StringUtilityFunctions, trimFunction) {
    ASSERT_EQ(trim(""), "");
    ASSERT_EQ(trim("hello"), "hello");
    ASSERT_EQ(trim("  hello  "), "hello");
    ASSERT_EQ(trim("\t\nhello\t\n"), "hello");
    ASSERT_EQ(trim("   "), "");
    ASSERT_EQ(trim("hello world"), "hello world");
    ASSERT_EQ(trim("  hello world  "), "hello world");
}

TEST(StringUtilityFunctions, toLowerFunction) {
    ASSERT_EQ(toLower(""), "");
    ASSERT_EQ(toLower("hello"), "hello");
    ASSERT_EQ(toLower("HELLO"), "hello");
    ASSERT_EQ(toLower("Hello World"), "hello world");
    ASSERT_EQ(toLower("API_KEY"), "api_key");
    ASSERT_EQ(toLower("MixedCase123"), "mixedcase123");
}

TEST(StringUtilityFunctions, icontainsFunction) {
    ASSERT_TRUE(icontains("hello world", "world"));
    ASSERT_TRUE(icontains("hello world", "WORLD"));
    ASSERT_TRUE(icontains("HELLO WORLD", "world"));
    ASSERT_FALSE(icontains("hello world", "xyz"));
    ASSERT_TRUE(icontains("", ""));
    ASSERT_TRUE(icontains("hello", ""));
    ASSERT_FALSE(icontains("", "hello"));
    ASSERT_TRUE(icontains("API_KEY", "api"));
    ASSERT_TRUE(icontains("API_KEY", "KEY"));
}

TEST(MaskValueFunction, EmptyValue) {
    ASSERT_EQ(maskValue(""), "(empty)");
}

TEST(MaskValueFunction, ShortValuesGetFullyMasked) {
    ASSERT_EQ(maskValue("a"), "*");
    ASSERT_EQ(maskValue("ab"), "**");
    ASSERT_EQ(maskValue("abc"), "***");
    ASSERT_EQ(maskValue("abcd"), "****");
    ASSERT_EQ(maskValue("abcdefgh"), "********");
    ASSERT_EQ(maskValue("abcdefghi"), "*********");
    ASSERT_EQ(maskValue("abcdefghij"), "**********");
    ASSERT_EQ(maskValue("abcdefghijk"), "***********");
    ASSERT_EQ(maskValue("abcdefghijkl"), "************");
}

TEST(MaskValueFunction, LongValuesGetPrefixAndSuffix) {
    // 13 characters: 8 prefix + 4 suffix + 1 in between = 13, so this should show prefix***suffix
    ASSERT_EQ(maskValue("abcdefghijklm"), "abcdefgh***jklm");
    ASSERT_EQ(maskValue("sk-1234567890abcdef"), "sk-12345***cdef");
    ASSERT_EQ(maskValue("very_long_api_key_here"), "very_lon***here");
}

TEST(MaskValueFunction, ApiKeyPatterns) {
    std::string apiKey = "sk-abcdefghijklmnopqrstuvwxyz1234567890";
    std::string masked = maskValue(apiKey);
    
    // Should start with first 8 characters
    ASSERT_EQ(masked.substr(0, 8), "sk-abcde");
    // Should end with last 4 characters
    ASSERT_EQ(masked.substr(masked.length() - 4), "7890");
    // Should contain *** in the middle
    ASSERT_NE(masked.find("***"), std::string::npos);
}

TEST(ConfigStructure, ConfigInitialization) {
    Config cfg;
    
    // Default values should be set
    ASSERT_FALSE(cfg.gpgAvailable);
    ASSERT_FALSE(cfg.json);
    ASSERT_FALSE(cfg.forcePlain);
    ASSERT_TRUE(cfg.presetPassphrase.empty());
    ASSERT_TRUE(cfg.configDir.empty());
    ASSERT_TRUE(cfg.vaultPath.empty());
    ASSERT_TRUE(cfg.profilesDir.empty());
    ASSERT_TRUE(cfg.auditLogPath.empty());
    ASSERT_TRUE(cfg.instanceId.empty());
    ASSERT_TRUE(cfg.persistDir.empty());
}

TEST(KeyStoreStructure, KeyStoreInitialization) {
    KeyStore ks;
    ASSERT_TRUE(ks.kv.empty());
}

TEST(KeyStoreStructure, KeyStoreOperations) {
    KeyStore ks;
    
    ks.kv["API_KEY"] = "secret-value";
    ks.kv["DB_URL"] = "postgres://user:pass@host:5432/db";
    
    ASSERT_EQ(ks.kv.size(), 2u);
    ASSERT_EQ(ks.kv["API_KEY"], "secret-value");
    ASSERT_EQ(ks.kv["DB_URL"], "postgres://user:pass@host:5432/db");
    ASSERT_EQ(ks.kv.find("NONEXISTENT"), ks.kv.end());
}

TEST(CommandExistsFunction, ShouldDetectExistingCommands) {
    // These commands should exist on most systems
    ASSERT_TRUE(commandExists("ls"));
    ASSERT_TRUE(commandExists("echo"));
}

TEST(CommandExistsFunction, ShouldNotDetectNonExistingCommands) {
    ASSERT_FALSE(commandExists("nonexistent_command_12345"));
}

TEST(GetenvsFunction, ShouldReturnDefaultForNonExistentVariable) {
    ASSERT_EQ(getenvs("NONEXISTENT_VAR_12345"), "");
    ASSERT_EQ(getenvs("NONEXISTENT_VAR_12345", "default"), "default");
}

TEST(GetenvsFunction, ShouldReturnActualValueForExistingVariable) {
    // PATH should exist on all systems
    std::string path = getenvs("PATH");
    ASSERT_FALSE(path.empty());
    
    // Set a test variable (this won't actually set it in environment for this test)
    // but we can test the function logic with existing env vars
    std::string user = getenvs("USER", "unknown");
    // USER might exist on Unix systems, but if not, should return "unknown"
}