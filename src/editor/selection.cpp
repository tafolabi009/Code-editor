/**
 * @file selection.cpp
 * @brief Text selection implementation
 */

#include "editor/selection.hpp"
#include <algorithm>
#include <cctype>

namespace editor {

Selection::Selection(TextBuffer* buffer, Cursor* cursor)
    : m_buffer(buffer), m_cursor(cursor) {}

Range Selection::getRange() const {
    return {m_anchor, m_cursor->getPosition()};
}

Position Selection::getHead() const {
    return m_cursor->getPosition();
}

Range Selection::getNormalizedRange() const {
    Position start = m_anchor;
    Position end = m_cursor->getPosition();
    
    if (end < start) {
        std::swap(start, end);
    }
    
    return {start, end};
}

std::string Selection::getSelectedText() const {
    if (!m_hasSelection) {
        return "";
    }
    
    return m_buffer->getText(getNormalizedRange());
}

void Selection::startSelection(SelectionMode mode) {
    startSelection(m_cursor->getPosition(), mode);
}

void Selection::startSelection(const Position& anchor, SelectionMode mode) {
    m_anchor = anchor;
    m_mode = mode;
    m_hasSelection = true;
}

void Selection::extendSelection(const Position& to) {
    if (!m_hasSelection) {
        startSelection();
    }
    m_cursor->setPosition(to);
}

void Selection::clearSelection() {
    m_hasSelection = false;
    m_mode = SelectionMode::None;
    m_isDragging = false;
}

void Selection::selectAll() {
    m_anchor = {0, 0};
    size_t lastLine = m_buffer->lineCount() > 0 ? m_buffer->lineCount() - 1 : 0;
    m_cursor->setPosition(lastLine, m_buffer->lineLength(lastLine));
    m_hasSelection = true;
    m_mode = SelectionMode::Normal;
}

void Selection::selectLine(size_t lineIndex) {
    if (lineIndex >= m_buffer->lineCount()) {
        return;
    }
    
    m_anchor = {lineIndex, 0};
    
    if (lineIndex + 1 < m_buffer->lineCount()) {
        m_cursor->setPosition(lineIndex + 1, 0);
    } else {
        m_cursor->setPosition(lineIndex, m_buffer->lineLength(lineIndex));
    }
    
    m_hasSelection = true;
    m_mode = SelectionMode::Line;
}

void Selection::selectWord() {
    selectWord(m_cursor->getPosition());
}

void Selection::selectWord(const Position& pos) {
    Position start = findWordStart(pos);
    Position end = findWordEnd(pos);
    
    m_anchor = start;
    m_cursor->setPosition(end);
    m_hasSelection = true;
    m_mode = SelectionMode::Normal;
}

void Selection::extendSelectionLeft() {
    if (!m_hasSelection) {
        startSelection();
    }
    m_cursor->move(CursorDirection::Left);
}

void Selection::extendSelectionRight() {
    if (!m_hasSelection) {
        startSelection();
    }
    m_cursor->move(CursorDirection::Right);
}

void Selection::extendSelectionUp() {
    if (!m_hasSelection) {
        startSelection();
    }
    m_cursor->move(CursorDirection::Up);
}

void Selection::extendSelectionDown() {
    if (!m_hasSelection) {
        startSelection();
    }
    m_cursor->move(CursorDirection::Down);
}

void Selection::extendSelectionWordLeft() {
    if (!m_hasSelection) {
        startSelection();
    }
    m_cursor->move(CursorDirection::WordLeft);
}

void Selection::extendSelectionWordRight() {
    if (!m_hasSelection) {
        startSelection();
    }
    m_cursor->move(CursorDirection::WordRight);
}

void Selection::extendSelectionToLineStart() {
    if (!m_hasSelection) {
        startSelection();
    }
    m_cursor->move(CursorDirection::LineStart);
}

void Selection::extendSelectionToLineEnd() {
    if (!m_hasSelection) {
        startSelection();
    }
    m_cursor->move(CursorDirection::LineEnd);
}

void Selection::startDragSelection(const Position& pos) {
    m_anchor = pos;
    m_hasSelection = true;
    m_isDragging = true;
    m_mode = SelectionMode::Normal;
}

void Selection::updateDragSelection(const Position& pos) {
    if (!m_isDragging) return;
    // Cursor position is updated by the caller
}

void Selection::endDragSelection() {
    m_isDragging = false;
    
    // Clear selection if anchor equals cursor position
    if (m_anchor == m_cursor->getPosition()) {
        clearSelection();
    }
}

void Selection::selectWordAtPosition(const Position& pos) {
    selectWord(pos);
}

void Selection::selectLineAtPosition(const Position& pos) {
    selectLine(pos.line);
}

std::vector<Range> Selection::getBlockRanges() const {
    if (m_mode != SelectionMode::Block || !m_hasSelection) {
        return {};
    }
    
    std::vector<Range> ranges;
    Range normalized = getNormalizedRange();
    
    size_t minCol = std::min(m_anchor.column, m_cursor->getPosition().column);
    size_t maxCol = std::max(m_anchor.column, m_cursor->getPosition().column);
    
    for (size_t line = normalized.start.line; line <= normalized.end.line; ++line) {
        size_t lineLen = m_buffer->lineLength(line);
        size_t startCol = std::min(minCol, lineLen);
        size_t endCol = std::min(maxCol, lineLen);
        
        if (startCol < endCol) {
            ranges.push_back({{line, startCol}, {line, endCol}});
        }
    }
    
    return ranges;
}

bool Selection::isWordChar(char c) const {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

Position Selection::findWordStart(const Position& pos) const {
    if (pos.line >= m_buffer->lineCount()) {
        return pos;
    }
    
    std::string line = m_buffer->getLine(pos.line);
    if (line.empty() || pos.column == 0) {
        return {pos.line, 0};
    }
    
    size_t col = std::min(pos.column, line.length());
    
    // If in whitespace, find previous word
    while (col > 0 && std::isspace(line[col - 1])) {
        --col;
    }
    
    // Find start of word
    while (col > 0 && isWordChar(line[col - 1])) {
        --col;
    }
    
    return {pos.line, col};
}

Position Selection::findWordEnd(const Position& pos) const {
    if (pos.line >= m_buffer->lineCount()) {
        return pos;
    }
    
    std::string line = m_buffer->getLine(pos.line);
    if (line.empty()) {
        return {pos.line, 0};
    }
    
    size_t col = std::min(pos.column, line.length());
    
    // Find end of word
    while (col < line.length() && isWordChar(line[col])) {
        ++col;
    }
    
    return {pos.line, col};
}

} // namespace editor
