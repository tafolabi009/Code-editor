#pragma once

#include "../editor/text_buffer.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace ui {

class EditorPane;
struct EditorTheme;
struct EditorConfig;

/**
 * @brief Tab information for a single open file
 */
struct TabInfo {
    std::string title;
    std::string filePath;
    std::shared_ptr<editor::TextBuffer> buffer;
    std::shared_ptr<EditorPane> pane;
    bool isModified = false;
    bool isPinned = false;
    size_t lastAccessTime = 0;
};

/**
 * @brief Tab bar for managing multiple open files
 */
class TabBar {
public:
    using TabCloseCallback = std::function<bool(size_t tabIndex)>;  // Return false to cancel close
    using TabChangeCallback = std::function<void(size_t newTabIndex)>;
    
    TabBar();
    ~TabBar();
    
    // Tab operations
    size_t addTab(const std::string& title, const std::string& filePath = "");
    size_t addTab(const TabInfo& info);
    void removeTab(size_t index);
    void closeTab(size_t index);  // With confirmation if modified
    void closeAllTabs();
    void closeOtherTabs(size_t keepIndex);
    
    // Tab access
    size_t getTabCount() const { return m_tabs.size(); }
    size_t getActiveTabIndex() const { return m_activeTab; }
    void setActiveTab(size_t index);
    TabInfo* getTab(size_t index);
    const TabInfo* getTab(size_t index) const;
    TabInfo* getActiveTab();
    const TabInfo* getActiveTab() const;
    
    // Find tabs
    int findTabByPath(const std::string& filePath) const;
    int findTabByTitle(const std::string& title) const;
    
    // Tab state
    void markModified(size_t index, bool modified = true);
    void pinTab(size_t index, bool pinned = true);
    void updateTitle(size_t index, const std::string& title);
    
    // Reordering
    void moveTab(size_t fromIndex, size_t toIndex);
    void swapTabs(size_t index1, size_t index2);
    
    // Navigation
    void nextTab();
    void previousTab();
    void goToTab(size_t index);
    
    // Rendering
    void render(const EditorTheme& theme, const EditorConfig& config);
    
    // Callbacks
    void setTabCloseCallback(TabCloseCallback callback) { m_closeCallback = callback; }
    void setTabChangeCallback(TabChangeCallback callback) { m_changeCallback = callback; }
    
    // Drag and drop
    void beginDrag(size_t index);
    void updateDrag(float mouseX);
    void endDrag();
    bool isDragging() const { return m_isDragging; }
    
private:
    std::vector<TabInfo> m_tabs;
    size_t m_activeTab = 0;
    size_t m_tabIdCounter = 0;
    
    TabCloseCallback m_closeCallback;
    TabChangeCallback m_changeCallback;
    
    // Drag state
    bool m_isDragging = false;
    size_t m_dragSourceIndex = 0;
    float m_dragStartX = 0.0f;
    
    // Rendering helpers
    float calculateTabWidth(const TabInfo& tab) const;
    void renderTab(size_t index, float x, float width, const EditorTheme& theme);
    void renderCloseButton(size_t index, float x, float y);
};

} // namespace ui
