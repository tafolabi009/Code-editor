#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <cstdint>

namespace editor {

/**
 * @brief Position in a text buffer (line and column)
 */
struct Position {
    size_t line = 0;
    size_t column = 0;
    
    bool operator==(const Position& other) const {
        return line == other.line && column == other.column;
    }
    
    bool operator<(const Position& other) const {
        return line < other.line || (line == other.line && column < other.column);
    }
    
    bool operator<=(const Position& other) const {
        return *this < other || *this == other;
    }
};

/**
 * @brief A range in the text buffer
 */
struct Range {
    Position start;
    Position end;
    
    bool isEmpty() const { return start == end; }
    bool contains(const Position& pos) const { return start <= pos && pos < end; }
};

/**
 * @brief Operation type for undo/redo
 */
enum class EditOperation {
    Insert,
    Delete,
    Replace
};

/**
 * @brief Record of an edit operation for undo/redo
 */
struct EditRecord {
    EditOperation operation;
    Position position;
    std::string text;
    std::string replacedText;  // For replace operations
    size_t timestamp;
};

/**
 * @brief Gap buffer implementation for efficient text editing
 * 
 * The gap buffer maintains a contiguous array with a "gap" that can be 
 * moved to any position. This allows O(1) insertions and deletions at 
 * the cursor position.
 * 
 * Memory layout: [text before gap][GAP][text after gap]
 */
class GapBuffer {
public:
    static constexpr size_t DEFAULT_GAP_SIZE = 1024;
    static constexpr size_t MIN_GAP_SIZE = 256;
    
    GapBuffer();
    explicit GapBuffer(size_t initialCapacity);
    GapBuffer(const GapBuffer& other);
    GapBuffer(GapBuffer&& other) noexcept;
    GapBuffer& operator=(const GapBuffer& other);
    GapBuffer& operator=(GapBuffer&& other) noexcept;
    ~GapBuffer();
    
    // Basic operations
    void insert(size_t position, char c);
    void insert(size_t position, std::string_view text);
    void remove(size_t position, size_t count = 1);
    void erase(size_t position, size_t count = 1) { remove(position, count); }  // Alias for tests
    
    // Access
    char at(size_t position) const;
    char operator[](size_t position) const { return at(position); }
    std::string substr(size_t position, size_t count) const;
    std::string toString() const;
    std::string getText() const { return toString(); }  // Alias for consistency
    std::string getText(size_t position, size_t count) const { return substr(position, count); }
    
    // Properties
    size_t size() const noexcept { return m_size; }
    size_t capacity() const noexcept { return m_capacity; }
    bool empty() const noexcept { return m_size == 0; }
    
    // Gap management
    void moveGapTo(size_t position);
    void ensureGapSize(size_t requiredSize);
    
    // Iterator support
    class Iterator;
    Iterator begin() const;
    Iterator end() const;
    
private:
    char* m_buffer = nullptr;
    size_t m_capacity = 0;
    size_t m_size = 0;
    size_t m_gapStart = 0;
    size_t m_gapEnd = 0;
    
    void expandGap(size_t minSize);
    size_t gapSize() const { return m_gapEnd - m_gapStart; }
    size_t bufferIndexToLogical(size_t bufferIndex) const;
    size_t logicalToBufferIndex(size_t logicalIndex) const;
};

/**
 * @brief Iterator for GapBuffer
 */
class GapBuffer::Iterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = char;
    using difference_type = std::ptrdiff_t;
    using pointer = const char*;
    using reference = const char&;
    
    Iterator(const GapBuffer* buffer, size_t position);
    
    char operator*() const;
    Iterator& operator++();
    Iterator operator++(int);
    Iterator& operator--();
    Iterator operator--(int);
    Iterator operator+(difference_type n) const;
    Iterator operator-(difference_type n) const;
    difference_type operator-(const Iterator& other) const;
    
    bool operator==(const Iterator& other) const;
    bool operator!=(const Iterator& other) const;
    bool operator<(const Iterator& other) const;
    
private:
    const GapBuffer* m_buffer;
    size_t m_position;
};

/**
 * @brief High-level text buffer with line indexing and undo/redo
 */
class TextBuffer {
public:
    using ChangeCallback = std::function<void(const Range&, const std::string&)>;
    
    TextBuffer();
    explicit TextBuffer(std::string_view content);
    ~TextBuffer();
    
    // Content management
    void setText(std::string_view text);
    std::string getText() const;
    std::string getText(const Range& range) const;
    
    // Line operations
    size_t lineCount() const;
    std::string getLine(size_t lineIndex) const;
    size_t lineLength(size_t lineIndex) const;
    size_t lineStartOffset(size_t lineIndex) const;
    size_t lineStart(size_t lineIndex) const { return lineStartOffset(lineIndex); }  // Alias
    size_t lineEndOffset(size_t lineIndex) const;
    
    // Position conversion
    size_t positionToOffset(const Position& pos) const;
    Position offsetToPosition(size_t offset) const;
    
    // Editing
    void insert(const Position& pos, std::string_view text);
    void insert(size_t offset, std::string_view text);
    void remove(const Range& range);
    void remove(size_t offset, size_t count);
    void erase(size_t offset, size_t count) { remove(offset, count); }  // Alias for tests
    void replace(const Range& range, std::string_view newText);
    
    // Undo/Redo
    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();
    void clearUndoHistory();
    void beginUndoGroup();
    void endUndoGroup();
    
    // Change notifications
    void addChangeCallback(ChangeCallback callback);
    
    // Properties
    size_t size() const { return m_buffer.size(); }
    bool empty() const { return m_buffer.empty(); }
    bool isModified() const { return m_modified; }
    void setModified(bool modified) { m_modified = modified; }
    
    // File operations
    bool loadFromFile(const std::string& path);
    bool saveToFile(const std::string& path) const;
    
    // Encoding
    enum class Encoding { UTF8, UTF16LE, UTF16BE, ASCII, Unknown };
    Encoding getEncoding() const { return m_encoding; }
    void setEncoding(Encoding encoding) { m_encoding = encoding; }
    
private:
    GapBuffer m_buffer;
    std::vector<size_t> m_lineStarts;  // Offset of each line start
    std::vector<EditRecord> m_undoStack;
    std::vector<EditRecord> m_redoStack;
    std::vector<ChangeCallback> m_changeCallbacks;
    Encoding m_encoding = Encoding::UTF8;
    bool m_modified = false;
    bool m_inUndoGroup = false;
    size_t m_undoGroupStart = 0;
    
    void rebuildLineIndex();
    void updateLineIndex(size_t fromOffset, int64_t delta);
    void notifyChange(const Range& range, const std::string& newText);
    void pushUndo(const EditRecord& record);
};

} // namespace editor
