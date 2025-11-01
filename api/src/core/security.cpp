#include "cp_api/core/security.hpp"
#include <mbedtls/aes.h>
#include <stdexcept>
#include <random>
#include <algorithm>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace cp_api::security {

    // ---------------- Helper: CSPRNG ----------------
    static void FillRandom(uint8_t* buffer, size_t size)
    {
    #ifdef _WIN32
        if (BCryptGenRandom(nullptr, buffer, static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
            throw std::runtime_error("Failed to generate cryptographic random bytes");
    #else
        int fd = open("/dev/urandom", O_RDONLY);
        if (fd < 0) throw std::runtime_error("Failed to open /dev/urandom");

        size_t total_read = 0;
        while (total_read < size)
        {
            ssize_t read_bytes = read(fd, buffer + total_read, size - total_read);
            if (read_bytes <= 0)
            {
                close(fd);
                throw std::runtime_error("Failed to read enough random bytes from /dev/urandom");
            }
            total_read += static_cast<size_t>(read_bytes);
        }
        close(fd);
    #endif
    }

    // ---------------- Encryption ----------------
    std::vector<uint8_t> EncryptCBC(std::span<const uint8_t> data, const SecurityData& securityData)
    {
        if (data.empty()) return {};

        mbedtls_aes_context aes{};
        mbedtls_aes_init(&aes);
        mbedtls_aes_setkey_enc(&aes, securityData.key.data(), 128);

        // PKCS7 padding
        const size_t block_size = IV_SIZE; // AES block size
        size_t padding = block_size - (data.size() % block_size);
        std::vector<uint8_t> padded(data.size() + padding);
        std::memcpy(padded.data(), data.data(), data.size());
        std::fill(padded.begin() + data.size(), padded.end(), static_cast<uint8_t>(padding));

        std::vector<uint8_t> output(padded.size());
        std::array<uint8_t, IV_SIZE> iv_copy = securityData.iv;

        mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padded.size(), iv_copy.data(), padded.data(), output.data());
        mbedtls_aes_free(&aes);

        // Clear sensitive data
        std::fill(padded.begin(), padded.end(), 0);
        std::fill(iv_copy.begin(), iv_copy.end(), 0);

        return output;
    }

    // ---------------- Decryption ----------------
    std::vector<uint8_t> DecryptCBC(std::span<const uint8_t> encrypted, const SecurityData& securityData)
    {
        if (encrypted.empty() || encrypted.size() % IV_SIZE != 0)
            throw std::runtime_error("Invalid encrypted data size");

        mbedtls_aes_context aes{};
        mbedtls_aes_init(&aes);
        mbedtls_aes_setkey_dec(&aes, securityData.key.data(), 128);

        std::vector<uint8_t> decrypted(encrypted.size());
        std::array<uint8_t, IV_SIZE> iv_copy = securityData.iv;

        try
        {
            mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, encrypted.size(), iv_copy.data(), encrypted.data(), decrypted.data());
        }
        catch (...)
        {
            std::fill(decrypted.begin(), decrypted.end(), 0);
            mbedtls_aes_free(&aes);
            std::fill(iv_copy.begin(), iv_copy.end(), 0);
            throw;
        }

        mbedtls_aes_free(&aes);

        // Remove PKCS7 padding safely
        uint8_t padding = decrypted.back();
        if (padding == 0 || padding > IV_SIZE)
            throw std::runtime_error("Invalid PKCS7 padding");

        for (size_t i = decrypted.size() - padding; i < decrypted.size(); ++i)
        {
            if (decrypted[i] != padding)
                throw std::runtime_error("Invalid PKCS7 padding");
        }

        decrypted.resize(decrypted.size() - padding);

        std::fill(iv_copy.begin(), iv_copy.end(), 0);

        return decrypted;
    }

    // ---------------- Generate Random Key/IV ----------------
    SecurityData GenerateRandomKeyAndIV()
    {
        SecurityData data{};
        FillRandom(data.key.data(), KEY_SIZE);
        FillRandom(data.iv.data(), IV_SIZE);
        return data;
    }

} // namespace cp_api::security
