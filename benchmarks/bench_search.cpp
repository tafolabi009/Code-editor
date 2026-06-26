/**
 * @file bench_search.cpp
 * @brief Performance benchmarks for the search engine.
 *
 * Uses the real SearchEngine API: search() returns a std::vector<SearchMatch>
 * over a std::string_view, and replace() returns a new std::string.
 */

#include <benchmark/benchmark.h>
#include "search/search.hpp"

#include <random>
#include <string>
#include <sstream>

using namespace search;

// ============================================================================
// Helpers
// ============================================================================

static std::string generateRandomText(size_t size) {
    static const char chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 \n";
    std::string result;
    result.reserve(size);
    std::mt19937 gen(42);
    std::uniform_int_distribution<> dist(0, sizeof(chars) - 2);
    for (size_t i = 0; i < size; ++i) {
        result += chars[dist(gen)];
    }
    return result;
}

static std::string generateTextWithPatterns(size_t size, const std::string& pattern,
                                             size_t frequency) {
    std::string result;
    result.reserve(size + pattern.size());
    std::mt19937 gen(42);
    std::uniform_int_distribution<> dist(0, 25);
    size_t pos = 0;
    while (result.size() < size) {
        if (pos % frequency == 0) {
            result += pattern;
        } else {
            result += static_cast<char>('a' + dist(gen));
        }
        ++pos;
    }
    return result;
}

static std::string generateCodeText(size_t lines) {
    std::ostringstream ss;
    for (size_t i = 0; i < lines; ++i) {
        ss << "void function" << i << "(int param) {\n";
        ss << "    // TODO: implement\n";
        ss << "    int result = param * 2;\n";
        ss << "    return result;\n";
        ss << "}\n\n";
    }
    return ss.str();
}

// ============================================================================
// Literal search
// ============================================================================

static void BM_Search_ShortPattern(benchmark::State& state) {
    std::string text = generateRandomText(state.range(0));
    SearchEngine engine;
    SearchOptions options;
    options.caseSensitive = true;
    for (auto _ : state) {
        auto results = engine.search(text, "xyz", options);
        benchmark::DoNotOptimize(results.size());
    }
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Search_ShortPattern)->Range(10000, 10000000);

static void BM_Search_LongPattern(benchmark::State& state) {
    std::string text = generateRandomText(state.range(0));
    SearchEngine engine;
    SearchOptions options;
    options.caseSensitive = true;
    for (auto _ : state) {
        auto results = engine.search(text, "thequickbrownfox", options);
        benchmark::DoNotOptimize(results.size());
    }
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Search_LongPattern)->Range(10000, 10000000);

static void BM_Search_WithMatches(benchmark::State& state) {
    std::string text = generateTextWithPatterns(state.range(0), "PATTERN", 100);
    SearchEngine engine;
    SearchOptions options;
    options.caseSensitive = true;
    for (auto _ : state) {
        auto results = engine.search(text, "PATTERN", options);
        benchmark::DoNotOptimize(results.size());
    }
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Search_WithMatches)->Range(10000, 1000000);

// ============================================================================
// SIMD assembly vs scalar memchr path
// ============================================================================

static void BM_Search_Scalar_vs_SIMD(benchmark::State& state) {
    std::string text = generateRandomText(state.range(0));
    SearchEngine engine;
    engine.setUseSIMD(state.range(1) == 1);  // arg1: 0 = scalar, 1 = SIMD (if built)
    SearchOptions options;
    options.caseSensitive = true;
    for (auto _ : state) {
        auto results = engine.search(text, "target", options);
        benchmark::DoNotOptimize(results.size());
    }
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Search_Scalar_vs_SIMD)
    ->Args({10000000, 0})   // scalar (memchr) path
    ->Args({10000000, 1});  // SIMD path (no-op unless ENABLE_ASM)

// ============================================================================
// Case sensitivity
// ============================================================================

static void BM_Search_CaseSensitive(benchmark::State& state) {
    std::string text = generateCodeText(state.range(0));
    SearchEngine engine;
    SearchOptions options;
    options.caseSensitive = true;
    for (auto _ : state) {
        auto results = engine.search(text, "function", options);
        benchmark::DoNotOptimize(results.size());
    }
    state.SetBytesProcessed(state.iterations() * text.size());
}
BENCHMARK(BM_Search_CaseSensitive)->Range(100, 10000);

static void BM_Search_CaseInsensitive(benchmark::State& state) {
    std::string text = generateCodeText(state.range(0));
    SearchEngine engine;
    SearchOptions options;
    options.caseSensitive = false;
    for (auto _ : state) {
        auto results = engine.search(text, "FUNCTION", options);
        benchmark::DoNotOptimize(results.size());
    }
    state.SetBytesProcessed(state.iterations() * text.size());
}
BENCHMARK(BM_Search_CaseInsensitive)->Range(100, 10000);

// ============================================================================
// Regex
// ============================================================================

static void BM_Search_SimpleRegex(benchmark::State& state) {
    std::string text = generateCodeText(state.range(0));
    SearchEngine engine;
    SearchOptions options;
    options.useRegex = true;
    for (auto _ : state) {
        auto results = engine.search(text, R"(function\d+)", options);
        benchmark::DoNotOptimize(results.size());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Search_SimpleRegex)->Range(100, 10000);

static void BM_Search_ComplexRegex(benchmark::State& state) {
    std::string text = generateCodeText(state.range(0));
    SearchEngine engine;
    SearchOptions options;
    options.useRegex = true;
    for (auto _ : state) {
        auto results = engine.search(text, R"(\b(int|void|return)\b)", options);
        benchmark::DoNotOptimize(results.size());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Search_ComplexRegex)->Range(100, 10000);

// ============================================================================
// Whole word
// ============================================================================

static void BM_Search_WholeWord(benchmark::State& state) {
    std::string text = generateCodeText(state.range(0));
    SearchEngine engine;
    SearchOptions options;
    options.caseSensitive = true;
    options.wholeWord = true;
    for (auto _ : state) {
        auto results = engine.search(text, "int", options);
        benchmark::DoNotOptimize(results.size());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Search_WholeWord)->Range(100, 10000);

// ============================================================================
// Replace
// ============================================================================

static void BM_Replace_Literal(benchmark::State& state) {
    std::string text = generateCodeText(state.range(0));
    SearchEngine engine;
    SearchOptions options;
    options.caseSensitive = true;
    for (auto _ : state) {
        std::string out = engine.replace(text, "int", "long", options);
        benchmark::DoNotOptimize(out.size());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Replace_Literal)->Range(100, 10000);

static void BM_Replace_Regex(benchmark::State& state) {
    std::string text = generateCodeText(state.range(0));
    SearchEngine engine;
    SearchOptions options;
    options.useRegex = true;
    for (auto _ : state) {
        std::string out = engine.replace(text, R"(\d+)", "N", options);
        benchmark::DoNotOptimize(out.size());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Replace_Regex)->Range(100, 10000);

BENCHMARK_MAIN();
