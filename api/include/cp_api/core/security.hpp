#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <span>

namespace cp_api::security {
    // ---------------- AES Constants ----------------
    constexpr size_t KEY_SIZE = 16; // AES-128 key size in bytes
    constexpr size_t IV_SIZE  = 16; // AES block size in bytes

    struct SecurityData
    {
        std::array<uint8_t, KEY_SIZE> key{};
        std::array<uint8_t, IV_SIZE> iv{};
    };

    std::vector<uint8_t> EncryptCBC(std::span<const uint8_t> data, const SecurityData& securityData);
    std::vector<uint8_t> DecryptCBC(std::span<const uint8_t> encrypted, const SecurityData& securityData);
    SecurityData GenerateRandomKeyAndIV();
} // namespace cp_api
