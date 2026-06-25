/**
 * @file file_io.cpp
 * @brief File I/O utilities implementation
 */

#include "utils/file_io.hpp"
#include <fstream>
#include <algorithm>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstring>

namespace fs = std::filesystem;

namespace utils {

// ============================================================================
// FileIO Static Methods
// ============================================================================

FileReadResult FileIO::readFile(const std::string& path) {
    FileReadResult result;
    result.success = false;
    result.fileSize = 0;

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        result.error = "Failed to open file: " + path;
        return result;
    }

    // Get file size
    file.seekg(0, std::ios::end);
    result.fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    // Read content
    result.content.resize(result.fileSize);
    if (result.fileSize > 0) {
        file.read(&result.content[0], static_cast<std::streamsize>(result.fileSize));
        if (!file) {
            result.error = "Failed to read file: " + path;
            return result;
        }
    }

    result.encoding = detectEncoding(result.content.data(), result.content.size());
    result.lineEnding = detectLineEnding(result.content.data(), result.content.size());
    result.success = true;

    return result;
}

bool FileIO::writeFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    return file.good();
}

std::future<FileReadResult> FileIO::readFileAsync(const std::string& path,
                                                  FileProgressCallback progress) {
    return std::async(std::launch::async, [path, progress]() {
        FileReadResult result = readFile(path);
        if (progress && result.success) {
            progress(result.fileSize, result.fileSize);
        }
        return result;
    });
}

std::future<bool> FileIO::writeFileAsync(const std::string& path,
                                         const std::string& content,
                                         FileProgressCallback progress) {
    return std::async(std::launch::async, [path, content, progress]() {
        bool ok = writeFile(path, content);
        if (progress && ok) {
            progress(content.size(), content.size());
        }
        return ok;
    });
}

FileReadResult FileIO::readFileMapped(const std::string& path) {
    // Memory-mapping is a future optimisation for very large files; for now
    // fall back to a regular buffered read which is correct on all platforms.
    return readFile(path);
}

// ============================================================================
// FileStream Implementation (PIMPL)
// ============================================================================

struct FileIO::FileStream::Impl {
    std::ifstream file;
    std::string path;
    size_t size = 0;
};

FileIO::FileStream::FileStream(const std::string& path)
    : m_impl(std::make_unique<Impl>()) {
    m_impl->path = path;
    m_impl->file.open(path, std::ios::binary);
    if (m_impl->file) {
        m_impl->file.seekg(0, std::ios::end);
        m_impl->size = static_cast<size_t>(m_impl->file.tellg());
        m_impl->file.seekg(0, std::ios::beg);
    }
}

FileIO::FileStream::~FileStream() = default;

bool FileIO::FileStream::isOpen() const {
    return m_impl->file.is_open();
}

bool FileIO::FileStream::isEOF() const {
    return m_impl->file.eof();
}

std::string FileIO::FileStream::readLine() {
    std::string line;
    std::getline(m_impl->file, line);
    // Strip a trailing CR so CRLF files yield clean lines.
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return line;
}

std::string FileIO::FileStream::readChunk(size_t bytes) {
    std::string chunk(bytes, '\0');
    m_impl->file.read(&chunk[0], static_cast<std::streamsize>(bytes));
    chunk.resize(static_cast<size_t>(m_impl->file.gcount()));
    return chunk;
}

bool FileIO::FileStream::readInto(char* buffer, size_t bytes) {
    m_impl->file.read(buffer, static_cast<std::streamsize>(bytes));
    return m_impl->file.gcount() == static_cast<std::streamsize>(bytes);
}

size_t FileIO::FileStream::getPosition() const {
    return static_cast<size_t>(m_impl->file.tellg());
}

void FileIO::FileStream::seek(size_t position) {
    m_impl->file.clear();  // clear EOF/fail bits so seeking after EOF works
    m_impl->file.seekg(static_cast<std::streamoff>(position), std::ios::beg);
}

size_t FileIO::FileStream::getFileSize() const {
    return m_impl->size;
}

// ============================================================================
// Encoding / line-ending detection
// ============================================================================

std::string FileIO::detectEncoding(const char* data, size_t size) {
    // BOM sniffing first.
    if (size >= 3 && static_cast<unsigned char>(data[0]) == 0xEF &&
        static_cast<unsigned char>(data[1]) == 0xBB &&
        static_cast<unsigned char>(data[2]) == 0xBF) {
        return "UTF-8";
    }
    if (size >= 2 && static_cast<unsigned char>(data[0]) == 0xFF &&
        static_cast<unsigned char>(data[1]) == 0xFE) {
        return "UTF-16LE";
    }
    if (size >= 2 && static_cast<unsigned char>(data[0]) == 0xFE &&
        static_cast<unsigned char>(data[1]) == 0xFF) {
        return "UTF-16BE";
    }

    // No BOM: scan for any high-bit byte to distinguish ASCII from UTF-8.
    for (size_t i = 0; i < size; ++i) {
        if (static_cast<unsigned char>(data[i]) >= 0x80) {
            return "UTF-8";
        }
    }
    return "ASCII";
}

std::string FileIO::detectLineEnding(const char* data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        if (data[i] == '\r') {
            return (i + 1 < size && data[i + 1] == '\n') ? "CRLF" : "CR";
        }
        if (data[i] == '\n') {
            return "LF";
        }
    }
    return "LF";  // default for files with no line breaks
}

// ============================================================================
// File utilities
// ============================================================================

bool FileIO::fileExists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec) && fs::is_regular_file(path, ec);
}

size_t FileIO::getFileSize(const std::string& path) {
    std::error_code ec;
    auto size = fs::file_size(path, ec);
    return ec ? 0 : static_cast<size_t>(size);
}

std::string FileIO::getFileName(const std::string& path) {
    return fs::path(path).filename().string();
}

std::string FileIO::getFileExtension(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    // Normalise to lower-case without the leading dot.
    if (!ext.empty() && ext.front() == '.') {
        ext.erase(ext.begin());
    }
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

std::string FileIO::getDirectory(const std::string& path) {
    return fs::path(path).parent_path().string();
}

std::string FileIO::normalizePath(const std::string& path) {
    std::error_code ec;
    fs::path p = fs::weakly_canonical(path, ec);
    if (ec) {
        p = fs::path(path).lexically_normal();
    }
    return p.string();
}

// ============================================================================
// Backup and temp files
// ============================================================================

std::string FileIO::createBackup(const std::string& path) {
    if (!fileExists(path)) {
        return {};
    }
    std::string backupPath = path + ".bak";
    std::error_code ec;
    fs::copy_file(path, backupPath, fs::copy_options::overwrite_existing, ec);
    return ec ? std::string{} : backupPath;
}

std::string FileIO::createTempFile(const std::string& prefix) {
    std::error_code ec;
    fs::path dir = fs::temp_directory_path(ec);
    if (ec) {
        dir = ".";
    }

    // Derive a unique-enough suffix without depending on RNG facilities.
    static std::atomic<uint64_t> counter{0};
    uint64_t n = counter.fetch_add(1);
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();

    fs::path candidate;
    for (int attempt = 0; attempt < 1000; ++attempt) {
        candidate = dir / (prefix + std::to_string(now) + "_" +
                           std::to_string(n) + "_" + std::to_string(attempt));
        if (!fs::exists(candidate, ec)) {
            break;
        }
    }

    std::ofstream create(candidate, std::ios::binary);
    return create ? candidate.string() : std::string{};
}

// ============================================================================
// Recent files (persisted to a small text file next to a stable location)
// ============================================================================

namespace {

std::string recentFilesPath() {
    std::error_code ec;
    fs::path dir = fs::temp_directory_path(ec);
    if (ec) {
        dir = ".";
    }
    return (dir / "code_editor_recent.txt").string();
}

std::mutex& recentMutex() {
    static std::mutex m;
    return m;
}

}  // namespace

void FileIO::addToRecent(const std::string& path) {
    std::lock_guard<std::mutex> lock(recentMutex());

    std::vector<std::string> recent;
    {
        std::ifstream in(recentFilesPath());
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line != path) {
                recent.push_back(line);
            }
        }
    }

    recent.insert(recent.begin(), path);
    if (recent.size() > 50) {
        recent.resize(50);
    }

    std::ofstream out(recentFilesPath(), std::ios::trunc);
    for (const auto& p : recent) {
        out << p << '\n';
    }
}

std::vector<std::string> FileIO::getRecentFiles(size_t maxCount) {
    std::lock_guard<std::mutex> lock(recentMutex());

    std::vector<std::string> recent;
    std::ifstream in(recentFilesPath());
    std::string line;
    while (std::getline(in, line) && recent.size() < maxCount) {
        if (!line.empty()) {
            recent.push_back(line);
        }
    }
    return recent;
}

void FileIO::clearRecentFiles() {
    std::lock_guard<std::mutex> lock(recentMutex());
    std::ofstream out(recentFilesPath(), std::ios::trunc);
}

// ============================================================================
// FileWatcher Implementation (PIMPL, poll-based)
// ============================================================================

struct FileWatcher::Impl {
    std::mutex mutex;
    std::unordered_map<std::string, fs::file_time_type> watched;
    ChangeCallback callback;
    bool running = false;
};

FileWatcher::FileWatcher() : m_impl(std::make_unique<Impl>()) {}

FileWatcher::~FileWatcher() = default;

void FileWatcher::watchFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    std::error_code ec;
    auto t = fs::last_write_time(path, ec);
    if (!ec) {
        m_impl->watched[path] = t;
    }
}

void FileWatcher::unwatchFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->watched.erase(path);
}

void FileWatcher::unwatchAll() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->watched.clear();
}

void FileWatcher::setChangeCallback(ChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->callback = std::move(callback);
}

void FileWatcher::start() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->running = true;
}

void FileWatcher::stop() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->running = false;
}

bool FileWatcher::isRunning() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->running;
}

void FileWatcher::poll() {
    std::vector<std::string> changed;
    ChangeCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        if (!m_impl->running) {
            return;
        }
        cb = m_impl->callback;
        for (auto& [path, lastTime] : m_impl->watched) {
            std::error_code ec;
            auto t = fs::last_write_time(path, ec);
            if (!ec && t != lastTime) {
                lastTime = t;
                changed.push_back(path);
            }
        }
    }
    // Fire callbacks outside the lock so handlers can re-enter the watcher.
    if (cb) {
        for (const auto& path : changed) {
            cb(path);
        }
    }
}

} // namespace utils
