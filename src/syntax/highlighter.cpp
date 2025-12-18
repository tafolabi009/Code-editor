/**
 * @file highlighter.cpp
 * @brief Syntax highlighting implementation with language definitions
 */

#include "syntax/highlighter.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace syntax {

// ====================
// Highlighter Implementation
// ====================

Highlighter::Highlighter() {
    setLanguage(getPlainTextDefinition());
}

Highlighter::Highlighter(const LanguageDefinition& language) {
    setLanguage(language);
}

Highlighter::~Highlighter() = default;

void Highlighter::setLanguage(const LanguageDefinition& language) {
    m_language = language;
    invalidateAll();
}

void Highlighter::setLanguageByExtension(const std::string& extension) {
    if (auto* lang = LanguageRegistry::instance().getLanguageByExtension(extension)) {
        setLanguage(*lang);
    }
}

void Highlighter::setLanguageByFilename(const std::string& filename) {
    if (auto* lang = LanguageRegistry::instance().getLanguageByFilename(filename)) {
        setLanguage(*lang);
    }
}

TokenizedLine Highlighter::tokenizeLine(std::string_view line, bool startsInMultiLine,
                                        TokenType multiLineType) {
    TokenizedLine result;
    result.startsInMultiLineToken = startsInMultiLine;
    result.multiLineTokenType = multiLineType;
    
    TokenizerState state;
    state.line = line;
    state.position = 0;
    state.inMultiLineComment = (startsInMultiLine && multiLineType == TokenType::MultiLineComment);
    state.inMultiLineString = (startsInMultiLine && multiLineType == TokenType::String);
    
    while (state.position < line.length()) {
        Token token = readToken(state);
        if (token.length > 0) {
            result.tokens.push_back(token);
        }
    }
    
    return result;
}

std::vector<TokenizedLine> Highlighter::highlightText(std::string_view text) {
    std::vector<TokenizedLine> result;
    
    bool inMultiLine = false;
    TokenType multiLineType = TokenType::Normal;
    
    std::istringstream stream{std::string{text}};
    std::string line;
    size_t lineIndex = 0;
    
    while (std::getline(stream, line)) {
        auto tokenized = tokenizeLine(line, inMultiLine, multiLineType);
        result.push_back(tokenized);
        
        // Check if this line ends in a multi-line token
        if (!tokenized.tokens.empty()) {
            const auto& lastToken = tokenized.tokens.back();
            if (lastToken.continuesOnNextLine) {
                inMultiLine = true;
                multiLineType = lastToken.type;
            } else if (lastToken.type == TokenType::MultiLineComment || 
                      lastToken.type == TokenType::String) {
                // Check if multi-line comment/string ended
                inMultiLine = false;
            }
        }
        
        ++lineIndex;
    }
    
    // Cache results
    m_cache = result;
    ++m_cacheVersion;
    
    return result;
}

void Highlighter::invalidateLine(size_t lineIndex) {
    if (lineIndex < m_cache.size()) {
        m_cache[lineIndex].version = 0;
    }
}

void Highlighter::invalidateRange(size_t startLine, size_t endLine) {
    for (size_t i = startLine; i <= endLine && i < m_cache.size(); ++i) {
        m_cache[i].version = 0;
    }
}

void Highlighter::invalidateAll() {
    m_cache.clear();
    ++m_cacheVersion;
}

const TokenizedLine* Highlighter::getCachedLine(size_t lineIndex) const {
    if (lineIndex < m_cache.size()) {
        return &m_cache[lineIndex];
    }
    return nullptr;
}

Token Highlighter::readToken(TokenizerState& state) {
    if (state.position >= state.line.length()) {
        return {TokenType::Normal, state.position, 0, false, false};
    }
    
    // Handle multi-line states
    if (state.inMultiLineComment) {
        return readComment(state);
    }
    
    if (state.inMultiLineString) {
        return readString(state, state.stringDelimiter);
    }
    
    char c = peek(state);
    
    // Whitespace
    if (std::isspace(c)) {
        size_t start = state.position;
        while (state.position < state.line.length() && std::isspace(peek(state))) {
            advance(state);
        }
        return {TokenType::Whitespace, start, state.position - start, false, false};
    }
    
    // Single-line comment
    if (!m_language.singleLineComment.empty() && 
        match(state, m_language.singleLineComment)) {
        size_t start = state.position - m_language.singleLineComment.length();
        state.position = state.line.length();
        return {TokenType::Comment, start, state.position - start, false, false};
    }
    
    // Multi-line comment start
    if (!m_language.multiLineCommentStart.empty() &&
        match(state, m_language.multiLineCommentStart)) {
        state.inMultiLineComment = true;
        size_t start = state.position - m_language.multiLineCommentStart.length();
        
        // Look for end on same line
        size_t endPos = state.line.find(m_language.multiLineCommentEnd, state.position);
        if (endPos != std::string_view::npos) {
            state.position = endPos + m_language.multiLineCommentEnd.length();
            state.inMultiLineComment = false;
            return {TokenType::MultiLineComment, start, state.position - start, false, false};
        }
        
        state.position = state.line.length();
        return {TokenType::MultiLineComment, start, state.position - start, true, false};
    }
    
    // Preprocessor
    if (!m_language.preprocessorPrefix.empty() && c == m_language.preprocessorPrefix[0]) {
        return readPreprocessor(state);
    }
    
    // String literals
    if (m_language.stringDelimiters.find(c) != std::string::npos) {
        return readString(state, c);
    }
    
    // Numbers
    if (std::isdigit(c) || (c == '.' && state.position + 1 < state.line.length() && 
                           std::isdigit(state.line[state.position + 1]))) {
        return readNumber(state);
    }
    
    // Identifiers and keywords
    if (std::isalpha(c) || c == '_' || 
        m_language.identifierStartChars.find(c) != std::string::npos) {
        return readIdentifier(state);
    }
    
    // Operators
    if (m_language.operators.find(c) != std::string::npos) {
        return readOperator(state);
    }
    
    // Punctuation
    size_t start = state.position;
    advance(state);
    return {TokenType::Punctuation, start, 1, false, false};
}

Token Highlighter::readString(TokenizerState& state, char delimiter) {
    size_t start = state.position;
    bool continuesFromPrev = state.inMultiLineString;
    
    if (!continuesFromPrev) {
        advance(state);  // Skip opening delimiter
    }
    state.stringDelimiter = delimiter;
    
    while (state.position < state.line.length()) {
        char c = peek(state);
        
        if (c == m_language.escapeChar && state.position + 1 < state.line.length()) {
            advance(state);  // Skip escape char
            advance(state);  // Skip escaped char
            continue;
        }
        
        if (c == delimiter) {
            advance(state);  // Skip closing delimiter
            state.inMultiLineString = false;
            return {TokenType::String, start, state.position - start, false, continuesFromPrev};
        }
        
        advance(state);
    }
    
    // String continues on next line
    state.inMultiLineString = true;
    return {TokenType::String, start, state.position - start, true, continuesFromPrev};
}

Token Highlighter::readNumber(TokenizerState& state) {
    size_t start = state.position;
    
    // Check for hex/octal/binary prefix
    if (peek(state) == '0' && state.position + 1 < state.line.length()) {
        char next = state.line[state.position + 1];
        if ((next == 'x' || next == 'X') && m_language.supportsHexNumbers) {
            advance(state);
            advance(state);
            while (state.position < state.line.length() && 
                   std::isxdigit(peek(state))) {
                advance(state);
            }
            return {TokenType::Number, start, state.position - start, false, false};
        }
        if ((next == 'b' || next == 'B') && m_language.supportsBinaryNumbers) {
            advance(state);
            advance(state);
            while (state.position < state.line.length() && 
                   (peek(state) == '0' || peek(state) == '1')) {
                advance(state);
            }
            return {TokenType::Number, start, state.position - start, false, false};
        }
    }
    
    // Integer part
    while (state.position < state.line.length() && std::isdigit(peek(state))) {
        advance(state);
    }
    
    // Decimal part
    if (m_language.supportsFloatNumbers && peek(state) == '.' && 
        state.position + 1 < state.line.length() && 
        std::isdigit(state.line[state.position + 1])) {
        advance(state);
        while (state.position < state.line.length() && std::isdigit(peek(state))) {
            advance(state);
        }
    }
    
    // Exponent
    if (m_language.supportsFloatNumbers && 
        (peek(state) == 'e' || peek(state) == 'E')) {
        advance(state);
        if (peek(state) == '+' || peek(state) == '-') {
            advance(state);
        }
        while (state.position < state.line.length() && std::isdigit(peek(state))) {
            advance(state);
        }
    }
    
    // Suffix (f, l, u, etc.)
    while (state.position < state.line.length() && 
           (std::isalpha(peek(state)) || peek(state) == '_')) {
        advance(state);
    }
    
    return {TokenType::Number, start, state.position - start, false, false};
}

Token Highlighter::readIdentifier(TokenizerState& state) {
    size_t start = state.position;
    
    while (state.position < state.line.length()) {
        char c = peek(state);
        if (std::isalnum(c) || c == '_' ||
            m_language.identifierChars.find(c) != std::string::npos) {
            advance(state);
        } else {
            break;
        }
    }
    
    std::string_view word = state.line.substr(start, state.position - start);
    
    // Determine token type based on word
    TokenType type = TokenType::Identifier;
    
    if (isKeyword(word)) {
        type = TokenType::Keyword;
    } else if (isType(word)) {
        type = TokenType::Type;
    } else if (isBuiltin(word)) {
        type = TokenType::Function;
    } else {
        // Check if followed by '(' - likely a function
        size_t i = state.position;
        while (i < state.line.length() && std::isspace(state.line[i])) ++i;
        if (i < state.line.length() && state.line[i] == '(') {
            type = TokenType::Function;
        }
    }
    
    return {type, start, state.position - start, false, false};
}

Token Highlighter::readOperator(TokenizerState& state) {
    size_t start = state.position;
    
    // Try to match multi-character operators
    while (state.position < state.line.length() &&
           m_language.operators.find(peek(state)) != std::string::npos) {
        advance(state);
    }
    
    return {TokenType::Operator, start, state.position - start, false, false};
}

Token Highlighter::readComment(TokenizerState& state) {
    size_t start = state.position;
    
    // Look for multi-line comment end
    size_t endPos = state.line.find(m_language.multiLineCommentEnd, state.position);
    if (endPos != std::string_view::npos) {
        state.position = endPos + m_language.multiLineCommentEnd.length();
        state.inMultiLineComment = false;
        return {TokenType::MultiLineComment, start, state.position - start, false, true};
    }
    
    // Comment continues
    state.position = state.line.length();
    return {TokenType::MultiLineComment, start, state.position - start, true, true};
}

Token Highlighter::readPreprocessor(TokenizerState& state) {
    size_t start = state.position;
    
    // Read entire line as preprocessor directive
    state.position = state.line.length();
    
    return {TokenType::Preprocessor, start, state.position - start, false, false};
}

bool Highlighter::isKeyword(std::string_view word) const {
    std::string w(word);
    if (!m_language.caseSensitive) {
        std::transform(w.begin(), w.end(), w.begin(), ::tolower);
    }
    return m_language.keywords.count(w) > 0;
}

bool Highlighter::isType(std::string_view word) const {
    std::string w(word);
    if (!m_language.caseSensitive) {
        std::transform(w.begin(), w.end(), w.begin(), ::tolower);
    }
    return m_language.types.count(w) > 0;
}

bool Highlighter::isBuiltin(std::string_view word) const {
    std::string w(word);
    if (!m_language.caseSensitive) {
        std::transform(w.begin(), w.end(), w.begin(), ::tolower);
    }
    return m_language.builtins.count(w) > 0;
}

char Highlighter::peek(const TokenizerState& state, size_t offset) const {
    size_t pos = state.position + offset;
    if (pos >= state.line.length()) {
        return '\0';
    }
    return state.line[pos];
}

char Highlighter::advance(TokenizerState& state) {
    if (state.position >= state.line.length()) {
        return '\0';
    }
    return state.line[state.position++];
}

bool Highlighter::match(TokenizerState& state, char expected) {
    if (peek(state) == expected) {
        advance(state);
        return true;
    }
    return false;
}

bool Highlighter::match(TokenizerState& state, std::string_view expected) {
    if (state.position + expected.length() > state.line.length()) {
        return false;
    }
    
    if (state.line.substr(state.position, expected.length()) == expected) {
        state.position += expected.length();
        return true;
    }
    return false;
}

std::string Highlighter::tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::Normal: return "Normal";
        case TokenType::Keyword: return "Keyword";
        case TokenType::Type: return "Type";
        case TokenType::String: return "String";
        case TokenType::Char: return "Char";
        case TokenType::Number: return "Number";
        case TokenType::Comment: return "Comment";
        case TokenType::MultiLineComment: return "MultiLineComment";
        case TokenType::Preprocessor: return "Preprocessor";
        case TokenType::Function: return "Function";
        case TokenType::Identifier: return "Identifier";
        case TokenType::Operator: return "Operator";
        case TokenType::Punctuation: return "Punctuation";
        case TokenType::Whitespace: return "Whitespace";
        case TokenType::Error: return "Error";
        default: return "Unknown";
    }
}

// ====================
// Language Definitions
// ====================

LanguageDefinition Highlighter::getCppDefinition() {
    LanguageDefinition lang;
    lang.name = "C++";
    lang.extensions = {".cpp", ".hpp", ".cc", ".hh", ".cxx", ".hxx", ".h"};
    
    lang.keywords = {
        "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
        "break", "case", "catch", "class", "compl", "concept", "const", "consteval",
        "constexpr", "constinit", "const_cast", "continue", "co_await", "co_return",
        "co_yield", "decltype", "default", "delete", "do", "dynamic_cast", "else",
        "enum", "explicit", "export", "extern", "false", "for", "friend", "goto",
        "if", "inline", "mutable", "namespace", "new", "noexcept", "not", "not_eq",
        "nullptr", "operator", "or", "or_eq", "private", "protected", "public",
        "register", "reinterpret_cast", "requires", "return", "sizeof", "static",
        "static_assert", "static_cast", "struct", "switch", "template", "this",
        "thread_local", "throw", "true", "try", "typedef", "typeid", "typename",
        "union", "using", "virtual", "volatile", "while", "xor", "xor_eq",
        "override", "final"
    };
    
    lang.types = {
        "bool", "char", "char8_t", "char16_t", "char32_t", "double", "float",
        "int", "long", "short", "signed", "unsigned", "void", "wchar_t",
        "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t",
        "uint32_t", "uint64_t", "size_t", "ptrdiff_t", "intptr_t", "uintptr_t",
        "string", "vector", "map", "set", "unordered_map", "unordered_set",
        "array", "list", "deque", "queue", "stack", "pair", "tuple",
        "shared_ptr", "unique_ptr", "weak_ptr", "optional", "variant"
    };
    
    lang.builtins = {
        "std", "cout", "cin", "cerr", "endl", "printf", "scanf", "malloc",
        "free", "memcpy", "memset", "strlen", "strcpy", "strcmp"
    };
    
    lang.singleLineComment = "//";
    lang.multiLineCommentStart = "/*";
    lang.multiLineCommentEnd = "*/";
    lang.stringDelimiters = "\"'";
    lang.escapeChar = '\\';
    lang.preprocessorPrefix = "#";
    lang.operators = "+-*/%=<>!&|^~?:";
    lang.identifierStartChars = "_";
    lang.identifierChars = "_";
    lang.caseSensitive = true;
    
    return lang;
}

LanguageDefinition Highlighter::getPythonDefinition() {
    LanguageDefinition lang;
    lang.name = "Python";
    lang.extensions = {".py", ".pyw", ".pyi"};
    
    lang.keywords = {
        "False", "None", "True", "and", "as", "assert", "async", "await",
        "break", "class", "continue", "def", "del", "elif", "else", "except",
        "finally", "for", "from", "global", "if", "import", "in", "is",
        "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try",
        "while", "with", "yield"
    };
    
    lang.types = {
        "int", "float", "str", "bool", "list", "dict", "set", "tuple",
        "bytes", "bytearray", "complex", "frozenset", "object", "type"
    };
    
    lang.builtins = {
        "abs", "all", "any", "bin", "bool", "bytearray", "bytes", "callable",
        "chr", "classmethod", "compile", "complex", "delattr", "dict", "dir",
        "divmod", "enumerate", "eval", "exec", "filter", "float", "format",
        "frozenset", "getattr", "globals", "hasattr", "hash", "help", "hex",
        "id", "input", "int", "isinstance", "issubclass", "iter", "len",
        "list", "locals", "map", "max", "memoryview", "min", "next", "object",
        "oct", "open", "ord", "pow", "print", "property", "range", "repr",
        "reversed", "round", "set", "setattr", "slice", "sorted", "staticmethod",
        "str", "sum", "super", "tuple", "type", "vars", "zip", "__import__"
    };
    
    lang.singleLineComment = "#";
    lang.multiLineCommentStart = "\"\"\"";
    lang.multiLineCommentEnd = "\"\"\"";
    lang.stringDelimiters = "\"'";
    lang.escapeChar = '\\';
    lang.operators = "+-*/%=<>!&|^~@:";
    lang.identifierStartChars = "_";
    lang.identifierChars = "_";
    lang.caseSensitive = true;
    
    return lang;
}

LanguageDefinition Highlighter::getJavaScriptDefinition() {
    LanguageDefinition lang;
    lang.name = "JavaScript";
    lang.extensions = {".js", ".jsx", ".mjs", ".cjs", ".ts", ".tsx"};
    
    lang.keywords = {
        "async", "await", "break", "case", "catch", "class", "const", "continue",
        "debugger", "default", "delete", "do", "else", "enum", "export", "extends",
        "false", "finally", "for", "function", "if", "implements", "import", "in",
        "instanceof", "interface", "let", "new", "null", "package", "private",
        "protected", "public", "return", "static", "super", "switch", "this",
        "throw", "true", "try", "typeof", "undefined", "var", "void", "while",
        "with", "yield"
    };
    
    lang.types = {
        "Array", "Boolean", "Date", "Error", "Function", "Map", "Number",
        "Object", "Promise", "RegExp", "Set", "String", "Symbol", "WeakMap",
        "WeakSet", "any", "boolean", "never", "number", "string", "unknown"
    };
    
    lang.builtins = {
        "console", "document", "window", "Math", "JSON", "parseInt", "parseFloat",
        "isNaN", "isFinite", "setTimeout", "setInterval", "clearTimeout",
        "clearInterval", "fetch", "alert", "confirm", "prompt"
    };
    
    lang.singleLineComment = "//";
    lang.multiLineCommentStart = "/*";
    lang.multiLineCommentEnd = "*/";
    lang.stringDelimiters = "\"'`";
    lang.escapeChar = '\\';
    lang.operators = "+-*/%=<>!&|^~?:";
    lang.identifierStartChars = "_$";
    lang.identifierChars = "_$";
    lang.caseSensitive = true;
    
    return lang;
}

LanguageDefinition Highlighter::getCDefinition() {
    LanguageDefinition lang;
    lang.name = "C";
    lang.extensions = {".c", ".h"};
    
    lang.keywords = {
        "auto", "break", "case", "const", "continue", "default", "do", "else",
        "enum", "extern", "for", "goto", "if", "inline", "register", "restrict",
        "return", "sizeof", "static", "struct", "switch", "typedef", "union",
        "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool",
        "_Complex", "_Generic", "_Imaginary", "_Noreturn", "_Static_assert",
        "_Thread_local"
    };
    
    lang.types = {
        "char", "double", "float", "int", "long", "short", "signed", "unsigned",
        "void", "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t",
        "uint32_t", "uint64_t", "size_t", "ptrdiff_t", "intptr_t", "uintptr_t",
        "FILE", "bool"
    };
    
    lang.builtins = {
        "printf", "scanf", "malloc", "calloc", "realloc", "free", "memcpy",
        "memmove", "memset", "memcmp", "strlen", "strcpy", "strncpy", "strcmp",
        "strncmp", "strcat", "strncat", "fopen", "fclose", "fread", "fwrite",
        "fprintf", "fscanf", "fgets", "fputs", "getchar", "putchar", "puts",
        "gets", "exit", "abort", "assert"
    };
    
    lang.singleLineComment = "//";
    lang.multiLineCommentStart = "/*";
    lang.multiLineCommentEnd = "*/";
    lang.stringDelimiters = "\"'";
    lang.escapeChar = '\\';
    lang.preprocessorPrefix = "#";
    lang.operators = "+-*/%=<>!&|^~?:";
    lang.identifierStartChars = "_";
    lang.identifierChars = "_";
    lang.caseSensitive = true;
    
    return lang;
}

LanguageDefinition Highlighter::getPlainTextDefinition() {
    LanguageDefinition lang;
    lang.name = "Plain Text";
    lang.extensions = {".txt", ".text", ".log"};
    lang.caseSensitive = true;
    return lang;
}

const LanguageDefinition* Highlighter::detectLanguage(const std::string& filename) {
    return LanguageRegistry::instance().getLanguageByFilename(filename);
}

// ====================
// LanguageRegistry Implementation
// ====================

LanguageRegistry& LanguageRegistry::instance() {
    static LanguageRegistry registry;
    return registry;
}

LanguageRegistry::LanguageRegistry() {
    // Register built-in languages
    registerLanguage(Highlighter::getCppDefinition());
    registerLanguage(Highlighter::getCDefinition());
    registerLanguage(Highlighter::getPythonDefinition());
    registerLanguage(Highlighter::getJavaScriptDefinition());
    registerLanguage(Highlighter::getPlainTextDefinition());
}

void LanguageRegistry::registerLanguage(const LanguageDefinition& def) {
    m_languages[def.name] = def;
    
    for (const auto& ext : def.extensions) {
        m_extensionMap[ext] = def.name;
    }
    
    for (const auto& filename : def.filenames) {
        m_filenameMap[filename] = def.name;
    }
}

const LanguageDefinition* LanguageRegistry::getLanguageByName(const std::string& name) const {
    auto it = m_languages.find(name);
    return it != m_languages.end() ? &it->second : nullptr;
}

const LanguageDefinition* LanguageRegistry::getLanguageByExtension(const std::string& ext) const {
    std::string lowerExt = ext;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
    
    auto it = m_extensionMap.find(lowerExt);
    if (it != m_extensionMap.end()) {
        return getLanguageByName(it->second);
    }
    return nullptr;
}

const LanguageDefinition* LanguageRegistry::getLanguageByFilename(const std::string& filename) const {
    // Check exact filename match first
    auto it = m_filenameMap.find(filename);
    if (it != m_filenameMap.end()) {
        return getLanguageByName(it->second);
    }
    
    // Try extension
    size_t dotPos = filename.rfind('.');
    if (dotPos != std::string::npos) {
        return getLanguageByExtension(filename.substr(dotPos));
    }
    
    return nullptr;
}

std::vector<std::string> LanguageRegistry::getLanguageNames() const {
    std::vector<std::string> names;
    names.reserve(m_languages.size());
    for (const auto& [name, _] : m_languages) {
        names.push_back(name);
    }
    return names;
}

} // namespace syntax
