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

# Include FetchContent for dependencies (used only as a fallback when a
# system-installed package is not found, so the project builds offline / in CI
# without network access to clone dependencies).
include(FetchContent)

# JSON library for configuration (always needed).
# Prefer a system-installed nlohmann_json; fall back to fetching it.
find_package(nlohmann_json 3.11 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found on system - fetching via FetchContent")
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(json)
else()
    message(STATUS "Using system nlohmann_json ${nlohmann_json_VERSION}")
endif()

# GUI dependencies (optional)
option(USE_SYSTEM_IMGUI "Link against a system-installed Dear ImGui instead of fetching it" OFF)

if(BUILD_GUI)
    find_package(OpenGL REQUIRED)

    # GLFW: prefer a CMake config package, fall back to pkg-config. Resolve a
    # concrete target name so downstream linking works in both cases.
    find_package(glfw3 CONFIG QUIET)
    if(glfw3_FOUND)
        set(GLFW_TARGET glfw)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GLFW3 REQUIRED IMPORTED_TARGET glfw3)
        set(GLFW_TARGET PkgConfig::GLFW3)
    endif()

    # Decide whether a usable system Dear ImGui is available.
    set(_imgui_use_system OFF)
    if(USE_SYSTEM_IMGUI)
        find_path(IMGUI_INCLUDE_DIR imgui.h PATH_SUFFIXES imgui)
        find_library(IMGUI_LIBRARY NAMES imgui)
        if(IMGUI_INCLUDE_DIR AND IMGUI_LIBRARY AND
           EXISTS "${IMGUI_INCLUDE_DIR}/backends/imgui_impl_glfw.h")
            set(_imgui_use_system ON)
        else()
            message(WARNING "USE_SYSTEM_IMGUI requested but a complete system "
                            "Dear ImGui was not found; falling back to FetchContent")
        endif()
    endif()

    if(_imgui_use_system)
        message(STATUS "Using system Dear ImGui: ${IMGUI_LIBRARY}")

        # Some distributions (e.g. Debian) ship libimgui without the bundled stb
        # implementation, expecting stb_truetype / stb_rect_pack to be provided
        # externally. When that is the case, compile a tiny implementation TU and
        # link it AFTER the imgui library so the linker can resolve the symbols.
        option(IMGUI_SYSTEM_NEEDS_STB
               "System Dear ImGui needs external stb_truetype/stb_rect_pack" ON)

        add_library(imgui INTERFACE)
        target_include_directories(imgui INTERFACE
            ${IMGUI_INCLUDE_DIR}
            ${IMGUI_INCLUDE_DIR}/backends
        )

        if(IMGUI_SYSTEM_NEEDS_STB)
            find_path(STB_INCLUDE_DIR stb_truetype.h PATH_SUFFIXES stb)
            if(STB_INCLUDE_DIR)
                set(_stb_rp "${CMAKE_BINARY_DIR}/imgui_stb_rectpack.cpp")
                set(_stb_tt "${CMAKE_BINARY_DIR}/imgui_stb_truetype.cpp")
                file(WRITE "${_stb_rp}"
                     "#define STB_RECT_PACK_IMPLEMENTATION\n#include <stb/stb_rect_pack.h>\n")
                file(WRITE "${_stb_tt}"
                     "#define STB_TRUETYPE_IMPLEMENTATION\n#include <stb/stb_truetype.h>\n")
                add_library(imgui_stb STATIC "${_stb_rp}" "${_stb_tt}")
                target_include_directories(imgui_stb PRIVATE ${STB_INCLUDE_DIR})
                target_compile_options(imgui_stb PRIVATE -w)  # silence third-party warnings
                # Order matters: libimgui references stb, so stb must follow it.
                target_link_libraries(imgui INTERFACE ${IMGUI_LIBRARY} imgui_stb)
            else()
                message(WARNING "IMGUI_SYSTEM_NEEDS_STB is ON but stb headers were "
                                "not found; linking may fail. Install libstb-dev "
                                "or set -DIMGUI_SYSTEM_NEEDS_STB=OFF.")
                target_link_libraries(imgui INTERFACE ${IMGUI_LIBRARY})
            endif()
        else()
            target_link_libraries(imgui INTERFACE ${IMGUI_LIBRARY})
        endif()

        target_link_libraries(imgui INTERFACE ${GLFW_TARGET} OpenGL::GL)
    else()
        # Default: fetch and build Dear ImGui (with its bundled stb) from source.
        FetchContent_Declare(
            imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG v1.90.1
        )
        FetchContent_MakeAvailable(imgui)

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
        target_link_libraries(imgui PUBLIC ${GLFW_TARGET} OpenGL::GL)
    endif()
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
    
    # Prefer a system-installed GTest; fall back to fetching it.
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found on system - fetching via FetchContent")
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0
        )
        # Prevent googletest from overriding our compiler/linker options on MSVC
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    else()
        message(STATUS "Using system GTest ${GTest_VERSION}")
    endif()

    add_executable(test_text_buffer tests/test_text_buffer.cpp)
    target_link_libraries(test_text_buffer PRIVATE editor_core GTest::gtest_main)
    
    add_executable(test_search tests/test_search.cpp)
    target_link_libraries(test_search PRIVATE editor_core GTest::gtest_main)

    add_executable(test_highlighter tests/test_highlighter.cpp)
    target_link_libraries(test_highlighter PRIVATE editor_core GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(test_text_buffer)
    gtest_discover_tests(test_search)
    gtest_discover_tests(test_highlighter)
endif()

# Benchmarks
if(BUILD_BENCHMARKS)
    find_package(benchmark QUIET)
    if(NOT benchmark_FOUND)
        message(STATUS "Google Benchmark not found on system - fetching via FetchContent")
        FetchContent_Declare(
            benchmark
            GIT_REPOSITORY https://github.com/google/benchmark.git
            GIT_TAG v1.8.3
        )
        set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(benchmark)
    else()
        message(STATUS "Using system Google Benchmark ${benchmark_VERSION}")
    endif()

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
