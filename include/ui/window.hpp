#pragma once

#include "imgui.h"
#include "../editor/text_buffer.hpp"
#include "../editor/cursor.hpp"
#include "../editor/selection.hpp"
#include "../editor/clipboard.hpp"
#include "../editor/multi_cursor.hpp"
#include "../syntax/highlighter.hpp"
#include "../search/search.hpp"
#include <memory>
#include <string>
#include <functional>
#include <vector>

struct GLFWwindow;

namespace ui {

/**
 * @brief Theme colors for the editor
 */
struct EditorTheme {
    ImVec4 background{0.1f, 0.1f, 0.1f, 1.0f};
    ImVec4 text{0.9f, 0.9f, 0.9f, 1.0f};
    ImVec4 selection{0.3f, 0.4f, 0.6f, 0.5f};
    ImVec4 cursor{1.0f, 1.0f, 1.0f, 1.0f};
    ImVec4 lineNumber{0.5f, 0.5f, 0.5f, 1.0f};
    ImVec4 currentLine{0.15f, 0.15f, 0.15f, 1.0f};
    ImVec4 gutterBackground{0.08f, 0.08f, 0.08f, 1.0f};
    ImVec4 scrollbar{0.3f, 0.3f, 0.3f, 1.0f};
    
    // Syntax highlighting colors
    ImVec4 keyword{0.4f, 0.6f, 0.9f, 1.0f};
    ImVec4 string{0.8f, 0.6f, 0.4f, 1.0f};
    ImVec4 comment{0.5f, 0.5f, 0.5f, 1.0f};
    ImVec4 number{0.7f, 0.8f, 0.5f, 1.0f};
    ImVec4 function{0.9f, 0.8f, 0.4f, 1.0f};
    ImVec4 type{0.5f, 0.8f, 0.7f, 1.0f};
    ImVec4 preprocessor{0.8f, 0.5f, 0.8f, 1.0f};
    ImVec4 operator_{0.9f, 0.9f, 0.9f, 1.0f};
};

/**
 * @brief Editor configuration settings
 */
struct EditorConfig {
    std::string fontPath;
    float fontSize = 14.0f;
    int tabWidth = 4;
    bool showLineNumbers = true;
    bool showWhitespace = false;
    bool wordWrap = false;
    bool autoIndent = true;
    bool highlightCurrentLine = true;
    bool showMinimap = false;
    float lineHeight = 1.2f;
    bool relativeLineNumbers = false;
    int scrollSpeed = 3;
};

/**
 * @brief Single editor pane (can have multiple for split view)
 */
class EditorPane {
public:
    EditorPane();
    ~EditorPane();
    
    // Buffer management
    void setBuffer(std::shared_ptr<editor::TextBuffer> buffer);
    std::shared_ptr<editor::TextBuffer> getBuffer() const { return m_buffer; }
    
    // Rendering
    void render(const EditorTheme& theme, const EditorConfig& config);
    
    // Input handling
    void handleKeyInput(int key, int scancode, int action, int mods);
    void handleCharInput(unsigned int codepoint);
    void handleMouseButton(int button, int action, int mods, double x, double y);
    void handleMouseMove(double x, double y);
    void handleScroll(double xoffset, double yoffset);
    
    // Cursor & Selection
    editor::Cursor& getCursor() { return *m_cursor; }
    editor::Selection& getSelection() { return *m_selection; }

    // Multi-cursor
    void addNextOccurrence();     // Ctrl+D: select word, then add next match
    void clearSecondaryCursors(); // Esc: collapse back to a single caret
    bool hasMultipleCursors() const { return m_multiCursor.hasMultiple(); }
    
    // Scroll position
    float getScrollX() const { return m_scrollX; }
    float getScrollY() const { return m_scrollY; }
    void setScroll(float x, float y);
    void scrollToCursor();
    
    // Viewport
    void setViewportSize(float width, float height);
    size_t getFirstVisibleLine() const;
    size_t getLastVisibleLine() const;
    size_t getVisibleLineCount() const;
    
    // Syntax highlighting
    void setHighlighter(std::shared_ptr<syntax::Highlighter> highlighter);
    std::shared_ptr<syntax::Highlighter> getHighlighter() const { return m_highlighter; }
    // Force a full re-highlight on the next render (used after undo/redo or a
    // language change, which don't go through the incremental edit path).
    void markHighlightDirty() { m_needsFullHighlight = true; }

    // File association (one file per pane / tab)
    const std::string& getFilePath() const { return m_filePath; }
    void setFilePath(const std::string& path) { m_filePath = path; }
    std::string getDisplayName() const;

private:
    // Sentinel: no line is pending incremental re-highlight.
    static constexpr size_t kNoDirtyLine = static_cast<size_t>(-1);

    // Hook the buffer's change notifications so edits mark the smallest changed
    // line for incremental re-highlighting.
    void hookBufferChanges();

    std::shared_ptr<editor::TextBuffer> m_buffer;
    std::unique_ptr<editor::Cursor> m_cursor;
    std::unique_ptr<editor::Selection> m_selection;
    editor::MultiCursor m_multiCursor;  // active when it holds >1 caret
    std::shared_ptr<syntax::Highlighter> m_highlighter;
    std::string m_filePath;
    bool m_needsFullHighlight = false;
    size_t m_dirtyFromLine = kNoDirtyLine;

    // Editing options snapshotted from EditorConfig on render (used by key
    // handlers like auto-indent that run outside render()).
    int m_tabWidth = 4;
    bool m_autoIndent = true;
    
    float m_scrollX = 0.0f;
    float m_scrollY = 0.0f;
    float m_viewportWidth = 800.0f;
    float m_viewportHeight = 600.0f;
    float m_charWidth = 8.0f;
    float m_lineHeight = 16.0f;
    
    ImVec2 m_renderPos{0, 0};
    ImVec2 m_contentPos{0, 0};
    
    // Rendering helpers
    void renderGutter(const EditorTheme& theme, const EditorConfig& config);
    void renderText(const EditorTheme& theme, const EditorConfig& config);
    void renderCursor(const EditorTheme& theme);
    void renderSelection(const EditorTheme& theme);
    void renderScrollbars(const EditorTheme& theme);
    
    // Position conversion
    editor::Position screenToPosition(float x, float y) const;
    ImVec2 positionToScreen(const editor::Position& pos) const;
    
    // Input helpers
    void insertChar(char c);
    void insertText(const std::string& text);
    void deleteSelection();
    void handleBackspace();
    void handleDelete();
    void handleEnter();
    void handleTab();

    // Multi-cursor helpers
    void syncPrimaryFromMulti();
    editor::Range wordRangeAt(const editor::Position& pos) const;
};

/**
 * @brief Main application window
 */
class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();
    
    // Main loop
    bool shouldClose() const;
    void pollEvents();
    void beginFrame();
    void endFrame();
    void render();
    
    // Window properties
    int getWidth() const;
    int getHeight() const;
    void setTitle(const std::string& title);
    
    // Theme & Config
    EditorTheme& getTheme() { return m_theme; }
    EditorConfig& getConfig() { return m_config; }
    void applyTheme();
    
    // File operations
    void openFile(const std::string& path);
    void saveCurrentFile();
    void saveFileAs(const std::string& path);
    void newFile();
    void closeCurrentFile();
    void doCloseCurrentFile();
    
    // Clipboard
    editor::Clipboard& getClipboard() { return m_clipboard; }
    void copySelection();
    void cutSelection();
    void pasteClipboard();

    // Editor commands (operate on the current pane)
    void duplicateCurrentLine();
    void moveCurrentLineUp();
    void moveCurrentLineDown();
    void toggleCommentCurrent();
    void jumpToMatchingBracket();
    
    // Editor panes
    EditorPane* getCurrentPane() { return m_currentPane; }
    void splitHorizontal();
    void splitVertical();
    
    // Dialogs
    void showOpenFileDialog();
    void showSaveFileDialog();
    void showFindDialog();
    void showReplaceDialog();
    void showSettingsDialog();
    void showFindInFilesDialog();
    
    // Status
    std::string getStatusText() const;
    
private:
    GLFWwindow* m_window = nullptr;
    EditorTheme m_theme;
    EditorConfig m_config;
    editor::Clipboard m_clipboard;
    
    std::vector<std::unique_ptr<EditorPane>> m_panes;
    EditorPane* m_currentPane = nullptr;

    // UI state
    bool m_showFindDialog = false;
    bool m_showReplaceDialog = false;
    bool m_showSettingsDialog = false;
    std::string m_findText;
    std::string m_replaceText;
    bool m_findCaseSensitive = false;
    bool m_findRegex = false;
    
    // Search state
    search::SearchEngine m_searchEngine;
    std::vector<search::SearchMatch> m_searchResults;
    size_t m_currentMatchIndex = 0;

    // Find-in-all-tabs state
    struct GlobalSearchResult {
        size_t paneIndex;
        std::string file;
        size_t line;
        size_t column;
        size_t offset;
        size_t length;
        std::string preview;
    };
    bool m_showFindInFilesDialog = false;
    std::vector<GlobalSearchResult> m_globalResults;

    // Search helper methods
    void performSearch();
    void selectMatch(const search::SearchMatch& match);
    void performGlobalSearch();
    void jumpToGlobalResult(const GlobalSearchResult& result);
    
    // Initialization
    void initGLFW();
    void initImGui();
    void setupCallbacks();
    void loadConfig();
    void saveConfig();
    
    // UI rendering
    void renderMenuBar();
    void renderTabs();
    void renderStatusBar();
    void renderDialogs();
    
    // Callbacks
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void charCallback(GLFWwindow* window, unsigned int codepoint);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void dropCallback(GLFWwindow* window, int count, const char** paths);
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
};

} // namespace ui
