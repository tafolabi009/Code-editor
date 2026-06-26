/**
 * @file bench_text_buffer.cpp
 * @brief Performance benchmarks for TextBuffer
 */

#include <benchmark/benchmark.h>
#include "editor/text_buffer.hpp"
#include <random>
#include <string>
#include <sstream>

using namespace editor;

// ============================================================================
// Helper Functions
// ============================================================================

static std::string generateRandomText(size_t size) {
    static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 \n";
    std::string result;
    result.reserve(size);
    
    std::mt19937 gen(42);  // Fixed seed for reproducibility
    std::uniform_int_distribution<> dist(0, sizeof(chars) - 2);
    
    for (size_t i = 0; i < size; ++i) {
        result += chars[dist(gen)];
    }
    return result;
}

static std::string generateCodeLikeText(size_t lines) {
    std::ostringstream ss;
    for (size_t i = 0; i < lines; ++i) {
        ss << "    void function" << i << "(int param) {\n";
        ss << "        // Comment line " << i << "\n";
        ss << "        int result = param * 2;\n";
        ss << "        return result;\n";
        ss << "    }\n\n";
    }
    return ss.str();
}

// ============================================================================
// GapBuffer Benchmarks
// ============================================================================

static void BM_GapBuffer_InsertEnd(benchmark::State& state) {
    for (auto _ : state) {
        GapBuffer buffer;
        for (int i = 0; i < state.range(0); ++i) {
            buffer.insert(buffer.size(), 'A');
        }
        benchmark::DoNotOptimize(buffer.size());
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_GapBuffer_InsertEnd)->Range(1000, 1000000);

static void BM_GapBuffer_InsertBeginning(benchmark::State& state) {
    for (auto _ : state) {
        GapBuffer buffer;
        for (int i = 0; i < state.range(0); ++i) {
            buffer.insert(0, 'A');
        }
        benchmark::DoNotOptimize(buffer.size());
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_GapBuffer_InsertBeginning)->Range(1000, 100000);

static void BM_GapBuffer_InsertMiddle(benchmark::State& state) {
    GapBuffer buffer;
    std::string initial(state.range(0), 'X');
    buffer.insert(0, initial);
    
    for (auto _ : state) {
        size_t pos = buffer.size() / 2;
        buffer.insert(pos, 'A');
        benchmark::DoNotOptimize(buffer.size());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_GapBuffer_InsertMiddle)->Range(1000, 1000000);

static void BM_GapBuffer_InsertString(benchmark::State& state) {
    std::string chunk(100, 'A');
    
    for (auto _ : state) {
        GapBuffer buffer;
        for (int i = 0; i < state.range(0); ++i) {
            buffer.insert(buffer.size(), chunk);
        }
        benchmark::DoNotOptimize(buffer.size());
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_GapBuffer_InsertString)->Range(100, 10000);

static void BM_GapBuffer_DeleteEnd(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        GapBuffer buffer;
        buffer.insert(0, std::string(state.range(0), 'A'));
        state.ResumeTiming();
        
        while (!buffer.empty()) {
            buffer.erase(buffer.size() - 1, 1);
        }
        benchmark::DoNotOptimize(buffer.size());
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_GapBuffer_DeleteEnd)->Range(1000, 100000);

static void BM_GapBuffer_GetText(benchmark::State& state) {
    GapBuffer buffer;
    buffer.insert(0, std::string(state.range(0), 'A'));
    
    for (auto _ : state) {
        auto text = buffer.getText();
        benchmark::DoNotOptimize(text.data());
    }
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_GapBuffer_GetText)->Range(1000, 10000000);

static void BM_GapBuffer_RandomAccess(benchmark::State& state) {
    GapBuffer buffer;
    buffer.insert(0, std::string(state.range(0), 'A'));
    
    std::mt19937 gen(42);
    std::uniform_int_distribution<size_t> dist(0, state.range(0) - 1);
    
    for (auto _ : state) {
        char c = buffer.at(dist(gen));
        benchmark::DoNotOptimize(c);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_GapBuffer_RandomAccess)->Range(1000, 1000000);

// ============================================================================
// TextBuffer Benchmarks
// ============================================================================

static void BM_TextBuffer_InsertText(benchmark::State& state) {
    std::string text = generateRandomText(1000);
    
    for (auto _ : state) {
        TextBuffer buffer;
        for (int i = 0; i < state.range(0); ++i) {
            buffer.insert(buffer.size(), text);
        }
        benchmark::DoNotOptimize(buffer.size());
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_TextBuffer_InsertText)->Range(10, 1000);

static void BM_TextBuffer_LineCount(benchmark::State& state) {
    TextBuffer buffer;
    buffer.insert(0, generateCodeLikeText(state.range(0)));
    
    for (auto _ : state) {
        auto count = buffer.lineCount();
        benchmark::DoNotOptimize(count);
    }
}
BENCHMARK(BM_TextBuffer_LineCount)->Range(100, 100000);

static void BM_TextBuffer_GetLine(benchmark::State& state) {
    TextBuffer buffer;
    buffer.insert(0, generateCodeLikeText(state.range(0)));
    
    std::mt19937 gen(42);
    std::uniform_int_distribution<size_t> dist(0, state.range(0) - 1);
    
    for (auto _ : state) {
        auto line = buffer.getLine(dist(gen));
        benchmark::DoNotOptimize(line.data());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_TextBuffer_GetLine)->Range(100, 100000);

static void BM_TextBuffer_PositionToOffset(benchmark::State& state) {
    TextBuffer buffer;
    buffer.insert(0, generateCodeLikeText(state.range(0)));
    size_t lineCount = buffer.lineCount();
    
    std::mt19937 gen(42);
    std::uniform_int_distribution<size_t> dist(0, lineCount - 1);
    
    for (auto _ : state) {
        Position pos{dist(gen), 10};
        auto offset = buffer.positionToOffset(pos);
        benchmark::DoNotOptimize(offset);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_TextBuffer_PositionToOffset)->Range(100, 100000);

static void BM_TextBuffer_OffsetToPosition(benchmark::State& state) {
    TextBuffer buffer;
    buffer.insert(0, generateCodeLikeText(state.range(0)));
    size_t size = buffer.size();
    
    std::mt19937 gen(42);
    std::uniform_int_distribution<size_t> dist(0, size - 1);
    
    for (auto _ : state) {
        auto pos = buffer.offsetToPosition(dist(gen));
        benchmark::DoNotOptimize(pos);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_TextBuffer_OffsetToPosition)->Range(100, 100000);

static void BM_TextBuffer_UndoRedo(benchmark::State& state) {
    for (auto _ : state) {
        TextBuffer buffer;
        
        // Make many edits
        for (int i = 0; i < state.range(0); ++i) {
            buffer.insert(buffer.size(), "X");
        }
        
        // Undo all
        for (int i = 0; i < state.range(0); ++i) {
            buffer.undo();
        }
        
        // Redo all
        for (int i = 0; i < state.range(0); ++i) {
            buffer.redo();
        }
        
        benchmark::DoNotOptimize(buffer.size());
    }
    state.SetItemsProcessed(state.iterations() * state.range(0) * 3);
}
BENCHMARK(BM_TextBuffer_UndoRedo)->Range(100, 10000);

// ============================================================================
// Realistic Editing Patterns
// ============================================================================

static void BM_SimulateTyping(benchmark::State& state) {
    std::string typedText = "The quick brown fox jumps over the lazy dog. ";
    
    for (auto _ : state) {
        TextBuffer buffer;
        
        // Simulate typing character by character
        for (int rep = 0; rep < state.range(0); ++rep) {
            for (char c : typedText) {
                buffer.insert(buffer.size(), std::string(1, c));
            }
        }
        
        benchmark::DoNotOptimize(buffer.size());
    }
    state.SetItemsProcessed(state.iterations() * state.range(0) * typedText.size());
}
BENCHMARK(BM_SimulateTyping)->Range(10, 1000);

static void BM_SimulateLineEditing(benchmark::State& state) {
    TextBuffer buffer;
    buffer.insert(0, generateCodeLikeText(1000));
    
    std::mt19937 gen(42);
    std::uniform_int_distribution<size_t> lineDist(0, buffer.lineCount() - 1);
    
    for (auto _ : state) {
        size_t line = lineDist(gen);
        size_t lineStart = buffer.lineStart(line);
        size_t lineLen = buffer.lineLength(line);
        
        // Delete line content
        buffer.erase(lineStart, lineLen);
        
        // Insert new content
        buffer.insert(lineStart, "    // Modified line content");
        
        benchmark::DoNotOptimize(buffer.size());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SimulateLineEditing)->Iterations(10000);

static void BM_SimulateCopyPaste(benchmark::State& state) {
    TextBuffer buffer;
    buffer.insert(0, generateCodeLikeText(state.range(0)));
    
    std::string clipboard;
    
    std::mt19937 gen(42);
    std::uniform_int_distribution<size_t> lineDist(0, buffer.lineCount() - 10);
    
    for (auto _ : state) {
        // Copy 5 lines
        size_t startLine = lineDist(gen);
        size_t startOffset = buffer.lineStart(startLine);
        size_t endOffset = buffer.lineStart(startLine + 5);
        clipboard = buffer.getText().substr(startOffset, endOffset - startOffset);
        
        // Paste at another location
        size_t pasteLine = lineDist(gen);
        size_t pasteOffset = buffer.lineStart(pasteLine);
        buffer.insert(pasteOffset, clipboard);
        
        benchmark::DoNotOptimize(buffer.size());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SimulateCopyPaste)->Range(100, 10000);

// ============================================================================
// Memory Efficiency
// ============================================================================

static void BM_MemoryUsage(benchmark::State& state) {
    for (auto _ : state) {
        GapBuffer buffer;
        buffer.insert(0, std::string(state.range(0), 'A'));
        
        // Report memory efficiency
        size_t dataSize = buffer.size();
        size_t capacity = buffer.capacity();
        
        benchmark::DoNotOptimize(dataSize);
        benchmark::DoNotOptimize(capacity);
    }
    state.SetBytesProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_MemoryUsage)->Range(1000, 10000000);

// ============================================================================
// Main
// ============================================================================

BENCHMARK_MAIN();
