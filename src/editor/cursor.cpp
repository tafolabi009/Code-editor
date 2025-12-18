/**
 * @file cursor.cpp
 * @brief Cursor management implementation
 */

#include "editor/cursor.hpp"
#include <algorithm>
#include <cctype>

namespace editor {

Cursor::Cursor(TextBuffer* buffer) : m_buffer(buffer) {}

void Cursor::setPosition(const Position& pos) {
    m_position = pos;
    clampToBuffer();
}

void Cursor::setPosition(size_t line, size_t column) {
    setPosition({line, column});
}

size_t Cursor::getOffset() const {
    return m_buffer->positionToOffset(m_position);
}

void Cursor::setOffset(size_t offset) {
    m_position = m_buffer->offsetToPosition(offset);
}

void Cursor::move(CursorDirection direction) {
    switch (direction) {
        case CursorDirection::Left:
            if (m_position.column > 0) {
                --m_position.column;
            } else if (m_position.line > 0) {
                --m_position.line;
                m_position.column = m_buffer->lineLength(m_position.line);
            }
            updatePreferredColumn();
            break;
            
        case CursorDirection::Right:
            if (m_position.column < m_buffer->lineLength(m_position.line)) {
                ++m_position.column;
            } else if (m_position.line + 1 < m_buffer->lineCount()) {
                ++m_position.line;
                m_position.column = 0;
            }
            updatePreferredColumn();
            break;
            
        case CursorDirection::Up:
            if (m_position.line > 0) {
                --m_position.line;
                m_position.column = std::min(m_preferredColumn, m_buffer->lineLength(m_position.line));
            }
            break;
            
        case CursorDirection::Down:
            if (m_position.line + 1 < m_buffer->lineCount()) {
                ++m_position.line;
                m_position.column = std::min(m_preferredColumn, m_buffer->lineLength(m_position.line));
            }
            break;
            
        case CursorDirection::LineStart:
            moveToLineStart();
            break;
            
        case CursorDirection::LineEnd:
            moveToLineEnd();
            break;
            
        case CursorDirection::WordLeft:
            moveWordLeft();
            break;
            
        case CursorDirection::WordRight:
            moveWordRight();
            break;
            
        case CursorDirection::DocumentStart:
            m_position = {0, 0};
            updatePreferredColumn();
            break;
            
        case CursorDirection::DocumentEnd: {
            size_t lastLine = m_buffer->lineCount() > 0 ? m_buffer->lineCount() - 1 : 0;
            m_position = {lastLine, m_buffer->lineLength(lastLine)};
            updatePreferredColumn();
            break;
        }
            
        case CursorDirection::PageUp:
            // Move ~20 lines up
            if (m_position.line >= 20) {
                m_position.line -= 20;
            } else {
                m_position.line = 0;
            }
            clampColumn();
            break;
            
        case CursorDirection::PageDown:
            // Move ~20 lines down
            m_position.line += 20;
            if (m_position.line >= m_buffer->lineCount()) {
                m_position.line = m_buffer->lineCount() > 0 ? m_buffer->lineCount() - 1 : 0;
            }
            clampColumn();
            break;
    }
}

void Cursor::moveBy(int lineDelta, int columnDelta) {
    int newLine = static_cast<int>(m_position.line) + lineDelta;
    int newColumn = static_cast<int>(m_position.column) + columnDelta;
    
    newLine = std::max(0, newLine);
    newColumn = std::max(0, newColumn);
    
    m_position.line = static_cast<size_t>(newLine);
    m_position.column = static_cast<size_t>(newColumn);
    
    clampToBuffer();
}

void Cursor::moveTo(size_t line, size_t column) {
    setPosition(line, column);
    updatePreferredColumn();
}

void Cursor::moveToLineStart() {
    m_position.column = 0;
    updatePreferredColumn();
}

void Cursor::moveToLineEnd() {
    m_position.column = m_buffer->lineLength(m_position.line);
    updatePreferredColumn();
}

void Cursor::moveWordLeft() {
    if (m_position.column == 0 && m_position.line == 0) {
        return;
    }
    
    // Move to previous position first
    if (m_position.column == 0) {
        move(CursorDirection::Left);
    }
    
    std::string line = currentLine();
    
    // Skip whitespace
    while (m_position.column > 0 && std::isspace(line[m_position.column - 1])) {
        --m_position.column;
    }
    
    // Skip word characters
    while (m_position.column > 0 && isWordChar(line[m_position.column - 1])) {
        --m_position.column;
    }
    
    updatePreferredColumn();
}

void Cursor::moveWordRight() {
    std::string line = currentLine();
    size_t lineLen = line.length();
    
    // Skip current word
    while (m_position.column < lineLen && isWordChar(line[m_position.column])) {
        ++m_position.column;
    }
    
    // Skip whitespace
    while (m_position.column < lineLen && std::isspace(line[m_position.column])) {
        ++m_position.column;
    }
    
    // If at end of line, move to next line
    if (m_position.column >= lineLen && m_position.line + 1 < m_buffer->lineCount()) {
        ++m_position.line;
        m_position.column = 0;
    }
    
    updatePreferredColumn();
}

void Cursor::clampToBuffer() {
    if (m_buffer->lineCount() == 0) {
        m_position = {0, 0};
        return;
    }
    
    m_position.line = std::min(m_position.line, m_buffer->lineCount() - 1);
    clampColumn();
}

void Cursor::clampColumn() {
    m_position.column = std::min(m_position.column, m_buffer->lineLength(m_position.line));
}

bool Cursor::isValid() const {
    if (m_buffer->lineCount() == 0) {
        return m_position.line == 0 && m_position.column == 0;
    }
    
    return m_position.line < m_buffer->lineCount() &&
           m_position.column <= m_buffer->lineLength(m_position.line);
}

void Cursor::resetBlink() {
    m_visible = true;
}

char Cursor::currentChar() const {
    size_t offset = getOffset();
    if (offset >= m_buffer->size()) {
        return '\0';
    }
    return m_buffer->getText().at(offset);
}

char Cursor::prevChar() const {
    size_t offset = getOffset();
    if (offset == 0) {
        return '\0';
    }
    return m_buffer->getText().at(offset - 1);
}

char Cursor::nextChar() const {
    size_t offset = getOffset();
    if (offset + 1 >= m_buffer->size()) {
        return '\0';
    }
    return m_buffer->getText().at(offset + 1);
}

size_t Cursor::currentLineLength() const {
    return m_buffer->lineLength(m_position.line);
}

std::string Cursor::currentLine() const {
    return m_buffer->getLine(m_position.line);
}

bool Cursor::isWordChar(char c) const {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

} // namespace editor
