#include "gtest/gtest.h"
#include "crypto/crypto.hpp"

using namespace ak::crypto;

TEST(Base64Encoding, EmptyString) {
    ASSERT_EQ(base64Encode(""), "");
}

TEST(Base64Encoding, SingleCharacter) {
    ASSERT_EQ(base64Encode("f"), "Zg==");
}

TEST(Base64Encoding, TwoCharacters) {
    ASSERT_EQ(base64Encode("fo"), "Zm8=");
}

TEST(Base64Encoding, ThreeCharacters) {
    ASSERT_EQ(base64Encode("foo"), "Zm9v");
}

TEST(Base64Encoding, LongerString) {
    ASSERT_EQ(base64Encode("foobar"), "Zm9vYmFy");
}

TEST(Base64Encoding, StringWithSpecialCharacters) {
    ASSERT_EQ(base64Encode("Hello, World!"), "SGVsbG8sIFdvcmxkIQ==");
}

TEST(Base64Decoding, EmptyString) {
    ASSERT_EQ(base64Decode(""), "");
}

TEST(Base64Decoding, SingleCharacter) {
    ASSERT_EQ(base64Decode("Zg=="), "f");
}

TEST(Base64Decoding, TwoCharacters) {
    ASSERT_EQ(base64Decode("Zm8="), "fo");
}

TEST(Base64Decoding, ThreeCharacters) {
    ASSERT_EQ(base64Decode("Zm9v"), "foo");
}

TEST(Base64Decoding, LongerString) {
    ASSERT_EQ(base64Decode("Zm9vYmFy"), "foobar");
}

TEST(Base64Decoding, StringWithSpecialCharacters) {
    ASSERT_EQ(base64Decode("SGVsbG8sIFdvcmxkIQ=="), "Hello, World!");
}

TEST(Base64RoundTrip, VariousStringsShouldEncodeAndDecodeCorrectly) {
    std::vector<std::string> testStrings = {
        "",
        "a",
        "ab",
        "abc",
        "abcd",
        "Hello, World!",
        "The quick brown fox jumps over the lazy dog",
        "API_KEY=sk-1234567890abcdef",
        "Special chars: !@#$%^&*()_+-=[]{}|;':\",./<>?"
    };
    
    for (const auto& original : testStrings) {
        std::string encoded = base64Encode(original);
        std::string decoded = base64Decode(encoded);
        ASSERT_EQ(decoded, original);
    }
}

TEST(SHA256Hashing, EmptyString) {
    SHA256 hasher;
    hasher.update("");
    std::string hash = hasher.final();
    ASSERT_EQ(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(SHA256Hashing, SingleCharacter) {
    SHA256 hasher;
    hasher.update("a");
    std::string hash = hasher.final();
    ASSERT_EQ(hash, "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb");
}

TEST(SHA256Hashing, HelloWorld) {
    SHA256 hasher;
    hasher.update("Hello, World!");
    std::string hash = hasher.final();
    ASSERT_EQ(hash, "dffd6021bb2bd5b0af676290809ec3a53191dd81c7f70a4b28688a362182986f");
}

TEST(SHA256Hashing, MultipleUpdatesShouldProduceSameResult) {
    SHA256 hasher1, hasher2;
    
    hasher1.update("Hello, World!");
    
    hasher2.update("Hello, ");
    hasher2.update("World!");
    
    ASSERT_EQ(hasher1.final(), hasher2.final());
}

TEST(HashKeyNameFunction, ShouldProduceConsistent16CharacterHash) {
    std::string hash1 = hashKeyName("API_KEY");
    std::string hash2 = hashKeyName("API_KEY");
    
    ASSERT_EQ(hash1, hash2);
    ASSERT_EQ(hash1.length(), 16u);
}

TEST(HashKeyNameFunction, DifferentKeysShouldProduceDifferentHashes) {
    std::string hash1 = hashKeyName("API_KEY");
    std::string hash2 = hashKeyName("SECRET_KEY");
    
    ASSERT_NE(hash1, hash2);
}

TEST(HashKeyNameFunction, ShouldOnlyContainHexCharacters) {
    std::string hash = hashKeyName("TEST_KEY");
    
    for (char c : hash) {
        ASSERT_TRUE(((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')));
    }
}