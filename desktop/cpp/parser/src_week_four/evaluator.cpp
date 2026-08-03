#include "../include_week_four/evaluator.hpp"
#include <cmath>
#include <stdexcept>

Evaluator::Evaluator(bool exact) : exactMode(exact) {}

void Evaluator::setExactMode(bool exact)
{
    exactMode = exact;
}

bool Evaluator::getExactMode() const
{
    return exactMode;
}

std::unique_ptr<Value> Evaluator::evaluate(const ASTNode *root)
{
    return evaluateNode(root);
}

std::unique_ptr<Value> Evaluator::evaluateNode(const ASTNode *node)
{
    if (!node)
    {
        throw std::runtime_error("Null node in AST");
    }

    switch (node->type)
    {
    case ASTNodeType::NUMBER:
    {
        const auto *numNode = dynamic_cast<const NumberNode *>(node);
        return std::make_unique<NumericValue>(numNode->value);
    }

    case ASTNodeType::CONSTANT:
    {
        const auto *constNode = dynamic_cast<const ConstantNode *>(node);
        if (exactMode)
        {
            return std::make_unique<SymbolicValue>(constNode->constant);
        }
        else
        {
            // Convert to numeric value
            SymbolicValue temp(constNode->constant);
            return std::make_unique<NumericValue>(temp.evaluate());
        }
    }

    case ASTNodeType::BINARY_OP:
    {
        const auto *b = dynamic_cast<const BinaryOpNode *>(node);
        if (!b)
            throw std::runtime_error("evaluateNode: BINARY_OP cast failed");
        return evaluateBinaryOp(b);
    }

    case ASTNodeType::UNARY_OP:
    {
        const auto *u = dynamic_cast<const UnaryOpNode *>(node);
        if (!u)
            throw std::runtime_error("evaluateNode: UNARY_OP cast failed");
        return evaluateUnaryOp(u);
    }

    case ASTNodeType::FUNCTION:
    {
        const auto *fn = dynamic_cast<const FunctionNode *>(node);
        if (!fn)
            throw std::runtime_error("evaluateNode: FUNCTION cast failed");
        return evaluateFunction(fn);
    }

    default:
        throw std::runtime_error("Unknown node type");
    }
}

std::unique_ptr<Value> Evaluator::evaluateBinaryOp(const BinaryOpNode *node)
{
    if (!node)
        throw std::runtime_error("evaluateBinaryOp: null node");
    if (!node->left)
        throw std::runtime_error("evaluateBinaryOp: null left child");
    if (!node->right)
        throw std::runtime_error("evaluateBinaryOp: null right child");
    auto left = evaluateNode(node->left.get());
    auto right = evaluateNode(node->right.get());

    // In exact mode, preserve symbolic expressions
    if (exactMode && (left->isSymbolic() || right->isSymbolic()))
    {
        std::string leftStr = left->toString();
        std::string rightStr = right->toString();
        double leftVal = left->evaluate();
        double rightVal = right->evaluate();
        double result;
        std::string exprStr;

        // Handle special case of coefficient * symbolic constant
        if (node->op == "*")
        {
            // Check if one is numeric and the other is a simple symbolic constant
            if (!left->isSymbolic() && dynamic_cast<SymbolicValue *>(right.get()))
            {
                auto symbolicRight = dynamic_cast<SymbolicValue *>(right.get());
                if (symbolicRight->getCoefficient() == 1.0)
                {
                    double coeff = leftVal;
                    return std::make_unique<SymbolicValue>(symbolicRight->getConstant(), coeff);
                }
            }
            else if (!right->isSymbolic() && dynamic_cast<SymbolicValue *>(left.get()))
            {
                auto symbolicLeft = dynamic_cast<SymbolicValue *>(left.get());
                if (symbolicLeft->getCoefficient() == 1.0)
                {
                    double coeff = rightVal;
                    return std::make_unique<SymbolicValue>(symbolicLeft->getConstant(), coeff);
                }
            }
        }

        // Build symbolic expression string
        bool needLeftParen = false;
        bool needRightParen = false;

        // Determine if we need parentheses
        if ((node->op == "*" || node->op == "/") &&
            (leftStr.find('+') != std::string::npos || leftStr.find('-') != std::string::npos))
        {
            needLeftParen = true;
        }
        if ((node->op == "*" || node->op == "/" || node->op == "^") &&
            (rightStr.find('+') != std::string::npos || rightStr.find('-') != std::string::npos))
        {
            needRightParen = true;
        }
        if (node->op == "^" &&
            (rightStr.find('*') != std::string::npos || rightStr.find('/') != std::string::npos))
        {
            needRightParen = true;
        }

        // Build expression
        if (needLeftParen)
            exprStr += "(" + leftStr + ")";
        else
            exprStr += leftStr;

        exprStr += " " + node->op + " ";

        if (needRightParen)
            exprStr += "(" + rightStr + ")";
        else
            exprStr += rightStr;

        // Calculate result
        if (node->op == "+")
        {
            result = leftVal + rightVal;
        }
        else if (node->op == "-")
        {
            result = leftVal - rightVal;
        }
        else if (node->op == "*")
        {
            result = leftVal * rightVal;
        }
        else if (node->op == "/")
        {
            if (rightVal == 0.0)
            {
                throw std::runtime_error("Division by zero");
            }
            result = leftVal / rightVal;
        }
        else if (node->op == "^")
        {
            result = std::pow(leftVal, rightVal);
        }
        else
        {
            throw std::runtime_error("Unknown operator: " + node->op);
        }

        return std::make_unique<SymbolicExpression>(exprStr, result);
    }

    // For approximate mode or no symbolic values, evaluate to numeric
    double leftVal = left->evaluate();
    double rightVal = right->evaluate();
    double result;

    if (node->op == "+")
    {
        result = leftVal + rightVal;
    }
    else if (node->op == "-")
    {
        result = leftVal - rightVal;
    }
    else if (node->op == "*")
    {
        result = leftVal * rightVal;
    }
    else if (node->op == "/")
    {
        if (rightVal == 0.0)
        {
            throw std::runtime_error("Division by zero");
        }
        result = leftVal / rightVal;
    }
    else if (node->op == "^")
    {
        result = std::pow(leftVal, rightVal);
    }
    else
    {
        throw std::runtime_error("Unknown operator: " + node->op);
    }

    return std::make_unique<NumericValue>(result);
}

std::unique_ptr<Value> Evaluator::evaluateUnaryOp(const UnaryOpNode *node)
{
    if (!node)
        throw std::runtime_error("evaluateUnaryOp: null node");
    if (!node->operand)
        throw std::runtime_error("evaluateUnaryOp: null operand");
    auto operand = evaluateNode(node->operand.get());
    double value = operand->evaluate();

    if (node->op == "-")
    {
        return std::make_unique<NumericValue>(-value);
    }
    else if (node->op == "+")
    {
        return std::make_unique<NumericValue>(value);
    }
    else
    {
        throw std::runtime_error("Unknown unary operator: " + node->op);
    }
}

std::unique_ptr<Value> Evaluator::evaluateFunction(const FunctionNode *node)
{
    if (!node)
        throw std::runtime_error("evaluateFunction: null node");
    if (!node->argument)
        throw std::runtime_error("evaluateFunction: null argument");

    // Variable placeholder tokens produced by the lexer (e.g. "var_x").
    // In ARITHMETIC mode variables have no numeric value — treat as 0
    // and emit a clear error so the user knows to use a numeric expression.
    if (node->name.rfind("var_", 0) == 0)
    {
        std::string varName = node->name.substr(4);
        throw std::runtime_error(
            "Variable '" + varName + "' has no numeric value in Arithmetic mode. "
                                     "Use a numeric expression, or switch to the correct operation (e.g. Derivative, Integral).");
    }

    auto argument = evaluateNode(node->argument.get());
    double argValue = argument->evaluate();
    double result;

    if (node->name == "sin")
    {
        result = std::sin(argValue);
    }
    else if (node->name == "cos")
    {
        result = std::cos(argValue);
    }
    else if (node->name == "tan")
    {
        result = std::tan(argValue);
    }
    else if (node->name == "asin")
    {
        if (argValue < -1.0 || argValue > 1.0)
        {
            throw std::runtime_error("asin argument out of range [-1, 1]");
        }
        result = std::asin(argValue);
    }
    else if (node->name == "acos")
    {
        if (argValue < -1.0 || argValue > 1.0)
        {
            throw std::runtime_error("acos argument out of range [-1, 1]");
        }
        result = std::acos(argValue);
    }
    else if (node->name == "atan")
    {
        result = std::atan(argValue);
    }
    else if (node->name == "log")
    {
        if (argValue <= 0.0)
        {
            throw std::runtime_error("log argument must be positive");
        }
        result = std::log10(argValue);
    }
    else if (node->name == "ln")
    {
        if (argValue <= 0.0)
        {
            throw std::runtime_error("ln argument must be positive");
        }
        result = std::log(argValue);
    }
    else if (node->name == "sqrt")
    {
        if (argValue < 0.0)
        {
            throw std::runtime_error("sqrt argument must be non-negative");
        }
        result = std::sqrt(argValue);
    }
    else if (node->name == "exp")
    {
        result = std::exp(argValue);
    }
    else
    {
        throw std::runtime_error("Unknown function: " + node->name);
    }

    // In exact mode, preserve symbolic function expressions
    if (exactMode && argument->isSymbolic())
    {
        std::string exprStr = node->name + "(" + argument->toString() + ")";
        return std::make_unique<SymbolicExpression>(exprStr, result);
    }

    return std::make_unique<NumericValue>(result);
}