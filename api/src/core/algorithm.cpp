#include "cp_api/core/algorithm.hpp"
#include <sstream>
#include <iomanip>
#include <charconv>
#include <cctype>
#include <array>
#include <cstring>
#include <algorithm>

namespace cp_api::algorithm {

    // ---- MD5 internals ----
    #define F(x, y, z) ((x & y) | (~x & z))
    #define G(x, y, z) ((x & z) | (y & ~z))
    #define H(x, y, z) (x ^ y ^ z)
    #define I(x, y, z) (y ^ (x | ~z))
    #define ROTATE_LEFT(x, n) ((x << n) | (x >> (32 - n)))

    #define FF(a,b,c,d,x,s,ac) { (a)+=F((b),(c),(d))+(x)+(uint32_t)(ac); (a)=ROTATE_LEFT((a),(s)); (a)+=(b); }
    #define GG(a,b,c,d,x,s,ac) { (a)+=G((b),(c),(d))+(x)+(uint32_t)(ac); (a)=ROTATE_LEFT((a),(s)); (a)+=(b); }
    #define HH(a,b,c,d,x,s,ac) { (a)+=H((b),(c),(d))+(x)+(uint32_t)(ac); (a)=ROTATE_LEFT((a),(s)); (a)+=(b); }
    #define II(a,b,c,d,x,s,ac) { (a)+=I((b),(c),(d))+(x)+(uint32_t)(ac); (a)=ROTATE_LEFT((a),(s)); (a)+=(b); }

    static const unsigned char PADDING[64] = { 0x80 };

    MD5::MD5() 
        : finalized(false), buffer{0}, count{0,0},
          state{0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476}, digest{0}
    {}

    void MD5::update(const unsigned char* input, size_t length) {
        size_t index = (count[0] >> 3) & 0x3F;
        count[0] += static_cast<uint32_t>(length << 3);
        if (count[0] < (length << 3)) count[1]++;
        count[1] += static_cast<uint32_t>(length >> 29);

        size_t partLen = 64 - index;
        size_t i = 0;

        if (length >= partLen) {
            std::copy_n(input, partLen, buffer + index);
            transform(buffer);
            for (i = partLen; i + 63 < length; i += 64)
                transform(input + i);
            index = 0;
        }

        std::copy_n(input + i, length - i, buffer + index);
    }

    void MD5::update(const std::string& input) {
        update(reinterpret_cast<const unsigned char*>(input.data()), input.size());
    }

    void MD5::finalize() {
        if (finalized) return;

        unsigned char bits[8];
        encode(count, bits, 8);

        unsigned int index = (count[0] >> 3) & 0x3f;
        unsigned int padLen = (index < 56) ? (56 - index) : (120 - index);
        update(PADDING, padLen);
        update(bits, 8);
        encode(state, digest, 16);

        std::fill(std::begin(buffer), std::end(buffer), 0);
        std::fill(std::begin(count), std::end(count), 0);

        finalized = true;
    }

    void MD5::transform(const unsigned char block[64]) {
        uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];

        // Decodifica o bloco de 64 bytes em 16 palavras de 32 bits
        decode(block, x, 64);

        // ===== Rodada 1 =====
        FF(a, b, c, d, x[ 0], 7, 0xd76aa478);
        FF(d, a, b, c, x[ 1], 12, 0xe8c7b756);
        FF(c, d, a, b, x[ 2], 17, 0x242070db);
        FF(b, c, d, a, x[ 3], 22, 0xc1bdceee);
        FF(a, b, c, d, x[ 4], 7, 0xf57c0faf);
        FF(d, a, b, c, x[ 5], 12, 0x4787c62a);
        FF(c, d, a, b, x[ 6], 17, 0xa8304613);
        FF(b, c, d, a, x[ 7], 22, 0xfd469501);
        FF(a, b, c, d, x[ 8], 7, 0x698098d8);
        FF(d, a, b, c, x[ 9], 12, 0x8b44f7af);
        FF(c, d, a, b, x[10], 17, 0xffff5bb1);
        FF(b, c, d, a, x[11], 22, 0x895cd7be);
        FF(a, b, c, d, x[12], 7, 0x6b901122);
        FF(d, a, b, c, x[13], 12, 0xfd987193);
        FF(c, d, a, b, x[14], 17, 0xa679438e);
        FF(b, c, d, a, x[15], 22, 0x49b40821);

        // ===== Rodada 2 =====
        GG(a, b, c, d, x[ 1], 5, 0xf61e2562);
        GG(d, a, b, c, x[ 6], 9, 0xc040b340);
        GG(c, d, a, b, x[11], 14, 0x265e5a51);
        GG(b, c, d, a, x[ 0], 20, 0xe9b6c7aa);
        GG(a, b, c, d, x[ 5], 5, 0xd62f105d);
        GG(d, a, b, c, x[10], 9, 0x02441453);
        GG(c, d, a, b, x[15], 14, 0xd8a1e681);
        GG(b, c, d, a, x[ 4], 20, 0xe7d3fbc8);
        GG(a, b, c, d, x[ 9], 5, 0x21e1cde6);
        GG(d, a, b, c, x[14], 9, 0xc33707d6);
        GG(c, d, a, b, x[ 3], 14, 0xf4d50d87);
        GG(b, c, d, a, x[ 8], 20, 0x455a14ed);
        GG(a, b, c, d, x[13], 5, 0xa9e3e905);
        GG(d, a, b, c, x[ 2], 9, 0xfcefa3f8);
        GG(c, d, a, b, x[ 7], 14, 0x676f02d9);
        GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);

        // ===== Rodada 3 =====
        HH(a, b, c, d, x[ 5], 4, 0xfffa3942);
        HH(d, a, b, c, x[ 8], 11, 0x8771f681);
        HH(c, d, a, b, x[11], 16, 0x6d9d6122);
        HH(b, c, d, a, x[14], 23, 0xfde5380c);
        HH(a, b, c, d, x[ 1], 4, 0xa4beea44);
        HH(d, a, b, c, x[ 4], 11, 0x4bdecfa9);
        HH(c, d, a, b, x[ 7], 16, 0xf6bb4b60);
        HH(b, c, d, a, x[10], 23, 0xbebfbc70);
        HH(a, b, c, d, x[13], 4, 0x289b7ec6);
        HH(d, a, b, c, x[ 0], 11, 0xeaa127fa);
        HH(c, d, a, b, x[ 3], 16, 0xd4ef3085);
        HH(b, c, d, a, x[ 6], 23, 0x04881d05);
        HH(a, b, c, d, x[ 9], 4, 0xd9d4d039);
        HH(d, a, b, c, x[12], 11, 0xe6db99e5);
        HH(c, d, a, b, x[15], 16, 0x1fa27cf8);
        HH(b, c, d, a, x[ 2], 23, 0xc4ac5665);

        // ===== Rodada 4 =====
        II(a, b, c, d, x[ 0], 6, 0xf4292244);
        II(d, a, b, c, x[ 7], 10, 0x432aff97);
        II(c, d, a, b, x[14], 15, 0xab9423a7);
        II(b, c, d, a, x[ 5], 21, 0xfc93a039);
        II(a, b, c, d, x[12], 6, 0x655b59c3);
        II(d, a, b, c, x[ 3], 10, 0x8f0ccc92);
        II(c, d, a, b, x[10], 15, 0xffeff47d);
        II(b, c, d, a, x[ 1], 21, 0x85845dd1);
        II(a, b, c, d, x[ 8], 6, 0x6fa87e4f);
        II(d, a, b, c, x[15], 10, 0xfe2ce6e0);
        II(c, d, a, b, x[ 6], 15, 0xa3014314);
        II(b, c, d, a, x[13], 21, 0x4e0811a1);
        II(a, b, c, d, x[ 4], 6, 0xf7537e82);
        II(d, a, b, c, x[11], 10, 0xbd3af235);
        II(c, d, a, b, x[ 2], 15, 0x2ad7d2bb);
        II(b, c, d, a, x[ 9], 21, 0xeb86d391);

        // Atualiza o estado interno
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;

        // Limpa o buffer temporário
        std::fill(std::begin(x), std::end(x), 0);
    }


    void MD5::encode(const uint32_t* input, unsigned char* output, size_t length) {
        for (size_t i = 0, j = 0; j < length; i++, j += 4) {
            output[j]   = static_cast<unsigned char>(input[i] & 0xFF);
            output[j+1] = static_cast<unsigned char>((input[i] >> 8) & 0xFF);
            output[j+2] = static_cast<unsigned char>((input[i] >> 16) & 0xFF);
            output[j+3] = static_cast<unsigned char>((input[i] >> 24) & 0xFF);
        }
    }

    void MD5::decode(const unsigned char* input, uint32_t* output, size_t length) {
        for (size_t i = 0, j = 0; j < length; i++, j += 4)
            output[i] = static_cast<uint32_t>(input[j]) |
                        (static_cast<uint32_t>(input[j+1]) << 8) |
                        (static_cast<uint32_t>(input[j+2]) << 16) |
                        (static_cast<uint32_t>(input[j+3]) << 24);
    }

    std::string MD5::hexdigest() {
        if (!finalized) finalize();
        std::string result(32, '0');
        static constexpr char HEX[] = "0123456789abcdef";
        for (int i = 0; i < 16; ++i) {
            result[i*2]   = HEX[(digest[i] >> 4) & 0xF];
            result[i*2+1] = HEX[digest[i] & 0xF];
        }
        return result;
    }

     // ---- Static Compute() helpers ----
    MD5 MD5::Compute(std::span<const uint8_t> data) {
        MD5 md5;
        md5.update(data.data(), data.size());
        md5.finalize();
        return md5;
    }

    MD5 MD5::Compute(std::string_view text) {
        return Compute(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(text.data()), text.size()));
    }

    // ---- Hex ----
    namespace Hex {
        static constexpr char HEX_LOWER[] = "0123456789abcdef";
        static constexpr char HEX_UPPER[] = "0123456789ABCDEF";

        std::string ToHexString(std::span<const uint8_t> data, bool uppercase, bool prefix) {
            std::string out;
            out.reserve(data.size() * (prefix ? 4 : 2));
            const char* table = uppercase ? HEX_UPPER : HEX_LOWER;
            for (uint8_t byte : data) {
                if (prefix) out.append("0x");
                out.push_back(table[byte >> 4]);
                out.push_back(table[byte & 0x0F]);
            }
            return out;
        }

        std::string ToHexString(const std::vector<uint8_t>& data, bool uppercase, bool prefix) {
            return ToHexString(std::span<const uint8_t>(data.data(), data.size()), uppercase, prefix);
        }

        std::vector<uint8_t> FromHexString(std::string_view hex) {
            std::string cleaned;
            cleaned.reserve(hex.size());
            for (char c : hex)
                if (!std::isspace(static_cast<unsigned char>(c)))
                    cleaned.push_back(c);

            if (cleaned.empty() || cleaned.size() % 2 != 0)
                return {};

            std::vector<uint8_t> bytes;
            bytes.reserve(cleaned.size() / 2);
            for (size_t i = 0; i < cleaned.size(); i += 2) {
                uint8_t byte;
                auto [ptr, ec] = std::from_chars(cleaned.data() + i, cleaned.data() + i + 2, byte, 16);
                if (ec != std::errc()) return {};
                bytes.push_back(byte);
            }
            return bytes;
        }

        std::vector<uint8_t> FromHexStringPrefixed(std::string_view hex)
        {
            std::vector<uint8_t> bytes;
            size_t i = 0;
            while (i < hex.size()) {
                while (i < hex.size() && std::isspace(static_cast<unsigned char>(hex[i]))) i++;
                if (i >= hex.size()) break;

                // Optional "0x" prefix
                if (hex[i] == '0' && i + 1 < hex.size() && (hex[i+1] == 'x' || hex[i+1] == 'X')) i += 2;
                if (i + 1 >= hex.size()) return {}; // incomplete byte

                char c1 = hex[i++];
                char c2 = hex[i++];
                if (!std::isxdigit(c1) || !std::isxdigit(c2)) return {}; // invalid hex digit

                int val;
                char buf[3] = {c1, c2, 0};
                if (sscanf_s(buf, "%2x", &val) != 1) return {};
                bytes.push_back(static_cast<uint8_t>(val));
            }
            return bytes;
        }
    }

    // ---- Base64 ----
    namespace Base64 {
        static constexpr char BASE64_ALPHABET[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";

        static constexpr std::array<int,256> BASE64_DECODE_TABLE = [] {
            std::array<int,256> table{};
            table.fill(-1);
            for (int i = 0; i < 64; ++i)
                table[static_cast<unsigned char>(BASE64_ALPHABET[i])] = i;
            return table;
        }();

        static inline std::string base64EncodeImpl(std::span<const uint8_t> bytes,
                                                   const char* alphabet,
                                                   bool with_padding)
        {
            std::string out;
            out.reserve(((bytes.size() + 2) / 3) * 4);
            uint32_t val = 0;
            int valb = -6;
            for (uint8_t c : bytes) {
                val = (val << 8) | c;
                valb += 8;
                while (valb >= 0) {
                    out.push_back(alphabet[(val >> valb) & 0x3F]);
                    valb -= 6;
                }
            }
            if (valb > -6)
                out.push_back(alphabet[((val << 8) >> (valb + 8)) & 0x3F]);
            if (with_padding)
                while (out.size() % 4) out.push_back('=');
            return out;
        }

        static inline std::vector<uint8_t> Base64DecodeImpl(std::string_view input,
                                                            const std::array<int,256>& table)
        {
            std::vector<uint8_t> out;
            out.reserve((input.size() * 3) / 4);
            int val = 0;
            int valb = -8;
            for (unsigned char c : input) {
                if (c == '=') break;
                int d = table[c];
                if (d == -1) {
                    if (!std::isspace(c)) return {}; // fail on invalid
                    continue;
                }
                val = (val << 6) | d;
                valb += 6;
                if (valb >= 0) {
                    out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
                    valb -= 8;
                }
            }
            return out;
        }

        std::string Base64Encode(std::span<const uint8_t> bytes) {
            return base64EncodeImpl(bytes, BASE64_ALPHABET, true);
        }

        std::string Base64Encode(const std::vector<uint8_t>& bytes) {
            return Base64Encode(std::span<const uint8_t>(bytes.data(), bytes.size()));
        }

        std::string Base64Encode(std::string_view text) {
            return Base64Encode(std::span<const uint8_t>(
                reinterpret_cast<const uint8_t*>(text.data()), text.size()));
        }

        std::string Base64EncodeUrlSafe(std::span<const uint8_t> bytes) {
            std::string s = base64EncodeImpl(bytes, BASE64_ALPHABET, true);
            for (char& ch : s) {
                if (ch == '+') ch = '-';
                else if (ch == '/') ch = '_';
            }
            while (!s.empty() && s.back() == '=') s.pop_back();
            return s;
        }

        std::string Base64EncodeUrlSafe(std::string_view text) {
            return Base64EncodeUrlSafe(std::span<const uint8_t>(
                reinterpret_cast<const uint8_t*>(text.data()), text.size()));
        }

        std::vector<uint8_t> Base64Decode(std::string_view encoded) {
            return Base64DecodeImpl(encoded, BASE64_DECODE_TABLE);
        }

        std::vector<uint8_t> Base64DecodeUrlSafe(std::string_view input) {
            std::string s(input);
            for (char& ch : s) {
                if (ch == '-') ch = '+';
                else if (ch == '_') ch = '/';
            }
            while (s.size() % 4) s.push_back('=');
            return Base64DecodeImpl(s, BASE64_DECODE_TABLE);
        }
    }

} // namespace cp_api::algorithm
