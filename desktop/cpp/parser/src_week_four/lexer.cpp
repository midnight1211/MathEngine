//
// Created by marle on 1/31/2026.
//

#include "../include_week_four/lexer.hpp"
#include <cctype>
#include <stdexcept>
#include <utility>

Lexer::Lexer(std::string inputStr) : input(std::move(inputStr)), position(0) {}

bool Lexer::isDigit(char c)
{
    return std::isdigit(static_cast<unsigned char>(c));
}

bool Lexer::isAlpha(char c)
{
    return std::isalpha(static_cast<unsigned char>(c));
}

void Lexer::skipWhitespace()
{
    while (position < input.length() && std::isspace(static_cast<unsigned char>(input[position])))
    {
        position++;
    }
}

Token Lexer::readNumber()
{
    std::string number;
    bool hasDecimal = false;

    while (position < input.length() &&
           (isDigit(input[position]) || input[position] == '.'))
    {
        if (input[position] == '.')
        {
            if (hasDecimal)
            {
                throw std::runtime_error("Invalid number format: multiple decimal points");
            }
            hasDecimal = true;
        }
        number += input[position];
        position++;
    }

    return {TokenType::NUMBER, number};
}

Token Lexer::readIdentifier()
{
    std::string identifier;

    while (position < input.length() && isAlpha(input[position]))
    {
        identifier += input[position];
        position++;
    }

    // Check if it's a function
    if (identifier == "sin" || identifier == "cos" || identifier == "tan" ||
        identifier == "log" || identifier == "ln" || identifier == "sqrt" ||
        identifier == "exp" || identifier == "asin" || identifier == "acos" ||
        identifier == "atan")
    {
        return {TokenType::FUNCTION, identifier};
    }

    // Check if it's a constant
    if (identifier == "pi" || identifier == "e")
    {
        return {TokenType::CONSTANT, identifier};
    }

    // Treat any other identifier as a symbolic variable.
    // We encode it as a FUNCTION token with the "var_" prefix so the parser
    // and Week4Bridge can detect it without needing a separate VARIABLE token type.
    // Common variable names: x, y, z, t, n, r, s, u, v, w, k, m, a, b, c
    // Also accept multi-letter names (alpha, beta, theta, etc.) via CalcExprBuilder cleanup.
    return {TokenType::FUNCTION, "var_" + identifier};
}

std::vector<Token> Lexer::tokenize()
{
    std::vector<Token> tokens;

    while (position < input.length())
    {
        skipWhitespace();

        if (position >= input.length())
        {
            break;
        }

        char current = input[position];

        // Handle numbers
        if (isDigit(current) || current == '.')
        {
            tokens.push_back(readNumber());
        }
        // Handle identifiers (functions and constants)
        else if (isAlpha(current))
        {
            tokens.push_back(readIdentifier());
        }
        // Handle operators
        else if (current == '+' || current == '-' || current == '*' ||
                 current == '/' || current == '^')
        {
            tokens.emplace_back(TokenType::OPERATOR, std::string(1, current));
            position++;
        }
        // Handle parentheses and such
        else if (current == '(')
        {
            tokens.emplace_back(TokenType::LPAREN, "(");
            position++;
        }
        else if (current == ')')
        {
            tokens.emplace_back(TokenType::RPAREN, ")");
            position++;
        }
        else if (current == '[')
        {
            tokens.emplace_back(TokenType::LBRACKET, "[");
            position++;
        }
        else if (current == ']')
        {
            tokens.emplace_back(TokenType::RBRACKET, "]");
            position++;
        }
        else if (current == '{')
        {
            tokens.emplace_back(TokenType::LBRACE, "{");
            position++;
        }
        else if (current == '}')
        {
            tokens.emplace_back(TokenType::RBRACE, "}");
            position++; // ← was missing: caused infinite loop → crash
            // Comma: used in some expression formats (e.g. "f, a, b" for integrals).
            // If it reaches week_4 as ARITHMETIC, skip it gracefully rather than crashing.
        }
        else if (current == ',')
        {
            position++; // skip comma
        }
        else
        {
            throw std::runtime_error(std::string("Unexpected character: ") + current);
        }
    }

    tokens.emplace_back(TokenType::END, "");
    return tokens;
}