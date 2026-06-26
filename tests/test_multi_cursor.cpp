/**
 * @file test_multi_cursor.cpp
 * @brief Tests for the multi-cursor editing model.
 *
 * Focus: every caret must end up at the correct offset after edits at other
 * carets grow or shrink the buffer.
 */

#include <gtest/gtest.h>
#include "editor/multi_cursor.hpp"
#include "editor/text_buffer.hpp"

using namespace editor;

namespace {
TextBuffer makeBuffer(const std::string& s) {
    TextBuffer b;
    b.insert(0, s);
    return b;
}
}  // namespace

TEST(MultiCursor, StartsWithOneCaret) {
    MultiCursor mc;
    EXPECT_EQ(mc.count(), 1u);
    EXPECT_FALSE(mc.hasMultiple());
}

TEST(MultiCursor, SingleCaretInsertMatchesPlainInsert) {
    TextBuffer b = makeBuffer("hello");
    MultiCursor mc;
    mc.setPrimary(5);
    mc.insertText(b, "!");
    EXPECT_EQ(b.getText(), "hello!");
    EXPECT_EQ(mc.primary().pos, 6u);
}

TEST(MultiCursor, TwoCollapsedCaretsInsert) {
    TextBuffer b = makeBuffer("hello");
    MultiCursor mc;
    mc.setPrimary(0);
    mc.addCaret(5);
    mc.insertText(b, "X");
    // X at the front and the back; the second caret is shifted by the first insert.
    EXPECT_EQ(b.getText(), "XhelloX");
    ASSERT_EQ(mc.count(), 2u);
    EXPECT_EQ(mc.carets()[0].pos, 1u);
    EXPECT_EQ(mc.carets()[1].pos, 7u);
}

TEST(MultiCursor, ThreeCaretsInsertMultiChar) {
    TextBuffer b = makeBuffer("a.b.c");  // carets after each of a, b, c
    MultiCursor mc;
    mc.setPrimary(1);
    mc.addCaret(3);
    mc.addCaret(5);
    mc.insertText(b, "00");
    EXPECT_EQ(b.getText(), "a00.b00.c00");
}

TEST(MultiCursor, SelectionsReplacedAtEachCaret) {
    TextBuffer b = makeBuffer("foo foo");
    MultiCursor mc;
    mc.setPrimary(3, 0);   // selects "foo"
    mc.addCaret(7, 4);     // selects the second "foo"
    mc.insertText(b, "bar");
    EXPECT_EQ(b.getText(), "bar bar");
}

TEST(MultiCursor, SelectionShorterReplacementShiftsLaterCaret) {
    TextBuffer b = makeBuffer("aaaa bbbb");
    MultiCursor mc;
    mc.setPrimary(4, 0);   // "aaaa"
    mc.addCaret(9, 5);     // "bbbb"
    mc.insertText(b, "x"); // each 4-char selection -> 1 char
    EXPECT_EQ(b.getText(), "x x");
}

TEST(MultiCursor, BackspaceAtMultipleCollapsedCarets) {
    TextBuffer b = makeBuffer("ab cd");
    MultiCursor mc;
    mc.setPrimary(2);  // after 'b'
    mc.addCaret(5);    // end (after 'd')
    mc.backspace(b);
    EXPECT_EQ(b.getText(), "a c");
    ASSERT_EQ(mc.count(), 2u);
    EXPECT_EQ(mc.carets()[0].pos, 1u);
    EXPECT_EQ(mc.carets()[1].pos, 3u);
}

TEST(MultiCursor, DeleteForwardAtMultipleCarets) {
    TextBuffer b = makeBuffer("ab cd");
    MultiCursor mc;
    mc.setPrimary(0);  // before 'a'
    mc.addCaret(3);    // before 'c'
    mc.deleteForward(b);
    EXPECT_EQ(b.getText(), "b d");
}

TEST(MultiCursor, BackspaceDeletesSelections) {
    TextBuffer b = makeBuffer("foo foo");
    MultiCursor mc;
    mc.setPrimary(3, 0);
    mc.addCaret(7, 4);
    mc.backspace(b);
    EXPECT_EQ(b.getText(), " ");
}

TEST(MultiCursor, NormalizeMergesDuplicateCarets) {
    MultiCursor mc;
    mc.setPrimary(3);
    mc.addCaret(3);  // duplicate -> merged by addCaret/normalize
    EXPECT_EQ(mc.count(), 1u);
}

TEST(MultiCursor, NormalizeMergesOverlappingSelections) {
    MultiCursor mc;
    mc.setPrimary(5, 0);   // [0,5]
    mc.addCaret(8, 3);     // [3,8] overlaps -> merge to [0,8]
    ASSERT_EQ(mc.count(), 1u);
    EXPECT_EQ(mc.carets()[0].selStart(), 0u);
    EXPECT_EQ(mc.carets()[0].selEnd(), 8u);
}

TEST(MultiCursor, MergedTypingDoesNotDoubleInsert) {
    // Two carets that collapse to the same spot must insert once.
    TextBuffer b = makeBuffer("hello");
    MultiCursor mc;
    mc.setPrimary(2);
    mc.addCaret(2);
    mc.insertText(b, "Z");
    EXPECT_EQ(b.getText(), "heZllo");
}

TEST(MultiCursor, CollapseToPrimaryKeepsLowest) {
    MultiCursor mc;
    mc.setPrimary(1);
    mc.addCaret(9);
    mc.addCaret(5);
    EXPECT_EQ(mc.count(), 3u);
    mc.collapseToPrimary();
    ASSERT_EQ(mc.count(), 1u);
    EXPECT_EQ(mc.primary().pos, 1u);  // lowest offset after normalize
}
