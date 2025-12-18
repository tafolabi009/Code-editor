/**
 * @file main.cpp
 * @brief Entry point for the Code Editor application
 * 
 * High-performance code editor built with C++ and x86-64 Assembly optimizations.
 * Uses Dear ImGui for the UI and SIMD instructions for hot paths.
 */

#include "ui/window.hpp"
#include "utils/config.hpp"
#include "utils/file_io.hpp"
#include "syntax/highlighter.hpp"
#include <iostream>
#include <cstdlib>
#include <csignal>

// Version info
constexpr const char* APP_NAME = "Code Editor";
constexpr const char* APP_VERSION = "1.0.0";
constexpr int DEFAULT_WIDTH = 1280;
constexpr int DEFAULT_HEIGHT = 720;

// Global config
static utils::Config g_config;

/**
 * @brief Signal handler for graceful shutdown
 */
void signalHandler(int signal) {
    std::cerr << "\nReceived signal " << signal << ", shutting down...\n";
    // Cleanup will happen in Window destructor
    std::exit(signal);
}

/**
 * @brief Print usage information
 */
void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options] [file...]\n\n"
              << "Options:\n"
              << "  -h, --help       Show this help message\n"
              << "  -v, --version    Show version information\n"
              << "  -c, --config     Path to config file\n"
              << "  -n, --new        Start with a new empty file\n"
              << "\n"
              << "Examples:\n"
              << "  " << programName << " main.cpp          Open main.cpp\n"
              << "  " << programName << " *.cpp             Open all .cpp files\n"
              << "  " << programName << " -n                Start with new file\n";
}

/**
 * @brief Print version information
 */
void printVersion() {
    std::cout << APP_NAME << " v" << APP_VERSION << "\n"
              << "Built with C++20 and x86-64 Assembly optimizations\n"
              << "SIMD support: ";
    
#ifdef HAS_ASM_OPTIMIZATIONS
    std::cout << "Enabled (AVX2/SSE4.2)\n";
#else
    std::cout << "Disabled (C++ fallback)\n";
#endif
}

/**
 * @brief Parse command line arguments
 */
struct CommandLineArgs {
    std::vector<std::string> files;
    std::string configPath;
    bool showHelp = false;
    bool showVersion = false;
    bool newFile = false;
};

CommandLineArgs parseArgs(int argc, char* argv[]) {
    CommandLineArgs args;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            args.showHelp = true;
        } else if (arg == "-v" || arg == "--version") {
            args.showVersion = true;
        } else if (arg == "-n" || arg == "--new") {
            args.newFile = true;
        } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            args.configPath = argv[++i];
        } else if (arg[0] != '-') {
            args.files.push_back(arg);
        }
    }
    
    return args;
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    // Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Parse command line arguments
    auto args = parseArgs(argc, argv);
    
    if (args.showHelp) {
        printUsage(argv[0]);
        return 0;
    }
    
    if (args.showVersion) {
        printVersion();
        return 0;
    }
    
    // Load configuration
    if (!args.configPath.empty()) {
        g_config.load(args.configPath);
    } else {
        g_config.load(utils::Config::getDefaultConfigPath());
    }
    
    // Get window dimensions from config
    int width = g_config.get<int>(utils::ConfigKeys::WINDOW_WIDTH, DEFAULT_WIDTH);
    int height = g_config.get<int>(utils::ConfigKeys::WINDOW_HEIGHT, DEFAULT_HEIGHT);
    
    try {
        // Create main window
        ui::Window window(width, height, APP_NAME);
        
        // Apply configuration
        window.getConfig().fontSize = g_config.get<double>(utils::ConfigKeys::FONT_SIZE, 14.0);
        window.getConfig().tabWidth = g_config.get<int>(utils::ConfigKeys::TAB_WIDTH, 4);
        window.getConfig().showLineNumbers = g_config.get<bool>(utils::ConfigKeys::LINE_NUMBERS, true);
        window.getConfig().wordWrap = g_config.get<bool>(utils::ConfigKeys::WORD_WRAP, false);
        window.getConfig().autoIndent = g_config.get<bool>(utils::ConfigKeys::AUTO_INDENT, true);
        window.applyTheme();
        
        // Open files from command line
        if (args.newFile || args.files.empty()) {
            window.newFile();
        }
        
        for (const auto& file : args.files) {
            window.openFile(file);
        }
        
        // Main loop
        while (!window.shouldClose()) {
            window.pollEvents();
            window.beginFrame();
            window.render();
            window.endFrame();
        }
        
        // Save configuration on exit
        g_config.save();
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
