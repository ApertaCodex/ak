#pragma once

#include <string>
#include <cstdint>

namespace ak {
namespace crypto {

// Base64 encoding/decoding
std::string base64Encode(const std::string& input);
std::string base64Decode(const std::string& input);

// SHA256 hashing class
class SHA256 {
public:
    SHA256();
    void update(const std::string& str);
    void update(const uint8_t* data, size_t length);
    std::string final();

private:
    uint8_t data[64];
    uint32_t datalen = 0;
    unsigned long long bitlen = 0;
    uint32_t state[8];
    
    static uint32_t ror(uint32_t x, int n);
    static uint32_t ch(uint32_t x, uint32_t y, uint32_t z);
    static uint32_t maj(uint32_t x, uint32_t y, uint32_t z);
    static uint32_t s0(uint32_t x);
    static uint32_t s1(uint32_t x);
    
    void transform();
    void reset();
};

// Utility function for hashing key names
std::string hashKeyName(const std::string& name);

} // namespace crypto
} // namespace ak