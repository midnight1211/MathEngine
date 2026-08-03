//
// Created by marle on 1/31/2026.
//

#ifndef CAPSTONE_STUFF_PARSER_HPP
#define CAPSTONE_STUFF_PARSER_HPP

#include "lexer.hpp"
#include "value.hpp"
#include <vector>
#include <memory>

// AST Node types
enum class ASTNodeType {
    NUMBER,
    CONSTANT,
    BINARY_OP,
    UNARY_OP,
    FUNCTION
};

// Abstract Syntax Tree Node
class ASTNode {
public:
    ASTNodeType type;

    virtual ~ASTNode() = default;
    [[nodiscard]] explicit ASTNode(ASTNodeType t) : type(t) {}
};

// Number node
class NumberNode : public ASTNode {
public:
    double value;

    [[nodiscard]] explicit NumberNode(double val)
        : ASTNode(ASTNodeType::NUMBER), value (val) {}
};

// Constant node (pi, e)
class ConstantNode : public ASTNode {
public:
    SymbolicConstant constant;

    explicit ConstantNode(SymbolicConstant sym)
        : ASTNode(ASTNodeType::CONSTANT), constant(sym) {}
};

// Binary operation node (+, -, *, /, ^)
class BinaryOpNode : public ASTNode {
public:
    std::string op;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;

    BinaryOpNode(const std::string& operation,
                 std::unique_ptr<ASTNode> l,
                 std::unique_ptr<ASTNode> r)
        : ASTNode(ASTNodeType::BINARY_OP), op(operation),
          left(std::move(l)), right(std::move(r)) {}
};

// Unary operation node (unary minus)
class UnaryOpNode : public ASTNode {
public:
    std::string op;
    std::unique_ptr<ASTNode> operand;

    UnaryOpNode(const std::string& operation, std::unique_ptr<ASTNode> expr)
        : ASTNode(ASTNodeType::UNARY_OP), op(operation), operand(std::move(expr)) {}
};

// Function node (sin, cos, log, etc.)
class FunctionNode : public ASTNode {
public:
    std::string name;
    std::unique_ptr<ASTNode> argument;

    FunctionNode(const std::string& funcName, std::unique_ptr<ASTNode> arg)
        : ASTNode(ASTNodeType::FUNCTION), name(funcName), argument(std::move(arg)) {}
};

// Recursive Descent Parser
class Parser {
private:
    std::vector<Token> tokens;
    size_t position;

    Token currentToken() const;
    void advance();
    bool match(TokenType type) const;
    bool matchOperator(const std::string& op) const;

    std::unique_ptr<ASTNode> parseExpression();
    std::unique_ptr<ASTNode> parseTerm();
    std::unique_ptr<ASTNode> parseFactor();
    std::unique_ptr<ASTNode> parsePower();
    std::unique_ptr<ASTNode> parsePrimary();

public:
    explicit Parser(const std::vector<Token>& tokenList);
    std::unique_ptr<ASTNode> parse();
};

#endif //CAPSTONE_STUFF_PARSER_HPP