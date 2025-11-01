#pragma once
#include <string>
#include <cstdint>
#include <span>
#include <vector>
#include <string_view>

namespace cp_api::algorithm {

    class MD5 
    {
    public:
        MD5();
        void update(const unsigned char* input, size_t length);
        void update(const std::string& input);
        void finalize();
        std::string hexdigest();

        // 🔥 Novo: método utilitário para uso direto
        static MD5 Compute(std::span<const uint8_t> data);
        static MD5 Compute(std::string_view text);

    private:
        void transform(const unsigned char block[64]);
        static void encode(const uint32_t* input, unsigned char* output, size_t length);
        static void decode(const unsigned char* input, uint32_t* output, size_t length);

        bool finalized;
        unsigned char buffer[64];
        uint32_t count[2];
        uint32_t state[4];
        unsigned char digest[16];
    };

    namespace Hex {
        std::string ToHexString(std::span<const uint8_t> data, bool uppercase=false, bool prefix=false);
        std::string ToHexString(const std::vector<uint8_t>& data, bool uppercase=false, bool prefix=false);
        std::vector<uint8_t> FromHexString(std::string_view hex);
        std::vector<uint8_t> FromHexStringPrefixed(std::string_view hex);
    }

    namespace Base64 {
        std::string Base64Encode(std::span<const uint8_t> bytes);
        std::string Base64Encode(const std::vector<uint8_t>& bytes);
        std::string Base64Encode(std::string_view text);
        std::string Base64EncodeUrlSafe(std::span<const uint8_t> bytes);
        std::string Base64EncodeUrlSafe(std::string_view text);
        std::vector<uint8_t> Base64Decode(std::string_view encoded);
        std::vector<uint8_t> Base64DecodeUrlSafe(std::string_view encoded);
    }

} // namespace cp_api::algorithm
