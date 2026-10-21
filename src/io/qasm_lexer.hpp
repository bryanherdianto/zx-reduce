#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace zx {

enum class TokenType {
    Eof,
    Identifier,
    Number,
    Pi,
    Semicolon,
    Comma,
    LParen,
    RParen,
    LBracket,
    RBracket,
    LBrace,
    RBrace,
    Plus,
    Minus,
    Slash,
    Star,
    Arrow, // ->
    StringLiteral
};

struct Token {
    TokenType type = TokenType::Eof;
    std::string text;
    size_t line = 1;
    size_t col = 1;
};

class QasmLexer {
public:
    explicit QasmLexer(std::string_view source);
    Token next_token();
    std::vector<Token> tokenize_all();

private:
    std::string_view src;
    size_t pos = 0;
    size_t line = 1;
    size_t col = 1;

    char peek() const;
    char get();
    void skip_whitespace_and_comments();
};

} // namespace zx
