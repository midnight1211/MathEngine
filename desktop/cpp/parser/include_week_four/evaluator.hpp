//
// Created by marle on 1/31/2026.
//

#ifndef CAPSTONE_STUFF_EVALUATOR_HPP
#define CAPSTONE_STUFF_EVALUATOR_HPP

#include "parser.hpp"
#include "value.hpp"
#include <memory>

class Evaluator {
private:
    bool exactMode;

    std::unique_ptr<Value> evaluateNode(const ASTNode* node);
    std::unique_ptr<Value> evaluateBinaryOp(const BinaryOpNode* node);
    std::unique_ptr<Value> evaluateUnaryOp(const UnaryOpNode* node);
    std::unique_ptr<Value> evaluateFunction(const FunctionNode* node);

public:
    explicit Evaluator(bool exact = false);
    std::unique_ptr<Value> evaluate(const ASTNode* root);
    void setExactMode(bool exact);
    bool getExactMode() const;
};

#endif //CAPSTONE_STUFF_EVALUATOR_HPP