/**
 * @file test_highlighter.cpp
 * @brief Tests for syntax highlighting, focused on incremental re-tokenization.
 *
 * The core correctness property: after any sequence of edits, the incrementally
 * updated cache must be identical to a full re-highlight of the final text.
 */

#include <gtest/gtest.h>
#include "syntax/highlighter.hpp"

#include <string>
#include <vector>

using namespace syntax;

namespace {

// A line accessor backed by an in-memory vector of lines.
struct LineDoc {
    std::vector<std::string> lines;

    Highlighter::LineAccessor accessor() {
        return [this](size_t i) { return i < lines.size() ? lines[i] : std::string(); };
    }
    size_t count() const { return lines.size(); }
};

bool tokensEqual(const Token& a, const Token& b) {
    return a.type == b.type && a.start == b.start && a.length == b.length &&
           a.continuesOnNextLine == b.continuesOnNextLine &&
           a.continuesFromPrevLine == b.continuesFromPrevLine;
}

// Assert that an incrementally-updated highlighter matches a fresh full pass
// over the same document.
void expectMatchesFull(Highlighter& incremental, LineDoc& doc) {
    Highlighter full(Highlighter::getCppDefinition());
    full.highlightAllLines(doc.accessor(), doc.count());

    for (size_t i = 0; i < doc.count(); ++i) {
        const TokenizedLine* inc = incremental.getCachedLine(i);
        const TokenizedLine* ref = full.getCachedLine(i);
        ASSERT_NE(inc, nullptr) << "incremental missing line " << i;
        ASSERT_NE(ref, nullptr) << "full missing line " << i;
        ASSERT_EQ(inc->startsInMultiLineToken, ref->startsInMultiLineToken)
            << "carry-in mismatch on line " << i;
        ASSERT_EQ(inc->tokens.size(), ref->tokens.size())
            << "token count mismatch on line " << i;
        for (size_t t = 0; t < ref->tokens.size(); ++t) {
            EXPECT_TRUE(tokensEqual(inc->tokens[t], ref->tokens[t]))
                << "token " << t << " mismatch on line " << i;
        }
    }
}

LineDoc sampleDoc() {
    return LineDoc{{
        "#include <cstdio>",
        "int main() {",
        "    int x = 42;  // a number",
        "    return x;",
        "}",
    }};
}

}  // namespace

TEST(IncrementalHighlight, SingleCharEditMatchesFull) {
    LineDoc doc = sampleDoc();
    Highlighter h(Highlighter::getCppDefinition());
    h.highlightAllLines(doc.accessor(), doc.count());

    // Edit a single line in place (no line-count change).
    doc.lines[2] = "    int xy = 42;  // a number";
    h.applyEdit(doc.accessor(), doc.count(), 2);

    expectMatchesFull(h, doc);
}

TEST(IncrementalHighlight, InsertLineMatchesFull) {
    LineDoc doc = sampleDoc();
    Highlighter h(Highlighter::getCppDefinition());
    h.highlightAllLines(doc.accessor(), doc.count());

    // Split line 1 into two (newline inserted): line count grows by one.
    doc.lines[1] = "int main()";
    doc.lines.insert(doc.lines.begin() + 2, "{");
    h.applyEdit(doc.accessor(), doc.count(), 1);

    expectMatchesFull(h, doc);
}

TEST(IncrementalHighlight, DeleteLineMatchesFull) {
    LineDoc doc = sampleDoc();
    Highlighter h(Highlighter::getCppDefinition());
    h.highlightAllLines(doc.accessor(), doc.count());

    // Join lines 2 and 3 (a newline removed): line count shrinks by one.
    doc.lines[2] = "    int x = 42;  // a number    return x;";
    doc.lines.erase(doc.lines.begin() + 3);
    h.applyEdit(doc.accessor(), doc.count(), 2);

    expectMatchesFull(h, doc);
}

TEST(IncrementalHighlight, OpenBlockCommentCascadesMatchesFull) {
    LineDoc doc = sampleDoc();
    Highlighter h(Highlighter::getCppDefinition());
    h.highlightAllLines(doc.accessor(), doc.count());

    // Open a block comment on an early line: the carry state must cascade
    // downward and turn following lines into comment tokens.
    doc.lines[1] = "int main() { /* open";
    h.applyEdit(doc.accessor(), doc.count(), 1);

    expectMatchesFull(h, doc);

    // Following lines should now be inside the multi-line comment.
    const TokenizedLine* line3 = h.getCachedLine(3);
    ASSERT_NE(line3, nullptr);
    EXPECT_TRUE(line3->startsInMultiLineToken);
}

TEST(IncrementalHighlight, CloseBlockCommentMatchesFull) {
    LineDoc doc{{
        "int a = 1; /* start",
        "still comment",
        "",
        "end of comment */ int b = 2;",
        "int c = 3;",
    }};
    Highlighter h(Highlighter::getCppDefinition());
    h.highlightAllLines(doc.accessor(), doc.count());

    // Sanity: the blank middle line must still be 'in comment'.
    ASSERT_NE(h.getCachedLine(2), nullptr);
    EXPECT_TRUE(h.getCachedLine(2)->startsInMultiLineToken);

    // Now remove the comment opener; everything below should leave comment state.
    doc.lines[0] = "int a = 1;";
    h.applyEdit(doc.accessor(), doc.count(), 0);

    expectMatchesFull(h, doc);
    EXPECT_FALSE(h.getCachedLine(2)->startsInMultiLineToken);
}

TEST(IncrementalHighlight, MultiLinePasteMatchesFull) {
    LineDoc doc = sampleDoc();
    Highlighter h(Highlighter::getCppDefinition());
    h.highlightAllLines(doc.accessor(), doc.count());

    // Paste several lines at line 3 (line count grows by three).
    doc.lines[3] = "    int y = 7;";
    doc.lines.insert(doc.lines.begin() + 4, "    int z = 8;");
    doc.lines.insert(doc.lines.begin() + 5, "    /* multi");
    doc.lines.insert(doc.lines.begin() + 6, "       line */");
    h.applyEdit(doc.accessor(), doc.count(), 3);

    expectMatchesFull(h, doc);
}
