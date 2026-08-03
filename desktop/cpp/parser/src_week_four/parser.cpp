//
// Created by marle on 1/31/2026.
//

#include "../include_week_four/parser.hpp"
#include <stdexcept>

Parser::Parser(const std::vector<Token> &tokenList) : tokens(tokenList), position(0) {}

Token Parser::currentToken() const
{
    if (position < tokens.size())
    {
        return tokens[position];
    }
    return Token(TokenType::END, "");
}

void Parser::advance()
{
    if (position < tokens.size())
    {
        position++;
    }
}

bool Parser::match(TokenType type) const
{
    return currentToken().type == type;
}

bool Parser::matchOperator(const std::string &op) const
{
    return match(TokenType::OPERATOR) && currentToken().value == op;
}

std::unique_ptr<ASTNode> Parser::parse()
{
    auto result = parseExpression();
    if (!match(TokenType::END))
    {
        throw std::runtime_error("Unexpected token adter expression");
    }
    return result;
}

// Expression: Term (('+' | '-') Term)*
std::unique_ptr<ASTNode> Parser::parseExpression()
{
    auto left = parseTerm();

    while (matchOperator("+") || matchOperator("-"))
    {
        std::string op = currentToken().value;
        advance();
        auto right = parseTerm();
        left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
    }

    return left;
}

// Term: Factor (('*' | '/') Factor)*
std::unique_ptr<ASTNode> Parser::parseTerm()
{
    auto left = parseFactor();

    while (matchOperator("*") || matchOperator("/"))
    {
        std::string op = currentToken().value;
        advance();
        auto right = parseFactor();
        left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
    }

    return left;
}

// Factor: ('+' | '-') Factor | Power
std::unique_ptr<ASTNode> Parser::parseFactor()
{
    if (matchOperator("+"))
    {
        advance();
        return parseFactor();
    }

    if (matchOperator("-"))
    {
        advance();
        auto operand = parseFactor();
        return std::make_unique<UnaryOpNode>("-", std::move(operand));
    }

    return parsePower();
}

// Power: Primary ('^' Factor)*
std::unique_ptr<ASTNode> Parser::parsePower()
{
    auto left = parsePrimary();

    if (matchOperator("^"))
    {
        advance();
        auto right = parseFactor(); // Right associative
        left = std::make_unique<BinaryOpNode>("^", std::move(left), std::move(right));
    }

    return left;
}

// Primary: NUMBER | CONSTANT | FUNCTION '(' Expression ')' | '(' Expression ')'
std::unique_ptr<ASTNode> Parser::parsePrimary()
{
    // Handle numbers
    if (match(TokenType::NUMBER))
    {
        double value = std::stod(currentToken().value);
        advance();
        return std::make_unique<NumberNode>(value);
    }

    // Handle constants (pi, e)
    if (match(TokenType::CONSTANT))
    {
        std::string constName = currentToken().value;
        SymbolicConstant sym;

        if (constName == "pi")
        {
            sym = SymbolicConstant::PI;
        }
        else if (constName == "e")
        {
            sym = SymbolicConstant::E;
        }
        else
        {
            throw std::runtime_error("Unknown constant: " + constName);
        }

        advance();
        return std::make_unique<ConstantNode>(sym);
    }

    // Handle functions (and variable placeholders encoded as FUNCTION tokens)
    if (match(TokenType::FUNCTION))
    {
        std::string funcName = currentToken().value;
        advance();

        // Variables are encoded as FUNCTION tokens with "var_" prefix by the lexer.
        // They have no argument — wrap in a dummy FunctionNode with a zero argument.
        // Week4Bridge::makeFunctionNode will detect the prefix and return var(name).
        if (funcName.rfind("var_", 0) == 0)
        {
            return std::make_unique<FunctionNode>(funcName,
                                                  std::make_unique<NumberNode>(0.0));
        }

        if (!match(TokenType::LPAREN))
        {
            throw std::runtime_error("Expected '(' after function name");
        }
        advance();

        auto argument = parseExpression();

        if (!match(TokenType::RPAREN))
        {
            throw std::runtime_error("Expected ')' after function argument");
        }
        advance();

        return std::make_unique<FunctionNode>(funcName, std::move(argument));
    }

    // Handle parentheses
    if (match(TokenType::LPAREN))
    {
        advance();
        auto expr = parseExpression();

        if (!match(TokenType::RPAREN))
        {
            throw std::runtime_error("Expected ')'");
        }
        advance();

        return expr;
    }

    std::string tokVal = currentToken().value;
    std::string tokInfo = tokVal.empty() ? "<end of expression>" : tokVal;
    throw std::runtime_error("Unexpected token: " + tokInfo +
                             ". Check that your expression is complete and uses supported syntax.");
}