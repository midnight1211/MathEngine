//
// Created by marle on 1/31/2026.
//

#ifndef CAPSTONE_STUFF_LEXER_HPP
#define CAPSTONE_STUFF_LEXER_HPP

#include <string>
#include <utility>
#include <vector>

// Token types for the calculator
enum class TokenType {
    NUMBER,
    OPERATOR,
    LPAREN,
    RPAREN,
    LBRACKET,
    RBRACKET,
    LBRACE,                     // Added for set operations for later, might discard later
    RBRACE,                     // Added for set operations for later, might discard later
    FUNCTION,
    CONSTANT,
    END
};

// Token structure
struct Token {
    TokenType type;
    std::string value;

    Token(TokenType t, std::string  v) : type(t), value(std::move(v)) {}
};

// Lexer class for tokenization
class Lexer {
private:
    std::string input;
    size_t position;

    [[nodiscard]] static bool isDigit(char c) ;
    [[nodiscard]] static bool isAlpha(char c) ;
    void skipWhitespace();
    Token readNumber();
    Token readIdentifier();

public:
    explicit Lexer(std::string  inputStr);
    std::vector<Token> tokenize();
};


#endif //CAPSTONE_STUFF_LEXER_HPP