/**
 * @file tabs.cpp
 * @brief Tab bar implementation for multi-file editing
 */

#include "ui/tabs.hpp"
#include "ui/window.hpp"
#include "imgui.h"
#include <algorithm>
#include <chrono>

namespace ui {

TabBar::TabBar() {
    m_tabs.reserve(16);
}

TabBar::~TabBar() = default;

size_t TabBar::addTab(const std::string& title, const std::string& filePath) {
    TabInfo info;
    info.title = title;
    info.filePath = filePath;
    info.buffer = std::make_shared<editor::TextBuffer>();
    info.lastAccessTime = std::chrono::steady_clock::now().time_since_epoch().count();
    
    return addTab(info);
}

size_t TabBar::addTab(const TabInfo& info) {
    m_tabs.push_back(info);
    size_t index = m_tabs.size() - 1;
    setActiveTab(index);
    return index;
}

void TabBar::removeTab(size_t index) {
    if (index >= m_tabs.size()) return;
    
    m_tabs.erase(m_tabs.begin() + index);
    
    if (m_activeTab >= m_tabs.size() && m_activeTab > 0) {
        --m_activeTab;
    }
    
    if (m_changeCallback && !m_tabs.empty()) {
        m_changeCallback(m_activeTab);
    }
}

void TabBar::closeTab(size_t index) {
    if (index >= m_tabs.size()) return;
    
    auto& tab = m_tabs[index];
    
    // Check if modified and callback wants to cancel
    if (tab.isModified && m_closeCallback) {
        if (!m_closeCallback(index)) {
            return;  // Close cancelled
        }
    }
    
    removeTab(index);
}

void TabBar::closeAllTabs() {
    while (!m_tabs.empty()) {
        closeTab(m_tabs.size() - 1);
    }
}

void TabBar::closeOtherTabs(size_t keepIndex) {
    if (keepIndex >= m_tabs.size()) return;
    
    // Close tabs after keepIndex first (reverse order)
    for (size_t i = m_tabs.size() - 1; i > keepIndex; --i) {
        closeTab(i);
    }
    
    // Then close tabs before keepIndex
    while (m_tabs.size() > 1) {
        closeTab(0);
    }
}

void TabBar::setActiveTab(size_t index) {
    if (index >= m_tabs.size()) return;
    
    m_activeTab = index;
    m_tabs[index].lastAccessTime = std::chrono::steady_clock::now().time_since_epoch().count();
    
    if (m_changeCallback) {
        m_changeCallback(index);
    }
}

TabInfo* TabBar::getTab(size_t index) {
    if (index >= m_tabs.size()) return nullptr;
    return &m_tabs[index];
}

const TabInfo* TabBar::getTab(size_t index) const {
    if (index >= m_tabs.size()) return nullptr;
    return &m_tabs[index];
}

TabInfo* TabBar::getActiveTab() {
    return getTab(m_activeTab);
}

const TabInfo* TabBar::getActiveTab() const {
    return getTab(m_activeTab);
}

int TabBar::findTabByPath(const std::string& filePath) const {
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].filePath == filePath) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int TabBar::findTabByTitle(const std::string& title) const {
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].title == title) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void TabBar::markModified(size_t index, bool modified) {
    if (index >= m_tabs.size()) return;
    m_tabs[index].isModified = modified;
}

void TabBar::pinTab(size_t index, bool pinned) {
    if (index >= m_tabs.size()) return;
    m_tabs[index].isPinned = pinned;
}

void TabBar::updateTitle(size_t index, const std::string& title) {
    if (index >= m_tabs.size()) return;
    m_tabs[index].title = title;
}

void TabBar::moveTab(size_t fromIndex, size_t toIndex) {
    if (fromIndex >= m_tabs.size() || toIndex >= m_tabs.size()) return;
    if (fromIndex == toIndex) return;
    
    TabInfo tab = std::move(m_tabs[fromIndex]);
    m_tabs.erase(m_tabs.begin() + fromIndex);
    m_tabs.insert(m_tabs.begin() + toIndex, std::move(tab));
    
    // Update active tab index
    if (m_activeTab == fromIndex) {
        m_activeTab = toIndex;
    } else if (fromIndex < m_activeTab && toIndex >= m_activeTab) {
        --m_activeTab;
    } else if (fromIndex > m_activeTab && toIndex <= m_activeTab) {
        ++m_activeTab;
    }
}

void TabBar::swapTabs(size_t index1, size_t index2) {
    if (index1 >= m_tabs.size() || index2 >= m_tabs.size()) return;
    std::swap(m_tabs[index1], m_tabs[index2]);
    
    if (m_activeTab == index1) {
        m_activeTab = index2;
    } else if (m_activeTab == index2) {
        m_activeTab = index1;
    }
}

void TabBar::nextTab() {
    if (m_tabs.empty()) return;
    setActiveTab((m_activeTab + 1) % m_tabs.size());
}

void TabBar::previousTab() {
    if (m_tabs.empty()) return;
    setActiveTab(m_activeTab > 0 ? m_activeTab - 1 : m_tabs.size() - 1);
}

void TabBar::goToTab(size_t index) {
    setActiveTab(index);
}

void TabBar::render([[maybe_unused]] const EditorTheme& theme,
                    [[maybe_unused]] const EditorConfig& config) {
    if (m_tabs.empty()) return;
    
    ImGuiTabBarFlags flags = ImGuiTabBarFlags_Reorderable |
                             ImGuiTabBarFlags_AutoSelectNewTabs |
                             ImGuiTabBarFlags_FittingPolicyScroll |
                             ImGuiTabBarFlags_TabListPopupButton;
    
    if (ImGui::BeginTabBar("EditorTabs", flags)) {
        for (size_t i = 0; i < m_tabs.size(); ++i) {
            auto& tab = m_tabs[i];
            
            // Build tab label
            std::string label = tab.title;
            if (tab.isModified) {
                label += " *";
            }
            if (tab.isPinned) {
                label = "📌 " + label;
            }
            label += "##" + std::to_string(i);
            
            ImGuiTabItemFlags tabFlags = 0;
            if (tab.isModified) {
                tabFlags |= ImGuiTabItemFlags_UnsavedDocument;
            }
            
            bool open = true;
            if (ImGui::BeginTabItem(label.c_str(), &open, tabFlags)) {
                if (m_activeTab != i) {
                    setActiveTab(i);
                }
                ImGui::EndTabItem();
            }
            
            // Handle close
            if (!open) {
                closeTab(i);
                if (i > 0) --i;  // Adjust loop counter
            }
            
            // Context menu
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Close")) {
                    closeTab(i);
                }
                if (ImGui::MenuItem("Close Others")) {
                    closeOtherTabs(i);
                }
                if (ImGui::MenuItem("Close All")) {
                    closeAllTabs();
                }
                ImGui::Separator();
                if (ImGui::MenuItem(tab.isPinned ? "Unpin" : "Pin")) {
                    pinTab(i, !tab.isPinned);
                }
                ImGui::EndPopup();
            }
        }
        ImGui::EndTabBar();
    }
}

void TabBar::beginDrag(size_t index) {
    m_isDragging = true;
    m_dragSourceIndex = index;
}

void TabBar::updateDrag([[maybe_unused]] float mouseX) {
    // Calculate target position based on mouse X
    // This would require knowing tab widths
}

void TabBar::endDrag() {
    m_isDragging = false;
}

float TabBar::calculateTabWidth(const TabInfo& tab) const {
    // Calculate based on title length
    float charWidth = 8.0f;  // Approximate
    float padding = 40.0f;   // Close button + padding
    return tab.title.length() * charWidth + padding;
}

} // namespace ui
