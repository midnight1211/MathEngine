#ifndef VALUE_HPP
#define VALUE_HPP

#include <string>
#include <memory>
#include <cmath>
#include <sstream>
#include <utility>

// Cross-platform math constants (corecrt_math_defines.h is MSVC-only)
#ifndef M_PI
#  define M_PI    3.14159265358979323846
#endif
#ifndef M_E
#  define M_E     2.71828182845904523536
#endif
#ifndef M_SQRT2
#  define M_SQRT2 1.41421356237309504880
#endif

// Abstract base class for all value types
class Value {
public:
    virtual ~Value() = default;

    // Pure virtual methods that must be implemented by derived classes
    [[nodiscard]] virtual double evaluate() const = 0;
    [[nodiscard]] virtual std::string toString() const = 0;
    [[nodiscard]] virtual bool isSymbolic() const = 0;
    [[nodiscard]] virtual std::unique_ptr<Value> clone() const = 0;
};

// Numeric value class for standard floating-point results
class NumericValue : public Value {
private:
    double value;

public:
    explicit NumericValue(double val) : value(val) {}

    [[nodiscard]] double evaluate() const override {
        return value;
    }

    [[nodiscard]] std::string toString() const override {
        return std::to_string(value);
    }

    [[nodiscard]] bool isSymbolic() const override {
        return false;
    }

    [[nodiscard]] std::unique_ptr<Value> clone() const override {
        return std::make_unique<NumericValue>(value);
    }

    [[nodiscard]] double getValue() const {
        return value;
    }
};

// Enumeration for symbolic constants
enum class SymbolicConstant {
    PI,
    E,
    UNKNOWN
};

// Symbolic value class for exact mathematical representations
class SymbolicValue : public Value {
private:
    SymbolicConstant constant;
    double coefficient;  // For expressions like 2*pi

public:
    explicit SymbolicValue(SymbolicConstant sym, double coeff = 1.0)
        : constant(sym), coefficient(coeff) {}

    [[nodiscard]] double evaluate() const override {
        double baseValue;
        switch (constant) {
            case SymbolicConstant::PI:
                baseValue = M_PI;
                break;
            case SymbolicConstant::E:
                baseValue = M_E;
                break;
            default:
                baseValue = 0.0;
        }
        return coefficient * baseValue;
    }

    [[nodiscard]] std::string toString() const override {
        std::string result;

        if (coefficient != 1.0) {
            // Check if coefficient is a whole number
            if (coefficient == static_cast<int>(coefficient)) {
                result = std::to_string(static_cast<int>(coefficient));
            } else {
                result = std::to_string(coefficient);
            }
        }

        switch (constant) {
            case SymbolicConstant::PI:
                result += "π";
                break;
            case SymbolicConstant::E:
                result += "e";
                break;
            default:
                result += "unknown";
        }

        return result;
    }

    [[nodiscard]] bool isSymbolic() const override {
        return true;
    }

    [[nodiscard]] std::unique_ptr<Value> clone() const override {
        return std::make_unique<SymbolicValue>(constant, coefficient);
    }

    [[nodiscard]] SymbolicConstant getConstant() const {
        return constant;
    }

    [[nodiscard]] double getCoefficient() const {
        return coefficient;
    }
};

// Symbolic expression class for compound expressions
class SymbolicExpression : public Value {
private:
    std::string expression;
    double cachedValue;

public:
    SymbolicExpression(std::string  expr, double val)
        : expression(std::move(expr)), cachedValue(val) {}

    [[nodiscard]] double evaluate() const override {
        return cachedValue;
    }

    [[nodiscard]] std::string toString() const override {
        return expression;
    }

    [[nodiscard]] bool isSymbolic() const override {
        return true;
    }

    [[nodiscard]] std::unique_ptr<Value> clone() const override {
        return std::make_unique<SymbolicExpression>(expression, cachedValue);
    }
};

#endif // VALUE_HPP
