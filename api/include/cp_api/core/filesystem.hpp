#pragma once
#include <filesystem>
#include <memory>
#include <span>
#include <cstdint>

namespace cp_api::filesystem {
    class MMapFile {
    public:
        MMapFile() = default;
        ~MMapFile() noexcept;
        MMapFile(const MMapFile&) = delete;
        MMapFile& operator=(const MMapFile&) = delete;
        MMapFile(MMapFile&& other) noexcept;
        MMapFile& operator=(MMapFile&& other) noexcept;

        bool open(const std::filesystem::path& filepath) noexcept;
        void release() noexcept;

        [[nodiscard]] void* data() const noexcept { return m_data; }
        [[nodiscard]] size_t size() const noexcept { return m_size; }

    private:
    #ifdef _WIN32
        void* m_data = nullptr;
        void* m_handle = nullptr;
        void* m_mapHandle = nullptr;
    #else
        void* m_data = nullptr;
        int m_fd = -1;
    #endif
        size_t m_size = 0;
    };

    // Path utilities
    std::filesystem::path NormalizePath(const std::filesystem::path& path) noexcept;
    void SetGamePath(const std::filesystem::path& path);
    std::filesystem::path GetGamePath();

    // File operations
    std::shared_ptr<uint8_t[]> ReadBytes(const std::filesystem::path& path, size_t& outSize);
    std::pair<std::shared_ptr<uint8_t[]>, std::span<const uint8_t>> ReadBytesAuto(const std::filesystem::path& path);
    void WriteBytes(const std::filesystem::path& path, std::span<const uint8_t> data, bool append = false);
    bool FileExists(const std::filesystem::path& path) noexcept;
    bool DeleteFileSafe(const std::filesystem::path& path) noexcept;
}
