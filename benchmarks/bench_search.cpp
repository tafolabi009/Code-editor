/**
 * @file bench_search.cpp
 * @brief Performance benchmarks for Search functionality
 */

#include <benchmark/benchmark.h>
#include "search/search.hpp"
#include "editor/text_buffer.hpp"
#include <random>
#include <string>

using namespace search;
using namespace editor;

// ============================================================================
// Helper Functions
// ============================================================================

static std::string generateRandomText(size_t size) {
    static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 \n";
    std::string result;
    result.reserve(size);
    
    std::mt19937 gen(42);
    std::uniform_int_distribution<> dist(0, sizeof(chars) - 2);
    
    for (size_t i = 0; i < size; ++i) {
        result += chars[dist(gen)];
    }
    return result;
}

static std::string generateTextWithPatterns(size_t size, const std::string& pattern, size_t frequency) {
    std::string result;
    result.reserve(size + pattern.size() * (size / frequency));
    
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
// Basic Search Benchmarks
// ============================================================================

static void BM_Search_ShortPattern(benchmark::State& state) {
    TextBuffer buffer;
    buffer.insert(0, generateRandomText(state.range(0)));
    
    SearchEngine engine;
    SearchOptions options;
    
    for (auto _ : state) {
        auto results = engine.search(buffer, "xyz", options);
        benchmark::DoNotOptimize(results.matches.size());
    }
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Search_ShortPattern)->Range(10000, 10000000);

static void BM_Search_LongPattern(benchmark::State& state) {
    TextBuffer buffer;
    buffer.insert(0, generateRandomText(state.range(0)));
    
    SearchEngine engine;
    SearchOptions options;
    
    for (auto _ : state) {
        auto results = engine.search(buffer, "thequickbrownfox", options);
        benchmark::DoNotOptimize(results.matches.size());
    }
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Search_LongPattern)->Range(10000, 10000000);

static void BM_Search_WithMatches(benchmark::State& state) {
    TextBuffer buffer;
    buffer.insert(0, generateTextWithPatterns(state.range(0), "PATTERN", 100));
    
    SearchEngine engine;
    SearchOptions options;
    
    for (auto _ : state) {
        auto results = engine.search(buffer, "PATTERN", options);
        benchmark::DoNotOptimize(results.matches.size());
    }
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Search_WithMatches)->Range(10000, 1000000);

static void BM_Search_ManyMatches(benchmark::State& state) {
    TextBuffer buffer;
    // Create text with very frequent matches
    std::string text(state.range(0), 'A');
    buffer.insert(0, text);
    
    SearchEngine engine;
    SearchOptions options;
    options.maxResults = 10000;  // Limit to prevent explosion
    
    for (auto _ : state) {
        auto results = engine.search(buffer, "AAA", options);
        benchmark::DoNotOptimize(results.matches.size());
    }
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Search_ManyMatches)->Range(10000, 1000000);

// ============================================================================
// Case Sensitivity Benchmarks
// ============================================================================

static void BM_Search_CaseSensitive(benchmark::State& state) {
    TextBuffer buffer;
    buffer.insert(0, generateCodeText(state.range(0)));
    
    SearchEngine engine;
    SearchOptions options;
    options.caseSensitive = true;
    
    for (auto _ : state) {
        auto results = engine.search(buffer, "function", options);
        benchmark::DoNotOptimize(results.matches.size());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Search_CaseSensitive)->Range(100, 10000);

static void BM_Search_CaseInsensitive(benchmark::State& state) {
    TextBuffer buffer;
    buffer.insert(0, generateCodeText(state.range(0)));
    
    SearchEngine engine;
    SearchOptions options;
    options.caseSensitive = false;
    
    for (auto _ : state) {
        auto results = engine.search(buffer, "FUNCTION", options);
        benchmark::DoNotOptimize(results.matches.size());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Search_CaseInsensitive)->Range(100, 10000);

// ============================================================================
// Regex Search Benchmarks
// ============================================================================

static void BM_Search_SimpleRegex(benchmark::State& state) {
    TextBuffer buffer;
    buffer.insert(0, generateCodeText(state.range(0)));
    
    SearchEngine engine;
    SearchOptions options;
    options.useRegex = true;
    
    for (auto _ : state) {
        auto results = engine.search(buffer, R"(function\d+)", options);
        benchmark::DoNotOptimize(results.matches.size());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Search_SimpleRegex)->Range(100, 10000);

static void BM_Search_ComplexRegex(benchmark::State& state) {
    TextBuffer buffer;
    buffer.insert(0, generateCodeText(state.range(0)));
    
    SearchEngine engine;
    SearchOptions options;
    options.useRegex = true;
    
    for (auto _ : state) {
        auto results = engine.search(buffer, R"(\b(int|void|return)\b)", options);
        benchmark::DoNotOptimize(results.matches.size());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Search_ComplexRegex)->Range(100, 10000);

static void BM_Search_RegexVsLiteral(benchmark::State& state) {
    TextBuffer buffer;
    buffer.insert(0, generateCodeText(state.range(0)));
    
    SearchEngine engine;
    SearchOptions options;
    options.useRegex = state.range(1) == 1;
    
    std::string pattern = options.useRegex ? R"(TODO)" : "TODO";
    
    for (auto _ : state) {
        auto results = engine.search(buffer, pattern, options);
        benchmark::DoNotOptimize(results.matches.size());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Search_RegexVsLiteral)
    ->Args({1000, 0})   // Literal
    ->Args({1000, 1})   // Regex
    ->Args({10000, 0})
    ->Args({10000, 1});

// ============================================================================
// Replace Benchmarks
// ============================================================================

static void BM_ReplaceAll_Few(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        TextBuffer buffer;
        buffer.insert(0, generateCodeText(state.range(0)));
        SearchEngine engine;
        SearchOptions options;
        state.ResumeTiming();
        
        int count = engine.replaceAll(buffer, "TODO", "DONE", options);
        benchmark::DoNotOptimize(count);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ReplaceAll_Few)->Range(100, 10000);

static void BM_ReplaceAll_Many(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        TextBuffer buffer;
        buffer.insert(0, generateCodeText(state.range(0)));
        SearchEngine engine;
        SearchOptions options;
        state.ResumeTiming();
        
        int count = engine.replaceAll(buffer, "int", "long", options);
        benchmark::DoNotOptimize(count);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ReplaceAll_Many)->Range(100, 10000);

static void BM_ReplaceAll_Regex(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        TextBuffer buffer;
        buffer.insert(0, generateCodeText(state.range(0)));
        SearchEngine engine;
        SearchOptions options;
        options.useRegex = true;
        state.ResumeTiming();
        
        int count = engine.replaceAll(buffer, R"(\d+)", "N", options);
        benchmark::DoNotOptimize(count);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ReplaceAll_Regex)->Range(100, 10000);

// ============================================================================
// Navigation Benchmarks
// ============================================================================

static void BM_FindNext(benchmark::State& state) {
    TextBuffer buffer;
    buffer.insert(0, generateTextWithPatterns(100000, "PATTERN", 1000));
    
    SearchEngine engine;
    SearchOptions options;
    engine.search(buffer, "PATTERN", options);
    
    for (auto _ : state) {
        auto match = engine.findNext();
        benchmark::DoNotOptimize(match);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_FindNext)->Iterations(100000);

static void BM_FindPrevious(benchmark::State& state) {
    TextBuffer buffer;
    buffer.insert(0, generateTextWithPatterns(100000, "PATTERN", 1000));
    
    SearchEngine engine;
    SearchOptions options;
    engine.search(buffer, "PATTERN", options);
    
    for (auto _ : state) {
        auto match = engine.findPrevious();
        benchmark::DoNotOptimize(match);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_FindPrevious)->Iterations(100000);

// ============================================================================
// SIMD vs Non-SIMD Comparison
// ============================================================================

static void BM_Search_SimulatedSIMD(benchmark::State& state) {
    // This would compare SIMD vs scalar if we had runtime detection
    TextBuffer buffer;
    buffer.insert(0, generateRandomText(state.range(0)));
    
    SearchEngine engine;
    SearchOptions options;
    
    for (auto _ : state) {
        auto results = engine.search(buffer, "target", options);
        benchmark::DoNotOptimize(results.matches.size());
    }
    
    // Report throughput in GB/s
    double gb = static_cast<double>(state.iterations() * state.range(0)) / (1024.0 * 1024.0 * 1024.0);
    state.counters["Throughput_GB_s"] = benchmark::Counter(
        gb,
        benchmark::Counter::kIsRate
    );
}
BENCHMARK(BM_Search_SimulatedSIMD)->Range(1000000, 100000000);

// ============================================================================
// Whole Word Search
// ============================================================================

static void BM_Search_WholeWord(benchmark::State& state) {
    TextBuffer buffer;
    buffer.insert(0, generateCodeText(state.range(0)));
    
    SearchEngine engine;
    SearchOptions options;
    options.wholeWord = true;
    
    for (auto _ : state) {
        auto results = engine.search(buffer, "int", options);
        benchmark::DoNotOptimize(results.matches.size());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Search_WholeWord)->Range(100, 10000);

// ============================================================================
// Incremental Search
// ============================================================================

static void BM_IncrementalSearch(benchmark::State& state) {
    TextBuffer buffer;
    buffer.insert(0, generateCodeText(10000));
    
    SearchEngine engine;
    SearchOptions options;
    
    std::string fullPattern = "function";
    
    for (auto _ : state) {
        // Simulate typing pattern character by character
        for (size_t i = 1; i <= fullPattern.size(); ++i) {
            std::string partialPattern = fullPattern.substr(0, i);
            auto results = engine.search(buffer, partialPattern, options);
            benchmark::DoNotOptimize(results.matches.size());
        }
    }
    state.SetItemsProcessed(state.iterations() * fullPattern.size());
}
BENCHMARK(BM_IncrementalSearch);

// ============================================================================
// Memory Allocation During Search
// ============================================================================

static void BM_Search_MemoryEfficiency(benchmark::State& state) {
    TextBuffer buffer;
    buffer.insert(0, generateTextWithPatterns(state.range(0), "PATTERN", 100));
    
    SearchEngine engine;
    SearchOptions options;
    
    for (auto _ : state) {
        auto results = engine.search(buffer, "PATTERN", options);
        
        // Report match count
        state.counters["Matches"] = results.matches.size();
        benchmark::DoNotOptimize(results);
    }
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Search_MemoryEfficiency)->Range(10000, 1000000);

// ============================================================================
// Main
// ============================================================================

BENCHMARK_MAIN();
