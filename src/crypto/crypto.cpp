#include "crypto/crypto.hpp"
#include <sstream>
#include <iomanip>
#include <cstring>

namespace ak {
namespace crypto {

// Base64 encoding
std::string base64Encode(const std::string& input) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);
    
    unsigned char a3[3];
    for (size_t pos = 0; pos < input.size();) {
        int len = 0;
        for (; len < 3 && pos < input.size(); ++len) {
            a3[len] = static_cast<unsigned char>(input[pos++]);
        }
        if (len < 3) {
            for (int j = len; j < 3; ++j) {
                a3[j] = 0;
            }
        }
        
        unsigned int triple = (a3[0] << 16) | (a3[1] << 8) | a3[2];
        output.push_back(table[(triple >> 18) & 0x3F]);
        output.push_back(table[(triple >> 12) & 0x3F]);
        output.push_back(len >= 2 ? table[(triple >> 6) & 0x3F] : '=');
        output.push_back(len >= 3 ? table[triple & 0x3F] : '=');
    }
    
    return output;
}

// Base64 decoding
std::string base64Decode(const std::string& input) {
    auto val = [&](char c) -> int {
        if ('A' <= c && c <= 'Z') return c - 'A';
        if ('a' <= c && c <= 'z') return c - 'a' + 26;
        if ('0' <= c && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    
    std::string output;
    output.reserve((input.size() / 4) * 3);
    
    int accum = 0, bits = 0;
    for (char c : input) {
        if (c == '=') break;
        int v = val(c);
        if (v < 0) continue;
        
        accum = (accum << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            output.push_back(static_cast<char>((accum >> bits) & 0xFF));
        }
    }
    
    return output;
}

// SHA256 implementation
SHA256::SHA256() {
    reset();
}

void SHA256::update(const std::string& str) {
    update(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

void SHA256::update(const uint8_t* data_ptr, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        data[datalen] = data_ptr[i];
        datalen++;
        if (datalen == 64) {
            transform();
            bitlen += 512;
            datalen = 0;
        }
    }
}

std::string SHA256::final() {
    uint32_t i = datalen;
    if (datalen < 56) {
        data[i++] = 0x80;
        while (i < 56) {
            data[i++] = 0x00;
        }
    } else {
        data[i++] = 0x80;
        while (i < 64) {
            data[i++] = 0x00;
        }
        transform();
        std::memset(data, 0, 56);
    }
    
    bitlen += datalen * 8ULL;
    for (int j = 0; j < 8; ++j) {
        data[63 - j] = (bitlen >> (8 * j)) & 0xFF;
    }
    transform();
    
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int j = 0; j < 8; ++j) {
        oss << std::setw(8) << state[j];
    }
    
    reset();
    return oss.str();
}

uint32_t SHA256::ror(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

uint32_t SHA256::ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

uint32_t SHA256::maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

uint32_t SHA256::s0(uint32_t x) {
    return ror(x, 7) ^ ror(x, 18) ^ (x >> 3);
}

uint32_t SHA256::s1(uint32_t x) {
    return ror(x, 17) ^ ror(x, 19) ^ (x >> 10);
}

void SHA256::transform() {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };
    
    uint32_t m[64];
    for (int i = 0, j = 0; i < 16; ++i, j += 4) {
        m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | data[j + 3];
    }
    
    for (int i = 16; i < 64; ++i) {
        m[i] = s1(m[i - 2]) + m[i - 7] + s0(m[i - 15]) + m[i - 16];
    }
    
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    
    for (int i = 0; i < 64; ++i) {
        uint32_t t1 = h + (ror(e, 6) ^ ror(e, 11) ^ ror(e, 25)) + ch(e, f, g) + K[i] + m[i];
        uint32_t t2 = (ror(a, 2) ^ ror(a, 13) ^ ror(a, 22)) + maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void SHA256::reset() {
    datalen = 0;
    bitlen = 0;
    state[0] = 0x6a09e667;
    state[1] = 0xbb67ae85;
    state[2] = 0x3c6ef372;
    state[3] = 0xa54ff53a;
    state[4] = 0x510e527f;
    state[5] = 0x9b05688c;
    state[6] = 0x1f83d9ab;
    state[7] = 0x5be0cd19;
}

// Utility function for hashing key names
std::string hashKeyName(const std::string& name) {
    SHA256 hash;
    hash.update(name);
    return hash.final().substr(0, 16);
}

} // namespace crypto
} // namespace ak