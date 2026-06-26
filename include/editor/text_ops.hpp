#pragma once

#include "editor/text_buffer.hpp"
#include <optional>
#include <string>

namespace editor {

/**
 * @brief Higher-level editing operations built on top of TextBuffer.
 *
 * These are the "table-stakes" editor commands (bracket matching, auto-indent,
 * line manipulation, comment toggling). They are deliberately UI-independent so
 * they can be unit-tested directly against a TextBuffer; the UI layer wires them
 * to keystrokes and updates the cursor afterwards.
 */
namespace ops {

/// The three bracket pairs understood by findMatchingBracket.
bool isOpenBracket(char c);
bool isCloseBracket(char c);

/**
 * @brief Find the bracket matching the one at @p offset.
 *
 * If the character at @p offset is an opening bracket, scans forward for the
 * matching close (respecting nesting of the same pair); if it is a closing
 * bracket, scans backward. Returns the offset of the match, or std::nullopt if
 * @p offset is not on a bracket or the bracket is unbalanced.
 */
std::optional<size_t> findMatchingBracket(const TextBuffer& buffer, size_t offset);

/**
 * @brief Return the leading whitespace (indent) of @p line.
 */
std::string lineIndent(const TextBuffer& buffer, size_t line);

/**
 * @brief Compute the indentation for a new line opened after @p line.
 *
 * Uses @p line's indent, plus one additional level if @p line ends with an
 * opening bracket. One level is @p tabWidth spaces when @p useSpaces, else a tab.
 */
std::string computeNewlineIndent(const TextBuffer& buffer, size_t line,
                                 int tabWidth, bool useSpaces);

/**
 * @brief Duplicate @p line, inserting the copy immediately above it.
 */
void duplicateLine(TextBuffer& buffer, size_t line);

/**
 * @brief Swap @p line with the one below it (no-op on the last line).
 */
void moveLineDown(TextBuffer& buffer, size_t line);

/**
 * @brief Swap @p line with the one above it (no-op on the first line).
 */
void moveLineUp(TextBuffer& buffer, size_t line);

/**
 * @brief Toggle a line comment on lines [startLine, endLine].
 *
 * If every non-blank line in the range already begins with @p token, the token
 * (and one following space, if present) is removed from each; otherwise the
 * token plus a space is inserted at the first non-whitespace column of each
 * non-blank line. Applied as a single undo group.
 */
void toggleLineComment(TextBuffer& buffer, size_t startLine, size_t endLine,
                       const std::string& token);

}  // namespace ops
}  // namespace editor
