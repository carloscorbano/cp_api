#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <span>
#include <limits>

namespace cp_api::compression {

    /**
     * @brief Comprime os dados fornecidos usando zlib, armazenando também o tamanho original.
     *
     * O formato do buffer resultante será:
     * [8 bytes: tamanho_original (uint64_t)] [dados comprimidos]
     *
     * @param data Vetor de bytes a ser comprimido.
     * @param level Nível de compressão (1 = mais rápido, 9 = melhor compressão, padrão: Z_BEST_SPEED).
     * @return std::vector<uint8_t> Vetor contendo os dados comprimidos, incluindo cabeçalho com o tamanho original.
     *
     * @note Em caso de erro, retorna um vetor vazio.
     */
    [[nodiscard]] std::vector<uint8_t> CompressData(std::span<const uint8_t> data, int level = 1);

    /**
     * @brief Descomprime dados comprimidos pelo CompressData().
     *
     * A função lê o tamanho original do cabeçalho do buffer comprimido e reconstrói os dados originais.
     *
     * @param compressedData Vetor contendo os dados comprimidos com cabeçalho.
     * @param maxAllowedSize Limite máximo de bytes a descomprimir (padrão: 4 GB) para evitar ataques de decompression bomb.
     * @return std::vector<uint8_t> Vetor com os dados descomprimidos.
     *
     * @note Em caso de erro ou dados inválidos, retorna um vetor vazio.
     */
    [[nodiscard]] std::vector<uint8_t> UncompressData(std::span<const uint8_t> compressedData,
                                                      uint64_t maxAllowedSize = 4ULL * 1024 * 1024 * 1024);
} // namespace cp_api
