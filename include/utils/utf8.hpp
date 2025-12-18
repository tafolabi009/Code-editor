#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace utils {

/**
 * @brief UTF-8 validation result
 */
enum class Utf8ValidationResult {
    Valid,
    InvalidStartByte,
    InvalidContinuationByte,
    OverlongEncoding,
    InvalidCodePoint,
    UnexpectedEnd,
    SurrogateCodePoint
};

/**
 * @brief UTF-8 validation with detailed error info
 */
struct Utf8ValidationInfo {
    Utf8ValidationResult result;
    size_t errorOffset;       // Offset of first error
    size_t validBytes;        // Number of valid bytes before error
    size_t codePointCount;    // Number of valid code points
};

/**
 * @brief UTF-8 utilities
 */
class Utf8 {
public:
    // Validation
    static bool isValid(std::string_view text);
    static Utf8ValidationInfo validate(std::string_view text);
    
    // Character properties
    static bool isASCII(char c) { return (static_cast<uint8_t>(c) & 0x80) == 0; }
    static bool isLeadByte(char c);
    static bool isContinuationByte(char c);
    static int byteCount(char leadByte);
    
    // Code point operations
    static uint32_t decodeCodePoint(const char* data, size_t& bytesRead);
    static size_t encodeCodePoint(uint32_t codePoint, char* output);
    
    // String operations
    static size_t codePointCount(std::string_view text);
    static size_t byteOffset(std::string_view text, size_t codePointIndex);
    static size_t codePointIndex(std::string_view text, size_t byteOffset);
    
    // Navigation
    static size_t nextCodePoint(std::string_view text, size_t offset);
    static size_t prevCodePoint(std::string_view text, size_t offset);
    
    // Character classification (for word boundaries, etc.)
    static bool isWhitespace(uint32_t codePoint);
    static bool isAlphanumeric(uint32_t codePoint);
    static bool isPunctuation(uint32_t codePoint);
    
    // Conversion
    static std::string fromUtf16(const char16_t* text, size_t length);
    static std::string fromUtf32(const char32_t* text, size_t length);
    static std::u16string toUtf16(std::string_view utf8);
    static std::u32string toUtf32(std::string_view utf8);
    
private:
    // SIMD-accelerated validation (if available)
    static bool validateSIMD(const char* data, size_t length);
    static bool validateScalar(const char* data, size_t length);
};

// ====================
// ASM-optimized functions (extern "C" for linkage with assembly)
// ====================

#ifdef HAS_ASM_OPTIMIZATIONS
extern "C" {
    /**
     * @brief SIMD-accelerated UTF-8 validation using SSE4.2
     * @param text Pointer to UTF-8 text
     * @param length Length of text in bytes
     * @return 1 if valid, 0 if invalid
     */
    int simd_utf8_validate(const char* text, size_t length);
    
    /**
     * @brief Find first invalid UTF-8 byte
     * @param text Pointer to UTF-8 text
     * @param length Length of text in bytes
     * @return Offset of first invalid byte, or length if all valid
     */
    size_t simd_utf8_find_invalid(const char* text, size_t length);
    
    /**
     * @brief Count UTF-8 code points using SIMD
     * @param text Pointer to UTF-8 text
     * @param length Length of text in bytes
     * @return Number of code points
     */
    size_t simd_utf8_count_codepoints(const char* text, size_t length);
}
#endif

} // namespace utils
