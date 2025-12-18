#pragma once

#include "text_buffer.hpp"
#include <functional>

namespace editor {

/**
 * @brief Cursor movement directions
 */
enum class CursorDirection {
    Left,
    Right,
    Up,
    Down,
    LineStart,
    LineEnd,
    WordLeft,
    WordRight,
    DocumentStart,
    DocumentEnd,
    PageUp,
    PageDown
};

/**
 * @brief Cursor state and movement for text editing
 */
class Cursor {
public:
    explicit Cursor(TextBuffer* buffer);
    ~Cursor() = default;
    
    // Position
    Position getPosition() const { return m_position; }
    void setPosition(const Position& pos);
    void setPosition(size_t line, size_t column);
    size_t getOffset() const;
    void setOffset(size_t offset);
    
    // Movement
    void move(CursorDirection direction);
    void moveBy(int lineDelta, int columnDelta);
    void moveTo(size_t line, size_t column);
    void moveToLineStart();
    void moveToLineEnd();
    void moveWordLeft();
    void moveWordRight();
    
    // Preferred column (for vertical movement)
    size_t getPreferredColumn() const { return m_preferredColumn; }
    void setPreferredColumn(size_t column) { m_preferredColumn = column; }
    void updatePreferredColumn() { m_preferredColumn = m_position.column; }
    
    // Blink state for rendering
    bool isVisible() const { return m_visible; }
    void setVisible(bool visible) { m_visible = visible; }
    void toggleVisibility() { m_visible = !m_visible; }
    void resetBlink();
    
    // Validation
    void clampToBuffer();
    bool isValid() const;
    
    // Character at cursor
    char currentChar() const;
    char prevChar() const;
    char nextChar() const;
    
    // Line info
    size_t currentLineLength() const;
    std::string currentLine() const;
    
private:
    TextBuffer* m_buffer;
    Position m_position{0, 0};
    size_t m_preferredColumn = 0;
    bool m_visible = true;
    
    void clampColumn();
    bool isWordChar(char c) const;
};

} // namespace editor
