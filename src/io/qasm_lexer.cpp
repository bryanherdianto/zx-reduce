#include "io/qasm_lexer.hpp"
#include <cctype>

namespace zx {

QasmLexer::QasmLexer(std::string_view source) : src(source) {}

char QasmLexer::peek() const {
    if (pos >= src.size()) return '\0';
    return src[pos];
}

char QasmLexer::get() {
    if (pos >= src.size()) return '\0';
    char c = src[pos++];
    if (c == '\n') {
        line++;
        col = 1;
    } else {
        col++;
    }
    return c;
}

void QasmLexer::skip_whitespace_and_comments() {
    while (pos < src.size()) {
        char c = peek();
        if (std::isspace(static_cast<unsigned char>(c))) {
            get();
        } else if (c == '/' && pos + 1 < src.size() && src[pos + 1] == '/') {
            // Line comment
            while (pos < src.size() && peek() != '\n') {
                get();
            }
        } else {
            break;
        }
    }
}

Token QasmLexer::next_token() {
    skip_whitespace_and_comments();
    if (pos >= src.size()) {
        return {TokenType::Eof, "", line, col};
    }

    size_t tok_line = line;
    size_t tok_col = col;
    char c = peek();

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        std::string text;
        while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
            text.push_back(get());
        }
        if (text == "pi") {
            return {TokenType::Pi, text, tok_line, tok_col};
        }
        return {TokenType::Identifier, text, tok_line, tok_col};
    }

    if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && pos + 1 < src.size() && std::isdigit(static_cast<unsigned char>(src[pos + 1])))) {
        std::string text;
        bool has_dot = false;
        while (std::isdigit(static_cast<unsigned char>(peek())) || (!has_dot && peek() == '.')) {
            if (peek() == '.') has_dot = true;
            text.push_back(get());
        }
        return {TokenType::Number, text, tok_line, tok_col};
    }

    if (c == '"') {
        get(); // skip quote
        std::string text;
        while (pos < src.size() && peek() != '"') {
            text.push_back(get());
        }
        if (peek() == '"') get();
        return {TokenType::StringLiteral, text, tok_line, tok_col};
    }

    get();
    switch (c) {
        case ';': return {TokenType::Semicolon, ";", tok_line, tok_col};
        case ',': return {TokenType::Comma, ",", tok_line, tok_col};
        case '(': return {TokenType::LParen, "(", tok_line, tok_col};
        case ')': return {TokenType::RParen, ")", tok_line, tok_col};
        case '[': return {TokenType::LBracket, "[", tok_line, tok_col};
        case ']': return {TokenType::RBracket, "]", tok_line, tok_col};
        case '{': return {TokenType::LBrace, "{", tok_line, tok_col};
        case '}': return {TokenType::RBrace, "}", tok_line, tok_col};
        case '+': return {TokenType::Plus, "+", tok_line, tok_col};
        case '-': {
            if (peek() == '>') {
                get();
                return {TokenType::Arrow, "->", tok_line, tok_col};
            }
            return {TokenType::Minus, "-", tok_line, tok_col};
        }
        case '/': return {TokenType::Slash, "/", tok_line, tok_col};
        case '*': return {TokenType::Star, "*", tok_line, tok_col};
        default:
            return {TokenType::Identifier, std::string(1, c), tok_line, tok_col};
    }
}

std::vector<Token> QasmLexer::tokenize_all() {
    std::vector<Token> tokens;
    while (true) {
        auto tok = next_token();
        tokens.push_back(tok);
        if (tok.type == TokenType::Eof) break;
    }
    return tokens;
}

} // namespace zx
