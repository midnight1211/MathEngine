// calculus/core/Week4Bridge.cpp
// Implements the week_4 → ExprPtr adapter.

#include "Week4Bridge.hpp"
#include <stdexcept>
#include <algorithm>

namespace Calculus
{

	// parse()
	// Runs the full week_4 pipeline then converts the result.

	ExprPtr Week4Bridge::parse(const std::string &input)
	{
		if (input.empty())
			throw std::invalid_argument("Cannot parse empty expression");

		std::string processed = preprocessInput(input);

		Lexer lexer(processed);
		std::vector<Token> tokens = lexer.tokenize(); // throws on unknown identifier

		Parser parser(tokens);
		std::unique_ptr<ASTNode> ast = parser.parse(); // throws on syntax error

		return fromASTNode(ast.get());
	}

	// evaluate()

	double Week4Bridge::evaluate(const ExprPtr &expr, const SymbolTable &syms)
	{
		return Calculus::evaluate(expr, syms);
	}

	// fromASTNode()
	// week_4 node types (from parser.hpp):
	//   ASTNodeType::NUMBER     → NumberNode   { double value }

	ExprPtr Week4Bridge::fromASTNode(const ASTNode *node)
	{
		if (!node)
			throw std::runtime_error("Week4Bridge::fromASTNode: null node");

		switch (node->type)
		{

		case ASTNodeType::NUMBER:
		{
			const auto *n = dynamic_cast<const NumberNode *>(node);
			if (!n)
				throw std::runtime_error("fromASTNode: bad NUMBER cast");
			return num(n->value);
		}

		case ASTNodeType::CONSTANT:
		{
			const auto *c = dynamic_cast<const ConstantNode *>(node);
			if (!c)
				throw std::runtime_error("fromASTNode: bad CONSTANT cast");
			switch (c->constant)
			{
			case SymbolicConstant::PI:
				return pi_expr();
			case SymbolicConstant::E:
				return e_expr();
			default:
				throw std::runtime_error("fromASTNode: unknown SymbolicConstant");
			}
		}

		case ASTNodeType::BINARY_OP:
		{
			const auto *b = dynamic_cast<const BinaryOpNode *>(node);
			if (!b)
				throw std::runtime_error("fromASTNode: bad BINARY_OP cast");

			ExprPtr left = fromASTNode(b->left.get());
			ExprPtr right = fromASTNode(b->right.get());

			if (b->op == "+")
				return add(left, right);
			if (b->op == "-")
				return sub(left, right);
			if (b->op == "*")
				return mul(left, right);
			if (b->op == "/")
				return div_expr(left, right);
			if (b->op == "^")
				return pow_expr(left, right);

			throw std::runtime_error("fromASTNode: unknown binary operator: " + b->op);
		}

		case ASTNodeType::UNARY_OP:
		{
			const auto *u = dynamic_cast<const UnaryOpNode *>(node);
			if (!u)
				throw std::runtime_error("fromASTNode: bad UNARY_OP cast");

			ExprPtr operand = fromASTNode(u->operand.get());

			if (u->op == "-")
				return neg(operand);
			if (u->op == "+")
				return operand; // unary plus is a no-op

			throw std::runtime_error("fromASTNode: unknown unary operator: " + u->op);
		}

		case ASTNodeType::FUNCTION:
		{
			const auto *f = dynamic_cast<const FunctionNode *>(node);
			if (!f)
				throw std::runtime_error("fromASTNode: bad FUNCTION cast");

			ExprPtr arg = fromASTNode(f->argument.get());
			return makeFunctionNode(f->name, arg);
		}

		default:
			throw std::runtime_error("fromASTNode: unhandled ASTNodeType");
		}
	}

	// makeFunctionNode()
	//   atanh, abs, floor, ceil, cbrt, sign, log2

	ExprPtr Week4Bridge::makeFunctionNode(const std::string &name, ExprPtr arg)
	{

		// Variables encoded by the lexer as "var_" + identifier
		if (name.rfind("var_", 0) == 0)
		{
			return var(name.substr(4)); // "var_x" → var("x"), arg is ignored
		}

		if (name == "sin")
			return sin_expr(arg);
		if (name == "cos")
			return cos_expr(arg);
		if (name == "tan")
			return tan_expr(arg);
		if (name == "asin" || name == "arcsin")
			return asin_expr(arg);
		if (name == "acos" || name == "arccos")
			return acos_expr(arg);
		if (name == "atan" || name == "arctan")
			return atan_expr(arg);
		if (name == "sqrt")
			return sqrt_expr(arg);
		if (name == "exp")
			return exp_expr(arg);

		// week_4 calls natural log "ln" and log base 10 "log"
		if (name == "ln")
			return log_expr(arg); // ln(x) = natural log
		if (name == "log")
			return log10_expr(arg); // log(x) = log base 10

		if (name == "sinh")
			return sinh_expr(arg);
		if (name == "cosh")
			return cosh_expr(arg);
		if (name == "tanh")
			return tanh_expr(arg);
		if (name == "asinh" || name == "arcsinh")
			return asinh_expr(arg);
		if (name == "acosh" || name == "arccosh")
			return acosh_expr(arg);
		if (name == "atanh" || name == "arctanh")
			return atanh_expr(arg);
		if (name == "cbrt")
			return cbrt_expr(arg);
		if (name == "abs")
			return abs_expr(arg);
		if (name == "log2")
			return log2_expr(arg);
		if (name == "sign" || name == "sgn")
			return std::make_shared<Expr>(ExprType::SIGN, arg);
		if (name == "floor")
			return std::make_shared<Expr>(ExprType::FLOOR, arg);
		if (name == "ceil")
			return std::make_shared<Expr>(ExprType::CEIL, arg);

		// Unknown — preserve symbolically so it doesn't blow up
		return std::make_shared<Expr>(ExprType::FUNC, name,
									  std::vector<ExprPtr>{arg});
	}

	// preprocessInput()
	// accidentally match inside some longer identifier.

	std::string Week4Bridge::preprocessInput(const std::string &input)
	{
		// and the function returns the input unchanged.
		// the recognised strings.
		// lexer workarounds. Lexer extension is the correct fix.

		std::string s = input;

		// Trim leading/trailing whitespace
		size_t start = s.find_first_not_of(" \t\r\n");
		size_t end = s.find_last_not_of(" \t\r\n");
		if (start == std::string::npos)
			return "";
		s = s.substr(start, end - start + 1);

		// Python-style ** → ^
		for (size_t i = 0; i + 1 < s.size(); ++i)
			if (s[i] == '*' && s[i + 1] == '*')
			{
				s[i] = '^';
				s.erase(i + 1, 1);
			}

		return s;
	}

} // namespace Calculus