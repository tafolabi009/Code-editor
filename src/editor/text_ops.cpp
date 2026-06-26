/**
 * @file text_ops.cpp
 * @brief Implementation of higher-level editing operations.
 */

#include "editor/text_ops.hpp"

#include <string>
#include <algorithm>

namespace editor {
namespace ops {

namespace {

char closingFor(char open) {
    switch (open) {
        case '(': return ')';
        case '[': return ']';
        case '{': return '}';
        default:  return '\0';
    }
}

char openingFor(char close) {
    switch (close) {
        case ')': return '(';
        case ']': return '[';
        case '}': return '{';
        default:  return '\0';
    }
}

size_t firstNonWhitespace(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
        ++i;
    }
    return i;
}

}  // namespace

bool isOpenBracket(char c) { return c == '(' || c == '[' || c == '{'; }
bool isCloseBracket(char c) { return c == ')' || c == ']' || c == '}'; }

std::optional<size_t> findMatchingBracket(const TextBuffer& buffer, size_t offset) {
    std::string text = buffer.getText();
    if (offset >= text.size()) {
        return std::nullopt;
    }

    char c = text[offset];

    if (isOpenBracket(c)) {
        char close = closingFor(c);
        int depth = 1;
        for (size_t i = offset + 1; i < text.size(); ++i) {
            if (text[i] == c) {
                ++depth;
            } else if (text[i] == close) {
                if (--depth == 0) {
                    return i;
                }
            }
        }
        return std::nullopt;
    }

    if (isCloseBracket(c)) {
        char open = openingFor(c);
        int depth = 1;
        for (size_t i = offset; i-- > 0;) {
            if (text[i] == c) {
                ++depth;
            } else if (text[i] == open) {
                if (--depth == 0) {
                    return i;
                }
            }
        }
        return std::nullopt;
    }

    return std::nullopt;
}

std::string lineIndent(const TextBuffer& buffer, size_t line) {
    std::string text = buffer.getLine(line);
    return text.substr(0, firstNonWhitespace(text));
}

std::string computeNewlineIndent(const TextBuffer& buffer, size_t line,
                                 int tabWidth, bool useSpaces) {
    std::string indent = lineIndent(buffer, line);

    // Find the last non-whitespace character on the line.
    std::string text = buffer.getLine(line);
    size_t last = text.find_last_not_of(" \t");
    if (last != std::string::npos && isOpenBracket(text[last])) {
        if (useSpaces) {
            indent.append(static_cast<size_t>(tabWidth > 0 ? tabWidth : 4), ' ');
        } else {
            indent.push_back('\t');
        }
    }
    return indent;
}

void duplicateLine(TextBuffer& buffer, size_t line) {
    if (line >= buffer.lineCount()) {
        return;
    }
    std::string text = buffer.getLine(line);
    buffer.insert(buffer.lineStartOffset(line), text + "\n");
}

void moveLineDown(TextBuffer& buffer, size_t line) {
    if (line + 1 >= buffer.lineCount()) {
        return;
    }
    std::string a = buffer.getLine(line);
    std::string b = buffer.getLine(line + 1);
    size_t aStart = buffer.lineStartOffset(line);
    size_t bEnd = buffer.lineEndOffset(line + 1);

    buffer.beginUndoGroup();
    buffer.remove(aStart, bEnd - aStart);   // removes "a\nb"
    buffer.insert(aStart, b + "\n" + a);     // inserts "b\na"
    buffer.endUndoGroup();
}

void moveLineUp(TextBuffer& buffer, size_t line) {
    if (line == 0 || line >= buffer.lineCount()) {
        return;
    }
    moveLineDown(buffer, line - 1);
}

void toggleLineComment(TextBuffer& buffer, size_t startLine, size_t endLine,
                       const std::string& token) {
    if (token.empty() || startLine > endLine || startLine >= buffer.lineCount()) {
        return;
    }
    endLine = std::min(endLine, buffer.lineCount() - 1);

    // Decide direction: uncomment only if every non-blank line is commented.
    bool allCommented = true;
    for (size_t l = startLine; l <= endLine; ++l) {
        std::string text = buffer.getLine(l);
        size_t nws = firstNonWhitespace(text);
        if (nws == text.size()) {
            continue;  // blank line - ignored for the decision
        }
        if (text.compare(nws, token.size(), token) != 0) {
            allCommented = false;
            break;
        }
    }

    buffer.beginUndoGroup();
    // Process bottom-up so earlier lines' offsets stay valid as we edit.
    for (size_t l = endLine + 1; l-- > startLine;) {
        std::string text = buffer.getLine(l);
        size_t nws = firstNonWhitespace(text);
        if (nws == text.size()) {
            continue;  // skip blank lines
        }
        size_t at = buffer.lineStartOffset(l) + nws;

        if (allCommented) {
            // Remove the token, plus one following space if present.
            size_t removeLen = token.size();
            if (nws + token.size() < text.size() &&
                text[nws + token.size()] == ' ') {
                ++removeLen;
            }
            buffer.remove(at, removeLen);
        } else {
            buffer.insert(at, token + " ");
        }
    }
    buffer.endUndoGroup();
}

}  // namespace ops
}  // namespace editor
