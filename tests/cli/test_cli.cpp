#include "gtest/gtest.h"
#include "cli/cli.hpp"
#include <sstream>
#include <iostream>
#include <fstream>

using namespace ak::cli;

TEST(ExpandShortFlagsFunction, EmptyInput) {
    std::vector<std::string> input;
    auto result = expandShortFlags(input);
    ASSERT_TRUE(result.empty());
}

TEST(ExpandShortFlagsFunction, NoShortFlags) {
    std::vector<std::string> input = {"command", "--profile", "prod", "--format", "json"};
    auto result = expandShortFlags(input);
    ASSERT_EQ(result, input);
}

TEST(ExpandShortFlagsFunction, SingleShortFlag) {
    std::vector<std::string> input = {"-p"};
    auto result = expandShortFlags(input);
    
    ASSERT_EQ(result.size(), 1u);
    ASSERT_EQ(result[0], "--profile");
}

TEST(ExpandShortFlagsFunction, MultipleShortFlagsInOneArgument) {
    std::vector<std::string> input = {"-pf"};
    auto result = expandShortFlags(input);
    
    ASSERT_EQ(result.size(), 2u);
    ASSERT_EQ(result[0], "--profile");
    ASSERT_EQ(result[1], "--format");
}

TEST(ExpandShortFlagsFunction, MixedShortAndLongFlags) {
    std::vector<std::string> input = {"command", "-p", "prod", "--json", "-f", "env"};
    auto result = expandShortFlags(input);
    
    std::vector<std::string> expected = {"command", "--profile", "prod", "--json", "--format", "env"};
    ASSERT_EQ(result, expected);
}

TEST(ExpandShortFlagsFunction, ComplexShortFlagCombination) {
    std::vector<std::string> input = {"-pfo", "value"};
    auto result = expandShortFlags(input);
    
    ASSERT_EQ(result.size(), 4u);
    ASSERT_EQ(result[0], "--profile");
    ASSERT_EQ(result[1], "--format");
    ASSERT_EQ(result[2], "--output");
    ASSERT_EQ(result[3], "value");
}

TEST(ExpandShortFlagsFunction, UnknownShortFlagsArePreserved) {
    std::vector<std::string> input = {"-x", "-y"};
    auto result = expandShortFlags(input);
    
    ASSERT_EQ(result.size(), 2u);
    ASSERT_EQ(result[0], "-x");
    ASSERT_EQ(result[1], "-y");
}

TEST(ExpandShortFlagsFunction, HelpAndVersionFlags) {
    std::vector<std::string> input1 = {"-h"};
    auto result1 = expandShortFlags(input1);
    ASSERT_EQ(result1[0], "--help");
    
    std::vector<std::string> input2 = {"-v"};
    auto result2 = expandShortFlags(input2);
    ASSERT_EQ(result2[0], "--version");
}

// Note: We can't easily test the output functions (showLogo, showTips, cmd_help, etc.)
// without capturing stdout, but we can test that they don't crash
 
TEST(HelpFunctions, ShowLogoDoesNotCrash) {
    // Redirect cout to avoid cluttering test output
    std::ostringstream buffer;
    std::streambuf* orig = std::cout.rdbuf(buffer.rdbuf());
    
    ASSERT_NO_FATAL_FAILURE(showLogo());
    
    std::cout.rdbuf(orig);
    
    // Check that something was output
    std::string output = buffer.str();
    ASSERT_FALSE(output.empty());
}

TEST(HelpFunctions, ShowTipsDoesNotCrash) {
    std::ostringstream buffer;
    std::streambuf* orig = std::cout.rdbuf(buffer.rdbuf());
    
    ASSERT_NO_FATAL_FAILURE(showTips());
    
    std::cout.rdbuf(orig);
    
    std::string output = buffer.str();
    ASSERT_FALSE(output.empty());
    ASSERT_NE(output.find("Tips"), std::string::npos);
}

TEST(HelpFunctions, CmdHelpDoesNotCrash) {
    std::ostringstream buffer;
    std::streambuf* orig = std::cout.rdbuf(buffer.rdbuf());
    
    ASSERT_NO_FATAL_FAILURE(cmd_help());
    
    std::cout.rdbuf(orig);
    
    std::string output = buffer.str();
    ASSERT_FALSE(output.empty());
    ASSERT_NE(output.find("USAGE"), std::string::npos);
}

TEST(HelpFunctions, ShowWelcomeDoesNotCrash) {
    std::ostringstream buffer;
    std::streambuf* orig = std::cout.rdbuf(buffer.rdbuf());
    
    ASSERT_NO_FATAL_FAILURE(showWelcome());
    
    std::cout.rdbuf(orig);
    
    std::string output = buffer.str();
    ASSERT_FALSE(output.empty());
}

TEST(CompletionGeneration, GenerateBashCompletionDoesNotCrash) {
    std::ostringstream buffer;
    std::streambuf* orig = std::cout.rdbuf(buffer.rdbuf());
    
    ASSERT_NO_FATAL_FAILURE(generateBashCompletion());
    
    std::cout.rdbuf(orig);
    
    std::string output = buffer.str();
    ASSERT_FALSE(output.empty());
    ASSERT_NE(output.find("bash"), std::string::npos);
}

TEST(CompletionGeneration, GenerateZshCompletionDoesNotCrash) {
    std::ostringstream buffer;
    std::streambuf* orig = std::cout.rdbuf(buffer.rdbuf());
    
    ASSERT_NO_FATAL_FAILURE(generateZshCompletion());
    
    std::cout.rdbuf(orig);
    
    std::string output = buffer.str();
    ASSERT_FALSE(output.empty());
}

TEST(CompletionGeneration, GenerateFishCompletionDoesNotCrash) {
    std::ostringstream buffer;
    std::streambuf* orig = std::cout.rdbuf(buffer.rdbuf());
    
    ASSERT_NO_FATAL_FAILURE(generateFishCompletion());
    
    std::cout.rdbuf(orig);
    
    std::string output = buffer.str();
    ASSERT_FALSE(output.empty());
}

TEST(CompletionFileWriters, WriteBashCompletionToFileDoesNotCrash) {
    std::string tempPath = "/tmp/test_bash_completion";
    ASSERT_NO_FATAL_FAILURE(writeBashCompletionToFile(tempPath));
    
    // Check that file was created
    std::ifstream file(tempPath);
    ASSERT_TRUE(file.good());
    file.close();
    
    // Clean up
    std::remove(tempPath.c_str());
}

TEST(CompletionFileWriters, WriteZshCompletionToFileDoesNotCrash) {
    std::string tempPath = "/tmp/test_zsh_completion";
    ASSERT_NO_FATAL_FAILURE(writeZshCompletionToFile(tempPath));
    
    std::ifstream file(tempPath);
    ASSERT_TRUE(file.good());
    file.close();
    
    std::remove(tempPath.c_str());
}

TEST(CompletionFileWriters, WriteFishCompletionToFileDoesNotCrash) {
    std::string tempPath = "/tmp/test_fish_completion";
    ASSERT_NO_FATAL_FAILURE(writeFishCompletionToFile(tempPath));
    
    std::ifstream file(tempPath);
    ASSERT_TRUE(file.good());
    file.close();
    
    std::remove(tempPath.c_str());
}