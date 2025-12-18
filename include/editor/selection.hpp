#pragma once

#include "text_buffer.hpp"
#include "cursor.hpp"

namespace editor {

/**
 * @brief Selection mode types
 */
enum class SelectionMode {
    None,       // No active selection
    Normal,     // Standard character selection
    Line,       // Full line selection
    Block       // Column/block selection
};

/**
 * @brief Text selection handling
 */
class Selection {
public:
    explicit Selection(TextBuffer* buffer, Cursor* cursor);
    ~Selection() = default;
    
    // Selection state
    bool hasSelection() const { return m_hasSelection; }
    SelectionMode getMode() const { return m_mode; }
    
    // Selection range
    Range getRange() const;
    Position getAnchor() const { return m_anchor; }
    Position getHead() const;  // Current cursor position
    
    // Get selected text
    std::string getSelectedText() const;
    
    // Selection operations
    void startSelection(SelectionMode mode = SelectionMode::Normal);
    void startSelection(const Position& anchor, SelectionMode mode = SelectionMode::Normal);
    void extendSelection(const Position& to);
    void clearSelection();
    void selectAll();
    void selectLine(size_t lineIndex);
    void selectWord();
    void selectWord(const Position& pos);
    
    // Keyboard selection
    void extendSelectionLeft();
    void extendSelectionRight();
    void extendSelectionUp();
    void extendSelectionDown();
    void extendSelectionWordLeft();
    void extendSelectionWordRight();
    void extendSelectionToLineStart();
    void extendSelectionToLineEnd();
    
    // Mouse selection
    void startDragSelection(const Position& pos);
    void updateDragSelection(const Position& pos);
    void endDragSelection();
    
    // Block selection (column mode)
    bool isBlockMode() const { return m_mode == SelectionMode::Block; }
    std::vector<Range> getBlockRanges() const;
    
    // Double/triple click selection
    void selectWordAtPosition(const Position& pos);
    void selectLineAtPosition(const Position& pos);
    
    // Normalize selection (ensure start <= end)
    Range getNormalizedRange() const;
    
private:
    TextBuffer* m_buffer;
    Cursor* m_cursor;
    Position m_anchor{0, 0};
    SelectionMode m_mode = SelectionMode::None;
    bool m_hasSelection = false;
    bool m_isDragging = false;
    
    bool isWordChar(char c) const;
    Position findWordStart(const Position& pos) const;
    Position findWordEnd(const Position& pos) const;
};

} // namespace editor
