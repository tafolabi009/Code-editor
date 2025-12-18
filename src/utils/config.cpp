/**
 * @file config.cpp
 * @brief Configuration management implementation
 */

#include "utils/config.hpp"
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <algorithm>

using json = nlohmann::json;

namespace utils {

// ============================================================================
// Config Implementation
// ============================================================================

Config::Config() {
    setDefaults();
}

Config::Config(const std::string& path) : m_path(path) {
    setDefaults();
    load(path);
}

Config::~Config() = default;

bool Config::load(const std::string& path) {
    m_path = path;
    
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    parseJson(content);
    m_modified = false;
    return true;
}

bool Config::save(const std::string& path) const {
    // Create directory if it doesn't exist
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }
    
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    file << toJson();
    return file.good();
}

bool Config::save() const {
    if (m_path.empty()) {
        return false;
    }
    return save(m_path);
}

std::optional<ConfigValue> Config::get(const std::string& key) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool Config::has(const std::string& key) const {
    return m_values.find(key) != m_values.end();
}

void Config::remove(const std::string& key) {
    m_values.erase(key);
    m_modified = true;
}

void Config::clear() {
    m_values.clear();
    m_modified = true;
}

void Config::setDefaults() {
    // Editor settings
    set(ConfigKeys::TAB_WIDTH, 4);
    set(ConfigKeys::INSERT_SPACES, true);
    set(ConfigKeys::LINE_NUMBERS, true);
    set(ConfigKeys::RELATIVE_LINE_NUMBERS, false);
    set(ConfigKeys::WORD_WRAP, false);
    set(ConfigKeys::AUTO_INDENT, true);
    set(ConfigKeys::HIGHLIGHT_CURRENT_LINE, true);
    set(ConfigKeys::SHOW_WHITESPACE, false);
    set(ConfigKeys::MINIMAP, false);
    set(ConfigKeys::SCROLL_SPEED, 3.0);
    set(ConfigKeys::FONT_SIZE, 14);
    set(ConfigKeys::FONT_FAMILY, std::string("JetBrains Mono"));
    
    // Theme
    set(ConfigKeys::THEME, std::string("dark"));
    set(ConfigKeys::THEME_BACKGROUND, std::string("#1e1e1e"));
    set(ConfigKeys::THEME_FOREGROUND, std::string("#d4d4d4"));
    set(ConfigKeys::THEME_SELECTION, std::string("#264f78"));
    set(ConfigKeys::THEME_CURSOR, std::string("#ffffff"));
    
    // Files
    set(ConfigKeys::AUTO_SAVE, false);
    set(ConfigKeys::AUTO_SAVE_DELAY, 1000);
    set(ConfigKeys::DEFAULT_ENCODING, std::string("UTF-8"));
    set(ConfigKeys::DEFAULT_LINE_ENDING, std::string("LF"));
    set(ConfigKeys::TRIM_TRAILING_WHITESPACE, true);
    set(ConfigKeys::INSERT_FINAL_NEWLINE, true);
    
    // Window
    set(ConfigKeys::WINDOW_WIDTH, 1280);
    set(ConfigKeys::WINDOW_HEIGHT, 720);
    set(ConfigKeys::WINDOW_MAXIMIZED, false);
    
    m_modified = false;
}

void Config::resetToDefaults() {
    clear();
    setDefaults();
}

std::vector<std::string> Config::keys() const {
    std::vector<std::string> result;
    result.reserve(m_values.size());
    for (const auto& [key, value] : m_values) {
        result.push_back(key);
    }
    return result;
}

std::string Config::getDefaultConfigPath() {
    // Get user config directory
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (appdata) {
        return std::string(appdata) + "/CodeEditor/config.json";
    }
    return "config.json";
#else
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.config/code-editor/config.json";
    }
    return "config.json";
#endif
}

void Config::parseJson(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        parseJsonValue(j, "");
    } catch (const json::exception& e) {
        // Invalid JSON, keep defaults
    }
}

void Config::parseJsonValue(const json& j, const std::string& prefix) {
    for (auto& [key, value] : j.items()) {
        std::string fullKey = prefix.empty() ? key : prefix + "." + key;
        
        if (value.is_object()) {
            parseJsonValue(value, fullKey);
        } else if (value.is_boolean()) {
            set(fullKey, value.get<bool>());
        } else if (value.is_number_integer()) {
            set(fullKey, value.get<int>());
        } else if (value.is_number_float()) {
            set(fullKey, value.get<double>());
        } else if (value.is_string()) {
            set(fullKey, value.get<std::string>());
        } else if (value.is_array()) {
            std::vector<std::string> arr;
            for (const auto& item : value) {
                if (item.is_string()) {
                    arr.push_back(item.get<std::string>());
                }
            }
            set(fullKey, arr);
        }
    }
}

std::string Config::toJson() const {
    json j;
    
    for (const auto& [key, value] : m_values) {
        // Parse dotted keys into nested objects
        std::vector<std::string> parts;
        std::istringstream stream(key);
        std::string part;
        
        while (std::getline(stream, part, '.')) {
            parts.push_back(part);
        }
        
        json* current = &j;
        for (size_t i = 0; i < parts.size() - 1; ++i) {
            current = &(*current)[parts[i]];
        }
        
        // Set the value based on variant type
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, bool>) {
                (*current)[parts.back()] = arg;
            } else if constexpr (std::is_same_v<T, int>) {
                (*current)[parts.back()] = arg;
            } else if constexpr (std::is_same_v<T, double>) {
                (*current)[parts.back()] = arg;
            } else if constexpr (std::is_same_v<T, std::string>) {
                (*current)[parts.back()] = arg;
            } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                (*current)[parts.back()] = arg;
            }
        }, value);
    }
    
    return j.dump(2);
}

// ============================================================================
// KeyBindings Implementation
// ============================================================================

KeyBindings::KeyBindings() {
    setDefaults();
}

KeyBindings::~KeyBindings() = default;

bool KeyBindings::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    try {
        json j;
        file >> j;
        
        m_bindings.clear();
        for (const auto& item : j["keybindings"]) {
            KeyBinding binding;
            binding.key = item["key"].get<int>();
            binding.modifiers = item["modifiers"].get<int>();
            binding.action = item["action"].get<std::string>();
            binding.description = item.value("description", "");
            m_bindings.push_back(binding);
        }
        return true;
    } catch (const json::exception&) {
        return false;
    }
}

bool KeyBindings::save(const std::string& path) const {
    json j;
    j["keybindings"] = json::array();
    
    for (const auto& binding : m_bindings) {
        json item;
        item["key"] = binding.key;
        item["modifiers"] = binding.modifiers;
        item["action"] = binding.action;
        item["description"] = binding.description;
        j["keybindings"].push_back(item);
    }
    
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    file << j.dump(2);
    return file.good();
}

void KeyBindings::bind(const std::string& action, int key, int modifiers) {
    // Remove existing binding for this action
    unbind(action);
    
    KeyBinding binding;
    binding.key = key;
    binding.modifiers = modifiers;
    binding.action = action;
    m_bindings.push_back(binding);
}

void KeyBindings::unbind(const std::string& action) {
    m_bindings.erase(
        std::remove_if(m_bindings.begin(), m_bindings.end(),
                      [&action](const KeyBinding& b) { return b.action == action; }),
        m_bindings.end()
    );
}

std::optional<std::string> KeyBindings::getAction(int key, int modifiers) const {
    for (const auto& binding : m_bindings) {
        if (binding.key == key && binding.modifiers == modifiers) {
            return binding.action;
        }
    }
    return std::nullopt;
}

std::optional<KeyBinding> KeyBindings::getBinding(const std::string& action) const {
    for (const auto& binding : m_bindings) {
        if (binding.action == action) {
            return binding;
        }
    }
    return std::nullopt;
}

void KeyBindings::setDefaults() {
    m_bindings.clear();
    
    // File operations
    addDefault("file.new", 'N', 2, "New File");              // Ctrl+N
    addDefault("file.open", 'O', 2, "Open File");            // Ctrl+O
    addDefault("file.save", 'S', 2, "Save File");            // Ctrl+S
    addDefault("file.saveAs", 'S', 3, "Save As");            // Ctrl+Shift+S
    addDefault("file.close", 'W', 2, "Close File");          // Ctrl+W
    
    // Edit operations
    addDefault("edit.undo", 'Z', 2, "Undo");                  // Ctrl+Z
    addDefault("edit.redo", 'Y', 2, "Redo");                  // Ctrl+Y
    addDefault("edit.cut", 'X', 2, "Cut");                    // Ctrl+X
    addDefault("edit.copy", 'C', 2, "Copy");                  // Ctrl+C
    addDefault("edit.paste", 'V', 2, "Paste");                // Ctrl+V
    addDefault("edit.selectAll", 'A', 2, "Select All");       // Ctrl+A
    addDefault("edit.find", 'F', 2, "Find");                  // Ctrl+F
    addDefault("edit.replace", 'H', 2, "Replace");            // Ctrl+H
    addDefault("edit.goToLine", 'G', 2, "Go to Line");        // Ctrl+G
}

void KeyBindings::resetToDefaults() {
    setDefaults();
}

std::string KeyBindings::keyToString(int key, int modifiers) {
    std::string result;
    
    if (modifiers & 2) result += "Ctrl+";
    if (modifiers & 1) result += "Shift+";
    if (modifiers & 4) result += "Alt+";
    if (modifiers & 8) result += "Super+";
    
    if (key >= 'A' && key <= 'Z') {
        result += static_cast<char>(key);
    } else {
        result += std::to_string(key);
    }
    
    return result;
}

void KeyBindings::addDefault(const std::string& action, int key, int modifiers,
                             const std::string& description) {
    KeyBinding binding;
    binding.key = key;
    binding.modifiers = modifiers;
    binding.action = action;
    binding.description = description;
    m_bindings.push_back(binding);
}

} // namespace utils
