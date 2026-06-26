/**
 * @file window.cpp
 * @brief Main application window implementation using Dear ImGui + GLFW + OpenGL
 */

#include "ui/window.hpp"
#include "ui/tabs.hpp"
#include "ui/statusbar.hpp"
#include "syntax/highlighter.hpp"
#include "editor/text_ops.hpp"

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <cstring>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#include <commdlg.h>
#endif

namespace ui {

// ====================
// EditorPane Implementation
// ====================

EditorPane::EditorPane()
    : m_buffer(std::make_shared<editor::TextBuffer>())
    , m_cursor(std::make_unique<editor::Cursor>(m_buffer.get()))
    , m_selection(std::make_unique<editor::Selection>(m_buffer.get(), m_cursor.get()))
{
    hookBufferChanges();
}

EditorPane::~EditorPane() = default;

void EditorPane::hookBufferChanges() {
    // Record the smallest changed line on every buffer mutation so the next
    // render can re-highlight incrementally from there.
    m_buffer->addChangeCallback([this](const editor::Range& range, const std::string&) {
        if (range.start.line < m_dirtyFromLine) {
            m_dirtyFromLine = range.start.line;
        }
    });
}

void EditorPane::setBuffer(std::shared_ptr<editor::TextBuffer> buffer) {
    m_buffer = buffer;
    m_cursor = std::make_unique<editor::Cursor>(m_buffer.get());
    m_selection = std::make_unique<editor::Selection>(m_buffer.get(), m_cursor.get());
    hookBufferChanges();
    m_needsFullHighlight = true;
}

void EditorPane::setHighlighter(std::shared_ptr<syntax::Highlighter> highlighter) {
    m_highlighter = highlighter;
    m_needsFullHighlight = true;  // build the token cache on next render
}

std::string EditorPane::getDisplayName() const {
    if (m_filePath.empty()) {
        return "Untitled";
    }
    size_t slash = m_filePath.find_last_of("/\\");
    return (slash == std::string::npos) ? m_filePath : m_filePath.substr(slash + 1);
}

void EditorPane::setViewportSize(float width, float height) {
    m_viewportWidth = width;
    m_viewportHeight = height;
}

size_t EditorPane::getFirstVisibleLine() const {
    return static_cast<size_t>(m_scrollY / m_lineHeight);
}

size_t EditorPane::getLastVisibleLine() const {
    size_t first = getFirstVisibleLine();
    size_t visible = getVisibleLineCount();
    size_t total = m_buffer->lineCount();
    return std::min(first + visible + 1, total);
}

size_t EditorPane::getVisibleLineCount() const {
    return static_cast<size_t>(m_viewportHeight / m_lineHeight) + 1;
}

void EditorPane::scrollToCursor() {
    auto pos = m_cursor->getPosition();
    float cursorY = pos.line * m_lineHeight;
    float cursorX = pos.column * m_charWidth;
    
    // Vertical scroll
    if (cursorY < m_scrollY) {
        m_scrollY = cursorY;
    } else if (cursorY + m_lineHeight > m_scrollY + m_viewportHeight) {
        m_scrollY = cursorY + m_lineHeight - m_viewportHeight;
    }
    
    // Horizontal scroll
    float gutterWidth = 60.0f;  // Approximate gutter width
    float textAreaWidth = m_viewportWidth - gutterWidth;
    if (cursorX < m_scrollX) {
        m_scrollX = cursorX;
    } else if (cursorX + m_charWidth > m_scrollX + textAreaWidth) {
        m_scrollX = cursorX + m_charWidth - textAreaWidth;
    }
}

void EditorPane::setScroll(float x, float y) {
    m_scrollX = std::max(0.0f, x);
    m_scrollY = std::max(0.0f, y);
}

editor::Position EditorPane::screenToPosition(float x, float y) const {
    float gutterWidth = 60.0f;
    size_t line = static_cast<size_t>((y + m_scrollY) / m_lineHeight);
    size_t column = static_cast<size_t>((x - gutterWidth + m_scrollX) / m_charWidth);
    
    line = std::min(line, m_buffer->lineCount() > 0 ? m_buffer->lineCount() - 1 : 0);
    size_t lineLen = m_buffer->lineLength(line);
    column = std::min(column, lineLen);
    
    return {line, column};
}

ImVec2 EditorPane::positionToScreen(const editor::Position& pos) const {
    float gutterWidth = 60.0f;
    float x = gutterWidth + pos.column * m_charWidth - m_scrollX;
    float y = pos.line * m_lineHeight - m_scrollY;
    return ImVec2(m_renderPos.x + x, m_renderPos.y + y);
}

void EditorPane::render(const EditorTheme& theme, const EditorConfig& config) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.background);
    
    // Get current window position and size
    m_renderPos = ImGui::GetCursorScreenPos();
    ImVec2 contentRegion = ImGui::GetContentRegionAvail();
    m_viewportWidth = contentRegion.x;
    m_viewportHeight = contentRegion.y;
    
    // Calculate sizes
    m_lineHeight = config.fontSize * config.lineHeight;
    m_charWidth = config.fontSize * 0.6f;  // Approximate monospace width
    
    // Create child window for scrolling
    ImGui::BeginChild("EditorContent", contentRegion, false, 
                      ImGuiWindowFlags_HorizontalScrollbar);
    
    // Render components
    renderGutter(theme, config);
    renderSelection(theme);
    renderText(theme, config);
    renderCursor(theme);
    
    // Handle scroll
    m_scrollX = ImGui::GetScrollX();
    m_scrollY = ImGui::GetScrollY();
    
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void EditorPane::renderGutter(const EditorTheme& theme, const EditorConfig& config) {
    if (!config.showLineNumbers) return;
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float gutterWidth = 50.0f;
    
    // Gutter background
    drawList->AddRectFilled(
        pos,
        ImVec2(pos.x + gutterWidth, pos.y + m_viewportHeight),
        ImGui::ColorConvertFloat4ToU32(theme.gutterBackground)
    );
    
    // Line numbers
    size_t firstLine = getFirstVisibleLine();
    size_t lastLine = getLastVisibleLine();
    size_t currentLine = m_cursor->getPosition().line;
    
    for (size_t i = firstLine; i < lastLine && i < m_buffer->lineCount(); ++i) {
        float y = pos.y + (i - firstLine) * m_lineHeight;
        
        // Highlight current line number
        ImVec4 color = (i == currentLine) ? theme.text : theme.lineNumber;
        
        char lineNumStr[16];
        if (config.relativeLineNumbers && i != currentLine) {
            int relative = static_cast<int>(i) - static_cast<int>(currentLine);
            snprintf(lineNumStr, sizeof(lineNumStr), "%d", std::abs(relative));
        } else {
            snprintf(lineNumStr, sizeof(lineNumStr), "%zu", i + 1);
        }
        
        // Right-align line numbers
        ImVec2 textSize = ImGui::CalcTextSize(lineNumStr);
        float x = pos.x + gutterWidth - textSize.x - 8.0f;
        
        drawList->AddText(ImVec2(x, y), ImGui::ColorConvertFloat4ToU32(color), lineNumStr);
    }
}

void EditorPane::renderText(const EditorTheme& theme, const EditorConfig& config) {
    // Snapshot editing options for key handlers that run outside render().
    m_tabWidth = config.tabWidth > 0 ? config.tabWidth : 4;
    m_autoIndent = config.autoIndent;

    // Refresh syntax highlighting if the buffer changed since the last frame.
    if (m_highlighter) {
        auto getLine = [this](size_t i) { return m_buffer->getLine(i); };
        if (m_needsFullHighlight) {
            m_highlighter->highlightAllLines(getLine, m_buffer->lineCount());
            m_needsFullHighlight = false;
            m_dirtyFromLine = kNoDirtyLine;
        } else if (m_dirtyFromLine != kNoDirtyLine) {
            m_highlighter->applyEdit(getLine, m_buffer->lineCount(), m_dirtyFromLine);
            m_dirtyFromLine = kNoDirtyLine;
        }
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float gutterWidth = config.showLineNumbers ? 60.0f : 10.0f;
    
    pos.x += gutterWidth;
    
    size_t firstLine = getFirstVisibleLine();
    size_t lastLine = getLastVisibleLine();
    size_t currentLine = m_cursor->getPosition().line;
    
    // Highlight current line background
    if (config.highlightCurrentLine && currentLine >= firstLine && currentLine < lastLine) {
        float y = pos.y + (currentLine - firstLine) * m_lineHeight;
        drawList->AddRectFilled(
            ImVec2(pos.x - gutterWidth, y),
            ImVec2(pos.x + m_viewportWidth, y + m_lineHeight),
            ImGui::ColorConvertFloat4ToU32(theme.currentLine)
        );
    }
    
    // Render lines
    for (size_t i = firstLine; i < lastLine && i < m_buffer->lineCount(); ++i) {
        float y = pos.y + (i - firstLine) * m_lineHeight;
        std::string line = m_buffer->getLine(i);
        
        if (m_highlighter) {
            // Syntax highlighted rendering
            const auto* tokenizedLine = m_highlighter->getCachedLine(i);
            if (tokenizedLine) {
                float x = pos.x - m_scrollX;
                for (const auto& token : tokenizedLine->tokens) {
                    ImVec4 color = theme.text;
                    switch (token.type) {
                        case syntax::TokenType::Keyword: color = theme.keyword; break;
                        case syntax::TokenType::Type: color = theme.type; break;
                        case syntax::TokenType::String: color = theme.string; break;
                        case syntax::TokenType::Comment:
                        case syntax::TokenType::MultiLineComment: color = theme.comment; break;
                        case syntax::TokenType::Number: color = theme.number; break;
                        case syntax::TokenType::Function: color = theme.function; break;
                        case syntax::TokenType::Preprocessor: color = theme.preprocessor; break;
                        case syntax::TokenType::Operator: color = theme.operator_; break;
                        default: break;
                    }
                    
                    std::string tokenText = line.substr(token.start, token.length);
                    drawList->AddText(ImVec2(x, y), ImGui::ColorConvertFloat4ToU32(color), 
                                     tokenText.c_str());
                    x += token.length * m_charWidth;
                }
            } else {
                // Fallback: render without highlighting
                drawList->AddText(ImVec2(pos.x - m_scrollX, y), 
                                 ImGui::ColorConvertFloat4ToU32(theme.text), 
                                 line.c_str());
            }
        } else {
            // Plain text rendering
            drawList->AddText(ImVec2(pos.x - m_scrollX, y), 
                             ImGui::ColorConvertFloat4ToU32(theme.text), 
                             line.c_str());
        }
    }
    
    // Set content size for scroll
    float totalHeight = m_buffer->lineCount() * m_lineHeight;
    
    // Calculate max line width by sampling lines (full calculation would be expensive)
    float maxLineWidth = 0.0f;
    size_t sampleStep = std::max(size_t(1), m_buffer->lineCount() / 100);  // Sample ~100 lines
    for (size_t i = 0; i < m_buffer->lineCount(); i += sampleStep) {
        float lineWidth = ImGui::CalcTextSize(m_buffer->getLine(i).c_str()).x;
        maxLineWidth = std::max(maxLineWidth, lineWidth);
    }
    // Add padding for safety margin
    float totalWidth = maxLineWidth + 100.0f;
    
    ImGui::Dummy(ImVec2(totalWidth, totalHeight));
}

void EditorPane::renderCursor(const EditorTheme& theme) {
    if (!m_cursor->isVisible()) return;
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    auto screenPos = positionToScreen(m_cursor->getPosition());
    
    // Draw cursor line
    drawList->AddRectFilled(
        screenPos,
        ImVec2(screenPos.x + 2.0f, screenPos.y + m_lineHeight),
        ImGui::ColorConvertFloat4ToU32(theme.cursor)
    );
}

void EditorPane::renderSelection(const EditorTheme& theme) {
    if (!m_selection->hasSelection()) return;
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    auto range = m_selection->getNormalizedRange();
    
    size_t firstLine = getFirstVisibleLine();
    size_t lastLine = getLastVisibleLine();
    
    for (size_t line = range.start.line; line <= range.end.line; ++line) {
        if (line < firstLine || line >= lastLine) continue;
        
        size_t startCol = (line == range.start.line) ? range.start.column : 0;
        size_t endCol = (line == range.end.line) ? range.end.column : m_buffer->lineLength(line);
        
        auto startPos = positionToScreen({line, startCol});
        auto endPos = positionToScreen({line, endCol});
        
        drawList->AddRectFilled(
            startPos,
            ImVec2(endPos.x, startPos.y + m_lineHeight),
            ImGui::ColorConvertFloat4ToU32(theme.selection)
        );
    }
}

void EditorPane::handleKeyInput(int key, [[maybe_unused]] int scancode, int action, int mods) {
    if (action == GLFW_RELEASE) return;
    
    bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
    bool shift = (mods & GLFW_MOD_SHIFT) != 0;
    
    // Navigation
    if (key == GLFW_KEY_LEFT) {
        if (shift) m_selection->extendSelectionLeft();
        else { m_selection->clearSelection(); m_cursor->move(editor::CursorDirection::Left); }
    }
    else if (key == GLFW_KEY_RIGHT) {
        if (shift) m_selection->extendSelectionRight();
        else { m_selection->clearSelection(); m_cursor->move(editor::CursorDirection::Right); }
    }
    else if (key == GLFW_KEY_UP) {
        if (shift) m_selection->extendSelectionUp();
        else { m_selection->clearSelection(); m_cursor->move(editor::CursorDirection::Up); }
    }
    else if (key == GLFW_KEY_DOWN) {
        if (shift) m_selection->extendSelectionDown();
        else { m_selection->clearSelection(); m_cursor->move(editor::CursorDirection::Down); }
    }
    else if (key == GLFW_KEY_HOME) {
        if (ctrl) m_cursor->move(editor::CursorDirection::DocumentStart);
        else m_cursor->move(editor::CursorDirection::LineStart);
        if (!shift) m_selection->clearSelection();
    }
    else if (key == GLFW_KEY_END) {
        if (ctrl) m_cursor->move(editor::CursorDirection::DocumentEnd);
        else m_cursor->move(editor::CursorDirection::LineEnd);
        if (!shift) m_selection->clearSelection();
    }
    // Editing
    else if (key == GLFW_KEY_BACKSPACE) {
        handleBackspace();
    }
    else if (key == GLFW_KEY_DELETE) {
        handleDelete();
    }
    else if (key == GLFW_KEY_ENTER) {
        handleEnter();
    }
    else if (key == GLFW_KEY_TAB) {
        handleTab();
    }
    // Select all
    else if (ctrl && key == GLFW_KEY_A) {
        m_selection->selectAll();
    }
    
    scrollToCursor();
}

void EditorPane::handleCharInput(unsigned int codepoint) {
    if (codepoint < 32) return;  // Control characters
    
    deleteSelection();
    
    // Properly encode UTF-8 codepoint
    std::string utf8Char;
    if (codepoint < 0x80) {
        utf8Char = static_cast<char>(codepoint);
    } else if (codepoint < 0x800) {
        utf8Char += static_cast<char>(0xC0 | (codepoint >> 6));
        utf8Char += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x10000) {
        utf8Char += static_cast<char>(0xE0 | (codepoint >> 12));
        utf8Char += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        utf8Char += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x110000) {
        utf8Char += static_cast<char>(0xF0 | (codepoint >> 18));
        utf8Char += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        utf8Char += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        utf8Char += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
    
    insertText(utf8Char);
    scrollToCursor();
}

void EditorPane::handleMouseButton(int button, int action, int mods, double x, double y) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        ImVec2 localPos = ImVec2(x - m_renderPos.x, y - m_renderPos.y);
        auto pos = screenToPosition(localPos.x, localPos.y);
        
        if (action == GLFW_PRESS) {
            if (mods & GLFW_MOD_SHIFT) {
                // Extend selection
                m_selection->extendSelection(pos);
            } else {
                // Start new selection
                m_cursor->setPosition(pos);
                m_selection->clearSelection();
                m_selection->startDragSelection(pos);
            }
        } else if (action == GLFW_RELEASE) {
            m_selection->endDragSelection();
        }
    }
}

void EditorPane::handleMouseMove(double x, double y) {
    if (m_selection->hasSelection()) {
        ImVec2 localPos = ImVec2(x - m_renderPos.x, y - m_renderPos.y);
        auto pos = screenToPosition(localPos.x, localPos.y);
        m_selection->updateDragSelection(pos);
        m_cursor->setPosition(pos);
    }
}

void EditorPane::handleScroll(double xoffset, double yoffset) {
    m_scrollY -= yoffset * m_lineHeight * 3;
    m_scrollX -= xoffset * m_charWidth * 3;
    m_scrollY = std::max(0.0f, m_scrollY);
    m_scrollX = std::max(0.0f, m_scrollX);
}

// NOTE: buffer mutations below flow through TextBuffer::insert/remove, which
// fire the change callback installed in hookBufferChanges(); that callback marks
// the changed line for incremental re-highlighting, so no explicit flag is set
// here.

void EditorPane::insertChar(char c) {
    auto pos = m_cursor->getPosition();
    m_buffer->insert(pos, std::string_view(&c, 1));
    m_cursor->move(editor::CursorDirection::Right);
}

void EditorPane::insertText(const std::string& text) {
    auto pos = m_cursor->getPosition();
    m_buffer->insert(pos, text);
    // Move cursor to end of inserted text
    for (size_t i = 0; i < text.length(); ++i) {
        m_cursor->move(editor::CursorDirection::Right);
    }
}

void EditorPane::deleteSelection() {
    if (m_selection->hasSelection()) {
        auto range = m_selection->getNormalizedRange();
        m_buffer->remove(range);
        m_cursor->setPosition(range.start);
        m_selection->clearSelection();
    }
}

void EditorPane::handleBackspace() {
    if (m_selection->hasSelection()) {
        deleteSelection();
    } else {
        auto pos = m_cursor->getPosition();
        if (pos.column > 0 || pos.line > 0) {
            m_cursor->move(editor::CursorDirection::Left);
            auto newPos = m_cursor->getPosition();
            m_buffer->remove(m_buffer->positionToOffset(newPos), 1);
        }
    }
}

void EditorPane::handleDelete() {
    if (m_selection->hasSelection()) {
        deleteSelection();
    } else {
        size_t offset = m_cursor->getOffset();
        if (offset < m_buffer->size()) {
            m_buffer->remove(offset, 1);
        }
    }
}

void EditorPane::handleEnter() {
    deleteSelection();
    if (m_autoIndent) {
        size_t line = m_cursor->getPosition().line;
        std::string indent =
            editor::ops::computeNewlineIndent(*m_buffer, line, m_tabWidth, /*useSpaces=*/true);
        insertText("\n" + indent);
    } else {
        insertChar('\n');
    }
}

void EditorPane::handleTab() {
    deleteSelection();
    insertText(std::string(static_cast<size_t>(m_tabWidth), ' '));
}

// ====================
// Window Implementation
// ====================

Window::Window(int width, int height, const std::string& title) {
    initGLFW();
    
    // Create window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    
    m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);  // Enable vsync
    
    // Set user pointer for callbacks
    glfwSetWindowUserPointer(m_window, this);
    setupCallbacks();
    
    initImGui();
    
    // Create default pane
    m_panes.push_back(std::make_unique<EditorPane>());
    m_currentPane = m_panes[0].get();
    
    // Set default theme
    applyTheme();
}

Window::~Window() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    if (m_window) {
        glfwDestroyWindow(m_window);
    }
    glfwTerminate();
}

void Window::initGLFW() {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
}

void Window::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void Window::setupCallbacks() {
    glfwSetKeyCallback(m_window, keyCallback);
    glfwSetCharCallback(m_window, charCallback);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
    glfwSetCursorPosCallback(m_window, cursorPosCallback);
    glfwSetScrollCallback(m_window, scrollCallback);
    glfwSetDropCallback(m_window, dropCallback);
    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);
}

void Window::applyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Dark theme
    ImGui::StyleColorsDark();
    
    style.WindowRounding = 0.0f;
    style.FrameRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.TabRounding = 2.0f;
    
    // Custom colors matching editor theme
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = m_theme.background;
    colors[ImGuiCol_Text] = m_theme.text;
    colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
    colors[ImGuiCol_TabActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(m_window);
}

void Window::pollEvents() {
    glfwPollEvents();
}

void Window::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Window::endFrame() {
    ImGui::Render();
    
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    
    glClearColor(m_theme.background.x, m_theme.background.y, 
                 m_theme.background.z, m_theme.background.w);
    glClear(GL_COLOR_BUFFER_BIT);
    
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(m_window);
}

void Window::render() {
    // Main menu bar
    renderMenuBar();
    
    // Main editor window fills the primary viewport's work area.
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar |
                                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                    ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    ImGui::Begin("MainWindow", nullptr, window_flags);
    ImGui::PopStyleVar(3);
    
    // Tabs
    renderTabs();
    
    // Editor content
    if (m_currentPane) {
        m_currentPane->render(m_theme, m_config);
    }
    
    ImGui::End();
    
    // Status bar
    renderStatusBar();
    
    // Dialogs
    renderDialogs();
}

void Window::renderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) newFile();
            if (ImGui::MenuItem("Open...", "Ctrl+O")) showOpenFileDialog();
            if (ImGui::MenuItem("Save", "Ctrl+S")) saveCurrentFile();
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) showSaveFileDialog();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) glfwSetWindowShouldClose(m_window, GLFW_TRUE);
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                if (m_currentPane && m_currentPane->getBuffer()->canUndo()) {
                    m_currentPane->getBuffer()->undo();
                    m_currentPane->markHighlightDirty();
                }
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                if (m_currentPane && m_currentPane->getBuffer()->canRedo()) {
                    m_currentPane->getBuffer()->redo();
                    m_currentPane->markHighlightDirty();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X")) cutSelection();
            if (ImGui::MenuItem("Copy", "Ctrl+C")) copySelection();
            if (ImGui::MenuItem("Paste", "Ctrl+V")) pasteClipboard();
            ImGui::Separator();
            if (ImGui::MenuItem("Duplicate Line", "Ctrl+D")) duplicateCurrentLine();
            if (ImGui::MenuItem("Move Line Up", "Alt+Up")) moveCurrentLineUp();
            if (ImGui::MenuItem("Move Line Down", "Alt+Down")) moveCurrentLineDown();
            if (ImGui::MenuItem("Toggle Comment", "Ctrl+/")) toggleCommentCurrent();
            if (ImGui::MenuItem("Go to Matching Bracket", "Ctrl+]")) jumpToMatchingBracket();
            ImGui::Separator();
            if (ImGui::MenuItem("Find...", "Ctrl+F")) showFindDialog();
            if (ImGui::MenuItem("Replace...", "Ctrl+H")) showReplaceDialog();
            if (ImGui::MenuItem("Find in All Tabs...", "Ctrl+Shift+F")) showFindInFilesDialog();
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Line Numbers", nullptr, &m_config.showLineNumbers);
            ImGui::MenuItem("Word Wrap", nullptr, &m_config.wordWrap);
            ImGui::MenuItem("Minimap", nullptr, &m_config.showMinimap);
            ImGui::Separator();
            if (ImGui::MenuItem("Settings...", "Ctrl+,")) showSettingsDialog();
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {}
            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }
}

void Window::renderTabs() {
    if (ImGui::BeginTabBar("FileTabs", ImGuiTabBarFlags_Reorderable | 
                                        ImGuiTabBarFlags_AutoSelectNewTabs |
                                        ImGuiTabBarFlags_FittingPolicyScroll)) {
        EditorPane* paneToClose = nullptr;
        for (size_t i = 0; i < m_panes.size(); ++i) {
            auto& pane = m_panes[i];
            std::string name = pane->getDisplayName();
            if (pane->getBuffer()->isModified()) {
                name += " *";
            }
            // Per-pane ImGui id so tabs with the same filename stay distinct.
            std::string label = name + "###pane" + std::to_string(i);

            bool open = true;
            if (ImGui::BeginTabItem(label.c_str(), &open)) {
                m_currentPane = pane.get();
                ImGui::EndTabItem();
            }

            if (!open) {
                paneToClose = pane.get();
            }
        }
        ImGui::EndTabBar();

        // Handle close after iteration so we don't mutate m_panes mid-loop.
        if (paneToClose) {
            m_currentPane = paneToClose;
            closeCurrentFile();
        }
    }
}

void Window::renderStatusBar() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float statusBarHeight = 22.0f;
    
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, 
                                   viewport->WorkPos.y + viewport->WorkSize.y - statusBarHeight));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, statusBarHeight));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                            ImGuiWindowFlags_NoSavedSettings;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
    
    if (ImGui::Begin("StatusBar", nullptr, flags)) {
        if (m_currentPane) {
            auto pos = m_currentPane->getCursor().getPosition();
            ImGui::Text("Ln %zu, Col %zu", pos.line + 1, pos.column + 1);
            
            ImGui::SameLine(200);
            ImGui::Text("UTF-8");
            
            ImGui::SameLine(280);
            ImGui::Text("LF");
            
            ImGui::SameLine(viewport->WorkSize.x - 150);
            ImGui::Text("C++");
        }
    }
    ImGui::End();
    
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void Window::renderDialogs() {
    // Find dialog
    if (m_showFindDialog) {
        ImGui::SetNextWindowSize(ImVec2(400, 100), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Find", &m_showFindDialog)) {
            // Ensure findText has capacity for input
            if (m_findText.capacity() < 256) {
                m_findText.reserve(256);
            }
            m_findText.resize(256, '\0');
            
            bool enterPressed = ImGui::InputText("Find", &m_findText[0], 256, 
                                                  ImGuiInputTextFlags_EnterReturnsTrue);
            
            // Trim trailing nulls for string operations
            m_findText = m_findText.c_str();
            
            ImGui::Checkbox("Case Sensitive", &m_findCaseSensitive);
            ImGui::SameLine();
            ImGui::Checkbox("Regex", &m_findRegex);
            
            if (ImGui::Button("Find Next") || enterPressed) {
                if (!m_findText.empty() && m_currentPane) {
                    performSearch();
                    if (!m_searchResults.empty()) {
                        // Move to next match
                        m_currentMatchIndex = (m_currentMatchIndex + 1) % m_searchResults.size();
                        selectMatch(m_searchResults[m_currentMatchIndex]);
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Find Previous")) {
                if (!m_findText.empty() && m_currentPane) {
                    performSearch();
                    if (!m_searchResults.empty()) {
                        // Move to previous match
                        if (m_currentMatchIndex == 0) {
                            m_currentMatchIndex = m_searchResults.size() - 1;
                        } else {
                            m_currentMatchIndex--;
                        }
                        selectMatch(m_searchResults[m_currentMatchIndex]);
                    }
                }
            }
            
            // Show match count
            if (!m_searchResults.empty()) {
                ImGui::Text("Match %zu of %zu", m_currentMatchIndex + 1, m_searchResults.size());
            } else if (!m_findText.empty()) {
                ImGui::Text("No matches found");
            }
        }
        ImGui::End();
    }
    
    // Replace dialog
    if (m_showReplaceDialog) {
        ImGui::SetNextWindowSize(ImVec2(400, 180), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Replace", &m_showReplaceDialog)) {
            // Ensure text buffers have capacity
            if (m_findText.capacity() < 256) m_findText.reserve(256);
            if (m_replaceText.capacity() < 256) m_replaceText.reserve(256);
            m_findText.resize(256, '\0');
            m_replaceText.resize(256, '\0');
            
            ImGui::InputText("Find", &m_findText[0], 256);
            ImGui::InputText("Replace", &m_replaceText[0], 256);
            
            // Trim trailing nulls
            m_findText = m_findText.c_str();
            m_replaceText = m_replaceText.c_str();
            
            ImGui::Checkbox("Case Sensitive", &m_findCaseSensitive);
            ImGui::SameLine();
            ImGui::Checkbox("Regex", &m_findRegex);
            
            if (ImGui::Button("Replace") && m_currentPane && !m_findText.empty()) {
                // Replace current selection if it matches
                auto& selection = m_currentPane->getSelection();
                if (selection.hasSelection()) {
                    auto range = selection.getNormalizedRange();
                    size_t startOffset = m_currentPane->getBuffer()->positionToOffset(range.start);
                    std::string selectedText = m_currentPane->getBuffer()->getText(range);
                    
                    // Check if selection matches search pattern
                    search::SearchOptions opts;
                    opts.caseSensitive = m_findCaseSensitive;
                    opts.useRegex = m_findRegex;
                    auto matches = m_searchEngine.search(selectedText, m_findText, opts);
                    
                    if (!matches.empty() && matches[0].offset == 0 && matches[0].length == selectedText.size()) {
                        // Selection matches - replace it
                        m_currentPane->getBuffer()->remove(startOffset, selectedText.size());
                        m_currentPane->getBuffer()->insert(startOffset, m_replaceText);
                        
                        // Move to next match
                        performSearch();
                        if (!m_searchResults.empty()) {
                            m_currentMatchIndex = 0;
                            selectMatch(m_searchResults[m_currentMatchIndex]);
                        }
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Replace All") && m_currentPane && !m_findText.empty()) {
                search::SearchOptions opts;
                opts.caseSensitive = m_findCaseSensitive;
                opts.useRegex = m_findRegex;
                
                std::string text = m_currentPane->getBuffer()->getText();
                std::string result = m_searchEngine.replace(text, m_findText, m_replaceText, opts);
                
                // Replace entire buffer content
                m_currentPane->getBuffer()->remove(0, m_currentPane->getBuffer()->size());
                m_currentPane->getBuffer()->insert(0, result);
                
                // Clear search results
                m_searchResults.clear();
                m_currentMatchIndex = 0;
            }
            
            // Show match count
            if (!m_searchResults.empty()) {
                ImGui::Text("%zu matches found", m_searchResults.size());
            }
        }
        ImGui::End();
    }
    
    // Unsaved changes confirmation popup
    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("The current file has unsaved changes.\nDo you want to save before closing?");
        ImGui::Separator();
        
        if (ImGui::Button("Save", ImVec2(80, 0))) {
            saveCurrentFile();
            doCloseCurrentFile();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save", ImVec2(100, 0))) {
            doCloseCurrentFile();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Find in all open tabs
    if (m_showFindInFilesDialog) {
        ImGui::SetNextWindowSize(ImVec2(560, 360), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Find in All Tabs", &m_showFindInFilesDialog)) {
            if (m_findText.capacity() < 256) m_findText.reserve(256);
            m_findText.resize(256, '\0');

            bool enterPressed = ImGui::InputText("Search", &m_findText[0], 256,
                                                 ImGuiInputTextFlags_EnterReturnsTrue);
            m_findText = m_findText.c_str();  // trim trailing nulls

            ImGui::Checkbox("Case Sensitive", &m_findCaseSensitive);
            ImGui::SameLine();
            ImGui::Checkbox("Regex", &m_findRegex);
            ImGui::SameLine();
            if (ImGui::Button("Search All") || enterPressed) {
                performGlobalSearch();
            }

            ImGui::Separator();
            if (m_globalResults.empty()) {
                ImGui::TextDisabled("%s", m_findText.empty()
                                        ? "Enter a query and press Search All."
                                        : "No matches across open tabs.");
            } else {
                ImGui::Text("%zu match%s", m_globalResults.size(),
                            m_globalResults.size() == 1 ? "" : "es");
            }

            ImGui::BeginChild("GlobalResults", ImVec2(0, 0), true);
            for (size_t i = 0; i < m_globalResults.size(); ++i) {
                const auto& r = m_globalResults[i];
                // Stable per-row id even when files share a name.
                std::string label = r.file + ":" + std::to_string(r.line + 1) +
                                    ":" + std::to_string(r.column + 1) + "  " +
                                    r.preview + "##g" + std::to_string(i);
                if (ImGui::Selectable(label.c_str())) {
                    jumpToGlobalResult(r);
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
}

void Window::openFile(const std::string& path) {
    // Reuse the current pane only if it is a pristine, untitled buffer;
    // otherwise open the file in its own new tab so multiple files coexist.
    EditorPane* pane = m_currentPane;
    bool reuseCurrent = pane && pane->getFilePath().empty() &&
                        pane->getBuffer()->empty() && !pane->getBuffer()->isModified();
    if (!reuseCurrent) {
        m_panes.push_back(std::make_unique<EditorPane>());
        pane = m_panes.back().get();
    }

    if (!pane->getBuffer()->loadFromFile(path)) {
        // Load failed: discard the speculatively-added pane, keep current.
        if (!reuseCurrent) {
            m_panes.pop_back();
        }
        return;
    }

    pane->setFilePath(path);
    pane->getBuffer()->setModified(false);
    m_currentPane = pane;

    // Set up syntax highlighting based on the file extension. The token cache
    // is (re)built lazily on the next render via the highlight-dirty flag.
    auto* langDef = syntax::LanguageRegistry::instance().getLanguageByFilename(path);
    if (langDef) {
        pane->setHighlighter(std::make_shared<syntax::Highlighter>(*langDef));
    }
}

void Window::saveCurrentFile() {
    if (m_currentPane && !m_currentPane->getFilePath().empty()) {
        if (m_currentPane->getBuffer()->saveToFile(m_currentPane->getFilePath())) {
            m_currentPane->getBuffer()->setModified(false);
        }
    } else {
        showSaveFileDialog();
    }
}

void Window::saveFileAs(const std::string& path) {
    if (!m_currentPane) {
        return;
    }
    if (!m_currentPane->getBuffer()->saveToFile(path)) {
        return;
    }
    m_currentPane->setFilePath(path);
    m_currentPane->getBuffer()->setModified(false);

    // Attach a highlighter for the new file type if the pane doesn't have one.
    if (!m_currentPane->getHighlighter()) {
        auto* langDef = syntax::LanguageRegistry::instance().getLanguageByFilename(path);
        if (langDef) {
            m_currentPane->setHighlighter(std::make_shared<syntax::Highlighter>(*langDef));
        }
    }
}

void Window::newFile() {
    // If the current pane is already a pristine untitled buffer, just keep it
    // rather than spawning a second empty tab.
    if (m_currentPane && m_currentPane->getFilePath().empty() &&
        m_currentPane->getBuffer()->empty() && !m_currentPane->getBuffer()->isModified()) {
        return;
    }
    m_panes.push_back(std::make_unique<EditorPane>());
    m_currentPane = m_panes.back().get();
}

void Window::closeCurrentFile() {
    // Check for unsaved changes
    if (m_currentPane && m_currentPane->getBuffer()->isModified()) {
        // Show confirmation dialog
        // For now, we use ImGui popup - could be native dialog
        ImGui::OpenPopup("Unsaved Changes");
    } else {
        doCloseCurrentFile();
    }
}

void Window::doCloseCurrentFile() {
    if (m_panes.size() > 1) {
        auto it = std::find_if(m_panes.begin(), m_panes.end(),
                              [this](const auto& p) { return p.get() == m_currentPane; });
        if (it != m_panes.end()) {
            m_panes.erase(it);
            m_currentPane = m_panes.back().get();
        }
    } else {
        newFile();
    }
}

void Window::showOpenFileDialog() {
    m_showFindDialog = false;
    m_showReplaceDialog = false;
    
#ifdef _WIN32
    // Windows native file dialog
    OPENFILENAMEA ofn;
    char szFile[260] = {0};
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = glfwGetWin32Window(m_window);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "All Files\0*.*\0Text Files\0*.TXT\0C++ Files\0*.CPP;*.HPP;*.H;*.CC\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    
    if (GetOpenFileNameA(&ofn)) {
        openFile(ofn.lpstrFile);
    }
#elif defined(__linux__) || defined(__unix__)
    // Linux: Use zenity if available, fallback to command line
    FILE* pipe = popen("zenity --file-selection 2>/dev/null", "r");
    if (pipe) {
        char path[512];
        if (fgets(path, sizeof(path), pipe)) {
            // Remove trailing newline
            size_t len = strlen(path);
            if (len > 0 && path[len-1] == '\n') {
                path[len-1] = '\0';
            }
            openFile(path);
        }
        pclose(pipe);
    }
#elif defined(__APPLE__)
    // macOS: Use osascript for native dialog
    FILE* pipe = popen("osascript -e 'POSIX path of (choose file)'", "r");
    if (pipe) {
        char path[512];
        if (fgets(path, sizeof(path), pipe)) {
            size_t len = strlen(path);
            if (len > 0 && path[len-1] == '\n') {
                path[len-1] = '\0';
            }
            openFile(path);
        }
        pclose(pipe);
    }
#endif
}

void Window::showSaveFileDialog() {
#ifdef _WIN32
    OPENFILENAMEA ofn;
    char szFile[260] = {0};
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = glfwGetWin32Window(m_window);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "All Files\0*.*\0Text Files\0*.TXT\0C++ Files\0*.CPP;*.HPP;*.H;*.CC\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.lpstrDefExt = "txt";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    
    if (GetSaveFileNameA(&ofn)) {
        saveFileAs(ofn.lpstrFile);
    }
#elif defined(__linux__) || defined(__unix__)
    FILE* pipe = popen("zenity --file-selection --save --confirm-overwrite 2>/dev/null", "r");
    if (pipe) {
        char path[512];
        if (fgets(path, sizeof(path), pipe)) {
            size_t len = strlen(path);
            if (len > 0 && path[len-1] == '\n') {
                path[len-1] = '\0';
            }
            saveFileAs(path);
        }
        pclose(pipe);
    }
#elif defined(__APPLE__)
    FILE* pipe = popen("osascript -e 'POSIX path of (choose file name default name \"untitled.txt\")'", "r");
    if (pipe) {
        char path[512];
        if (fgets(path, sizeof(path), pipe)) {
            size_t len = strlen(path);
            if (len > 0 && path[len-1] == '\n') {
                path[len-1] = '\0';
            }
            saveFileAs(path);
        }
        pclose(pipe);
    }
#endif
}

void Window::showFindDialog() {
    m_showFindDialog = true;
    m_showReplaceDialog = false;
}

void Window::showReplaceDialog() {
    m_showReplaceDialog = true;
    m_showFindDialog = false;
}

void Window::showSettingsDialog() {
    m_showSettingsDialog = true;
}

void Window::showFindInFilesDialog() {
    m_showFindInFilesDialog = true;
    m_showFindDialog = false;
    m_showReplaceDialog = false;
}

int Window::getWidth() const {
    int width, height;
    glfwGetWindowSize(m_window, &width, &height);
    return width;
}

int Window::getHeight() const {
    int width, height;
    glfwGetWindowSize(m_window, &width, &height);
    return height;
}

void Window::setTitle(const std::string& title) {
    glfwSetWindowTitle(m_window, title.c_str());
}

// Callbacks
void Window::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    
    // Handle global shortcuts
    if (action == GLFW_PRESS && (mods & GLFW_MOD_CONTROL)) {
        switch (key) {
            case GLFW_KEY_N: self->newFile(); return;
            case GLFW_KEY_O: self->showOpenFileDialog(); return;
            case GLFW_KEY_S: 
                if (mods & GLFW_MOD_SHIFT) self->showSaveFileDialog();
                else self->saveCurrentFile();
                return;
            case GLFW_KEY_F:
                if (mods & GLFW_MOD_SHIFT) self->showFindInFilesDialog();
                else self->showFindDialog();
                return;
            case GLFW_KEY_H: self->showReplaceDialog(); return;
            case GLFW_KEY_Z:
                if (self->m_currentPane && self->m_currentPane->getBuffer()->canUndo()) {
                    self->m_currentPane->getBuffer()->undo();
                    self->m_currentPane->markHighlightDirty();
                }
                return;
            case GLFW_KEY_Y:
                if (self->m_currentPane && self->m_currentPane->getBuffer()->canRedo()) {
                    self->m_currentPane->getBuffer()->redo();
                    self->m_currentPane->markHighlightDirty();
                }
                return;
            case GLFW_KEY_C: self->copySelection(); return;
            case GLFW_KEY_X: self->cutSelection(); return;
            case GLFW_KEY_V: self->pasteClipboard(); return;
            case GLFW_KEY_D: self->duplicateCurrentLine(); return;
            case GLFW_KEY_SLASH: self->toggleCommentCurrent(); return;
            case GLFW_KEY_RIGHT_BRACKET: self->jumpToMatchingBracket(); return;
        }
    }

    // Alt+Up / Alt+Down move the current line.
    if (action != GLFW_RELEASE && (mods & GLFW_MOD_ALT) && !(mods & GLFW_MOD_CONTROL)) {
        if (key == GLFW_KEY_UP) { self->moveCurrentLineUp(); return; }
        if (key == GLFW_KEY_DOWN) { self->moveCurrentLineDown(); return; }
    }

    if (self->m_currentPane) {
        self->m_currentPane->handleKeyInput(key, scancode, action, mods);
    }
}

void Window::charCallback(GLFWwindow* window, unsigned int codepoint) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self->m_currentPane) {
        self->m_currentPane->handleCharInput(codepoint);
    }
}

void Window::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    if (self->m_currentPane) {
        self->m_currentPane->handleMouseButton(button, action, mods, x, y);
    }
}

void Window::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self->m_currentPane) {
        self->m_currentPane->handleMouseMove(xpos, ypos);
    }
}

void Window::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self->m_currentPane) {
        self->m_currentPane->handleScroll(xoffset, yoffset);
    }
}

void Window::dropCallback(GLFWwindow* window, int count, const char** paths) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    for (int i = 0; i < count; ++i) {
        self->openFile(paths[i]);
    }
}

void Window::framebufferSizeCallback([[maybe_unused]] GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// ====================
// Search helper methods
// ====================

void Window::performSearch() {
    if (!m_currentPane || m_findText.empty()) {
        m_searchResults.clear();
        return;
    }
    
    search::SearchOptions opts;
    opts.caseSensitive = m_findCaseSensitive;
    opts.useRegex = m_findRegex;
    
    std::string text = m_currentPane->getBuffer()->getText();
    m_searchResults = m_searchEngine.search(text, m_findText, opts);
    
    if (!m_searchResults.empty() && m_currentMatchIndex >= m_searchResults.size()) {
        m_currentMatchIndex = 0;
    }
}

void Window::selectMatch(const search::SearchMatch& match) {
    if (!m_currentPane) return;
    
    auto& buffer = *m_currentPane->getBuffer();
    auto& cursor = m_currentPane->getCursor();
    auto& selection = m_currentPane->getSelection();
    
    // Convert offset to position
    auto startPos = buffer.offsetToPosition(match.offset);
    auto endPos = buffer.offsetToPosition(match.offset + match.length);
    
    // Set cursor to end of match and create selection from start to end
    cursor.setPosition(endPos);
    selection.startSelection(startPos, editor::SelectionMode::Normal);
    selection.extendSelection(endPos);
    
    // Scroll to make the match visible
    m_currentPane->scrollToCursor();
}

void Window::performGlobalSearch() {
    m_globalResults.clear();
    if (m_findText.empty()) {
        return;
    }

    search::SearchOptions opts;
    opts.caseSensitive = m_findCaseSensitive;
    opts.useRegex = m_findRegex;

    for (size_t paneIndex = 0; paneIndex < m_panes.size(); ++paneIndex) {
        auto& pane = m_panes[paneIndex];
        auto& buffer = *pane->getBuffer();
        std::string text = buffer.getText();

        auto matches = m_searchEngine.search(text, m_findText, opts);
        for (const auto& match : matches) {
            GlobalSearchResult result;
            result.paneIndex = paneIndex;
            result.file = pane->getDisplayName();
            result.offset = match.offset;
            result.length = match.length;

            auto pos = buffer.offsetToPosition(match.offset);
            result.line = pos.line;
            result.column = pos.column;

            // Build a trimmed single-line preview around the match.
            std::string lineText = buffer.getLine(pos.line);
            const size_t kMaxPreview = 120;
            if (lineText.size() > kMaxPreview) {
                lineText = lineText.substr(0, kMaxPreview) + "...";
            }
            size_t firstNonWs = lineText.find_first_not_of(" \t");
            if (firstNonWs != std::string::npos && firstNonWs > 0) {
                lineText = lineText.substr(firstNonWs);
            }
            result.preview = lineText;

            m_globalResults.push_back(std::move(result));
        }
    }
}

void Window::jumpToGlobalResult(const GlobalSearchResult& result) {
    if (result.paneIndex >= m_panes.size()) {
        return;
    }
    m_currentPane = m_panes[result.paneIndex].get();

    search::SearchMatch match;
    match.offset = result.offset;
    match.length = result.length;
    selectMatch(match);
}

// ====================
// Clipboard operations
// ====================

void Window::copySelection() {
    if (!m_currentPane) return;
    auto& selection = m_currentPane->getSelection();
    if (!selection.hasSelection()) return;

    auto range = selection.getNormalizedRange();
    std::string text = m_currentPane->getBuffer()->getText(range);

    m_clipboard.copy(text);
    editor::Clipboard::copyToSystem(text);
}

void Window::cutSelection() {
    if (!m_currentPane) return;
    auto& selection = m_currentPane->getSelection();
    if (!selection.hasSelection()) return;

    auto range = selection.getNormalizedRange();
    std::string text = m_currentPane->getBuffer()->getText(range);

    m_clipboard.cut(text);
    editor::Clipboard::copyToSystem(text);

    // Remove the selected text and collapse the cursor to the cut point.
    // (The buffer's change callback marks the line for incremental re-highlight.)
    m_currentPane->getBuffer()->remove(range);
    m_currentPane->getCursor().setPosition(range.start);
    selection.clearSelection();
}

void Window::pasteClipboard() {
    if (!m_currentPane) return;

    // Prefer the system clipboard; fall back to the internal history.
    std::string text;
    if (auto sys = editor::Clipboard::pasteFromSystem()) {
        text = *sys;
    } else if (auto entry = m_clipboard.paste()) {
        text = entry->text;
    }
    if (text.empty()) return;

    auto& selection = m_currentPane->getSelection();
    auto& buffer = *m_currentPane->getBuffer();
    auto& cursor = m_currentPane->getCursor();

    // Replace any active selection first.
    if (selection.hasSelection()) {
        auto range = selection.getNormalizedRange();
        buffer.remove(range);
        cursor.setPosition(range.start);
        selection.clearSelection();
    }

    size_t offset = buffer.positionToOffset(cursor.getPosition());
    buffer.insert(offset, text);
    cursor.setPosition(buffer.offsetToPosition(offset + text.size()));
    // The buffer's change callback marks the line for incremental re-highlight.
}

// ====================
// Editor commands
// ====================

void Window::duplicateCurrentLine() {
    if (!m_currentPane) return;
    auto& buf = *m_currentPane->getBuffer();
    auto& cursor = m_currentPane->getCursor();
    auto pos = cursor.getPosition();
    editor::ops::duplicateLine(buf, pos.line);
    // The copy is inserted above, so the original (and cursor) shift down one.
    cursor.setPosition({pos.line + 1, pos.column});
}

void Window::moveCurrentLineDown() {
    if (!m_currentPane) return;
    auto& buf = *m_currentPane->getBuffer();
    auto& cursor = m_currentPane->getCursor();
    auto pos = cursor.getPosition();
    if (pos.line + 1 >= buf.lineCount()) return;
    editor::ops::moveLineDown(buf, pos.line);
    cursor.setPosition({pos.line + 1, pos.column});
}

void Window::moveCurrentLineUp() {
    if (!m_currentPane) return;
    auto& buf = *m_currentPane->getBuffer();
    auto& cursor = m_currentPane->getCursor();
    auto pos = cursor.getPosition();
    if (pos.line == 0) return;
    editor::ops::moveLineUp(buf, pos.line);
    cursor.setPosition({pos.line - 1, pos.column});
}

void Window::toggleCommentCurrent() {
    if (!m_currentPane) return;
    auto& buf = *m_currentPane->getBuffer();
    auto& selection = m_currentPane->getSelection();

    size_t startLine, endLine;
    if (selection.hasSelection()) {
        auto range = selection.getNormalizedRange();
        startLine = range.start.line;
        endLine = range.end.line;
    } else {
        startLine = endLine = m_currentPane->getCursor().getPosition().line;
    }

    // Use the language's line-comment token when known, else default to "//".
    std::string token = "//";
    if (auto highlighter = m_currentPane->getHighlighter()) {
        const std::string& slc = highlighter->getLanguage().singleLineComment;
        if (!slc.empty()) {
            token = slc;
        }
    }
    editor::ops::toggleLineComment(buf, startLine, endLine, token);
}

void Window::jumpToMatchingBracket() {
    if (!m_currentPane) return;
    auto& buf = *m_currentPane->getBuffer();
    auto& cursor = m_currentPane->getCursor();

    size_t off = buf.positionToOffset(cursor.getPosition());
    auto match = editor::ops::findMatchingBracket(buf, off);
    if (!match && off > 0) {
        // The cursor may sit just after a closing bracket.
        match = editor::ops::findMatchingBracket(buf, off - 1);
    }
    if (match) {
        cursor.setPosition(buf.offsetToPosition(*match));
        m_currentPane->scrollToCursor();
    }
}

} // namespace ui
