/**
 * @file statusbar.cpp
 * @brief Status bar implementation
 */

#include "ui/statusbar.hpp"
#include "ui/window.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>

namespace ui {

StatusBar::StatusBar() = default;
StatusBar::~StatusBar() = default;

void StatusBar::update(const editor::TextBuffer& buffer,
                       const editor::Position& cursorPos,
                       bool hasSelection,
                       size_t selectedChars) {
    m_info.line = cursorPos.line + 1;  // 1-indexed for display
    m_info.column = cursorPos.column + 1;
    m_info.offset = buffer.positionToOffset(cursorPos);
    m_info.hasSelection = hasSelection;
    m_info.selectedChars = selectedChars;
    
    // Count selected lines if there's a selection
    if (hasSelection && selectedChars > 0) {
        // Count newlines in the selection to determine line count
        // This requires knowing the selection range, but we only have selectedChars
        // Approximate by counting average chars per line
        size_t totalChars = buffer.size();
        size_t totalLines = buffer.lineCount();
        if (totalChars > 0 && totalLines > 0) {
            double avgCharsPerLine = static_cast<double>(totalChars) / totalLines;
            m_info.selectedLines = std::max(size_t(1), 
                                            static_cast<size_t>(selectedChars / avgCharsPerLine + 0.5));
        } else {
            m_info.selectedLines = 1;
        }
    } else {
        m_info.selectedLines = 0;
    }
}

void StatusBar::setFileName(const std::string& name) {
    m_info.fileName = name;
}

void StatusBar::setFilePath(const std::string& path) {
    m_info.filePath = path;
}

void StatusBar::setModified(bool modified) {
    m_info.isModified = modified;
}

void StatusBar::setReadOnly(bool readOnly) {
    m_info.isReadOnly = readOnly;
}

void StatusBar::setEncoding(const std::string& encoding) {
    m_info.encoding = encoding;
}

void StatusBar::setLineEnding(const std::string& ending) {
    m_info.lineEnding = ending;
}

void StatusBar::setLanguage(const std::string& language) {
    m_info.language = language;
}

void StatusBar::setMode(const std::string& mode) {
    m_info.mode = mode;
}

void StatusBar::setMessage(const std::string& message, float timeoutSeconds) {
    m_info.message = message;
    m_info.messageTimeout = timeoutSeconds;
}

void StatusBar::tick(float deltaTime) {
    if (m_info.messageTimeout > 0) {
        m_info.messageTimeout -= deltaTime;
        if (m_info.messageTimeout <= 0) {
            m_info.message.clear();
            m_info.messageTimeout = 0;
        }
    }
}

void StatusBar::render(const EditorTheme& theme, float windowWidth) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x,
                                   viewport->WorkPos.y + viewport->WorkSize.y - m_height));
    ImGui::SetNextWindowSize(ImVec2(windowWidth, m_height));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                            ImGuiWindowFlags_NoResize |
                            ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoScrollbar |
                            ImGuiWindowFlags_NoSavedSettings;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
    
    if (ImGui::Begin("##StatusBar", nullptr, flags)) {
        // Left section - cursor position
        renderLeftSection(0, windowWidth * 0.3f, theme);
        
        // Center section - message or file info
        ImGui::SameLine(windowWidth * 0.3f);
        renderCenterSection(windowWidth * 0.3f, windowWidth * 0.4f, theme);
        
        // Right section - encoding, language, etc.
        ImGui::SameLine(windowWidth * 0.7f);
        renderRightSection(windowWidth * 0.7f, windowWidth * 0.3f, theme);
    }
    ImGui::End();
    
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void StatusBar::renderLeftSection([[maybe_unused]] float x, [[maybe_unused]] float width,
                                  [[maybe_unused]] const EditorTheme& theme) {
    // Cursor position
    if (m_info.hasSelection && m_info.selectedChars > 0) {
        ImGui::Text("Ln %zu, Col %zu (%zu selected)",
                   m_info.line, m_info.column, m_info.selectedChars);
    } else {
        ImGui::Text("Ln %zu, Col %zu", m_info.line, m_info.column);
    }
}

void StatusBar::renderCenterSection([[maybe_unused]] float x, [[maybe_unused]] float width,
                                    [[maybe_unused]] const EditorTheme& theme) {
    // Show message if present, otherwise show file info
    if (!m_info.message.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.4f, 1.0f), "%s", m_info.message.c_str());
    } else if (!m_info.fileName.empty()) {
        if (m_info.isModified) {
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), "%s [modified]",
                              m_info.fileName.c_str());
        } else if (m_info.isReadOnly) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s [read-only]",
                              m_info.fileName.c_str());
        }
    }
}

void StatusBar::renderRightSection(float x, [[maybe_unused]] float width,
                                   [[maybe_unused]] const EditorTheme& theme) {
    // Right-aligned items
    float itemSpacing = 80.0f;
    
    // Encoding
    ImGui::Text("%s", m_info.encoding.c_str());
    
    ImGui::SameLine(x + itemSpacing);
    ImGui::Text("%s", m_info.lineEnding.c_str());
    
    ImGui::SameLine(x + itemSpacing * 2);
    ImGui::Text("%s", m_info.language.c_str());
    
    // Mode (if not default)
    if (m_info.mode != "Insert") {
        ImGui::SameLine(x + itemSpacing * 3);
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "%s", m_info.mode.c_str());
    }
}

} // namespace ui
