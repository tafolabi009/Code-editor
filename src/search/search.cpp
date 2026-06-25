/**
 * @file search.cpp
 * @brief Text search implementation with SIMD optimization support
 */

#include "search/search.hpp"
#include <algorithm>
#include <chrono>
#include <regex>
#include <cstring>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif

namespace search {

// CPU feature detection
static bool detectAVX2() {
#ifdef _MSC_VER
    int cpuInfo[4];
    __cpuid(cpuInfo, 0);
    int nIds = cpuInfo[0];
    if (nIds >= 7) {
        __cpuidex(cpuInfo, 7, 0);
        return (cpuInfo[1] & (1 << 5)) != 0;  // AVX2 bit
    }
    return false;
#else
    unsigned int eax, ebx, ecx, edx;
    if (__get_cpuid_max(0, nullptr) >= 7) {
        __cpuid_count(7, 0, eax, ebx, ecx, edx);
        return (ebx & (1 << 5)) != 0;  // AVX2 bit
    }
    return false;
#endif
}

static bool detectSSE42() {
#ifdef _MSC_VER
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    return (cpuInfo[2] & (1 << 20)) != 0;  // SSE4.2 bit
#else
    unsigned int eax, ebx, ecx, edx;
    __cpuid(1, eax, ebx, ecx, edx);
    return (ecx & (1 << 20)) != 0;  // SSE4.2 bit
#endif
}

// Static feature flags
static const bool g_hasAVX2 = detectAVX2();
static const bool g_hasSSE42 = detectSSE42();

// ====================
// SearchEngine Implementation
// ====================

SearchEngine::SearchEngine() = default;
SearchEngine::~SearchEngine() = default;

bool SearchEngine::hasAVX2Support() {
    return g_hasAVX2;
}

bool SearchEngine::hasSSE42Support() {
    return g_hasSSE42;
}

std::vector<SearchMatch> SearchEngine::search(std::string_view text,
                                              std::string_view pattern,
                                              const SearchOptions& options) {
    m_cancelled.store(false);
    m_lastStats = {};
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    std::vector<SearchMatch> results;
    
    if (pattern.empty() || text.empty()) {
        return results;
    }
    
    if (options.useRegex) {
        results = searchRegex(text, pattern, options);
    } else if (!options.caseSensitive) {
        results = searchCaseInsensitive(text, pattern, options);
    } else {
        results = searchPlain(text, pattern, options);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    // Update statistics
    m_lastStats.totalMatches = results.size();
    m_lastStats.searchTimeUs = duration.count();
    m_lastStats.bytesSearched = text.size();
    
    if (duration.count() > 0) {
        double seconds = duration.count() / 1000000.0;
        m_lastStats.throughputGBps = (text.size() / 1e9) / seconds;
    }
    
    return results;
}

std::optional<SearchMatch> SearchEngine::findNext(std::string_view text,
                                                   std::string_view pattern,
                                                   size_t startOffset,
                                                   const SearchOptions& options) {
    if (startOffset >= text.size() || pattern.empty()) {
        if (options.wrapAround) {
            startOffset = 0;
        } else {
            return std::nullopt;
        }
    }
    
    std::string_view searchText = text.substr(startOffset);
    
    size_t pos;
    if (!options.caseSensitive) {
        // Case-insensitive search
        std::string lowerText(searchText);
        std::string lowerPattern(pattern);
        std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);
        std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);
        pos = lowerText.find(lowerPattern);
    } else {
        pos = searchText.find(pattern);
    }
    
    if (pos == std::string_view::npos) {
        if (options.wrapAround && startOffset > 0) {
            // Search from beginning
            return findNext(text, pattern, 0, {options.caseSensitive, options.wholeWord, 
                                               options.useRegex, false, false});
        }
        return std::nullopt;
    }
    
    size_t matchOffset = startOffset + pos;
    
    // Check whole word if needed
    if (options.wholeWord && !isWordBoundary(text, matchOffset, pattern.size())) {
        return findNext(text, pattern, matchOffset + 1, options);
    }
    
    SearchMatch match;
    match.offset = matchOffset;
    match.length = pattern.size();
    match.line = lineFromOffset(text, matchOffset);
    match.column = columnFromOffset(text, matchOffset);
    match.context = buildContext(text, matchOffset, pattern.size());
    
    return match;
}

std::optional<SearchMatch> SearchEngine::findPrevious(std::string_view text,
                                                       std::string_view pattern,
                                                       size_t startOffset,
                                                       const SearchOptions& options) {
    if (pattern.empty() || text.empty()) {
        return std::nullopt;
    }
    
    if (startOffset > text.size()) {
        startOffset = text.size();
    }
    
    std::string_view searchText = text.substr(0, startOffset);
    
    size_t pos;
    if (!options.caseSensitive) {
        std::string lowerText(searchText);
        std::string lowerPattern(pattern);
        std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);
        std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);
        pos = lowerText.rfind(lowerPattern);
    } else {
        pos = searchText.rfind(pattern);
    }
    
    if (pos == std::string_view::npos) {
        if (options.wrapAround) {
            return findPrevious(text, pattern, text.size(), 
                              {options.caseSensitive, options.wholeWord, 
                               options.useRegex, true, false});
        }
        return std::nullopt;
    }
    
    if (options.wholeWord && !isWordBoundary(text, pos, pattern.size())) {
        if (pos > 0) {
            return findPrevious(text, pattern, pos, options);
        }
        return std::nullopt;
    }
    
    SearchMatch match;
    match.offset = pos;
    match.length = pattern.size();
    match.line = lineFromOffset(text, pos);
    match.column = columnFromOffset(text, pos);
    match.context = buildContext(text, pos, pattern.size());
    
    return match;
}

size_t SearchEngine::countMatches(std::string_view text,
                                  std::string_view pattern,
                                  const SearchOptions& options) {
    auto matches = search(text, pattern, options);
    return matches.size();
}

std::string SearchEngine::replace(std::string_view text,
                                  std::string_view pattern,
                                  std::string_view replacement,
                                  const SearchOptions& options) {
    if (options.useRegex) {
        try {
            std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
            if (!options.caseSensitive) {
                flags |= std::regex_constants::icase;
            }
            std::regex re(pattern.data(), pattern.size(), flags);
            return std::regex_replace(std::string(text), re, std::string(replacement));
        } catch (const std::regex_error&) {
            return std::string(text);
        }
    }
    
    std::string result;
    result.reserve(text.size());
    
    auto matches = search(text, pattern, options);
    
    size_t lastEnd = 0;
    for (const auto& match : matches) {
        result.append(text.data() + lastEnd, match.offset - lastEnd);
        result.append(replacement);
        lastEnd = match.offset + match.length;
    }
    result.append(text.data() + lastEnd, text.size() - lastEnd);
    
    return result;
}

std::string SearchEngine::replaceFirst(std::string_view text,
                                       std::string_view pattern,
                                       std::string_view replacement,
                                       const SearchOptions& options) {
    auto match = findNext(text, pattern, 0, options);
    if (!match) {
        return std::string(text);
    }
    
    std::string result;
    result.reserve(text.size() + replacement.size() - match->length);
    result.append(text.data(), match->offset);
    result.append(replacement);
    result.append(text.data() + match->offset + match->length, 
                  text.size() - match->offset - match->length);
    
    return result;
}

void SearchEngine::setProgressCallback(SearchProgressCallback callback) {
    m_progressCallback = std::move(callback);
}

void SearchEngine::cancel() {
    m_cancelled.store(true);
}

// ====================
// Private Implementation
// ====================

std::vector<SearchMatch> SearchEngine::searchPlain(std::string_view text,
                                                   std::string_view pattern,
                                                   const SearchOptions& options) {
    std::vector<SearchMatch> results;
    
#ifdef HAS_ASM_OPTIMIZATIONS
    if (m_useSIMD && pattern.size() >= 4 && text.size() >= 64) {
        // Use SIMD search for larger texts
        std::vector<size_t> positions(text.size() / pattern.size() + 1);
        size_t count;
        
        if (g_hasAVX2) {
            count = simd_search_avx2(text.data(), text.size(), 
                                     pattern.data(), pattern.size(),
                                     positions.data(), positions.size());
        } else if (g_hasSSE42) {
            count = simd_search_sse42(text.data(), text.size(),
                                      pattern.data(), pattern.size(),
                                      positions.data(), positions.size());
        } else {
            count = 0;
        }
        
        if (count > 0) {
            results.reserve(count);
            for (size_t i = 0; i < count; ++i) {
                if (m_cancelled.load()) break;
                
                size_t offset = positions[i];
                if (!options.wholeWord || isWordBoundary(text, offset, pattern.size())) {
                    SearchMatch match;
                    match.offset = offset;
                    match.length = pattern.size();
                    match.line = lineFromOffset(text, offset);
                    match.column = columnFromOffset(text, offset);
                    results.push_back(match);
                }
            }
            return results;
        }
    }
#endif
    
    // Fallback: standard string search.
    //
    // Line/column are tracked incrementally as the scan position advances so
    // the whole search is O(n) rather than O(n * matches) - computing them
    // from scratch per match would be quadratic on match-dense input.
    if (pattern.empty()) {
        return results;
    }

    size_t pos = 0;
    size_t scanned = 0;       // offset up to which newlines have been counted
    size_t curLine = 0;       // line number at 'scanned'
    size_t curLineStart = 0;  // offset of the start of 'curLine'

    while ((pos = text.find(pattern, pos)) != std::string_view::npos) {
        if (m_cancelled.load()) break;

        if (!options.wholeWord || isWordBoundary(text, pos, pattern.size())) {
            // Advance the line/column cursor up to this match.
            for (; scanned < pos; ++scanned) {
                if (text[scanned] == '\n') {
                    ++curLine;
                    curLineStart = scanned + 1;
                }
            }

            SearchMatch match;
            match.offset = pos;
            match.length = pattern.size();
            match.line = curLine;
            match.column = pos - curLineStart;
            results.push_back(match);

            // Non-overlapping: resume scanning after this match.
            pos += pattern.size();
        } else {
            // Rejected candidate (whole-word miss): try the next position.
            ++pos;
        }

        if (m_progressCallback && results.size() % 1000 == 0) {
            m_progressCallback(pos, text.size());
        }
    }

    return results;
}

std::vector<SearchMatch> SearchEngine::searchCaseInsensitive(std::string_view text,
                                                              std::string_view pattern,
                                                              const SearchOptions& options) {
    // Convert both to lowercase for comparison
    std::string lowerText(text);
    std::string lowerPattern(pattern);
    
    std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);
    std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);
    
    auto results = searchPlain(lowerText, lowerPattern, 
                               {true, options.wholeWord, false, false, false});
    
    // Fix up line/column info for original text
    for (auto& match : results) {
        match.context = buildContext(text, match.offset, match.length);
    }
    
    return results;
}

std::vector<SearchMatch> SearchEngine::searchRegex(std::string_view text,
                                                   std::string_view pattern,
                                                   const SearchOptions& options) {
    std::vector<SearchMatch> results;
    
    try {
        std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
        if (!options.caseSensitive) {
            flags |= std::regex_constants::icase;
        }
        
        std::regex re(pattern.data(), pattern.size(), flags);
        
        std::string textStr(text);
        auto begin = std::sregex_iterator(textStr.begin(), textStr.end(), re);
        auto end = std::sregex_iterator();
        
        for (auto it = begin; it != end; ++it) {
            if (m_cancelled.load()) break;
            
            SearchMatch match;
            match.offset = it->position();
            match.length = it->length();
            match.line = lineFromOffset(text, match.offset);
            match.column = columnFromOffset(text, match.offset);
            match.context = buildContext(text, match.offset, match.length);
            
            if (!options.wholeWord || isWordBoundary(text, match.offset, match.length)) {
                results.push_back(match);
            }
        }
    } catch (const std::regex_error& e) {
        // Invalid regex - return empty results
    }
    
    return results;
}

std::vector<size_t> SearchEngine::searchSIMD([[maybe_unused]] const char* text,
                                             [[maybe_unused]] size_t textLen,
                                             [[maybe_unused]] const char* pattern,
                                             [[maybe_unused]] size_t patternLen) {
    std::vector<size_t> results;
    
#ifdef HAS_ASM_OPTIMIZATIONS
    if (g_hasAVX2) {
        results.resize(textLen / patternLen + 1);
        size_t count = simd_search_avx2(text, textLen, pattern, patternLen,
                                        results.data(), results.size());
        results.resize(count);
    } else if (g_hasSSE42) {
        results.resize(textLen / patternLen + 1);
        size_t count = simd_search_sse42(text, textLen, pattern, patternLen,
                                         results.data(), results.size());
        results.resize(count);
    }
#endif
    
    return results;
}

bool SearchEngine::isWordBoundary(std::string_view text, size_t pos, size_t length) const {
    auto isWordChar = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    };

    // Character immediately before the match must not be a word character.
    if (pos > 0 && isWordChar(text[pos - 1])) {
        return false;
    }

    // Character immediately after the match must not be a word character.
    size_t after = pos + length;
    if (after < text.size() && isWordChar(text[after])) {
        return false;
    }

    return true;
}

std::string SearchEngine::buildContext(std::string_view text, size_t matchStart, 
                                        size_t matchLen) const {
    const size_t contextChars = 40;
    
    size_t start = (matchStart > contextChars) ? matchStart - contextChars : 0;
    size_t end = std::min(matchStart + matchLen + contextChars, text.size());
    
    // Adjust to word boundaries
    while (start > 0 && text[start] != '\n' && text[start - 1] != '\n') {
        --start;
        if (matchStart - start > contextChars * 2) break;
    }
    
    while (end < text.size() && text[end] != '\n') {
        ++end;
        if (end - matchStart - matchLen > contextChars * 2) break;
    }
    
    return std::string(text.substr(start, end - start));
}

size_t SearchEngine::lineFromOffset(std::string_view text, size_t offset) const {
    return std::count(text.begin(), text.begin() + std::min(offset, text.size()), '\n');
}

size_t SearchEngine::columnFromOffset(std::string_view text, size_t offset) const {
    size_t lastNewline = text.rfind('\n', offset > 0 ? offset - 1 : 0);
    if (lastNewline == std::string_view::npos) {
        return offset;
    }
    return offset - lastNewline - 1;
}

} // namespace search
