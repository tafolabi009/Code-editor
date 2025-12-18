/**
 * @file config.cpp
 * @brief Configuration management implementation
 */

#include "utils/config.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

#ifdef HAS_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif

namespace utils {

// ============================================================================
// Configuration Implementation
// ============================================================================

Configuration::Configuration() {
    setDefaults();
}

Configuration::Configuration(const std::string& filePath) : m_filePath(filePath) {
    setDefaults();
    load(filePath);
}

Configuration::~Configuration() {
    if (m_autoSave && m_dirty) {
        save();
    }
}

void Configuration::setDefaults() {
    // Editor settings
    m_values["editor.tabSize"] = "4";
    m_values["editor.useSpaces"] = "true";
    m_values["editor.autoIndent"] = "true";
    m_values["editor.wordWrap"] = "false";
    m_values["editor.wrapColumn"] = "80";
    m_values["editor.showLineNumbers"] = "true";
    m_values["editor.showWhitespace"] = "false";
    m_values["editor.highlightCurrentLine"] = "true";
    m_values["editor.scrollPastEnd"] = "true";
    m_values["editor.smoothScrolling"] = "true";
    m_values["editor.cursorBlinkRate"] = "530";
    m_values["editor.cursorStyle"] = "line";
    
    // Font settings
    m_values["font.family"] = "JetBrains Mono";
    m_values["font.size"] = "14";
    m_values["font.lineHeight"] = "1.5";
    
    // Theme settings
    m_values["theme.name"] = "dark";
    m_values["theme.background"] = "#1e1e1e";
    m_values["theme.foreground"] = "#d4d4d4";
    m_values["theme.selection"] = "#264f78";
    m_values["theme.cursor"] = "#ffffff";
    m_values["theme.lineNumber"] = "#858585";
    m_values["theme.currentLine"] = "#2a2a2a";
    
    // Syntax colors
    m_values["syntax.keyword"] = "#569cd6";
    m_values["syntax.string"] = "#ce9178";
    m_values["syntax.number"] = "#b5cea8";
    m_values["syntax.comment"] = "#6a9955";
    m_values["syntax.type"] = "#4ec9b0";
    m_values["syntax.function"] = "#dcdcaa";
    m_values["syntax.operator"] = "#d4d4d4";
    m_values["syntax.preprocessor"] = "#c586c0";
    
    // File settings
    m_values["file.encoding"] = "UTF-8";
    m_values["file.lineEnding"] = "LF";
    m_values["file.trimTrailingWhitespace"] = "true";
    m_values["file.insertFinalNewline"] = "true";
    m_values["file.autoSave"] = "false";
    m_values["file.autoSaveDelay"] = "1000";
    
    // Search settings
    m_values["search.caseSensitive"] = "false";
    m_values["search.wholeWord"] = "false";
    m_values["search.useRegex"] = "false";
    m_values["search.highlightMatches"] = "true";
    
    // Window settings
    m_values["window.width"] = "1280";
    m_values["window.height"] = "720";
    m_values["window.maximized"] = "false";
    m_values["window.showTabBar"] = "true";
    m_values["window.showStatusBar"] = "true";
    m_values["window.showMinimap"] = "false";
    
    // Performance settings
    m_values["performance.maxFileSize"] = "50";  // MB
    m_values["performance.useSIMD"] = "true";
    m_values["performance.syntaxHighlightingDelay"] = "50";
    m_values["performance.maxUndoHistory"] = "1000";
}

bool Configuration::load(const std::string& filePath) {
    m_filePath = filePath;
    
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    return parse(content);
}

bool Configuration::save() const {
    return save(m_filePath);
}

bool Configuration::save(const std::string& filePath) const {
    // Create directory if it doesn't exist
    std::filesystem::path path(filePath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    
    file << serialize();
    file.close();
    
    return true;
}

bool Configuration::parse(const std::string& content) {
#ifdef HAS_JSON
    try {
        json j = json::parse(content);
        parseJsonObject(j, "");
        m_dirty = false;
        return true;
    } catch (const json::exception&) {
        return false;
    }
#else
    // Simple key=value parsing fallback
    std::istringstream stream(content);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t\""));
            value.erase(value.find_last_not_of(" \t\"") + 1);
            
            m_values[key] = value;
        }
    }
    
    m_dirty = false;
    return true;
#endif
}

std::string Configuration::serialize() const {
#ifdef HAS_JSON
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
        
        // Try to convert to appropriate type
        if (value == "true") {
            (*current)[parts.back()] = true;
        } else if (value == "false") {
            (*current)[parts.back()] = false;
        } else {
            try {
                if (value.find('.') != std::string::npos) {
                    (*current)[parts.back()] = std::stod(value);
                } else {
                    (*current)[parts.back()] = std::stoll(value);
                }
            } catch (...) {
                (*current)[parts.back()] = value;
            }
        }
    }
    
    return j.dump(2);
#else
    std::ostringstream output;
    
    // Group by prefix
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> groups;
    
    for (const auto& [key, value] : m_values) {
        size_t pos = key.find('.');
        if (pos != std::string::npos) {
            std::string prefix = key.substr(0, pos);
            groups[prefix].emplace_back(key, value);
        } else {
            groups[""].emplace_back(key, value);
        }
    }
    
    for (const auto& [prefix, values] : groups) {
        if (!prefix.empty()) {
            output << "\n# " << prefix << " settings\n";
        }
        for (const auto& [key, value] : values) {
            output << key << " = " << value << "\n";
        }
    }
    
    return output.str();
#endif
}

#ifdef HAS_JSON
void Configuration::parseJsonObject(const json& j, const std::string& prefix) {
    for (auto& [key, value] : j.items()) {
        std::string fullKey = prefix.empty() ? key : prefix + "." + key;
        
        if (value.is_object()) {
            parseJsonObject(value, fullKey);
        } else if (value.is_boolean()) {
            m_values[fullKey] = value.get<bool>() ? "true" : "false";
        } else if (value.is_number_integer()) {
            m_values[fullKey] = std::to_string(value.get<int64_t>());
        } else if (value.is_number_float()) {
            m_values[fullKey] = std::to_string(value.get<double>());
        } else if (value.is_string()) {
            m_values[fullKey] = value.get<std::string>();
        }
    }
}
#endif

std::string Configuration::getString(const std::string& key, const std::string& defaultValue) const {
    auto it = m_values.find(key);
    return it != m_values.end() ? it->second : defaultValue;
}

int Configuration::getInt(const std::string& key, int defaultValue) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        try {
            return std::stoi(it->second);
        } catch (...) {}
    }
    return defaultValue;
}

float Configuration::getFloat(const std::string& key, float defaultValue) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        try {
            return std::stof(it->second);
        } catch (...) {}
    }
    return defaultValue;
}

bool Configuration::getBool(const std::string& key, bool defaultValue) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        return it->second == "true" || it->second == "1" || it->second == "yes";
    }
    return defaultValue;
}

void Configuration::set(const std::string& key, const std::string& value) {
    m_values[key] = value;
    m_dirty = true;
    notifyObservers(key);
}

void Configuration::set(const std::string& key, int value) {
    set(key, std::to_string(value));
}

void Configuration::set(const std::string& key, float value) {
    set(key, std::to_string(value));
}

void Configuration::set(const std::string& key, bool value) {
    set(key, value ? std::string("true") : std::string("false"));
}

bool Configuration::has(const std::string& key) const {
    return m_values.find(key) != m_values.end();
}

void Configuration::remove(const std::string& key) {
    m_values.erase(key);
    m_dirty = true;
}

std::vector<std::string> Configuration::keys() const {
    std::vector<std::string> result;
    result.reserve(m_values.size());
    for (const auto& [key, value] : m_values) {
        result.push_back(key);
    }
    return result;
}

std::vector<std::string> Configuration::keysWithPrefix(const std::string& prefix) const {
    std::vector<std::string> result;
    for (const auto& [key, value] : m_values) {
        if (key.compare(0, prefix.size(), prefix) == 0) {
            result.push_back(key);
        }
    }
    return result;
}

void Configuration::addObserver(const std::string& key, ConfigObserver observer) {
    m_observers[key].push_back(std::move(observer));
}

void Configuration::notifyObservers(const std::string& key) {
    // Notify exact key observers
    auto it = m_observers.find(key);
    if (it != m_observers.end()) {
        for (const auto& observer : it->second) {
            observer(key);
        }
    }
    
    // Notify prefix observers (e.g., "editor.*")
    for (const auto& [pattern, observers] : m_observers) {
        if (pattern.back() == '*') {
            std::string prefix = pattern.substr(0, pattern.size() - 1);
            if (key.compare(0, prefix.size(), prefix) == 0) {
                for (const auto& observer : observers) {
                    observer(key);
                }
            }
        }
    }
}

// ============================================================================
// ConfigManager Implementation
// ============================================================================

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    // Load default config
    m_configs["default"] = std::make_shared<Configuration>();
}

bool ConfigManager::loadUserConfig(const std::string& path) {
    auto config = std::make_shared<Configuration>(path);
    m_configs["user"] = config;
    m_userConfigPath = path;
    return true;  // Even if file doesn't exist, we create with defaults
}

bool ConfigManager::loadWorkspaceConfig(const std::string& path) {
    auto config = std::make_shared<Configuration>(path);
    m_configs["workspace"] = config;
    return true;
}

bool ConfigManager::saveUserConfig() {
    auto it = m_configs.find("user");
    if (it != m_configs.end() && !m_userConfigPath.empty()) {
        return it->second->save(m_userConfigPath);
    }
    return false;
}

std::string ConfigManager::getString(const std::string& key, const std::string& defaultValue) const {
    // Check workspace config first
    auto workspace = m_configs.find("workspace");
    if (workspace != m_configs.end() && workspace->second->has(key)) {
        return workspace->second->getString(key, defaultValue);
    }
    
    // Then user config
    auto user = m_configs.find("user");
    if (user != m_configs.end() && user->second->has(key)) {
        return user->second->getString(key, defaultValue);
    }
    
    // Finally default config
    auto def = m_configs.find("default");
    if (def != m_configs.end()) {
        return def->second->getString(key, defaultValue);
    }
    
    return defaultValue;
}

int ConfigManager::getInt(const std::string& key, int defaultValue) const {
    auto workspace = m_configs.find("workspace");
    if (workspace != m_configs.end() && workspace->second->has(key)) {
        return workspace->second->getInt(key, defaultValue);
    }
    
    auto user = m_configs.find("user");
    if (user != m_configs.end() && user->second->has(key)) {
        return user->second->getInt(key, defaultValue);
    }
    
    auto def = m_configs.find("default");
    if (def != m_configs.end()) {
        return def->second->getInt(key, defaultValue);
    }
    
    return defaultValue;
}

bool ConfigManager::getBool(const std::string& key, bool defaultValue) const {
    auto workspace = m_configs.find("workspace");
    if (workspace != m_configs.end() && workspace->second->has(key)) {
        return workspace->second->getBool(key, defaultValue);
    }
    
    auto user = m_configs.find("user");
    if (user != m_configs.end() && user->second->has(key)) {
        return user->second->getBool(key, defaultValue);
    }
    
    auto def = m_configs.find("default");
    if (def != m_configs.end()) {
        return def->second->getBool(key, defaultValue);
    }
    
    return defaultValue;
}

void ConfigManager::set(const std::string& key, const std::string& value, ConfigLevel level) {
    std::string configName = (level == ConfigLevel::User) ? "user" : "workspace";
    
    auto it = m_configs.find(configName);
    if (it != m_configs.end()) {
        it->second->set(key, value);
    }
}

} // namespace utils
