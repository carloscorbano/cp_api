#include "cp_api/core/filesystem.hpp"
#include <fstream>
#include <vector>
#include <mutex>
#include <cstring>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

namespace cp_api::filesystem {

    namespace {
        std::filesystem::path GamePath;
        std::mutex GamePathMutex;
        size_t ReadBytesAutoThreshold = 1 * 1024 * 1024;
    }

    std::filesystem::path NormalizePath(const std::filesystem::path& path) noexcept {
        std::error_code ec;
        auto normalized = std::filesystem::weakly_canonical(path, ec);
        if (ec) normalized = std::filesystem::path(path);
        return normalized.make_preferred();
    }

    void SetGamePath(const std::filesystem::path& path) {
        std::scoped_lock lock(GamePathMutex);
        GamePath = NormalizePath(path);
    }

    std::filesystem::path GetGamePath() {
        std::scoped_lock lock(GamePathMutex);
        return GamePath;
    }

    MMapFile::~MMapFile() noexcept { release(); }

    void MMapFile::release() noexcept {
    #ifdef _WIN32
        if (m_data) UnmapViewOfFile(m_data);
        if (m_mapHandle) CloseHandle(m_mapHandle);
        if (m_handle) CloseHandle(m_handle);
    #else
        if (m_data) munmap(m_data, m_size);
        if (m_fd >= 0) close(m_fd);
    #endif
        m_data = nullptr;
        m_size = 0;
    #ifdef _WIN32
        m_handle = nullptr;
        m_mapHandle = nullptr;
    #else
        m_fd = -1;
    #endif
    }

    MMapFile::MMapFile(MMapFile&& other) noexcept { *this = std::move(other); }

    MMapFile& MMapFile::operator=(MMapFile&& other) noexcept {
        if (this != &other) {
            release();
            m_data = other.m_data;
            m_size = other.m_size;
    #ifdef _WIN32
            m_handle = other.m_handle;
            m_mapHandle = other.m_mapHandle;
            other.m_handle = nullptr;
            other.m_mapHandle = nullptr;
    #else
            m_fd = other.m_fd;
            other.m_fd = -1;
    #endif
            other.m_data = nullptr;
            other.m_size = 0;
        }
        return *this;
    }

    bool MMapFile::open(const std::filesystem::path& filepath) noexcept {
        release();
    #ifdef _WIN32
        m_handle = CreateFileW(filepath.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (m_handle == INVALID_HANDLE_VALUE) return false;

        LARGE_INTEGER size;
        if (!GetFileSizeEx(m_handle, &size)) {
            CloseHandle(m_handle);
            m_handle = nullptr;
            return false;
        }

        m_mapHandle = CreateFileMappingW(m_handle, NULL, PAGE_READONLY, 0, 0, NULL);
        if (!m_mapHandle) {
            CloseHandle(m_handle);
            m_handle = nullptr;
            return false;
        }

        m_data = MapViewOfFile(m_mapHandle, FILE_MAP_READ, 0, 0, 0);
        if (!m_data) {
            CloseHandle(m_mapHandle);
            CloseHandle(m_handle);
            m_mapHandle = nullptr;
            m_handle = nullptr;
            return false;
        }

        m_size = static_cast<size_t>(size.QuadPart);
        return true;
    #else
        m_fd = open(filepath.string().c_str(), O_RDONLY);
        if (m_fd < 0) return false;

        off_t size = lseek(m_fd, 0, SEEK_END);
        if (size == (off_t)-1) {
            close(m_fd);
            m_fd = -1;
            return false;
        }

        m_data = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, m_fd, 0);
        if (m_data == MAP_FAILED) {
            close(m_fd);
            m_fd = -1;
            m_data = nullptr;
            return false;
        }

        m_size = static_cast<size_t>(size);
        return true;
    #endif
    }

    std::shared_ptr<uint8_t[]> ReadBytes(const std::filesystem::path& path, size_t& outSize) {
        auto file = NormalizePath(path);
        std::ifstream in(file, std::ios::binary | std::ios::ate);
        if (!in) throw std::runtime_error("Failed to open file: " + file.string());

        std::streamsize size = in.tellg();
        in.seekg(0, std::ios::beg);

        outSize = static_cast<size_t>(size);
        auto buffer = std::shared_ptr<uint8_t[]>(new uint8_t[outSize]);
        if (!in.read(reinterpret_cast<char*>(buffer.get()), size))
            throw std::runtime_error("Failed to read file: " + file.string());

        return buffer;
    }

    std::pair<std::shared_ptr<uint8_t[]>, std::span<const uint8_t>> ReadBytesAuto(const std::filesystem::path& path) {
        auto file = NormalizePath(path);
        size_t fileSize = std::filesystem::file_size(file);

        if (fileSize > ReadBytesAutoThreshold) {
            MMapFile mmap;
            if (!mmap.open(file))
                throw std::runtime_error("Failed to mmap file: " + file.string());

            auto data = std::shared_ptr<uint8_t[]>(reinterpret_cast<uint8_t*>(mmap.data()), [mmapCopy = std::move(mmap)](uint8_t*) mutable {});
            return { data, std::span<const uint8_t>(data.get(), fileSize) };
        } else {
            size_t size;
            auto buffer = ReadBytes(file, size);
            return { buffer, std::span<const uint8_t>(buffer.get(), size) };
        }
    }

    void WriteBytes(const std::filesystem::path& path, std::span<const uint8_t> data, bool append) {
        auto file = NormalizePath(path);
        std::filesystem::create_directories(file.parent_path());
        std::ofstream out(file, std::ios::binary | (append ? std::ios::app : std::ios::trunc));
        if (!out) throw std::runtime_error("Failed to open file for writing: " + file.string());

        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }

    bool FileExists(const std::filesystem::path& path) noexcept {
        std::error_code ec;
        return std::filesystem::is_regular_file(NormalizePath(path), ec);
    }

    bool DeleteFileSafe(const std::filesystem::path& path) noexcept {
        auto file = NormalizePath(path);
        if (std::filesystem::is_regular_file(file)) {
            std::error_code ec;
            std::filesystem::remove(file, ec);
            return !ec;
        }
        return false;
    }

}
