# High-Performance Code Editor

A high-performance code editor built from scratch in C++17/20 with optimized x86-64 Assembly for performance-critical paths. Focus on speed, minimal memory footprint, and sophisticated low-level optimizations.

## Features

- **Blazing Fast Performance**
  - < 100ms cold startup time
  - < 50MB RAM for typical usage
  - 10GB/sec search throughput with SIMD (AVX2/SSE4.2)
  - 60 FPS UI with ImGui

- **Core Editor Features**
  - Gap buffer text storage for O(1) cursor-local edits
  - Unlimited undo/redo with memory-efficient deltas
  - Multi-cursor and block selection support
  - Line numbers with configurable display

- **Syntax Highlighting**
  - C/C++, Python, JavaScript support
  - Extensible tokenizer architecture
  - Real-time highlighting with incremental updates

- **Search & Replace**
  - SIMD-accelerated literal search
  - Full regex support with capture groups
  - Case-sensitive/insensitive modes
  - Whole word matching

- **Modern UI**
  - Dear ImGui-based interface
  - Tab bar for multi-file editing
  - Status bar with cursor position, encoding info
  - Dark theme by default

## Architecture

```
Code-editor/
├── include/
│   ├── editor/          # Core editor components
│   │   ├── text_buffer.hpp   # Gap buffer implementation
│   │   ├── cursor.hpp        # Cursor navigation
│   │   ├── selection.hpp     # Text selection
│   │   └── clipboard.hpp     # Clipboard with history
│   ├── ui/              # User interface
│   │   ├── window.hpp        # Main window management
│   │   ├── tabs.hpp          # Tab bar
│   │   └── statusbar.hpp     # Status bar
│   ├── syntax/          # Syntax highlighting
│   │   └── highlighter.hpp   # Tokenizer and highlighting
│   ├── search/          # Search functionality
│   │   └── search.hpp        # Search engine
│   ├── memory/          # Memory management
│   │   └── allocator.hpp     # Custom allocator
│   └── utils/           # Utilities
│       ├── utf8.hpp          # UTF-8 handling
│       ├── file_io.hpp       # File operations
│       └── config.hpp        # Configuration
├── src/
│   ├── main.cpp         # Application entry point
│   ├── editor/          # Editor implementations
│   ├── ui/              # UI implementations
│   ├── syntax/          # Syntax implementations
│   ├── search/          # Search implementations
│   ├── utils/           # Utility implementations
│   └── asm/             # x86-64 Assembly optimizations
│       ├── search_simd.asm   # AVX2/SSE4.2 search
│       ├── utf8_validate.asm # SIMD UTF-8 validation
│       └── allocator.asm     # SIMD memory ops
├── tests/               # Unit tests (Google Test)
├── benchmarks/          # Performance benchmarks (Google Benchmark)
└── CMakeLists.txt       # Build configuration
```

## Building

### Prerequisites

- CMake 3.20+
- C++17 compatible compiler (GCC 9+, Clang 10+, MSVC 2019+)
- NASM (for assembly optimizations)
- OpenGL development libraries
- GLFW (fetched automatically)

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/yourusername/Code-editor.git
cd Code-editor

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --parallel

# Run
./code_editor
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_ASM` | ON | Enable x86-64 Assembly optimizations |
| `ENABLE_TESTS` | ON | Build unit tests |
| `ENABLE_BENCHMARKS` | ON | Build performance benchmarks |
| `CMAKE_BUILD_TYPE` | Release | Build type (Debug/Release/RelWithDebInfo) |

### Running Tests

```bash
cd build
ctest --output-on-failure
```

### Running Benchmarks

```bash
./bench_text_buffer
./bench_search
```

## Performance Targets

| Metric | Target | Implementation |
|--------|--------|---------------|
| Startup Time | < 100ms | Lazy initialization, minimal dependencies |
| Memory Usage | < 50MB | Gap buffer, efficient undo storage |
| Search Throughput | 10GB/sec | AVX2 SIMD with 32-byte processing |
| UTF-8 Validation | 5GB/sec | SSE4.2 SIMD validation |
| UI Framerate | 60 FPS | ImGui immediate mode |

## Key Components

### Gap Buffer (`text_buffer.hpp`)

The text storage uses a gap buffer data structure for efficient insertions and deletions at the cursor position:

```cpp
// O(1) insertion at cursor position
buffer.insert(cursorPos, "text");

// O(n) only when gap needs to move
buffer.insert(farPosition, "text");
```

### SIMD Search (`search_simd.asm`)

String search uses AVX2 instructions to process 32 bytes per iteration:

```asm
; Load 32 bytes of text and compare against pattern
vmovdqu ymm1, [rsi]
vpcmpeqb ymm2, ymm1, ymm0
vpmovmskb eax, ymm2
```

### Syntax Highlighting (`highlighter.hpp`)

The tokenizer produces colored tokens for real-time syntax highlighting:

```cpp
// Language definition
LanguageDef cpp;
cpp.keywords = {"if", "else", "for", "while", ...};
cpp.types = {"int", "void", "char", ...};

// Tokenize and highlight
auto tokens = highlighter.tokenize(buffer, cpp);
```

## Configuration

Configuration is stored in JSON format:

```json
{
  "editor": {
    "tabSize": 4,
    "useSpaces": true,
    "showLineNumbers": true
  },
  "theme": {
    "name": "dark",
    "background": "#1e1e1e",
    "foreground": "#d4d4d4"
  },
  "syntax": {
    "keyword": "#569cd6",
    "string": "#ce9178",
    "comment": "#6a9955"
  }
}
```

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+N | New file |
| Ctrl+O | Open file |
| Ctrl+S | Save file |
| Ctrl+Z | Undo |
| Ctrl+Y | Redo |
| Ctrl+F | Find |
| Ctrl+H | Find & Replace |
| Ctrl+G | Go to line |
| Ctrl+W | Close tab |
| Ctrl+Tab | Next tab |

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests: `ctest --output-on-failure`
5. Submit a pull request

## License

MIT License - see LICENSE file for details.

## Acknowledgments

- [Dear ImGui](https://github.com/ocornut/imgui) - Immediate mode GUI
- [GLFW](https://www.glfw.org/) - Window and input handling
- [nlohmann/json](https://github.com/nlohmann/json) - JSON parsing
- [Google Test](https://github.com/google/googletest) - Unit testing
- [Google Benchmark](https://github.com/google/benchmark) - Performance benchmarking