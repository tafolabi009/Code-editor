#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <functional>
#include <atomic>

namespace search {

/**
 * @brief Search options
 */
struct SearchOptions {
    bool caseSensitive = false;
    bool wholeWord = false;
    bool useRegex = false;
    bool searchBackward = false;
    bool wrapAround = true;
};

/**
 * @brief A single search match
 */
struct SearchMatch {
    size_t line;        // 0-indexed line number
    size_t column;      // 0-indexed column
    size_t offset;      // Absolute offset in text
    size_t length;      // Length of match
    std::string context; // Surrounding text for preview
};

/**
 * @brief Search result statistics
 */
struct SearchStats {
    size_t totalMatches = 0;
    size_t searchTimeUs = 0;  // Microseconds
    size_t bytesSearched = 0;
    double throughputGBps = 0.0;
};

/**
 * @brief Progress callback for long searches
 */
using SearchProgressCallback = std::function<void(size_t bytesSearched, size_t totalBytes)>;

/**
 * @brief Text search engine with SIMD optimization
 * 
 * Uses AVX2/SSE4.2 for fast string matching when available.
 */
class SearchEngine {
public:
    SearchEngine();
    ~SearchEngine();
    
    // Simple search
    std::vector<SearchMatch> search(std::string_view text, 
                                    std::string_view pattern,
                                    const SearchOptions& options = {});
    
    // Find next/previous
    std::optional<SearchMatch> findNext(std::string_view text,
                                        std::string_view pattern,
                                        size_t startOffset,
                                        const SearchOptions& options = {});
    
    std::optional<SearchMatch> findPrevious(std::string_view text,
                                            std::string_view pattern,
                                            size_t startOffset,
                                            const SearchOptions& options = {});
    
    // Count occurrences
    size_t countMatches(std::string_view text,
                        std::string_view pattern,
                        const SearchOptions& options = {});
    
    // Replace operations
    std::string replace(std::string_view text,
                       std::string_view pattern,
                       std::string_view replacement,
                       const SearchOptions& options = {});
    
    std::string replaceFirst(std::string_view text,
                            std::string_view pattern,
                            std::string_view replacement,
                            const SearchOptions& options = {});
    
    // Statistics
    const SearchStats& getLastStats() const { return m_lastStats; }
    
    // Progress and cancellation
    void setProgressCallback(SearchProgressCallback callback);
    void cancel();
    bool isCancelled() const { return m_cancelled.load(); }
    
    // SIMD status
    static bool hasAVX2Support();
    static bool hasSSE42Support();
    void setUseSIMD(bool enable) { m_useSIMD = enable; }
    bool isUsingSIMD() const { return m_useSIMD && (hasAVX2Support() || hasSSE42Support()); }
    
private:
    SearchStats m_lastStats;
    SearchProgressCallback m_progressCallback;
    std::atomic<bool> m_cancelled{false};
    bool m_useSIMD = true;
    
    // Implementation functions
    std::vector<SearchMatch> searchPlain(std::string_view text,
                                         std::string_view pattern,
                                         const SearchOptions& options);
    
    std::vector<SearchMatch> searchCaseInsensitive(std::string_view text,
                                                   std::string_view pattern,
                                                   const SearchOptions& options);
    
    std::vector<SearchMatch> searchRegex(std::string_view text,
                                         std::string_view pattern,
                                         const SearchOptions& options);
    
    // SIMD implementations (defined in search_simd.cpp or .asm)
    std::vector<size_t> searchSIMD(const char* text, size_t textLen,
                                   const char* pattern, size_t patternLen);
    
    // Helper functions
    bool isWordBoundary(std::string_view text, size_t pos) const;
    std::string buildContext(std::string_view text, size_t matchStart, size_t matchLen) const;
    size_t lineFromOffset(std::string_view text, size_t offset) const;
    size_t columnFromOffset(std::string_view text, size_t offset) const;
};

// ====================
// ASM-optimized functions (extern "C" for linkage with assembly)
// ====================

#ifdef HAS_ASM_OPTIMIZATIONS
extern "C" {
    /**
     * @brief SIMD-accelerated string search using AVX2
     * @param text Pointer to text buffer
     * @param textLen Length of text
     * @param pattern Pointer to pattern
     * @param patternLen Length of pattern  
     * @param results Output buffer for match positions
     * @param maxResults Maximum results to return
     * @return Number of matches found
     */
    size_t simd_search_avx2(const char* text, size_t textLen,
                            const char* pattern, size_t patternLen,
                            size_t* results, size_t maxResults);
    
    /**
     * @brief SIMD-accelerated string search using SSE4.2
     * @param text Pointer to text buffer
     * @param textLen Length of text
     * @param pattern Pointer to pattern
     * @param patternLen Length of pattern
     * @param results Output buffer for match positions
     * @param maxResults Maximum results to return
     * @return Number of matches found
     */
    size_t simd_search_sse42(const char* text, size_t textLen,
                             const char* pattern, size_t patternLen,
                             size_t* results, size_t maxResults);
    
    /**
     * @brief Find first occurrence of character using SIMD
     * @param text Pointer to text
     * @param textLen Length of text
     * @param c Character to find
     * @return Offset of first occurrence, or textLen if not found
     */
    size_t simd_memchr(const char* text, size_t textLen, char c);
}
#endif

} // namespace search
