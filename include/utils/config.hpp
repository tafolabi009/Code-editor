#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <variant>
#include <optional>
#include <filesystem>
#include <nlohmann/json_fwd.hpp>

namespace utils {

/**
 * @brief Configuration value types
 */
using ConfigValue = std::variant<bool, int, double, std::string, std::vector<std::string>>;

/**
 * @brief Configuration management with JSON persistence
 */
class Config {
public:
    Config();
    explicit Config(const std::string& path);
    ~Config();
    
    // Load/Save
    bool load(const std::string& path);
    bool save(const std::string& path) const;
    bool save() const;  // Save to last loaded path
    
    // Get values
    template<typename T>
    T get(const std::string& key, const T& defaultValue = T{}) const {
        auto it = m_values.find(key);
        if (it != m_values.end()) {
            if (auto* val = std::get_if<T>(&it->second)) {
                return *val;
            }
        }
        return defaultValue;
    }
    
    std::optional<ConfigValue> get(const std::string& key) const;
    
    // Set values
    template<typename T>
    void set(const std::string& key, const T& value) {
        m_values[key] = value;
        m_modified = true;
    }
    
    // Check existence
    bool has(const std::string& key) const;
    
    // Remove
    void remove(const std::string& key);
    void clear();
    
    // Defaults
    void setDefaults();
    void resetToDefaults();
    
    // Modified state
    bool isModified() const { return m_modified; }
    void clearModified() { m_modified = false; }
    
    // All keys
    std::vector<std::string> keys() const;
    
    // Default config path
    static std::string getDefaultConfigPath();
    
private:
    std::unordered_map<std::string, ConfigValue> m_values;
    std::string m_path;
    bool m_modified = false;
    
    void parseJson(const std::string& json);
    void parseJsonValue(const nlohmann::json& j, const std::string& prefix);
    std::string toJson() const;
};

/**
 * @brief Keybinding configuration
 */
struct KeyBinding {
    int key;
    int modifiers;  // Ctrl, Shift, Alt, Super
    std::string action;
    std::string description;
};

/**
 * @brief Keyboard shortcuts manager
 */
class KeyBindings {
public:
    KeyBindings();
    ~KeyBindings();
    
    // Load/Save
    bool load(const std::string& path);
    bool save(const std::string& path) const;
    
    // Binding operations
    void bind(const std::string& action, int key, int modifiers);
    void unbind(const std::string& action);
    
    // Lookup
    std::optional<std::string> getAction(int key, int modifiers) const;
    std::optional<KeyBinding> getBinding(const std::string& action) const;
    
    // All bindings
    const std::vector<KeyBinding>& getAll() const { return m_bindings; }
    
    // Defaults
    void setDefaults();
    void resetToDefaults();
    
    // Convert to string for display
    static std::string keyToString(int key, int modifiers);
    
private:
    std::vector<KeyBinding> m_bindings;
    
    void addDefault(const std::string& action, int key, int modifiers, 
                   const std::string& description);
};

// Configuration keys
namespace ConfigKeys {
    // Editor
    constexpr const char* FONT_SIZE = "editor.fontSize";
    constexpr const char* FONT_FAMILY = "editor.fontFamily";
    constexpr const char* TAB_WIDTH = "editor.tabWidth";
    constexpr const char* INSERT_SPACES = "editor.insertSpaces";
    constexpr const char* LINE_NUMBERS = "editor.lineNumbers";
    constexpr const char* RELATIVE_LINE_NUMBERS = "editor.relativeLineNumbers";
    constexpr const char* WORD_WRAP = "editor.wordWrap";
    constexpr const char* AUTO_INDENT = "editor.autoIndent";
    constexpr const char* HIGHLIGHT_CURRENT_LINE = "editor.highlightCurrentLine";
    constexpr const char* SHOW_WHITESPACE = "editor.showWhitespace";
    constexpr const char* MINIMAP = "editor.minimap";
    constexpr const char* SCROLL_SPEED = "editor.scrollSpeed";
    
    // Theme
    constexpr const char* THEME = "theme.name";
    constexpr const char* THEME_BACKGROUND = "theme.background";
    constexpr const char* THEME_FOREGROUND = "theme.foreground";
    constexpr const char* THEME_SELECTION = "theme.selection";
    constexpr const char* THEME_CURSOR = "theme.cursor";
    
    // Files
    constexpr const char* AUTO_SAVE = "files.autoSave";
    constexpr const char* AUTO_SAVE_DELAY = "files.autoSaveDelay";
    constexpr const char* DEFAULT_ENCODING = "files.encoding";
    constexpr const char* DEFAULT_LINE_ENDING = "files.lineEnding";
    constexpr const char* TRIM_TRAILING_WHITESPACE = "files.trimTrailingWhitespace";
    constexpr const char* INSERT_FINAL_NEWLINE = "files.insertFinalNewline";
    constexpr const char* RECENT_FILES = "files.recent";
    
    // Window
    constexpr const char* WINDOW_WIDTH = "window.width";
    constexpr const char* WINDOW_HEIGHT = "window.height";
    constexpr const char* WINDOW_MAXIMIZED = "window.maximized";
}

} // namespace utils
