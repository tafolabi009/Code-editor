/**
 * @file test_text_ops.cpp
 * @brief Tests for higher-level editing operations (editor::ops).
 */

#include <gtest/gtest.h>
#include "editor/text_ops.hpp"
#include "editor/text_buffer.hpp"

using namespace editor;

namespace {
TextBuffer makeBuffer(const std::string& content) {
    TextBuffer b;
    b.insert(0, content);
    return b;
}
}  // namespace

// ---------------------------------------------------------------------------
// Bracket matching
// ---------------------------------------------------------------------------

TEST(TextOps, MatchForwardSimple) {
    TextBuffer b = makeBuffer("(abc)");
    auto m = ops::findMatchingBracket(b, 0);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(*m, 4u);
}

TEST(TextOps, MatchBackwardSimple) {
    TextBuffer b = makeBuffer("(abc)");
    auto m = ops::findMatchingBracket(b, 4);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(*m, 0u);
}

TEST(TextOps, MatchRespectsNesting) {
    //            0123456789
    TextBuffer b = makeBuffer("a(b(c)d)e");
    auto outer = ops::findMatchingBracket(b, 1);
    ASSERT_TRUE(outer.has_value());
    EXPECT_EQ(*outer, 7u);  // matches the outer ')'
    auto inner = ops::findMatchingBracket(b, 3);
    ASSERT_TRUE(inner.has_value());
    EXPECT_EQ(*inner, 5u);  // matches the inner ')'
}

TEST(TextOps, MatchMixedTypesAcrossLines) {
    TextBuffer b = makeBuffer("void f() {\n    g[0];\n}");
    size_t brace = b.getText().find('{');
    auto m = ops::findMatchingBracket(b, brace);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(b.getText()[*m], '}');
}

TEST(TextOps, NoMatchWhenUnbalancedOrNotBracket) {
    TextBuffer b = makeBuffer("(abc");
    EXPECT_FALSE(ops::findMatchingBracket(b, 0).has_value());
    EXPECT_FALSE(ops::findMatchingBracket(b, 1).has_value());  // 'a' is not a bracket
}

// ---------------------------------------------------------------------------
// Indentation
// ---------------------------------------------------------------------------

TEST(TextOps, LineIndentExtractsLeadingWhitespace) {
    TextBuffer b = makeBuffer("    int x;\nnoindent");
    EXPECT_EQ(ops::lineIndent(b, 0), "    ");
    EXPECT_EQ(ops::lineIndent(b, 1), "");
}

TEST(TextOps, NewlineIndentMatchesCurrentLine) {
    TextBuffer b = makeBuffer("    foo;");
    EXPECT_EQ(ops::computeNewlineIndent(b, 0, 4, true), "    ");
}

TEST(TextOps, NewlineIndentAddsLevelAfterOpenBrace) {
    TextBuffer b = makeBuffer("    if (x) {");
    EXPECT_EQ(ops::computeNewlineIndent(b, 0, 4, true), "        ");  // 4 + 4 spaces
}

TEST(TextOps, NewlineIndentUsesTabsWhenRequested) {
    TextBuffer b = makeBuffer("\tif (x) {");
    EXPECT_EQ(ops::computeNewlineIndent(b, 0, 4, false), "\t\t");
}

// ---------------------------------------------------------------------------
// Line operations
// ---------------------------------------------------------------------------

TEST(TextOps, DuplicateLine) {
    TextBuffer b = makeBuffer("one\ntwo\nthree");
    ops::duplicateLine(b, 1);
    EXPECT_EQ(b.getText(), "one\ntwo\ntwo\nthree");
}

TEST(TextOps, DuplicateLastLine) {
    TextBuffer b = makeBuffer("a\nb");
    ops::duplicateLine(b, 1);
    EXPECT_EQ(b.getText(), "a\nb\nb");
}

TEST(TextOps, MoveLineDown) {
    TextBuffer b = makeBuffer("one\ntwo\nthree");
    ops::moveLineDown(b, 0);
    EXPECT_EQ(b.getText(), "two\none\nthree");
}

TEST(TextOps, MoveLineDownOntoLastLine) {
    TextBuffer b = makeBuffer("one\ntwo");
    ops::moveLineDown(b, 0);
    EXPECT_EQ(b.getText(), "two\none");
}

TEST(TextOps, MoveLineDownOnLastLineIsNoOp) {
    TextBuffer b = makeBuffer("one\ntwo");
    ops::moveLineDown(b, 1);
    EXPECT_EQ(b.getText(), "one\ntwo");
}

TEST(TextOps, MoveLineUp) {
    TextBuffer b = makeBuffer("one\ntwo\nthree");
    ops::moveLineUp(b, 2);
    EXPECT_EQ(b.getText(), "one\nthree\ntwo");
}

TEST(TextOps, MoveLineUpOnFirstLineIsNoOp) {
    TextBuffer b = makeBuffer("one\ntwo");
    ops::moveLineUp(b, 0);
    EXPECT_EQ(b.getText(), "one\ntwo");
}

// ---------------------------------------------------------------------------
// Comment toggling
// ---------------------------------------------------------------------------

TEST(TextOps, CommentSingleLine) {
    TextBuffer b = makeBuffer("int x;");
    ops::toggleLineComment(b, 0, 0, "//");
    EXPECT_EQ(b.getText(), "// int x;");
}

TEST(TextOps, CommentPreservesIndent) {
    TextBuffer b = makeBuffer("    int x;");
    ops::toggleLineComment(b, 0, 0, "//");
    EXPECT_EQ(b.getText(), "    // int x;");
}

TEST(TextOps, UncommentRoundTrip) {
    TextBuffer b = makeBuffer("    int x;");
    ops::toggleLineComment(b, 0, 0, "//");
    ops::toggleLineComment(b, 0, 0, "//");
    EXPECT_EQ(b.getText(), "    int x;");
}

TEST(TextOps, CommentRangeAddsToAllNonBlankLines) {
    TextBuffer b = makeBuffer("a;\n\nb;");
    ops::toggleLineComment(b, 0, 2, "//");
    // Blank line in the middle is left untouched.
    EXPECT_EQ(b.getText(), "// a;\n\n// b;");
}

TEST(TextOps, ToggleRangeUncommentsWhenAllCommented) {
    TextBuffer b = makeBuffer("// a;\n// b;");
    ops::toggleLineComment(b, 0, 1, "//");
    EXPECT_EQ(b.getText(), "a;\nb;");
}

TEST(TextOps, ToggleRangeCommentsWhenMixed) {
    // Not all lines are commented, so the whole range should become commented.
    TextBuffer b = makeBuffer("// a;\nb;");
    ops::toggleLineComment(b, 0, 1, "//");
    EXPECT_EQ(b.getText(), "// // a;\n// b;");
}
