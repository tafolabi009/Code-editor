/**
 * @file multi_cursor.cpp
 * @brief Multi-cursor editing model implementation.
 */

#include "editor/multi_cursor.hpp"

#include <string>

namespace editor {

void MultiCursor::setPrimary(size_t pos, size_t anchor) {
    m_carets.clear();
    m_carets.emplace_back(pos, anchor);
}

void MultiCursor::addCaret(size_t pos, size_t anchor) {
    m_carets.emplace_back(pos, anchor);
    normalize();
}

void MultiCursor::collapseToPrimary() {
    if (m_carets.size() > 1) {
        Caret primary = m_carets.front();
        m_carets.clear();
        m_carets.push_back(primary);
    }
}

void MultiCursor::normalize() {
    if (m_carets.size() <= 1) {
        return;
    }

    std::sort(m_carets.begin(), m_carets.end(), [](const Caret& a, const Caret& b) {
        if (a.selStart() != b.selStart()) {
            return a.selStart() < b.selStart();
        }
        return a.selEnd() < b.selEnd();
    });

    std::vector<Caret> merged;
    merged.push_back(m_carets.front());
    for (size_t i = 1; i < m_carets.size(); ++i) {
        Caret& last = merged.back();
        const Caret& cur = m_carets[i];

        // Overlapping or coincident carets collapse into one. Because the list
        // is sorted, cur.selStart() >= last.selStart(); they merge when cur
        // starts at or before last's end.
        if (cur.selStart() <= last.selEnd()) {
            size_t newStart = std::min(last.selStart(), cur.selStart());
            size_t newEnd = std::max(last.selEnd(), cur.selEnd());
            last = Caret(newEnd, newStart);  // pos = end, anchor = start
        } else {
            merged.push_back(cur);
        }
    }

    m_carets = std::move(merged);
}

void MultiCursor::insertText(TextBuffer& buffer, std::string_view text) {
    normalize();

    const std::string str(text);
    long delta = 0;

    buffer.beginUndoGroup();
    for (Caret& c : m_carets) {
        size_t start = static_cast<size_t>(static_cast<long>(c.selStart()) + delta);
        size_t end = static_cast<size_t>(static_cast<long>(c.selEnd()) + delta);

        if (end > start) {
            buffer.remove(start, end - start);
        }
        buffer.insert(start, str);

        size_t caretPos = start + str.size();
        c.pos = caretPos;
        c.anchor = caretPos;

        delta += static_cast<long>(str.size()) - static_cast<long>(end - start);
    }
    buffer.endUndoGroup();
}

void MultiCursor::backspace(TextBuffer& buffer) {
    normalize();

    long delta = 0;
    buffer.beginUndoGroup();
    for (Caret& c : m_carets) {
        size_t start = static_cast<size_t>(static_cast<long>(c.selStart()) + delta);
        size_t end = static_cast<size_t>(static_cast<long>(c.selEnd()) + delta);

        if (end > start) {
            // Delete the selection.
            buffer.remove(start, end - start);
            c.pos = c.anchor = start;
            delta -= static_cast<long>(end - start);
        } else if (start > 0) {
            // Delete the character before a collapsed caret.
            buffer.remove(start - 1, 1);
            c.pos = c.anchor = start - 1;
            delta -= 1;
        }
    }
    buffer.endUndoGroup();
    normalize();
}

void MultiCursor::deleteForward(TextBuffer& buffer) {
    normalize();

    long delta = 0;
    buffer.beginUndoGroup();
    for (Caret& c : m_carets) {
        size_t start = static_cast<size_t>(static_cast<long>(c.selStart()) + delta);
        size_t end = static_cast<size_t>(static_cast<long>(c.selEnd()) + delta);

        if (end > start) {
            buffer.remove(start, end - start);
            c.pos = c.anchor = start;
            delta -= static_cast<long>(end - start);
        } else if (start < buffer.size()) {
            buffer.remove(start, 1);
            c.pos = c.anchor = start;
            delta -= 1;
        }
    }
    buffer.endUndoGroup();
    normalize();
}

}  // namespace editor
