#pragma once

#include "../editor/text_buffer.hpp"
#include <string>

namespace ui {

struct EditorTheme;

/**
 * @brief Status bar information
 */
struct StatusInfo {
    // Cursor position
    size_t line = 1;
    size_t column = 1;
    size_t offset = 0;
    
    // Selection info
    bool hasSelection = false;
    size_t selectedLines = 0;
    size_t selectedChars = 0;
    
    // File info
    std::string fileName;
    std::string filePath;
    bool isModified = false;
    bool isReadOnly = false;
    
    // Encoding & format
    std::string encoding = "UTF-8";
    std::string lineEnding = "LF";  // LF, CRLF, CR
    
    // Language
    std::string language = "Plain Text";
    
    // Editor mode
    std::string mode = "Insert";  // Insert, Overwrite, Visual, etc.
    
    // Additional info
    std::string message;  // Temporary status message
    float messageTimeout = 0.0f;
};

/**
 * @brief Status bar rendering at bottom of window
 */
class StatusBar {
public:
    StatusBar();
    ~StatusBar();
    
    // Update status
    void update(const editor::TextBuffer& buffer, 
                const editor::Position& cursorPos,
                bool hasSelection,
                size_t selectedChars);
    
    // Set individual properties
    void setFileName(const std::string& name);
    void setFilePath(const std::string& path);
    void setModified(bool modified);
    void setReadOnly(bool readOnly);
    void setEncoding(const std::string& encoding);
    void setLineEnding(const std::string& ending);
    void setLanguage(const std::string& language);
    void setMode(const std::string& mode);
    void setMessage(const std::string& message, float timeoutSeconds = 3.0f);
    
    // Access
    const StatusInfo& getInfo() const { return m_info; }
    
    // Rendering
    void render(const EditorTheme& theme, float windowWidth);
    
    // Update (for message timeout)
    void tick(float deltaTime);
    
    // Height
    float getHeight() const { return m_height; }
    
private:
    StatusInfo m_info;
    float m_height = 22.0f;
    
    // Rendering sections
    void renderLeftSection(float x, float width, const EditorTheme& theme);
    void renderCenterSection(float x, float width, const EditorTheme& theme);
    void renderRightSection(float x, float width, const EditorTheme& theme);
};

} // namespace ui
