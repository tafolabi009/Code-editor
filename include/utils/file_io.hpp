#pragma once

#include <string>
#include <optional>
#include <filesystem>
#include <functional>
#include <future>
#include <vector>

namespace utils {

/**
 * @brief File read result
 */
struct FileReadResult {
    bool success;
    std::string content;
    std::string error;
    std::string encoding;  // Detected encoding
    std::string lineEnding; // Detected line ending (LF, CRLF, CR)
    size_t fileSize;
};

/**
 * @brief Progress callback for file operations
 */
using FileProgressCallback = std::function<void(size_t bytesProcessed, size_t totalBytes)>;

/**
 * @brief File I/O utilities with async support
 */
class FileIO {
public:
    // Synchronous operations
    static FileReadResult readFile(const std::string& path);
    static bool writeFile(const std::string& path, const std::string& content);
    
    // Asynchronous operations
    static std::future<FileReadResult> readFileAsync(const std::string& path,
                                                     FileProgressCallback progress = nullptr);
    static std::future<bool> writeFileAsync(const std::string& path,
                                           const std::string& content,
                                           FileProgressCallback progress = nullptr);
    
    // Memory-mapped file for large files
    static FileReadResult readFileMapped(const std::string& path);
    
    // Streaming for very large files
    class FileStream {
    public:
        explicit FileStream(const std::string& path);
        ~FileStream();
        
        bool isOpen() const;
        bool isEOF() const;
        
        std::string readLine();
        std::string readChunk(size_t bytes);
        bool readInto(char* buffer, size_t bytes);
        
        size_t getPosition() const;
        void seek(size_t position);
        
        size_t getFileSize() const;
        
    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
    
    // Encoding detection
    static std::string detectEncoding(const char* data, size_t size);
    static std::string detectLineEnding(const char* data, size_t size);
    
    // File utilities
    static bool fileExists(const std::string& path);
    static size_t getFileSize(const std::string& path);
    static std::string getFileName(const std::string& path);
    static std::string getFileExtension(const std::string& path);
    static std::string getDirectory(const std::string& path);
    static std::string normalizePath(const std::string& path);
    
    // Backup and temp files
    static std::string createBackup(const std::string& path);
    static std::string createTempFile(const std::string& prefix = "editor_");
    
    // Recent files
    static void addToRecent(const std::string& path);
    static std::vector<std::string> getRecentFiles(size_t maxCount = 10);
    static void clearRecentFiles();
};

/**
 * @brief File watcher for detecting external changes
 */
class FileWatcher {
public:
    using ChangeCallback = std::function<void(const std::string& path)>;
    
    FileWatcher();
    ~FileWatcher();
    
    void watchFile(const std::string& path);
    void unwatchFile(const std::string& path);
    void unwatchAll();
    
    void setChangeCallback(ChangeCallback callback);
    
    void start();
    void stop();
    bool isRunning() const;
    
    // Check for changes manually
    void poll();
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace utils
