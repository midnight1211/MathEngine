#pragma once
// =============================================================================
// calculus/core/Week4Bridge.hpp
//
// Connects the week_4 Lexer/Parser/Evaluator to the calculus module's
// ExprPtr tree, so we don't maintain two parsers.
//
// The bridge does two things:
//
//     1. parse(string) --> ExprPtr
//        Runs:  string -> Lexer -> tokens -> Parser -> ASTNode* -> ExprPtr
//        The ASTNode* tree from week_4 is walked once and converted into
//        the calculus ExprPtr tree. The ASTNode tree is then discarded.
//
//     2. evaluate(ExprPtr, SymbolTable) -> double
//        Walks the ExprPtr tree directly (fast, no round-trip through week_4).
//        Week_4's Evaluator is only used during the parse step - after that,
//        the ExprPtr tree is self-contained.
//
// Why this approach instead of calling week_4's Evaluator directly:
//   The calculus module needs a mutable, traversable tree for differentiation,
//   simplification, and substitution. week_4's ASTNode uses unique_ptr and
//   doesn't support the operations we need. So we convert once at parse time
//   and work with ExprPtr from that point on.
//
// What week_4 currently supports (from reading the source):
//   Operators:  + - * / ^
//   Functions:  sin, cos, tan, asin, acos, atan, log (log10), ln, sqrt, exp
//   Constants:  pi, e
//   Nodes:      NumberNode, ConstantNode, BinaryOpNode, UnaryOpNode, FunctionNode
//
// Anything the calculus module needs beyond that (sinh, cosh, tanh, abs, etc.)
// is handled by extending the lexer's identifier list — see extendLexer() below.
// =============================================================================

#ifndef WEEK4BRIDGE_HPP
#define WEEK4BRIDGE_HPP

#include "Expression.hpp"

// week_4 headers - paths use the project-relative location
#include "../../parser/include_week_four/lexer.hpp"
#include "../../parser/include_week_four/parser.hpp"
#include "../../parser/include_week_four/evaluator.hpp"

#include <string>
#include <memory>

namespace Calculus
{

	// =============================================================================
	// Week4Bridge
	//
	// All methods are static — the bridge has no state of its own.
	// =============================================================================

	class Week4Bridge
	{
	public:
		// ── Primary interface ─────────────────────────────────────────────────────

		// Parse a string into an ExprPtr using the week_4 Lexer and Parser.
		// Throws std::invalid_argument on syntax error (forwarded from week_4).
		//
		// The week_4 parser only knows about the functions listed in lexer.cpp.
		// Before tokenizing, we pre-process the string to expand function names
		// that week_4 doesn't recognize (sinh -> __sinh__, etc.) so the lexer
		// doesn't throw "Unknown identifier". After conversion we map them back.
		static ExprPtr parse(const std::string &input);

		// Evaluate an ExprPtr tree to a double.
		// Uses the Expression.cpp evaluate() directly - week_4 is not involved.
		static double evaluate(const ExprPtr &expr, const SymbolTable &syms = {});

		// ── Conversion ────────────────────────────────────────────────────────────

		// Convet a week_4 ASTNode* tree into an ExprPtr tree.
		// This is the core of the bridge. Called internally by parse().
		// Exposed publicly so you can also pass in an ASTNode*.
		static ExprPtr fromASTNode(const ASTNode *node);

	private:
		// Pre-process input string to make it safe for the week_4 lexer.
		// Expands function names week_4 doesn't know about into temporary tokens,
		// then fromASTNode() maps the FUNC nodes back to the right ExprType.
		//
		// Current expansions (week_4 lexer throws on these identifiers):
		//   sinh   → recognised via FUNC node name "sinh"   (if lexer is extended)
		//   cosh   → recognised via FUNC node name "cosh"
		//   tanh   → recognised via FUNC node name "tanh"
		//   abs    → recognised via FUNC node name "abs"
		//   floor  → recognised via FUNC node name "floor"
		//   ceil   → recognised via FUNC node name "ceil"
		//   cbrt   → recognised via FUNC node name "cbrt"
		//
		// NOTE: The cleanest long-term fix is to add these to the lexer's
		// readIdentifier() function. See the comment block at the bottom of
		// this file for the exact lines to add.
		static std::string preprocessInput(const std::string &input);

		// Map a FunctionNode name to the correct ExprType.
		// Handles both week_4's built-in names and any extras added via extension.
		static ExprPtr makeFunctionNode(const std::string &name, ExprPtr arg);
	};

}

#endif // WEEK4BRIDGE_HPP