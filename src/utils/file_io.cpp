/**
 * @file file_io.cpp
 * @brief File I/O utilities implementation
 */

#include "utils/file_io.hpp"
#include <fstream>
#include <algorithm>
#include <thread>

namespace utils {

// ============================================================================
// FileIO Static Methods
// ============================================================================

FileReadResult FileIO::readFile(const std::string& path) {
    FileReadResult result;
    result.success = false;
    
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
    file.read(&result.content[0], static_cast<std::streamsize>(result.fileSize));
    
    if (!file) {
        result.error = "Failed to read file: " + path;
        return result;
    }
    
    // Detect line ending
    if (result.content.find("\r\n") != std::string::npos) {
        result.lineEnding = "CRLF";
    } else if (result.content.find('\r') != std::string::npos) {
        result.lineEnding = "CR";
    } else {
        result.lineEnding = "LF";
    }
    
    // Default encoding
    result.encoding = "UTF-8";
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
        // For now, just use synchronous read
        // Could add progress callbacks for large files
        return readFile(path);
    });
}

std::future<bool> FileIO::writeFileAsync(const std::string& path,
                                         const std::string& content,
                                         FileProgressCallback progress) {
    return std::async(std::launch::async, [path, content, progress]() {
        return writeFile(path, content);
    });
}

FileReadResult FileIO::readFileMapped(const std::string& path) {
    // For now, fall back to regular read
    // Could use mmap on Linux/macOS for better performance
    return readFile(path);
}

// ============================================================================
// FileStream Implementation
// ============================================================================

FileIO::FileStream::FileStream(const std::string& path) 
    : m_path(path) {
    m_file.open(path, std::ios::binary);
    if (m_file) {
        m_file.seekg(0, std::ios::end);
        m_size = static_cast<size_t>(m_file.tellg());
        m_file.seekg(0, std::ios::beg);
    }
}

FileIO::FileStream::~FileStream() {
    if (m_file.is_open()) {
        m_file.close();
    }
}

bool FileIO::FileStream::isOpen() const {
    return m_file.is_open();
}

bool FileIO::FileStream::isEOF() const {
    return m_file.eof();
}

std::string FileIO::FileStream::readLine() {
    std::string line;
    std::getline(m_file, line);
    return line;
}

std::string FileIO::FileStream::readChunk(size_t bytes) {
    std::string chunk(bytes, '\0');
    m_file.read(&chunk[0], static_cast<std::streamsize>(bytes));
    chunk.resize(static_cast<size_t>(m_file.gcount()));
    return chunk;
}

bool FileIO::FileStream::readInto(char* buffer, size_t bytes) {
    m_file.read(buffer, static_cast<std::streamsize>(bytes));
    return m_file.gcount() == static_cast<std::streamsize>(bytes);
}

size_t FileIO::FileStream::size() const {
    return m_size;
}

size_t FileIO::FileStream::position() const {
    return static_cast<size_t>(const_cast<std::ifstream&>(m_file).tellg());
}

void FileIO::FileStream::seek(size_t pos) {
    m_file.seekg(static_cast<std::streamoff>(pos), std::ios::beg);
}

} // namespace utils
