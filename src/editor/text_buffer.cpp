/**
 * @file text_buffer.cpp
 * @brief Gap buffer and text buffer implementation
 * 
 * The gap buffer is a data structure optimized for text editing where 
 * most operations occur at or near the cursor position.
 */

#include "editor/text_buffer.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <chrono>

namespace editor {

// ====================
// GapBuffer Implementation
// ====================

GapBuffer::GapBuffer() : GapBuffer(DEFAULT_GAP_SIZE * 2) {}

GapBuffer::GapBuffer(size_t initialCapacity) {
    m_capacity = std::max(initialCapacity, DEFAULT_GAP_SIZE);
    m_buffer = new char[m_capacity];
    m_gapStart = 0;
    m_gapEnd = m_capacity;
    m_size = 0;
}

GapBuffer::GapBuffer(const GapBuffer& other) {
    m_capacity = other.m_capacity;
    m_size = other.m_size;
    m_gapStart = other.m_gapStart;
    m_gapEnd = other.m_gapEnd;
    m_buffer = new char[m_capacity];
    std::memcpy(m_buffer, other.m_buffer, m_capacity);
}

GapBuffer::GapBuffer(GapBuffer&& other) noexcept
    : m_buffer(other.m_buffer)
    , m_capacity(other.m_capacity)
    , m_size(other.m_size)
    , m_gapStart(other.m_gapStart)
    , m_gapEnd(other.m_gapEnd)
{
    other.m_buffer = nullptr;
    other.m_capacity = 0;
    other.m_size = 0;
    other.m_gapStart = 0;
    other.m_gapEnd = 0;
}

GapBuffer& GapBuffer::operator=(const GapBuffer& other) {
    if (this != &other) {
        delete[] m_buffer;
        m_capacity = other.m_capacity;
        m_size = other.m_size;
        m_gapStart = other.m_gapStart;
        m_gapEnd = other.m_gapEnd;
        m_buffer = new char[m_capacity];
        std::memcpy(m_buffer, other.m_buffer, m_capacity);
    }
    return *this;
}

GapBuffer& GapBuffer::operator=(GapBuffer&& other) noexcept {
    if (this != &other) {
        delete[] m_buffer;
        m_buffer = other.m_buffer;
        m_capacity = other.m_capacity;
        m_size = other.m_size;
        m_gapStart = other.m_gapStart;
        m_gapEnd = other.m_gapEnd;
        
        other.m_buffer = nullptr;
        other.m_capacity = 0;
        other.m_size = 0;
        other.m_gapStart = 0;
        other.m_gapEnd = 0;
    }
    return *this;
}

GapBuffer::~GapBuffer() {
    delete[] m_buffer;
}

void GapBuffer::insert(size_t position, char c) {
    insert(position, std::string_view(&c, 1));
}

void GapBuffer::insert(size_t position, std::string_view text) {
    if (position > m_size) {
        throw std::out_of_range("Insert position out of range");
    }
    
    // Ensure we have enough space
    ensureGapSize(text.length());
    
    // Move gap to insertion point
    moveGapTo(position);
    
    // Insert text at gap start
    std::memcpy(m_buffer + m_gapStart, text.data(), text.length());
    m_gapStart += text.length();
    m_size += text.length();
}

void GapBuffer::remove(size_t position, size_t count) {
    if (position >= m_size) {
        throw std::out_of_range("Remove position out of range");
    }
    
    count = std::min(count, m_size - position);
    
    // Move gap to position
    moveGapTo(position);
    
    // Expand gap to "delete" characters
    m_gapEnd += count;
    m_size -= count;
}

char GapBuffer::at(size_t position) const {
    if (position >= m_size) {
        throw std::out_of_range("Position out of range");
    }
    
    return m_buffer[logicalToBufferIndex(position)];
}

std::string GapBuffer::substr(size_t position, size_t count) const {
    if (position >= m_size) {
        return "";
    }
    
    count = std::min(count, m_size - position);
    std::string result;
    result.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        result += at(position + i);
    }
    
    return result;
}

std::string GapBuffer::toString() const {
    std::string result;
    result.reserve(m_size);
    
    // Copy before gap
    result.append(m_buffer, m_gapStart);
    
    // Copy after gap
    result.append(m_buffer + m_gapEnd, m_capacity - m_gapEnd);
    
    return result;
}

void GapBuffer::moveGapTo(size_t position) {
    if (position == m_gapStart) {
        return;  // Gap is already at position
    }
    
    if (position < m_gapStart) {
        // Move gap backward
        size_t moveCount = m_gapStart - position;
        std::memmove(m_buffer + m_gapEnd - moveCount, 
                     m_buffer + position, 
                     moveCount);
        m_gapStart = position;
        m_gapEnd -= moveCount;
    } else {
        // Move gap forward
        size_t moveCount = position - m_gapStart;
        std::memmove(m_buffer + m_gapStart, 
                     m_buffer + m_gapEnd, 
                     moveCount);
        m_gapStart = position;
        m_gapEnd += moveCount;
    }
}

void GapBuffer::ensureGapSize(size_t requiredSize) {
    if (gapSize() >= requiredSize) {
        return;
    }
    
    expandGap(requiredSize);
}

void GapBuffer::expandGap(size_t minSize) {
    size_t newGapSize = std::max(minSize, m_capacity / 2);
    newGapSize = std::max(newGapSize, MIN_GAP_SIZE);
    
    size_t newCapacity = m_size + newGapSize;
    char* newBuffer = new char[newCapacity];
    
    // Copy before gap
    std::memcpy(newBuffer, m_buffer, m_gapStart);
    
    // Copy after gap
    size_t afterGapSize = m_capacity - m_gapEnd;
    std::memcpy(newBuffer + m_gapStart + newGapSize, 
                m_buffer + m_gapEnd, 
                afterGapSize);
    
    delete[] m_buffer;
    m_buffer = newBuffer;
    m_gapEnd = m_gapStart + newGapSize;
    m_capacity = newCapacity;
}

size_t GapBuffer::logicalToBufferIndex(size_t logicalIndex) const {
    if (logicalIndex < m_gapStart) {
        return logicalIndex;
    }
    return logicalIndex + gapSize();
}

size_t GapBuffer::bufferIndexToLogical(size_t bufferIndex) const {
    if (bufferIndex < m_gapStart) {
        return bufferIndex;
    }
    if (bufferIndex >= m_gapEnd) {
        return bufferIndex - gapSize();
    }
    return m_gapStart;  // Inside gap
}

// Iterator implementation
GapBuffer::Iterator::Iterator(const GapBuffer* buffer, size_t position)
    : m_buffer(buffer), m_position(position) {}

char GapBuffer::Iterator::operator*() const {
    return m_buffer->at(m_position);
}

GapBuffer::Iterator& GapBuffer::Iterator::operator++() {
    ++m_position;
    return *this;
}

GapBuffer::Iterator GapBuffer::Iterator::operator++(int) {
    Iterator tmp = *this;
    ++m_position;
    return tmp;
}

GapBuffer::Iterator& GapBuffer::Iterator::operator--() {
    --m_position;
    return *this;
}

GapBuffer::Iterator GapBuffer::Iterator::operator--(int) {
    Iterator tmp = *this;
    --m_position;
    return tmp;
}

GapBuffer::Iterator GapBuffer::Iterator::operator+(difference_type n) const {
    return Iterator(m_buffer, m_position + n);
}

GapBuffer::Iterator GapBuffer::Iterator::operator-(difference_type n) const {
    return Iterator(m_buffer, m_position - n);
}

GapBuffer::Iterator::difference_type GapBuffer::Iterator::operator-(const Iterator& other) const {
    return m_position - other.m_position;
}

bool GapBuffer::Iterator::operator==(const Iterator& other) const {
    return m_position == other.m_position;
}

bool GapBuffer::Iterator::operator!=(const Iterator& other) const {
    return m_position != other.m_position;
}

bool GapBuffer::Iterator::operator<(const Iterator& other) const {
    return m_position < other.m_position;
}

GapBuffer::Iterator GapBuffer::begin() const {
    return Iterator(this, 0);
}

GapBuffer::Iterator GapBuffer::end() const {
    return Iterator(this, m_size);
}

// ====================
// TextBuffer Implementation
// ====================

TextBuffer::TextBuffer() {
    m_lineStarts.push_back(0);
}

TextBuffer::TextBuffer(std::string_view content) {
    setText(content);
}

TextBuffer::~TextBuffer() = default;

void TextBuffer::setText(std::string_view text) {
    m_buffer = GapBuffer(text.length() + GapBuffer::DEFAULT_GAP_SIZE);
    m_buffer.insert(0, text);
    rebuildLineIndex();
    m_modified = true;
    m_undoStack.clear();
    m_redoStack.clear();
}

std::string TextBuffer::getText() const {
    return m_buffer.toString();
}

std::string TextBuffer::getText(const Range& range) const {
    size_t start = positionToOffset(range.start);
    size_t end = positionToOffset(range.end);
    return m_buffer.substr(start, end - start);
}

size_t TextBuffer::lineCount() const {
    return m_lineStarts.size();
}

std::string TextBuffer::getLine(size_t lineIndex) const {
    if (lineIndex >= m_lineStarts.size()) {
        return "";
    }
    
    size_t start = m_lineStarts[lineIndex];
    size_t end = lineEndOffset(lineIndex);
    
    return m_buffer.substr(start, end - start);
}

size_t TextBuffer::lineLength(size_t lineIndex) const {
    if (lineIndex >= m_lineStarts.size()) {
        return 0;
    }
    
    size_t start = m_lineStarts[lineIndex];
    size_t end = lineEndOffset(lineIndex);
    
    return end - start;
}

size_t TextBuffer::lineStartOffset(size_t lineIndex) const {
    if (lineIndex >= m_lineStarts.size()) {
        return m_buffer.size();
    }
    return m_lineStarts[lineIndex];
}

size_t TextBuffer::lineEndOffset(size_t lineIndex) const {
    if (lineIndex >= m_lineStarts.size()) {
        return m_buffer.size();
    }
    
    if (lineIndex + 1 < m_lineStarts.size()) {
        // Don't include the newline character
        size_t nextLineStart = m_lineStarts[lineIndex + 1];
        if (nextLineStart > 0 && m_buffer.at(nextLineStart - 1) == '\n') {
            return nextLineStart - 1;
        }
        return nextLineStart;
    }
    
    return m_buffer.size();
}

size_t TextBuffer::positionToOffset(const Position& pos) const {
    if (pos.line >= m_lineStarts.size()) {
        return m_buffer.size();
    }
    
    size_t lineStart = m_lineStarts[pos.line];
    size_t lineLen = lineLength(pos.line);
    
    return lineStart + std::min(pos.column, lineLen);
}

Position TextBuffer::offsetToPosition(size_t offset) const {
    if (offset >= m_buffer.size()) {
        size_t lastLine = m_lineStarts.size() > 0 ? m_lineStarts.size() - 1 : 0;
        return {lastLine, lineLength(lastLine)};
    }
    
    // Binary search for the line
    auto it = std::upper_bound(m_lineStarts.begin(), m_lineStarts.end(), offset);
    size_t line = (it - m_lineStarts.begin()) - 1;
    size_t column = offset - m_lineStarts[line];
    
    return {line, column};
}

void TextBuffer::insert(const Position& pos, std::string_view text) {
    insert(positionToOffset(pos), text);
}

void TextBuffer::insert(size_t offset, std::string_view text) {
    if (text.empty()) return;

    // Clamp to end: inserting past the end appends (editor-friendly behaviour).
    offset = std::min(offset, m_buffer.size());

    // Create undo record
    EditRecord record;
    record.operation = EditOperation::Insert;
    record.position = offsetToPosition(offset);
    record.text = std::string(text);
    record.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    
    // Perform insertion
    m_buffer.insert(offset, text);
    
    // Update line index
    updateLineIndex(offset, static_cast<int64_t>(text.length()));
    
    // Update state
    m_modified = true;
    pushUndo(record);
    m_redoStack.clear();
    
    // Notify listeners
    Range affectedRange{offsetToPosition(offset), 
                        offsetToPosition(offset + text.length())};
    notifyChange(affectedRange, std::string(text));
}

void TextBuffer::remove(const Range& range) {
    size_t start = positionToOffset(range.start);
    size_t end = positionToOffset(range.end);
    remove(start, end - start);
}

void TextBuffer::remove(size_t offset, size_t count) {
    if (count == 0 || offset >= m_buffer.size()) return;
    
    count = std::min(count, m_buffer.size() - offset);
    
    // Create undo record
    EditRecord record;
    record.operation = EditOperation::Delete;
    record.position = offsetToPosition(offset);
    record.text = m_buffer.substr(offset, count);
    record.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    
    // Perform deletion
    m_buffer.remove(offset, count);
    
    // Update line index
    updateLineIndex(offset, -static_cast<int64_t>(count));
    
    // Update state
    m_modified = true;
    pushUndo(record);
    m_redoStack.clear();
    
    // Notify listeners
    Position pos = offsetToPosition(offset);
    notifyChange({pos, pos}, "");
}

void TextBuffer::replace(const Range& range, std::string_view newText) {
    // Create undo record for replace
    EditRecord record;
    record.operation = EditOperation::Replace;
    record.position = range.start;
    record.text = std::string(newText);
    record.replacedText = getText(range);
    record.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    
    // Perform replace
    size_t start = positionToOffset(range.start);
    size_t end = positionToOffset(range.end);
    size_t oldLen = end - start;
    
    m_buffer.remove(start, oldLen);
    m_buffer.insert(start, newText);
    
    // Update line index
    rebuildLineIndex();  // Simple approach; could be optimized
    
    // Update state
    m_modified = true;
    pushUndo(record);
    m_redoStack.clear();
    
    // Notify listeners
    notifyChange(range, std::string(newText));
}

bool TextBuffer::canUndo() const {
    return !m_undoStack.empty();
}

bool TextBuffer::canRedo() const {
    return !m_redoStack.empty();
}

void TextBuffer::undo() {
    if (!canUndo()) return;
    
    EditRecord record = m_undoStack.back();
    m_undoStack.pop_back();
    
    size_t offset = positionToOffset(record.position);
    
    switch (record.operation) {
        case EditOperation::Insert:
            // Undo insert by deleting
            m_buffer.remove(offset, record.text.length());
            break;
            
        case EditOperation::Delete:
            // Undo delete by inserting
            m_buffer.insert(offset, record.text);
            break;
            
        case EditOperation::Replace:
            // Undo replace by replacing back
            m_buffer.remove(offset, record.text.length());
            m_buffer.insert(offset, record.replacedText);
            break;
    }
    
    rebuildLineIndex();
    m_redoStack.push_back(record);
}

void TextBuffer::redo() {
    if (!canRedo()) return;
    
    EditRecord record = m_redoStack.back();
    m_redoStack.pop_back();
    
    size_t offset = positionToOffset(record.position);
    
    switch (record.operation) {
        case EditOperation::Insert:
            m_buffer.insert(offset, record.text);
            break;
            
        case EditOperation::Delete:
            m_buffer.remove(offset, record.text.length());
            break;
            
        case EditOperation::Replace:
            m_buffer.remove(offset, record.replacedText.length());
            m_buffer.insert(offset, record.text);
            break;
    }
    
    rebuildLineIndex();
    m_undoStack.push_back(record);
}

void TextBuffer::clearUndoHistory() {
    m_undoStack.clear();
    m_redoStack.clear();
}

void TextBuffer::beginUndoGroup() {
    m_inUndoGroup = true;
    m_undoGroupStart = m_undoStack.size();
}

void TextBuffer::endUndoGroup() {
    m_inUndoGroup = false;
}

void TextBuffer::addChangeCallback(ChangeCallback callback) {
    m_changeCallbacks.push_back(std::move(callback));
}

bool TextBuffer::loadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::string content(size, '\0');
    if (!file.read(content.data(), size)) {
        return false;
    }
    
    // Detect and normalize line endings
    std::string normalized;
    normalized.reserve(size);
    for (size_t i = 0; i < content.size(); ++i) {
        if (content[i] == '\r') {
            normalized += '\n';
            if (i + 1 < content.size() && content[i + 1] == '\n') {
                ++i;  // Skip LF in CRLF
            }
        } else {
            normalized += content[i];
        }
    }
    
    setText(normalized);
    m_modified = false;
    
    return true;
}

bool TextBuffer::saveToFile(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    
    std::string content = getText();
    file.write(content.data(), content.size());
    
    return file.good();
}

void TextBuffer::rebuildLineIndex() {
    m_lineStarts.clear();
    m_lineStarts.push_back(0);
    
    for (size_t i = 0; i < m_buffer.size(); ++i) {
        if (m_buffer.at(i) == '\n') {
            m_lineStarts.push_back(i + 1);
        }
    }
}

void TextBuffer::updateLineIndex(size_t fromOffset, int64_t delta) {
    // Find the first affected line
    auto it = std::upper_bound(m_lineStarts.begin(), m_lineStarts.end(), fromOffset);
    
    // For small deltas, try incremental update
    // For large changes or when line structure changes significantly, rebuild
    
    if (delta == 0) {
        return;
    }
    
    // Check if we need full rebuild by scanning affected region for newlines
    bool hasNewlineChange = false;
    if (delta > 0) {
        // Insertion - check if new content contains newlines
        for (size_t i = fromOffset; i < fromOffset + static_cast<size_t>(delta) && i < m_buffer.size(); ++i) {
            if (m_buffer.at(i) == '\n') {
                hasNewlineChange = true;
                break;
            }
        }
    } else {
        // Deletion always potentially changes line structure
        hasNewlineChange = true;
    }
    
    if (hasNewlineChange) {
        // Line structure changed - rebuild entire index
        rebuildLineIndex();
    } else {
        // No newlines affected - just adjust offsets after insertion point
        for (; it != m_lineStarts.end(); ++it) {
            *it = static_cast<size_t>(static_cast<int64_t>(*it) + delta);
        }
    }
}

void TextBuffer::notifyChange(const Range& range, const std::string& newText) {
    for (const auto& callback : m_changeCallbacks) {
        callback(range, newText);
    }
}

void TextBuffer::pushUndo(const EditRecord& record) {
    m_undoStack.push_back(record);
    
    // Limit undo stack size
    const size_t maxUndoSize = 1000;
    if (m_undoStack.size() > maxUndoSize) {
        m_undoStack.erase(m_undoStack.begin());
    }
}

} // namespace editor
