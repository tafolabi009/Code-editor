#pragma once

#include "editor/text_buffer.hpp"
#include <algorithm>
#include <cstddef>
#include <string_view>
#include <vector>

namespace editor {

/**
 * @brief A single caret, stored as buffer offsets.
 *
 * @c pos is the moving end (where text is inserted / the visible cursor);
 * @c anchor is the fixed end of a selection. When @c pos == @c anchor the caret
 * has no selection. Offsets (not line/column) are used so edits can be applied
 * and shifted with simple arithmetic.
 */
struct Caret {
    size_t pos = 0;
    size_t anchor = 0;

    Caret() = default;
    explicit Caret(size_t p) : pos(p), anchor(p) {}
    Caret(size_t p, size_t a) : pos(p), anchor(a) {}

    bool hasSelection() const { return pos != anchor; }
    size_t selStart() const { return std::min(pos, anchor); }
    size_t selEnd() const { return std::max(pos, anchor); }
};

/**
 * @brief A set of carets supporting simultaneous (multi-cursor) editing.
 *
 * The tricky part of multi-cursor editing is keeping every caret at the right
 * offset as edits at other carets grow or shrink the buffer. Edits here are
 * applied left-to-right while accumulating a running delta, so each caret lands
 * in the correct final position regardless of how much text earlier carets
 * inserted or removed.
 *
 * All editing methods are a single undo group.
 */
class MultiCursor {
public:
    MultiCursor() : m_carets(1) {}

    // --- caret management ---
    void setPrimary(size_t pos, size_t anchor);
    void setPrimary(size_t pos) { setPrimary(pos, pos); }
    void addCaret(size_t pos, size_t anchor);
    void addCaret(size_t pos) { addCaret(pos, pos); }
    /// Keep only the primary (lowest-offset) caret.
    void collapseToPrimary();

    const std::vector<Caret>& carets() const { return m_carets; }
    size_t count() const { return m_carets.size(); }
    bool hasMultiple() const { return m_carets.size() > 1; }
    const Caret& primary() const { return m_carets.front(); }

    /// Sort carets by start offset and merge overlapping/duplicate ones.
    void normalize();

    // --- editing (applied to every caret) ---
    /// Replace each caret's selection (if any) with @p text, else insert it.
    void insertText(TextBuffer& buffer, std::string_view text);
    /// Delete each selection, or the character before each collapsed caret.
    void backspace(TextBuffer& buffer);
    /// Delete each selection, or the character after each collapsed caret.
    void deleteForward(TextBuffer& buffer);

private:
    std::vector<Caret> m_carets;
};

}  // namespace editor
