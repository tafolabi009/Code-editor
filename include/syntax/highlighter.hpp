#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <regex>
#include <functional>

namespace syntax {

/**
 * @brief Token types for syntax highlighting
 */
enum class TokenType {
    Normal,
    Keyword,
    Type,
    String,
    Char,
    Number,
    Comment,
    MultiLineComment,
    Preprocessor,
    Function,
    Identifier,
    Operator,
    Punctuation,
    Whitespace,
    Error,
    
    // Language-specific
    Attribute,
    Namespace,
    Label,
    Macro,
    TemplateParam,
    Escape,  // Escape sequences in strings
    Regex,
    
    Count  // Number of token types
};

/**
 * @brief A single token in the source code
 */
struct Token {
    TokenType type;
    size_t start;      // Start offset in line
    size_t length;     // Length of token
    
    // For multiline tokens (comments, strings)
    bool continuesOnNextLine = false;
    bool continuesFromPrevLine = false;
};

/**
 * @brief Tokenized line with cached results
 */
struct TokenizedLine {
    std::vector<Token> tokens;
    size_t version = 0;  // For cache invalidation
    bool startsInMultiLineToken = false;
    TokenType multiLineTokenType = TokenType::Normal;
};

/**
 * @brief Language definition for syntax highlighting
 */
struct LanguageDefinition {
    std::string name;
    std::vector<std::string> extensions;  // File extensions (e.g., ".cpp", ".h")
    std::vector<std::string> filenames;   // Specific filenames (e.g., "Makefile")
    
    // Keywords
    std::unordered_set<std::string> keywords;
    std::unordered_set<std::string> types;
    std::unordered_set<std::string> builtins;
    
    // Syntax patterns
    std::string singleLineComment;         // e.g., "//"
    std::string multiLineCommentStart;     // e.g., "/*"
    std::string multiLineCommentEnd;       // e.g., "*/"
    std::string stringDelimiters;          // e.g., "\"'"
    char escapeChar = '\\';
    
    // Preprocessor
    std::string preprocessorPrefix;        // e.g., "#"
    
    // Operators
    std::string operators;                 // Characters that are operators
    
    // Identifier rules
    std::string identifierStartChars;
    std::string identifierChars;
    
    // Case sensitivity
    bool caseSensitive = true;
    
    // Numeric literals
    bool supportsHexNumbers = true;
    bool supportsOctalNumbers = true;
    bool supportsBinaryNumbers = true;
    bool supportsFloatNumbers = true;
};

/**
 * @brief Syntax highlighter for code
 * 
 * Supports incremental highlighting and caching for performance.
 */
class Highlighter {
public:
    Highlighter();
    explicit Highlighter(const LanguageDefinition& language);
    ~Highlighter();
    
    // Language
    void setLanguage(const LanguageDefinition& language);
    const LanguageDefinition& getLanguage() const { return m_language; }
    void setLanguageByExtension(const std::string& extension);
    void setLanguageByFilename(const std::string& filename);
    
    // Tokenization
    TokenizedLine tokenizeLine(std::string_view line, bool startsInMultiLine = false,
                               TokenType multiLineType = TokenType::Normal);
    
    // Highlight entire text (returns tokens per line)
    std::vector<TokenizedLine> highlightText(std::string_view text);

    // Line-accessor based highlighting. These keep the cache indexed by the
    // same lines the editor uses (TextBuffer::getLine), which is what makes
    // incremental updates correct.
    using LineAccessor = std::function<std::string(size_t)>;

    // (Re)build the whole cache from a line accessor.
    void highlightAllLines(const LineAccessor& getLine, size_t lineCount);

    // Incrementally re-tokenize after an edit: re-tokenizes from
    // firstChangedLine downward, cascading only while the multi-line carry
    // state keeps changing, then stops (convergence). O(changed lines) for the
    // common case instead of O(whole buffer).
    void applyEdit(const LineAccessor& getLine, size_t newLineCount,
                   size_t firstChangedLine);

    // Incremental updates
    void invalidateLine(size_t lineIndex);
    void invalidateRange(size_t startLine, size_t endLine);
    void invalidateAll();
    
    // Get cached tokens
    const TokenizedLine* getCachedLine(size_t lineIndex) const;
    
    // Token type helpers
    static std::string tokenTypeName(TokenType type);
    static bool isKeywordChar(char c);
    
    // Built-in language definitions
    static LanguageDefinition getCppDefinition();
    static LanguageDefinition getPythonDefinition();
    static LanguageDefinition getJavaScriptDefinition();
    static LanguageDefinition getCDefinition();
    static LanguageDefinition getPlainTextDefinition();
    
    // Language detection
    static const LanguageDefinition* detectLanguage(const std::string& filename);
    
private:
    LanguageDefinition m_language;
    std::vector<TokenizedLine> m_cache;
    size_t m_cacheVersion = 0;
    
    // Tokenizer state
    struct TokenizerState {
        std::string_view line;
        size_t position = 0;
        bool inMultiLineComment = false;
        bool inMultiLineString = false;
        char stringDelimiter = 0;
    };
    
    // Tokenization helpers
    Token readToken(TokenizerState& state);
    Token readString(TokenizerState& state, char delimiter);
    Token readNumber(TokenizerState& state);
    Token readIdentifier(TokenizerState& state);
    Token readOperator(TokenizerState& state);
    Token readComment(TokenizerState& state);
    Token readPreprocessor(TokenizerState& state);
    
    bool isKeyword(std::string_view word) const;
    bool isType(std::string_view word) const;
    bool isBuiltin(std::string_view word) const;
    
    char peek(const TokenizerState& state, size_t offset = 0) const;
    char advance(TokenizerState& state);
    bool match(TokenizerState& state, char expected);
    bool match(TokenizerState& state, std::string_view expected);
    void skipWhitespace(TokenizerState& state);
};

/**
 * @brief Registry of language definitions
 */
class LanguageRegistry {
public:
    static LanguageRegistry& instance();
    
    void registerLanguage(const LanguageDefinition& def);
    const LanguageDefinition* getLanguageByName(const std::string& name) const;
    const LanguageDefinition* getLanguageByExtension(const std::string& ext) const;
    const LanguageDefinition* getLanguageByFilename(const std::string& filename) const;
    
    std::vector<std::string> getLanguageNames() const;
    
private:
    LanguageRegistry();
    std::unordered_map<std::string, LanguageDefinition> m_languages;
    std::unordered_map<std::string, std::string> m_extensionMap;  // ext -> language name
    std::unordered_map<std::string, std::string> m_filenameMap;   // filename -> language name
};

} // namespace syntax
