/**
 * @file file_io.cpp
 * @brief File I/O operations implementation
 */

#include "utils/file_io.hpp"
#include "utils/utf8.hpp"
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace utils {

// ============================================================================
// FileReader Implementation
// ============================================================================

FileReader::FileReader(const std::string& path) : m_path(path) {}

FileReader::~FileReader() {
    close();
}

bool FileReader::open() {
    m_file.open(m_path, std::ios::binary | std::ios::ate);
    if (!m_file.is_open()) {
        return false;
    }
    
    m_size = static_cast<size_t>(m_file.tellg());
    m_file.seekg(0, std::ios::beg);
    
    return true;
}

void FileReader::close() {
    if (m_file.is_open()) {
        m_file.close();
    }
    
    if (m_mmapData) {
        unmapFile();
    }
}

std::string FileReader::readAll() {
    if (!m_file.is_open() && !open()) {
        return "";
    }
    
    m_file.seekg(0, std::ios::beg);
    
    std::string content;
    content.resize(m_size);
    m_file.read(&content[0], m_size);
    
    // Detect and normalize line endings
    detectLineEnding(content);
    
    return content;
}

std::string FileReader::readLine() {
    std::string line;
    if (std::getline(m_file, line)) {
        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
    }
    return line;
}

std::vector<std::string> FileReader::readLines() {
    std::vector<std::string> lines;
    std::string line;
    
    if (!m_file.is_open() && !open()) {
        return lines;
    }
    
    m_file.seekg(0, std::ios::beg);
    
    while (std::getline(m_file, line)) {
        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
    }
    
    return lines;
}

bool FileReader::mapFile() {
#ifdef _WIN32
    HANDLE hFile = CreateFileA(m_path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return false;
    }
    m_size = static_cast<size_t>(fileSize.QuadPart);
    
    HANDLE hMapping = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMapping) {
        CloseHandle(hFile);
        return false;
    }
    
    m_mmapData = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(hMapping);
    CloseHandle(hFile);
    
    return m_mmapData != nullptr;
#else
    int fd = ::open(m_path.c_str(), O_RDONLY);
    if (fd < 0) {
        return false;
    }
    
    struct stat st;
    if (fstat(fd, &st) < 0) {
        ::close(fd);
        return false;
    }
    m_size = static_cast<size_t>(st.st_size);
    
    if (m_size == 0) {
        ::close(fd);
        return true;
    }
    
    m_mmapData = mmap(nullptr, m_size, PROT_READ, MAP_PRIVATE, fd, 0);
    ::close(fd);
    
    if (m_mmapData == MAP_FAILED) {
        m_mmapData = nullptr;
        return false;
    }
    
    // Advise kernel about sequential access pattern
    madvise(m_mmapData, m_size, MADV_SEQUENTIAL);
    
    return true;
#endif
}

void FileReader::unmapFile() {
    if (!m_mmapData) return;
    
#ifdef _WIN32
    UnmapViewOfFile(m_mmapData);
#else
    munmap(m_mmapData, m_size);
#endif
    
    m_mmapData = nullptr;
}

void FileReader::detectLineEnding(const std::string& content) {
    for (size_t i = 0; i < content.size(); ++i) {
        if (content[i] == '\r') {
            if (i + 1 < content.size() && content[i + 1] == '\n') {
                m_lineEnding = LineEnding::CRLF;
            } else {
                m_lineEnding = LineEnding::CR;
            }
            return;
        } else if (content[i] == '\n') {
            m_lineEnding = LineEnding::LF;
            return;
        }
    }
}

// ============================================================================
// FileWriter Implementation
// ============================================================================

FileWriter::FileWriter(const std::string& path) : m_path(path) {}

FileWriter::~FileWriter() {
    close();
}

bool FileWriter::open(bool append) {
    auto mode = std::ios::binary | (append ? std::ios::app : std::ios::trunc);
    m_file.open(m_path, mode);
    return m_file.is_open();
}

void FileWriter::close() {
    if (m_file.is_open()) {
        m_file.close();
    }
}

bool FileWriter::write(const std::string& content) {
    if (!m_file.is_open() && !open(false)) {
        return false;
    }
    
    m_file.write(content.data(), content.size());
    return m_file.good();
}

bool FileWriter::write(const char* data, size_t size) {
    if (!m_file.is_open() && !open(false)) {
        return false;
    }
    
    m_file.write(data, size);
    return m_file.good();
}

bool FileWriter::writeLine(const std::string& line) {
    std::string ending;
    switch (m_lineEnding) {
        case LineEnding::LF:   ending = "\n"; break;
        case LineEnding::CR:   ending = "\r"; break;
        case LineEnding::CRLF: ending = "\r\n"; break;
    }
    
    return write(line + ending);
}

bool FileWriter::writeLines(const std::vector<std::string>& lines) {
    for (const auto& line : lines) {
        if (!writeLine(line)) {
            return false;
        }
    }
    return true;
}

bool FileWriter::flush() {
    if (m_file.is_open()) {
        m_file.flush();
        return m_file.good();
    }
    return false;
}

// ============================================================================
// Free Functions
// ============================================================================

std::string readFile(const std::string& path) {
    FileReader reader(path);
    return reader.readAll();
}

bool writeFile(const std::string& path, const std::string& content) {
    FileWriter writer(path);
    return writer.write(content);
}

bool appendFile(const std::string& path, const std::string& content) {
    FileWriter writer(path);
    if (!writer.open(true)) {
        return false;
    }
    return writer.write(content);
}

bool copyFile(const std::string& src, const std::string& dst) {
    FileReader reader(src);
    if (!reader.open()) {
        return false;
    }
    
    FileWriter writer(dst);
    return writer.write(reader.readAll());
}

bool fileExists(const std::string& path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
#endif
}

bool directoryExists(const std::string& path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

size_t fileSize(const std::string& path) {
#ifdef _WIN32
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return 0;
    }
    LARGE_INTEGER size;
    GetFileSizeEx(hFile, &size);
    CloseHandle(hFile);
    return static_cast<size_t>(size.QuadPart);
#else
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return static_cast<size_t>(st.st_size);
    }
    return 0;
#endif
}

std::string getFileExtension(const std::string& path) {
    size_t pos = path.rfind('.');
    if (pos != std::string::npos && pos > 0 && path[pos - 1] != '/' && path[pos - 1] != '\\') {
        return path.substr(pos);
    }
    return "";
}

std::string getFileName(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

std::string getDirectory(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(0, pos);
    }
    return ".";
}

std::string normalizePath(const std::string& path) {
    std::string result = path;
    
    // Convert backslashes to forward slashes
    std::replace(result.begin(), result.end(), '\\', '/');
    
    // Remove duplicate slashes
    std::string cleaned;
    bool lastWasSlash = false;
    for (char c : result) {
        if (c == '/') {
            if (!lastWasSlash) {
                cleaned += c;
            }
            lastWasSlash = true;
        } else {
            cleaned += c;
            lastWasSlash = false;
        }
    }
    
    // Remove trailing slash unless it's the root
    if (cleaned.size() > 1 && cleaned.back() == '/') {
        cleaned.pop_back();
    }
    
    return cleaned;
}

std::string joinPath(const std::string& base, const std::string& name) {
    if (base.empty()) {
        return name;
    }
    
    char lastChar = base.back();
    if (lastChar == '/' || lastChar == '\\') {
        return base + name;
    }
    
    return base + "/" + name;
}

bool createDirectory(const std::string& path) {
#ifdef _WIN32
    return CreateDirectoryA(path.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

bool createDirectories(const std::string& path) {
    std::string normalized = normalizePath(path);
    
    size_t pos = 0;
    while ((pos = normalized.find('/', pos + 1)) != std::string::npos) {
        std::string subpath = normalized.substr(0, pos);
        if (!directoryExists(subpath)) {
            if (!createDirectory(subpath)) {
                return false;
            }
        }
    }
    
    if (!directoryExists(normalized)) {
        return createDirectory(normalized);
    }
    
    return true;
}

// ============================================================================
// AsyncFileIO Implementation
// ============================================================================

AsyncFileIO::AsyncFileIO(size_t threadCount)
    : m_running(true) {
    for (size_t i = 0; i < threadCount; ++i) {
        m_workers.emplace_back([this]() { workerThread(); });
    }
}

AsyncFileIO::~AsyncFileIO() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running = false;
    }
    m_condition.notify_all();
    
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

std::future<std::string> AsyncFileIO::readAsync(const std::string& path) {
    auto promise = std::make_shared<std::promise<std::string>>();
    auto future = promise->get_future();
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tasks.push([promise, path]() {
            try {
                promise->set_value(readFile(path));
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });
    }
    
    m_condition.notify_one();
    return future;
}

std::future<bool> AsyncFileIO::writeAsync(const std::string& path, const std::string& content) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tasks.push([promise, path, content]() {
            try {
                promise->set_value(writeFile(path, content));
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });
    }
    
    m_condition.notify_one();
    return future;
}

void AsyncFileIO::workerThread() {
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition.wait(lock, [this]() {
                return !m_running || !m_tasks.empty();
            });
            
            if (!m_running && m_tasks.empty()) {
                return;
            }
            
            task = std::move(m_tasks.front());
            m_tasks.pop();
        }
        
        task();
    }
}

// ============================================================================
// Encoding Detection
// ============================================================================

Encoding detectEncoding(const char* data, size_t size) {
    if (size == 0) {
        return Encoding::UTF8;
    }
    
    // Check for BOM
    if (size >= 3 && static_cast<unsigned char>(data[0]) == 0xEF &&
                     static_cast<unsigned char>(data[1]) == 0xBB &&
                     static_cast<unsigned char>(data[2]) == 0xBF) {
        return Encoding::UTF8_BOM;
    }
    
    if (size >= 2) {
        if (static_cast<unsigned char>(data[0]) == 0xFE &&
            static_cast<unsigned char>(data[1]) == 0xFF) {
            return Encoding::UTF16_BE;
        }
        if (static_cast<unsigned char>(data[0]) == 0xFF &&
            static_cast<unsigned char>(data[1]) == 0xFE) {
            return Encoding::UTF16_LE;
        }
    }
    
    // Check if valid UTF-8
    bool validUTF8 = true;
    bool hasHighBytes = false;
    
    for (size_t i = 0; i < size && validUTF8; ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        
        if (c >= 0x80) {
            hasHighBytes = true;
            
            // Check UTF-8 multibyte sequence
            int extraBytes = 0;
            if ((c & 0xE0) == 0xC0) extraBytes = 1;
            else if ((c & 0xF0) == 0xE0) extraBytes = 2;
            else if ((c & 0xF8) == 0xF0) extraBytes = 3;
            else { validUTF8 = false; continue; }
            
            if (i + extraBytes >= size) {
                validUTF8 = false;
                continue;
            }
            
            for (int j = 1; j <= extraBytes; ++j) {
                if ((static_cast<unsigned char>(data[i + j]) & 0xC0) != 0x80) {
                    validUTF8 = false;
                    break;
                }
            }
            
            i += extraBytes;
        }
    }
    
    if (validUTF8) {
        return Encoding::UTF8;
    }
    
    // Assume Latin-1 if not valid UTF-8
    return Encoding::Latin1;
}

std::string encodingToString(Encoding encoding) {
    switch (encoding) {
        case Encoding::UTF8:     return "UTF-8";
        case Encoding::UTF8_BOM: return "UTF-8 with BOM";
        case Encoding::UTF16_LE: return "UTF-16 LE";
        case Encoding::UTF16_BE: return "UTF-16 BE";
        case Encoding::Latin1:   return "ISO-8859-1";
        case Encoding::ASCII:    return "ASCII";
        default:                 return "Unknown";
    }
}

} // namespace utils
