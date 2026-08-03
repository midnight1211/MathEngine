// CoreEngine.cpp
// Routes: "calc:<op>|{json}"	-> Calculus
// 	   "la:<op>|{json}"	-> LinearAlgebra
// 	   "de:<op>|{json}"	-> DifferentialEquations
// 	   "stat:<op>|{json}"	-> Statistics
// 	   "matrix:..."		-> LinearAlgebra (legacy)
// 	   anything else        -> week_4 Lexer/Parser/Evaluator

#include "CoreEngine.hpp"
#include "Calculus/Calculus.hpp"
#include "Linear_Algebra/LA.hpp"
#include "DiffEq/DE.hpp"
#include "Statistics/Statistics.hpp"
#include "AppliedMath/AM.hpp"
#include "NumberTheory/NumberTheory.hpp"
#include "DiscreteMath/DM.hpp"
#include "ComplexAnalysis/CA.hpp"
#include "NumericalAnalysis/NA.hpp"
#include "AbstractAlgebra/AA.hpp"
#include "ProbabilityTheory/PT.hpp"
#include "Geometry/Geom.hpp"

#include "parser/include_week_four/lexer.hpp"
#include "parser/include_week_four/parser.hpp"
#include "parser/include_week_four/evaluator.hpp"
#include "parser/include_week_four/value.hpp"

#include <sstream>
#include <iomanip>
#include <stdexcept>

// Human-readable input pre-processor (converts "opname[args]" → "prefix:op|{json}")
// Defined in preprocess.cpp, declared here for use within the same namespace.

namespace CoreEngine
{

    static std::string formatDouble(double val)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(10) << val;
        std::string s = oss.str();
        if (s.find('.') != std::string::npos)
        {
            s.erase(s.find_last_not_of('0') + 1);
            if (s.back() == '.')
                s.pop_back();
        }
        return s;
    }

    // Split "prefix:op|payload" → {op, payload}
    static std::pair<std::string, std::string> splitOp(const std::string &s, size_t prefixLen)
    {
        size_t sep = s.find('|', prefixLen);
        if (sep == std::string::npos)
            throw std::runtime_error("Missing | separator");
        return {s.substr(prefixLen, sep - prefixLen), s.substr(sep + 1)};
    }

    // Forward declaration — defined in preprocess.cpp (same namespace, same library)
    std::string preprocessExpression(const std::string &rawExpr);

    std::string compute(const std::string &expression, PrecisionMode mode)
    {
        if (expression.empty())
            throw std::runtime_error("Empty expression");

        // Translate human-readable formats to internal prefix:op|{json} protocol
        const std::string expr = preprocessExpression(expression);

        bool exact = (mode == PrecisionMode::SYMBOLIC);

        // ── Calculus ──────────────────────────────────────────────────────────────
        if (expr.rfind("calc:", 0) == 0)
        {
            auto [op, json] = splitOp(expr, 5);
            auto r = Calculus::dispatch(op, json);
            if (!r.ok)
                throw std::runtime_error(r.error);
            if (!exact && !r.numeric.empty())
                return r.numeric;
            if (!r.numeric.empty() && r.numeric != r.symbolic)
                return r.symbolic + "  ~  " + r.numeric;
            return r.symbolic;
        }

        // ── Linear Algebra ────────────────────────────────────────────────────────
        if (expr.rfind("la:", 0) == 0 || expr.rfind("matrix:", 0) == 0)
        {
            size_t prefixLen = expr.rfind("la:", 0) == 0 ? 3 : 7;
            // For "matrix:" legacy format, treat entire thing as input to classify
            if (prefixLen == 7)
            {
                std::string mat = expr.substr(7);
                auto r = LinearAlgebra::dispatch("classify", mat, exact);
                if (!r.ok)
                    throw std::runtime_error(r.error);
                return r.format(exact);
            }
            auto [op, input] = splitOp(expr, prefixLen);
            auto r = LinearAlgebra::dispatch(op, input, exact);
            if (!r.ok)
                throw std::runtime_error(r.error);
            return r.format(exact);
        }

        // ── Differential Equations ────────────────────────────────────────────────
        if (expr.rfind("de:", 0) == 0)
        {
            auto [op, json] = splitOp(expr, 3);
            auto r = DifferentialEquations::dispatch(op, json, exact);
            if (!r.ok)
                throw std::runtime_error(r.error);
            return r.format(exact);
        }

        // ── Statistics ────────────────────────────────────────────────────────────
        if (expr.rfind("stat:", 0) == 0)
        {
            auto [op, json] = splitOp(expr, 5);
            auto r = Statistics::dispatch(op, json, exact);
            if (!r.ok)
                throw std::runtime_error(r.error);
            return r.value + (r.detail.empty() ? "" : "\n" + r.detail);
        }

        // ── Applied Mathematics (Logan) ──────────────────────────────────────────
        if (expr.rfind("am:", 0) == 0)
        {
            auto [op, json] = splitOp(expr, 3);
            auto r = AppliedMath::dispatch(op, json, exact);
            if (!r.ok)
                throw std::runtime_error(r.error);
            return r.format(exact);
        }

        // ── Number Theory ─────────────────────────────────────────────────────────
        if (expr.rfind("nt:", 0) == 0)
        {
            auto sep = expression.find('|', 3);
            std::string op = (sep == std::string::npos) ? expr.substr(3) : expr.substr(3, sep - 3);
            std::string json = (sep == std::string::npos) ? "{}" : expr.substr(sep + 1);
            auto r = NumberTheory::dispatch(op, json);
            if (!r.ok)
                throw std::runtime_error(r.error);
            return r.value + (r.detail.empty() ? "" : "\n\n" + r.detail);
        }

        // ── Discrete Mathematics ─────────────────────────────────────────────────
        if (expr.rfind("dm:", 0) == 0)
        {
            auto sep = expression.find('|', 3);
            std::string op = (sep == std::string::npos) ? expr.substr(3) : expr.substr(3, sep - 3);
            std::string json = (sep == std::string::npos) ? "{}" : expr.substr(sep + 1);
            auto r = DiscreteMath::dispatch(op, json);
            if (!r.ok)
                throw std::runtime_error(r.error);
            return r.value + (r.detail.empty() ? "" : "\n\n" + r.detail);
        }

        // ── Complex Analysis ──────────────────────────────────────────────────────
        if (expr.rfind("ca:", 0) == 0)
        {
            auto sep = expression.find('|', 3);
            std::string op = (sep == std::string::npos) ? expr.substr(3) : expr.substr(3, sep - 3);
            std::string json = (sep == std::string::npos) ? "{}" : expr.substr(sep + 1);
            auto r = ComplexAnalysis::dispatch(op, json);
            if (!r.ok)
                throw std::runtime_error(r.error);
            return r.value + (r.detail.empty() ? "" : "\n\n" + r.detail);
        }

        // ── Numerical Analysis ────────────────────────────────────────────────────
        if (expr.rfind("na:", 0) == 0)
        {
            auto sep = expression.find('|', 3);
            std::string op = (sep == std::string::npos) ? expr.substr(3) : expr.substr(3, sep - 3);
            std::string json = (sep == std::string::npos) ? "{}" : expr.substr(sep + 1);
            auto r = NumericalAnalysis::dispatch(op, json);
            if (!r.ok)
                throw std::runtime_error(r.error);
            return r.value + (r.detail.empty() ? "" : "\n\n" + r.detail);
        }

        // ── Abstract Algebra ──────────────────────────────────────────────────────
        if (expr.rfind("aa:", 0) == 0)
        {
            auto sep = expression.find('|', 3);
            std::string op = (sep == std::string::npos) ? expr.substr(3) : expr.substr(3, sep - 3);
            std::string json = (sep == std::string::npos) ? "{}" : expr.substr(sep + 1);
            auto r = AbstractAlgebra::dispatch(op, json);
            if (!r.ok)
                throw std::runtime_error(r.error);
            return r.value + (r.detail.empty() ? "" : "\n\n" + r.detail);
        }

        // ── Probability Theory ────────────────────────────────────────────────────
        if (expr.rfind("prob:", 0) == 0)
        {
            auto sep = expression.find('|', 5);
            std::string op = (sep == std::string::npos) ? expr.substr(5) : expr.substr(5, sep - 5);
            std::string json = (sep == std::string::npos) ? "{}" : expr.substr(sep + 1);
            auto r = ProbabilityTheory::dispatch(op, json);
            if (!r.ok)
                throw std::runtime_error(r.error);
            return r.value + (r.detail.empty() ? "" : "\n\n" + r.detail);
        }

        // ── Geometry ──────────────────────────────────────────────────────────────
        if (expr.rfind("geo:", 0) == 0)
        {
            auto sep = expression.find('|', 4);
            std::string op = (sep == std::string::npos) ? expr.substr(4) : expr.substr(4, sep - 4);
            std::string json = (sep == std::string::npos) ? "{}" : expr.substr(sep + 1);
            auto r = Geometry::dispatch(op, json);
            if (!r.ok)
                throw std::runtime_error(r.error);
            return r.value + (r.detail.empty() ? "" : "\n\n" + r.detail);
        }

        // ── week_4 scalar pipeline (Arithmetic mode) ────────────────────────────
        // This pipeline is for pure numeric expressions like "2+3*sin(pi/4)".
        // It does NOT handle symbolic variables — use the calc: prefix operations instead.
        try
        {
            Lexer lexer(expression);
            std::vector<Token> tokens = lexer.tokenize();
            Parser parser(tokens);
            std::unique_ptr<ASTNode> ast = parser.parse();
            Evaluator eval(exact);
            std::unique_ptr<Value> result = eval.evaluate(ast.get());

            if (!result)
                throw std::runtime_error("Arithmetic evaluation returned no result");

            if (exact && result->isSymbolic())
                return result->toString() + "  ~  " + formatDouble(result->evaluate());
            return formatDouble(result->evaluate());
        }
        catch (const std::exception &ex)
        {
            // Re-throw with context so the Java UI shows a clear error
            throw std::runtime_error(
                std::string("Arithmetic error: ") + ex.what());
        }
    }

    std::string getVersion()
    {
        return "MathEngine-Core v5.0 (Calculus + LinAlg + DiffEq + Stats + Applied + NT + DM + CA + NA + AA + Prob + Geo)";
    }

    extern "C" {
    // Exported function accessible by Python ctypes
    const char* c_evaluate(const char* input_expr) {
        static std::string result_str;
        if (!input_expr) return "";

        try {
            // Adjust this call to match your CoreEngine computation method
            // Based on your nm output: CoreEngine::compute(string, PrecisionMode)
            result_str = CoreEngine::compute(std::string(input_expr), CoreEngine::PrecisionMode::NUMERICAL);
        } catch (const std::exception& e) {
            result_str = std::string("Engine Error: ") + e.what();
        } catch (...) {
            result_str = "Unknown C++ engine error";
        }

        return result_str.c_str();
    }
}

} // namespace CoreEngine
