#include "cp_api/core/compression.hpp"
#include "cp_api/core/debug.hpp"
#include <zlib.h>
#include <cstring>
#include <bit> // C++20 para std::endian

#if defined(_WIN32)
    #include <windows.h>
#endif

namespace cp_api::compression {

    // Função para conversão portável de endian
    inline uint64_t to_little_endian(uint64_t x)
    {
        if constexpr (std::endian::native == std::endian::little) return x;
        else return _byteswap_uint64(x);
    }

    inline uint64_t from_little_endian(uint64_t x)
    {
        if constexpr (std::endian::native == std::endian::little) return x;
        else return _byteswap_uint64(x);
    }

    [[nodiscard]] std::vector<uint8_t> CompressData(std::span<const uint8_t> data, int level)
    {
        if (data.empty())
        {
            CP_LOG_WARN("CompressData recebeu vetor vazio.");
            return {};
        }

        if (level < Z_NO_COMPRESSION || level > Z_BEST_COMPRESSION)
        {
            CP_LOG_WARN("Nível de compressão inválido: {}. Usando Z_BEST_SPEED.", level);
            level = Z_BEST_SPEED;
        }

        uLongf maxCompressedSize = compressBound(static_cast<uLong>(data.size()));
        static constexpr size_t HEADER_SIZE = sizeof(uint64_t);

        std::vector<uint8_t> compressed;
        compressed.resize(HEADER_SIZE + maxCompressedSize);

        // Cabeçalho com tamanho original (little-endian)
        uint64_t originalSizeLE = to_little_endian(static_cast<uint64_t>(data.size()));
        std::memcpy(compressed.data(), &originalSizeLE, HEADER_SIZE);

        uLongf compressedSize = maxCompressedSize;
        int res = compress2(compressed.data() + HEADER_SIZE, &compressedSize,
                            data.data(), static_cast<uLong>(data.size()), level);

        if (res != Z_OK)
        {
            CP_LOG_ERROR("Falha na compressão zlib: {}", zError(res));
            return {};
        }

        compressed.resize(HEADER_SIZE + compressedSize);

        return compressed;
    }

    [[nodiscard]] std::vector<uint8_t> UncompressData(std::span<const uint8_t> compressedData, uint64_t maxAllowedSize)
    {
        static constexpr size_t HEADER_SIZE = sizeof(uint64_t);

        if (compressedData.size() < HEADER_SIZE)
        {
            CP_LOG_ERROR("Dados comprimidos inválidos: tamanho menor que cabeçalho.");
            return {};
        }

        uint64_t originalSizeLE = 0;
        std::memcpy(&originalSizeLE, compressedData.data(), HEADER_SIZE);
        uint64_t originalSize = from_little_endian(originalSizeLE);

        if (originalSize == 0 || originalSize > maxAllowedSize)
        {
            CP_LOG_ERROR("Tamanho original inválido ou suspeito: {}", originalSize);
            return {};
        }

        // Prevenção contra underflow
        size_t compressedPayloadSize = 0;
        if (compressedData.size() >= HEADER_SIZE)
            compressedPayloadSize = compressedData.size() - HEADER_SIZE;
        else
        {
            CP_LOG_ERROR("Underflow ao calcular tamanho do payload comprimido.");
            return {};
        }

        std::vector<uint8_t> decompressed(static_cast<size_t>(originalSize));
        uLongf destLen = static_cast<uLongf>(originalSize);

        int res = uncompress(decompressed.data(), &destLen,
                             compressedData.data() + HEADER_SIZE,
                             static_cast<uLong>(compressedPayloadSize));

        if (res != Z_OK)
        {
            CP_LOG_ERROR("Falha na descompressão zlib: {}", zError(res));
            return {};
        }

        return decompressed;
    }

} // namespace cp_api::compression
