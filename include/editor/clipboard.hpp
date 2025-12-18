#pragma once

#include <string>
#include <vector>
#include <optional>

namespace editor {

/**
 * @brief Maximum clipboard history entries
 */
constexpr size_t MAX_CLIPBOARD_HISTORY = 10;

/**
 * @brief Clipboard entry with metadata
 */
struct ClipboardEntry {
    std::string text;
    bool isBlockMode = false;  // Block/column selection
    size_t timestamp = 0;
};

/**
 * @brief Clipboard management with history
 */
class Clipboard {
public:
    Clipboard();
    ~Clipboard();
    
    // System clipboard operations
    static bool copyToSystem(const std::string& text);
    static std::optional<std::string> pasteFromSystem();
    
    // Internal clipboard with history
    void copy(const std::string& text, bool isBlockMode = false);
    void cut(const std::string& text, bool isBlockMode = false);
    std::optional<ClipboardEntry> paste() const;
    
    // Clipboard history
    const std::vector<ClipboardEntry>& getHistory() const { return m_history; }
    std::optional<ClipboardEntry> getHistoryEntry(size_t index) const;
    void clearHistory();
    
    // Current clipboard content
    bool hasContent() const { return !m_history.empty(); }
    std::string getCurrentText() const;
    
private:
    std::vector<ClipboardEntry> m_history;
    size_t m_timestampCounter = 0;
    
    void addToHistory(const ClipboardEntry& entry);
};

} // namespace editor
