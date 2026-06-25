/**
 * @file test_search.cpp
 * @brief Unit tests for Search functionality
 * 
 * Tests the SearchEngine which uses SIMD (AVX2/SSE4.2) for optimized
 * string matching when available.
 */

#include <gtest/gtest.h>
#include "search/search.hpp"
#include <chrono>
#include <string>

using namespace search;

// ============================================================================
// Basic Search Tests
// ============================================================================

class SearchEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<SearchEngine>();
    }
    
    std::unique_ptr<SearchEngine> engine;
};

TEST_F(SearchEngineTest, BasicSearch) {
    std::string text = "Hello World Hello";
    
    SearchOptions options;
    auto results = engine->search(text, "Hello", options);
    
    EXPECT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].offset, 0u);
    EXPECT_EQ(results[1].offset, 12u);
}

TEST_F(SearchEngineTest, CaseSensitiveSearch) {
    std::string text = "Hello hello HELLO";
    
    SearchOptions options;
    options.caseSensitive = true;
    auto results = engine->search(text, "Hello", options);
    
    EXPECT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].offset, 0u);
}

TEST_F(SearchEngineTest, CaseInsensitiveSearch) {
    std::string text = "Hello hello HELLO";
    
    SearchOptions options;
    options.caseSensitive = false;
    auto results = engine->search(text, "Hello", options);
    
    EXPECT_EQ(results.size(), 3u);
}

TEST_F(SearchEngineTest, WholeWordSearch) {
    std::string text = "The cat category scattered";
    
    SearchOptions options;
    options.wholeWord = true;
    auto results = engine->search(text, "cat", options);
    
    EXPECT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].offset, 4u);
}

TEST_F(SearchEngineTest, NoMatches) {
    std::string text = "Hello World";
    
    SearchOptions options;
    auto results = engine->search(text, "xyz", options);
    
    EXPECT_TRUE(results.empty());
}

TEST_F(SearchEngineTest, EmptyQuery) {
    std::string text = "Hello World";
    
    SearchOptions options;
    auto results = engine->search(text, "", options);
    
    EXPECT_TRUE(results.empty());
}

TEST_F(SearchEngineTest, EmptyText) {
    std::string text = "";
    
    SearchOptions options;
    auto results = engine->search(text, "Hello", options);
    
    EXPECT_TRUE(results.empty());
}

// ============================================================================
// Regex Search Tests
// ============================================================================

TEST_F(SearchEngineTest, RegexSearch) {
    std::string text = "test123 test456 test789";
    
    SearchOptions options;
    options.useRegex = true;
    auto results = engine->search(text, R"(test\d+)", options);
    
    EXPECT_EQ(results.size(), 3u);
}

TEST_F(SearchEngineTest, RegexWordBoundary) {
    std::string text = "cat category concatenate";
    
    SearchOptions options;
    options.useRegex = true;
    auto results = engine->search(text, R"(\bcat\b)", options);
    
    EXPECT_EQ(results.size(), 1u);
}

TEST_F(SearchEngineTest, RegexCaptureGroups) {
    std::string text = "foo=bar baz=qux";
    
    SearchOptions options;
    options.useRegex = true;
    auto results = engine->search(text, R"((\w+)=(\w+))", options);
    
    EXPECT_EQ(results.size(), 2u);
}

TEST_F(SearchEngineTest, InvalidRegex) {
    std::string text = "Hello World";
    
    SearchOptions options;
    options.useRegex = true;
    auto results = engine->search(text, "[invalid(regex", options);
    
    // Invalid regex should return empty results (no crash)
    EXPECT_TRUE(results.empty());
}

// ============================================================================
// Replace Tests
// ============================================================================

TEST_F(SearchEngineTest, ReplaceFirst) {
    std::string text = "Hello World Hello";
    
    SearchOptions options;
    auto result = engine->replaceFirst(text, "Hello", "Hi", options);
    
    EXPECT_EQ(result, "Hi World Hello");
}

TEST_F(SearchEngineTest, ReplaceAll) {
    std::string text = "Hello World Hello";
    
    SearchOptions options;
    auto result = engine->replace(text, "Hello", "Hi", options);
    
    EXPECT_EQ(result, "Hi World Hi");
}

TEST_F(SearchEngineTest, ReplaceWithEmpty) {
    std::string text = "Hello World Hello";
    
    SearchOptions options;
    auto result = engine->replace(text, "Hello", "", options);
    
    EXPECT_EQ(result, " World ");
}

TEST_F(SearchEngineTest, ReplaceRegex) {
    std::string text = "foo123 bar456";
    
    SearchOptions options;
    options.useRegex = true;
    auto result = engine->replace(text, R"(\d+)", "XXX", options);
    
    EXPECT_EQ(result, "fooXXX barXXX");
}

TEST_F(SearchEngineTest, ReplaceCaseSensitive) {
    std::string text = "Hello hello HELLO";
    
    SearchOptions options;
    options.caseSensitive = true;
    auto result = engine->replace(text, "Hello", "Hi", options);
    
    EXPECT_EQ(result, "Hi hello HELLO");
}

// ============================================================================
// Navigation Tests (findNext/findPrevious)
// ============================================================================

TEST_F(SearchEngineTest, FindNext) {
    std::string text = "AAA BBB AAA BBB AAA";
    
    SearchOptions options;
    
    auto first = engine->findNext(text, "AAA", 0, options);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->offset, 0u);
    
    auto second = engine->findNext(text, "AAA", first->offset + first->length, options);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->offset, 8u);
    
    auto third = engine->findNext(text, "AAA", second->offset + second->length, options);
    ASSERT_TRUE(third.has_value());
    EXPECT_EQ(third->offset, 16u);
}

TEST_F(SearchEngineTest, FindNextWrapAround) {
    std::string text = "AAA BBB AAA";
    
    SearchOptions options;
    options.wrapAround = true;
    
    // Start after last match, should wrap around
    auto match = engine->findNext(text, "AAA", 10, options);
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->offset, 0u);
}

TEST_F(SearchEngineTest, FindPrevious) {
    std::string text = "AAA BBB AAA BBB AAA";
    
    SearchOptions options;
    
    // Start from end
    auto last = engine->findPrevious(text, "AAA", text.size(), options);
    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(last->offset, 16u);
    
    auto middle = engine->findPrevious(text, "AAA", last->offset, options);
    ASSERT_TRUE(middle.has_value());
    EXPECT_EQ(middle->offset, 8u);
    
    auto first = engine->findPrevious(text, "AAA", middle->offset, options);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->offset, 0u);
}

// ============================================================================
// Multiline Tests
// ============================================================================

TEST_F(SearchEngineTest, MultilineSearch) {
    std::string text = "Line1 search\nLine2 search\nLine3 search";
    
    SearchOptions options;
    auto results = engine->search(text, "search", options);
    
    EXPECT_EQ(results.size(), 3u);
}

TEST_F(SearchEngineTest, SearchAcrossLines) {
    std::string text = "Hello\nWorld";
    
    SearchOptions options;
    auto results = engine->search(text, "Hello\nWorld", options);
    
    EXPECT_EQ(results.size(), 1u);
}

TEST_F(SearchEngineTest, LineColumnInfo) {
    std::string text = "Line0\nLine1 match\nLine2";
    
    SearchOptions options;
    auto results = engine->search(text, "match", options);
    
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].line, 1u);
    EXPECT_EQ(results[0].column, 6u);
}

// ============================================================================
// Unicode Tests
// ============================================================================

TEST_F(SearchEngineTest, UnicodeSearch) {
    std::string text = "Hello 世界 World 世界";
    
    SearchOptions options;
    auto results = engine->search(text, "世界", options);
    
    EXPECT_EQ(results.size(), 2u);
}

TEST_F(SearchEngineTest, UnicodeReplace) {
    std::string text = "Hello 世界";
    
    SearchOptions options;
    auto result = engine->replace(text, "世界", "World", options);
    
    EXPECT_EQ(result, "Hello World");
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(SearchEngineTest, SearchStats) {
    std::string text(10000, 'A');
    
    SearchOptions options;
    auto results = engine->search(text, "AAA", options);
    
    auto stats = engine->getLastStats();
    EXPECT_GT(stats.totalMatches, 0u);
    EXPECT_EQ(stats.bytesSearched, text.size());
    EXPECT_GT(stats.searchTimeUs, 0u);
}

TEST_F(SearchEngineTest, CountMatches) {
    std::string text = "AAA BBB AAA CCC AAA";
    
    SearchOptions options;
    auto count = engine->countMatches(text, "AAA", options);
    
    EXPECT_EQ(count, 3u);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(SearchEngineTest, LargeTextSearch) {
    // Create a large text
    std::string content;
    content.reserve(500000);
    for (int i = 0; i < 10000; ++i) {
        content += "Line " + std::to_string(i) + " some text content\n";
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    SearchOptions options;
    auto results = engine->search(content, "5000", options);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_GE(results.size(), 1u);
    
    // Should complete quickly (< 500ms for 10K lines)
    EXPECT_LT(duration.count(), 500);
}

TEST_F(SearchEngineTest, ManyMatchesPerformance) {
    // Create text with many matches
    std::string content(100000, 'A');
    
    auto start = std::chrono::high_resolution_clock::now();
    
    SearchOptions options;
    auto results = engine->search(content, "AAA", options);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_GE(results.size(), 1u);
    
    // Should complete in reasonable time
    EXPECT_LT(duration.count(), 2000);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(SearchEngineTest, OverlappingMatches) {
    std::string text = "AAAA";
    
    SearchOptions options;
    auto results = engine->search(text, "AA", options);
    
    // Default: non-overlapping matches
    EXPECT_EQ(results.size(), 2u);
}

TEST_F(SearchEngineTest, SearchAtBoundary) {
    std::string text = "test";
    
    SearchOptions options;
    auto results = engine->search(text, "test", options);
    
    EXPECT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].offset, 0u);
    EXPECT_EQ(results[0].length, 4u);
}

TEST_F(SearchEngineTest, SpecialRegexCharacters) {
    std::string text = "a.b c.d e.f";
    
    SearchOptions options;
    options.useRegex = false;  // Literal search
    auto results = engine->search(text, ".", options);
    
    EXPECT_EQ(results.size(), 3u);
}

TEST_F(SearchEngineTest, ReplaceDoesNotMatchReplacement) {
    std::string text = "aaa";
    
    SearchOptions options;
    auto result = engine->replace(text, "a", "aa", options);
    
    EXPECT_EQ(result, "aaaaaa");
}

// ============================================================================
// SIMD Feature Detection
// ============================================================================

TEST_F(SearchEngineTest, SIMDFeatureDetection) {
    // Just verify these don't crash
    bool hasAVX2 = SearchEngine::hasAVX2Support();
    bool hasSSE42 = SearchEngine::hasSSE42Support();
    
    // At least one should be available on modern systems
    // But don't fail if not (could be running in VM)
    (void)hasAVX2;
    (void)hasSSE42;
    
    EXPECT_TRUE(true);
}

TEST_F(SearchEngineTest, DisableSIMD) {
    engine->setUseSIMD(false);
    EXPECT_FALSE(engine->isUsingSIMD());
    
    // Search should still work without SIMD
    std::string text = "Hello World";
    auto results = engine->search(text, "World", {});
    
    EXPECT_EQ(results.size(), 1u);
}

// ============================================================================
// SIMD-path coverage
//
// The SIMD kernel only runs for patterns >= 4 bytes on text >= 64 bytes. These
// cases ensure that path matches the scalar path: non-overlapping results,
// correct whole-word filtering, and correct line/column. They pass whether or
// not the assembly is compiled in (scalar fallback otherwise).
// ============================================================================

TEST_F(SearchEngineTest, SimdNonOverlappingLongPattern) {
    std::string text(100, 'A');  // long enough to take the SIMD path

    SearchOptions options;
    auto results = engine->search(text, "AAAA", options);

    // 100 / 4 == 25 non-overlapping matches.
    EXPECT_EQ(results.size(), 25u);
    for (size_t i = 0; i < results.size(); ++i) {
        EXPECT_EQ(results[i].offset, i * 4);
    }
}

TEST_F(SearchEngineTest, SimdWholeWordLongText) {
    // Pad so the text exceeds the 64-byte SIMD threshold.
    std::string text = std::string(80, ' ') + "word wordy word";

    SearchOptions options;
    options.wholeWord = true;
    auto results = engine->search(text, "word", options);

    // Two standalone "word"s; the "word" inside "wordy" must not match.
    EXPECT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].offset, 80u);
    EXPECT_EQ(results[1].offset, 91u);
}

TEST_F(SearchEngineTest, SimdLineColumnLongText) {
    std::string text = std::string(70, 'x') + "\nfind ABCD here";

    SearchOptions options;
    auto results = engine->search(text, "ABCD", options);

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].line, 1u);
    EXPECT_EQ(results[0].column, 5u);
}

// ============================================================================
// Cancellation Tests
// ============================================================================

TEST_F(SearchEngineTest, Cancellation) {
    EXPECT_FALSE(engine->isCancelled());

    engine->cancel();
    EXPECT_TRUE(engine->isCancelled());
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
