cmake_minimum_required(VERSION 3.20)
project(CodeEditor VERSION 1.0.0 LANGUAGES CXX)

# C++17 standard (for broader compatibility, C++20 features used are limited)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Option to enable ASM optimizations (disable for cross-compilation by default)
option(ENABLE_ASM "Enable x86-64 Assembly optimizations" OFF)
option(BUILD_GUI "Build GUI components (requires OpenGL)" ON)
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(CI_BUILD "CI build mode (treats warnings as errors)" OFF)

# Cross-compilation detection
if(CMAKE_CROSSCOMPILING)
    message(STATUS "Cross-compiling for ${CMAKE_SYSTEM_NAME}")
    set(CROSS_COMPILE_FLAGS "")
else()
    # Native compilation can use -march=native
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CROSS_COMPILE_FLAGS "-march=native -mtune=native")
    endif()
endif()

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    if(NOT CMAKE_CROSSCOMPILING)
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 ${CROSS_COMPILE_FLAGS} -flto")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto")
    endif()
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
    
    # Enable SIMD flags only for x86_64 and when not cross-compiling without proper support
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64" AND NOT CMAKE_CROSSCOMPILING)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -msse4.2")
    elseif(CMAKE_CROSSCOMPILING AND WIN32)
        # MinGW cross-compilation: use baseline x86-64 with SSE2 (universally supported)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2")
    endif()
    
    # Treat warnings as errors in CI builds
    if(CI_BUILD OR DEFINED ENV{CI})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    endif()
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /arch:AVX2")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /GL /LTCG")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /DDEBUG")
    if(CI_BUILD OR DEFINED ENV{CI})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
    endif()
endif()

# NASM flags (only when ASM is enabled)
if(ENABLE_ASM AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    if(WIN32)
        set(CMAKE_ASM_NASM_FLAGS "-f win64")
    elseif(APPLE)
        set(CMAKE_ASM_NASM_FLAGS "-f macho64")
    else()
        set(CMAKE_ASM_NASM_FLAGS "-f elf64")
    endif()
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include FetchContent for dependencies
include(FetchContent)

# JSON library for configuration (always needed)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# GUI dependencies (optional)
if(BUILD_GUI)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 CONFIG QUIET)
    if(NOT glfw3_FOUND)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GLFW3 REQUIRED glfw3)
    endif()
    
    # ImGui as subdirectory
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.90.1
    )
    FetchContent_MakeAvailable(imgui)

    # ImGui library
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC 
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Core library sources (no GUI dependency)
set(CORE_SOURCES
    src/editor/text_buffer.cpp
    src/editor/cursor.cpp
    src/editor/selection.cpp
    src/editor/clipboard.cpp
    src/syntax/highlighter.cpp
    src/search/search.cpp
    src/utils/profiler.cpp
    src/utils/config.cpp
    src/utils/file_io.cpp
)

# GUI sources
set(GUI_SOURCES
    src/main.cpp
    src/ui/window.cpp
    src/ui/tabs.cpp
    src/ui/statusbar.cpp
)

# Assembly source files (SIMD optimizations)
set(ASM_SOURCES
    src/asm/search_simd.asm
    src/asm/allocator.asm
    src/asm/utf8_validate.asm
)

# Check if NASM is available and ASM is enabled
if(ENABLE_ASM)
    find_program(NASM_FOUND nasm)
    if(NASM_FOUND AND NOT CMAKE_CROSSCOMPILING)
        enable_language(ASM_NASM)
        list(APPEND CORE_SOURCES ${ASM_SOURCES})
        add_compile_definitions(HAS_ASM_OPTIMIZATIONS)
        message(STATUS "NASM found - ASM optimizations enabled")
    else()
        message(WARNING "NASM not found or cross-compiling - using C++ fallback implementations")
    endif()
else()
    message(STATUS "ASM optimizations disabled - using C++ fallback implementations")
endif()

# Core library (always built)
add_library(editor_core STATIC ${CORE_SOURCES})
target_link_libraries(editor_core PUBLIC nlohmann_json::nlohmann_json)
if(UNIX AND NOT APPLE)
    target_link_libraries(editor_core PUBLIC pthread)
endif()

# Main executable (only built if GUI is enabled)
if(BUILD_GUI)
    add_executable(code_editor ${GUI_SOURCES})
    target_link_libraries(code_editor PRIVATE
        editor_core
        imgui
        ${CMAKE_DL_LIBS}
    )
    
    # Enable position independent code for shared libraries
    set_property(TARGET code_editor PROPERTY POSITION_INDEPENDENT_CODE ON)
    
    # Install
    install(TARGETS code_editor DESTINATION bin)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
    
    add_executable(test_text_buffer tests/test_text_buffer.cpp)
    target_link_libraries(test_text_buffer PRIVATE editor_core GTest::gtest_main)
    
    add_executable(test_search tests/test_search.cpp)
    target_link_libraries(test_search PRIVATE editor_core GTest::gtest_main)
    
    include(GoogleTest)
    gtest_discover_tests(test_text_buffer)
    gtest_discover_tests(test_search)
endif()

# Benchmarks
if(BUILD_BENCHMARKS)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)
    
    add_executable(bench_search benchmarks/bench_search.cpp)
    target_link_libraries(bench_search PRIVATE editor_core benchmark::benchmark)
    
    add_executable(bench_text_buffer benchmarks/bench_text_buffer.cpp)
    target_link_libraries(bench_text_buffer PRIVATE editor_core benchmark::benchmark)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "=== Code Editor Build Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ Flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "ASM Optimizations: ${NASM_FOUND}")
message(STATUS "Build GUI: ${BUILD_GUI}")
message(STATUS "Build Tests: ${BUILD_TESTS}")
message(STATUS "Build Benchmarks: ${BUILD_BENCHMARKS}")
message(STATUS "========================================")
