/**
 * @file clipboard.cpp
 * @brief Clipboard management with history
 */

#include "editor/clipboard.hpp"

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <cstdlib>
#include <cstdio>
#include <array>
#include <memory>
#endif

namespace editor {

Clipboard::Clipboard() {
    m_history.reserve(MAX_CLIPBOARD_HISTORY);
}

Clipboard::~Clipboard() = default;

bool Clipboard::copyToSystem(const std::string& text) {
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) {
        return false;
    }
    
    EmptyClipboard();
    
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (!hMem) {
        CloseClipboard();
        return false;
    }
    
    char* pMem = static_cast<char*>(GlobalLock(hMem));
    if (pMem) {
        memcpy(pMem, text.c_str(), text.size() + 1);
        GlobalUnlock(hMem);
    }
    
    SetClipboardData(CF_TEXT, hMem);
    CloseClipboard();
    return true;
    
#elif defined(__linux__)
    // Use xclip or xsel if available
    FILE* pipe = popen("xclip -selection clipboard 2>/dev/null", "w");
    if (!pipe) {
        pipe = popen("xsel --clipboard --input 2>/dev/null", "w");
    }
    
    if (!pipe) {
        return false;
    }
    
    fwrite(text.c_str(), 1, text.size(), pipe);
    int result = pclose(pipe);
    return result == 0;
    
#else
    // Fallback: store internally only
    return false;
#endif
}

std::optional<std::string> Clipboard::pasteFromSystem() {
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) {
        return std::nullopt;
    }
    
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (!hData) {
        CloseClipboard();
        return std::nullopt;
    }
    
    char* pText = static_cast<char*>(GlobalLock(hData));
    if (!pText) {
        CloseClipboard();
        return std::nullopt;
    }
    
    std::string text(pText);
    GlobalUnlock(hData);
    CloseClipboard();
    return text;
    
#elif defined(__linux__)
    // Use xclip or xsel if available
    std::array<char, 4096> buffer;
    std::string result;
    
    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen("xclip -selection clipboard -o 2>/dev/null", "r"), pclose);
    
    if (!pipe) {
        pipe = std::unique_ptr<FILE, decltype(&pclose)>(
            popen("xsel --clipboard --output 2>/dev/null", "r"), pclose);
    }
    
    if (!pipe) {
        return std::nullopt;
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    
    if (result.empty()) {
        return std::nullopt;
    }
    
    return result;
    
#else
    return std::nullopt;
#endif
}

void Clipboard::copy(const std::string& text, bool isBlockMode) {
    ClipboardEntry entry{text, isBlockMode, ++m_timestampCounter};
    addToHistory(entry);
    copyToSystem(text);
}

void Clipboard::cut(const std::string& text, bool isBlockMode) {
    copy(text, isBlockMode);
}

std::optional<ClipboardEntry> Clipboard::paste() const {
    // First try system clipboard
    if (auto systemText = pasteFromSystem()) {
        return ClipboardEntry{*systemText, false, 0};
    }
    
    // Fall back to internal clipboard
    if (!m_history.empty()) {
        return m_history.back();
    }
    
    return std::nullopt;
}

std::optional<ClipboardEntry> Clipboard::getHistoryEntry(size_t index) const {
    if (index >= m_history.size()) {
        return std::nullopt;
    }
    
    // History is ordered oldest to newest, so reverse index
    size_t reversedIndex = m_history.size() - 1 - index;
    return m_history[reversedIndex];
}

void Clipboard::clearHistory() {
    m_history.clear();
}

std::string Clipboard::getCurrentText() const {
    if (m_history.empty()) {
        return "";
    }
    return m_history.back().text;
}

void Clipboard::addToHistory(const ClipboardEntry& entry) {
    // Remove duplicate if exists
    auto it = std::find_if(m_history.begin(), m_history.end(),
                          [&entry](const ClipboardEntry& e) {
                              return e.text == entry.text;
                          });
    if (it != m_history.end()) {
        m_history.erase(it);
    }
    
    // Add to history
    m_history.push_back(entry);
    
    // Limit history size
    while (m_history.size() > MAX_CLIPBOARD_HISTORY) {
        m_history.erase(m_history.begin());
    }
}

} // namespace editor
