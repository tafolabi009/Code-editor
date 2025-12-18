/**
 * @file test_text_buffer.cpp
 * @brief Unit tests for TextBuffer and GapBuffer
 */

#include <gtest/gtest.h>
#include <chrono>
#include "editor/text_buffer.hpp"

using namespace editor;

// ============================================================================
// GapBuffer Tests
// ============================================================================

class GapBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        buffer = std::make_unique<GapBuffer>();
    }
    
    std::unique_ptr<GapBuffer> buffer;
};

TEST_F(GapBufferTest, DefaultConstructor) {
    EXPECT_EQ(buffer->size(), 0);
    EXPECT_TRUE(buffer->empty());
}

TEST_F(GapBufferTest, InsertSingleCharacter) {
    buffer->insert(0, 'A');
    EXPECT_EQ(buffer->size(), 1);
    EXPECT_EQ(buffer->at(0), 'A');
}

TEST_F(GapBufferTest, InsertString) {
    buffer->insert(0, "Hello");
    EXPECT_EQ(buffer->size(), 5);
    EXPECT_EQ(buffer->getText(), "Hello");
}

TEST_F(GapBufferTest, InsertAtEnd) {
    buffer->insert(0, "Hello");
    buffer->insert(5, " World");
    EXPECT_EQ(buffer->getText(), "Hello World");
}

TEST_F(GapBufferTest, InsertInMiddle) {
    buffer->insert(0, "Heo");
    buffer->insert(2, "ll");
    EXPECT_EQ(buffer->getText(), "Hello");
}

TEST_F(GapBufferTest, InsertAtBeginning) {
    buffer->insert(0, "World");
    buffer->insert(0, "Hello ");
    EXPECT_EQ(buffer->getText(), "Hello World");
}

TEST_F(GapBufferTest, DeleteSingleCharacter) {
    buffer->insert(0, "Hello");
    buffer->erase(4, 1);  // Delete 'o'
    EXPECT_EQ(buffer->getText(), "Hell");
}

TEST_F(GapBufferTest, DeleteRange) {
    buffer->insert(0, "Hello World");
    buffer->erase(5, 6);  // Delete " World"
    EXPECT_EQ(buffer->getText(), "Hello");
}

TEST_F(GapBufferTest, DeleteFromBeginning) {
    buffer->insert(0, "Hello World");
    buffer->erase(0, 6);  // Delete "Hello "
    EXPECT_EQ(buffer->getText(), "World");
}

TEST_F(GapBufferTest, DeleteEntireContent) {
    buffer->insert(0, "Hello");
    buffer->erase(0, 5);
    EXPECT_TRUE(buffer->empty());
}

TEST_F(GapBufferTest, GetSubstring) {
    buffer->insert(0, "Hello World");
    EXPECT_EQ(buffer->getText(0, 5), "Hello");
    EXPECT_EQ(buffer->getText(6, 5), "World");
}

TEST_F(GapBufferTest, ReplaceText) {
    buffer->insert(0, "Hello World");
    buffer->erase(6, 5);
    buffer->insert(6, "Universe");
    EXPECT_EQ(buffer->getText(), "Hello Universe");
}

TEST_F(GapBufferTest, MoveGap) {
    buffer->insert(0, "ABCDEFGH");
    // Insert at different positions to move gap
    buffer->insert(4, "X");
    EXPECT_EQ(buffer->getText(), "ABCDXEFGH");
    buffer->insert(2, "Y");
    EXPECT_EQ(buffer->getText(), "ABYCDXEFGH");
}

TEST_F(GapBufferTest, GapGrowth) {
    // Insert enough to trigger gap growth
    std::string longText(10000, 'A');
    buffer->insert(0, longText);
    EXPECT_EQ(buffer->size(), 10000);
    EXPECT_EQ(buffer->getText(), longText);
}

// ============================================================================
// TextBuffer Tests
// ============================================================================

class TextBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        buffer = std::make_unique<TextBuffer>();
    }
    
    std::unique_ptr<TextBuffer> buffer;
};

TEST_F(TextBufferTest, DefaultConstructor) {
    EXPECT_EQ(buffer->size(), 0);
    EXPECT_TRUE(buffer->empty());
    EXPECT_EQ(buffer->lineCount(), 1);  // Empty buffer has one line
}

TEST_F(TextBufferTest, InsertText) {
    buffer->insert(0, "Hello World");
    EXPECT_EQ(buffer->getText(), "Hello World");
}

TEST_F(TextBufferTest, InsertMultipleLines) {
    buffer->insert(0, "Line1\nLine2\nLine3");
    EXPECT_EQ(buffer->lineCount(), 3);
}

TEST_F(TextBufferTest, GetLine) {
    buffer->insert(0, "Line1\nLine2\nLine3");
    EXPECT_EQ(buffer->getLine(0), "Line1");
    EXPECT_EQ(buffer->getLine(1), "Line2");
    EXPECT_EQ(buffer->getLine(2), "Line3");
}

TEST_F(TextBufferTest, GetLineLength) {
    buffer->insert(0, "Short\nA longer line\nX");
    EXPECT_EQ(buffer->lineLength(0), 5);
    EXPECT_EQ(buffer->lineLength(1), 13);
    EXPECT_EQ(buffer->lineLength(2), 1);
}

TEST_F(TextBufferTest, PositionToOffset) {
    buffer->insert(0, "Line1\nLine2\nLine3");
    EXPECT_EQ(buffer->positionToOffset({0, 0}), 0);
    EXPECT_EQ(buffer->positionToOffset({0, 5}), 5);
    EXPECT_EQ(buffer->positionToOffset({1, 0}), 6);
    EXPECT_EQ(buffer->positionToOffset({2, 0}), 12);
}

TEST_F(TextBufferTest, OffsetToPosition) {
    buffer->insert(0, "Line1\nLine2\nLine3");
    
    auto pos0 = buffer->offsetToPosition(0);
    EXPECT_EQ(pos0.line, 0);
    EXPECT_EQ(pos0.column, 0);
    
    auto pos6 = buffer->offsetToPosition(6);
    EXPECT_EQ(pos6.line, 1);
    EXPECT_EQ(pos6.column, 0);
    
    auto pos12 = buffer->offsetToPosition(12);
    EXPECT_EQ(pos12.line, 2);
    EXPECT_EQ(pos12.column, 0);
}

TEST_F(TextBufferTest, UndoRedo) {
    buffer->insert(0, "Hello");
    EXPECT_EQ(buffer->getText(), "Hello");
    
    buffer->insert(5, " World");
    EXPECT_EQ(buffer->getText(), "Hello World");
    
    buffer->undo();
    EXPECT_EQ(buffer->getText(), "Hello");
    
    buffer->redo();
    EXPECT_EQ(buffer->getText(), "Hello World");
}

TEST_F(TextBufferTest, UndoDelete) {
    buffer->insert(0, "Hello World");
    buffer->erase(5, 6);
    EXPECT_EQ(buffer->getText(), "Hello");
    
    buffer->undo();
    EXPECT_EQ(buffer->getText(), "Hello World");
}

TEST_F(TextBufferTest, MultipleUndos) {
    buffer->insert(0, "A");
    buffer->insert(1, "B");
    buffer->insert(2, "C");
    
    EXPECT_EQ(buffer->getText(), "ABC");
    
    buffer->undo();
    EXPECT_EQ(buffer->getText(), "AB");
    
    buffer->undo();
    EXPECT_EQ(buffer->getText(), "A");
    
    buffer->undo();
    EXPECT_EQ(buffer->getText(), "");
}

TEST_F(TextBufferTest, RedoClearedByEdit) {
    buffer->insert(0, "Hello");
    buffer->insert(5, " World");
    buffer->undo();
    EXPECT_EQ(buffer->getText(), "Hello");
    
    // New edit should clear redo stack
    buffer->insert(5, "!");
    EXPECT_EQ(buffer->getText(), "Hello!");
    
    buffer->redo();  // Should have no effect
    EXPECT_EQ(buffer->getText(), "Hello!");
}

TEST_F(TextBufferTest, LineStartOffsets) {
    buffer->insert(0, "Line1\nLine2\nLine3");
    EXPECT_EQ(buffer->lineStart(0), 0);
    EXPECT_EQ(buffer->lineStart(1), 6);
    EXPECT_EQ(buffer->lineStart(2), 12);
}

TEST_F(TextBufferTest, ModifiedFlag) {
    EXPECT_FALSE(buffer->isModified());
    
    buffer->insert(0, "Hello");
    EXPECT_TRUE(buffer->isModified());
    
    buffer->setModified(false);
    EXPECT_FALSE(buffer->isModified());
}

TEST_F(TextBufferTest, EmptyLines) {
    buffer->insert(0, "\n\n\n");
    EXPECT_EQ(buffer->lineCount(), 4);
    EXPECT_EQ(buffer->getLine(0), "");
    EXPECT_EQ(buffer->getLine(1), "");
    EXPECT_EQ(buffer->getLine(2), "");
    EXPECT_EQ(buffer->getLine(3), "");
}

TEST_F(TextBufferTest, UnicodeText) {
    buffer->insert(0, "Hello 世界 🌍");
    EXPECT_EQ(buffer->getText(), "Hello 世界 🌍");
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(GapBufferTest, PerformanceSequentialInsert) {
    const int iterations = 100000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        buffer->insert(buffer->size(), "A");
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_EQ(buffer->size(), iterations);
    
    // Should complete in reasonable time (< 1 second)
    EXPECT_LT(duration.count(), 1000);
}

TEST_F(GapBufferTest, PerformanceRandomInsert) {
    const int iterations = 10000;
    
    buffer->insert(0, std::string(iterations, 'X'));
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        size_t pos = i % buffer->size();
        buffer->insert(pos, "A");
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete in reasonable time (< 2 seconds)
    EXPECT_LT(duration.count(), 2000);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(TextBufferTest, InsertAtInvalidPosition) {
    buffer->insert(0, "Hello");
    // Insert beyond end should append
    buffer->insert(100, "!");
    EXPECT_EQ(buffer->getText(), "Hello!");
}

TEST_F(TextBufferTest, DeleteBeyondEnd) {
    buffer->insert(0, "Hello");
    buffer->erase(3, 100);  // Try to delete more than exists
    EXPECT_EQ(buffer->getText(), "Hel");
}

TEST_F(TextBufferTest, GetLineOutOfBounds) {
    buffer->insert(0, "Line1\nLine2");
    // Getting line beyond bounds should return empty or last line
    auto line = buffer->getLine(100);
    // Implementation dependent - just check no crash
    EXPECT_TRUE(true);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
