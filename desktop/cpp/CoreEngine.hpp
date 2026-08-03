#pragma once
// CoreEngine.hpp — public API of the C++ math engine.

#include <string>

namespace CoreEngine
{
	enum class PrecisionMode
	{
		SYMBOLIC = 0, // exact form: "pi", "sqrt(2)", "1/3"
		NUMERICAL = 1 // floating-point: "3.1415926536"
	};

	// Main entry point called from MathBridgeJNI.cpp.
	//
	// Input formats:
	//   Plain expression:  "sin(pi/2) + 5"         -> week_4 pipeline
	//   LaTeX expression:  "\int_0^8 x^2\,dx"      -> LaTeXPreprocessor → week_4
	//   Matrix operation:  "matrix:..."            -> LinearAlgebra module
	//   Linear algebra:    "la:<op>|<payload>"     -> LinearAlgebra module
	//   Calculus op:       "calc:<op>|<json>"      -> Calculus module
	//   DiffEq op:         "de:<op>|<json>"        -> DiffEq module
	//   Statistics:        "stat:<op>|<json>"      -> Statistics module
	//   Applied Math:      "am:<op>|<json>"        -> AppliedMath module
	//   Number Theory:     "nt:<op>|<json>"        -> NumberTheory module
	//   Discrete Math:     "dm:<op>|<json>"        -> DiscreteMath module
	//   Complex Analysis:  "ca:<op>|<json>"        -> ComplexAnalysis module
	//   Numerical Analysis:"na:<op>|<json>"        -> NumericalAnalysis module
	//   Abstract Algebra:  "aa:<op>|<json>"        -> AbstractAlgebra module
	//   Probability:       "prob:<op>|<json>"      -> ProbabilityTheory module
	//   Geometry:          "geo:<op>|<json>"       -> Geometry module
	//
	// LaTeX handling:
	//   Any non-prefixed expression that "looks like" LaTeX is first passed
	//   through LaTeXPreprocessor::toEngine(), then evaluated as a plain
	//   expression by the week_4 pipeline.
	//
	// Returns: result string for display.
	// Throws:  std::runtime_error on bad input or engine failure.
	std::string compute(const std::string &expression, PrecisionMode mode);

	// Human-readable version string.
	std::string getVersion();

} // namespace CoreEngine
