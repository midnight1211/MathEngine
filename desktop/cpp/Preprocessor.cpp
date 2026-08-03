// ── CoreEngine pre-processor ─────────────────────────────────────────────────
// Converts human-readable input strings into the internal prefix:op|{json}
// format so the week_4 lexer never sees symbolic/multi-arg expressions.
// Every bug involving "Undefined variable", "Unexpected character/token",
// "Unknown operation", wrong routing, or raw positional args is fixed here.
// ─────────────────────────────────────────────────────────────────────────────

#include <string>
#include <vector>
#include <cctype>
#include <algorithm>
#include <stdexcept>

namespace CoreEngine
{

    // ── String helpers ────────────────────────────────────────────────────────────

    static std::string pp_trim(const std::string &s)
    {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos)
            return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    static std::string pp_lower(std::string s)
    {
        for (char &c : s)
            c = (char)std::tolower((unsigned char)c);
        return s;
    }

    // Escape a raw string value for embedding as a JSON string value.
    static std::string jv(const std::string &s)
    {
        std::string out = "\"";
        for (char c : s)
        {
            if (c == '"')
                out += "\\\"";
            else if (c == '\\')
                out += "\\\\";
            else
                out += c;
        }
        out += '"';
        return out;
    }

    // Is the token a plain number (integer or decimal, optional leading minus)?
    static bool isNum(const std::string &s)
    {
        if (s.empty())
            return false;
        size_t i = 0;
        if (s[i] == '-' || s[i] == '+')
            ++i;
        bool d = false;
        while (i < s.size() && std::isdigit((unsigned char)s[i]))
        {
            ++i;
            d = true;
        }
        if (i < s.size() && s[i] == '.')
        {
            ++i;
            while (i < s.size() && std::isdigit((unsigned char)s[i]))
            {
                ++i;
                d = true;
            }
        }
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E'))
        {
            ++i;
            if (i < s.size() && (s[i] == '+' || s[i] == '-'))
                ++i;
            while (i < s.size() && std::isdigit((unsigned char)s[i]))
                ++i;
        }
        return d && i == s.size();
    }

    // Is the token a symbolic constant or number?
    static bool isNumOrConst(const std::string &s)
    {
        if (isNum(s))
            return true;
        std::string l = pp_lower(s);
        return l == "pi" || l == "e" || l == "inf" || l == "-inf" || l == "infinity";
    }

    // Is the token a simple variable name (single or multi-letter identifier)?
    static bool isVarName(const std::string &s)
    {
        if (s.empty())
            return false;
        if (!std::isalpha((unsigned char)s[0]) && s[0] != '_')
            return false;
        for (char c : s)
            if (!std::isalnum((unsigned char)c) && c != '_')
                return false;
        // Exclude known function/constant names that look like vars
        std::string l = pp_lower(s);
        static const char *funcs[] = {
            "sin", "cos", "tan", "exp", "log", "ln", "sqrt", "abs", "floor", "ceil",
            "asin", "acos", "atan", "sinh", "cosh", "tanh", "pi", "inf", "infinity",
            "e", "tau", "phi", nullptr};
        for (int i = 0; funcs[i]; ++i)
            if (l == funcs[i])
                return false;
        return true;
    }

    // ── Bracket-aware arg splitter ────────────────────────────────────────────────
    // Splits `s` on `delim` only when bracket depth is 0.
    static std::vector<std::string> splitArgs(const std::string &s, char delim = ',')
    {
        std::vector<std::string> res;
        int depth = 0;
        std::string cur;
        for (char c : s)
        {
            if (c == '(' || c == '[' || c == '{')
                ++depth;
            else if (c == ')' || c == ']' || c == '}')
                --depth;
            if (c == delim && depth == 0)
            {
                res.push_back(pp_trim(cur));
                cur.clear();
            }
            else
            {
                cur += c;
            }
        }
        res.push_back(pp_trim(cur));
        // Remove empty trailing entry
        while (!res.empty() && res.back().empty())
            res.pop_back();
        return res;
    }

    // ── Extract inner content of the outermost bracket pair ──────────────────────
    // "gradient[x^2+y^2, x, y]" → returns "x^2+y^2, x, y" and sets op="gradient"
    static bool extractBracketOp(const std::string &e,
                                 std::string &opOut,
                                 std::string &innerOut)
    {
        size_t bOpen = e.find('[');
        size_t pOpen = e.find('(');
        size_t open = std::string::npos;
        char closeChar = ']';
        if (bOpen != std::string::npos && (pOpen == std::string::npos || bOpen < pOpen))
        {
            open = bOpen;
            closeChar = ']';
        }
        else if (pOpen != std::string::npos)
        {
            open = pOpen;
            closeChar = ')';
        }
        if (open == std::string::npos)
            return false;

        std::string opRaw = pp_trim(e.substr(0, open));
        if (opRaw.empty())
            return false;

        // Find matching close bracket
        int depth = 0;
        size_t close = std::string::npos;
        for (size_t i = open; i < e.size(); ++i)
        {
            if (e[i] == '[' || e[i] == '(')
                ++depth;
            else if (e[i] == ']' || e[i] == ')')
            {
                --depth;
                if (depth == 0)
                {
                    close = i;
                    break;
                }
            }
        }
        innerOut = (close != std::string::npos)
                       ? e.substr(open + 1, close - open - 1)
                       : e.substr(open + 1);

        // Normalize op name: lower-case, spaces→underscores
        opOut = pp_lower(opRaw);
        for (char &c : opOut)
            if (c == ' ')
                c = '_';
        return true;
    }

    // ── Build JSON helpers ────────────────────────────────────────────────────────
    // Detect variables in an expression (single-letter identifiers not in func names)
    static std::string firstVar(const std::string &expr)
    {
        // Walk the expression and return the first bare identifier that looks like a var
        std::string id;
        for (size_t i = 0; i <= expr.size(); ++i)
        {
            char c = (i < expr.size()) ? expr[i] : 0;
            if (std::isalpha((unsigned char)c) || c == '_')
            {
                id += c;
            }
            else
            {
                if (!id.empty() && isVarName(id))
                    return id;
                id.clear();
            }
        }
        return "x"; // fallback
    }

    // Build JSON array of strings: ["a","b","c"]
    static std::string jsonStrArr(const std::vector<std::string> &v)
    {
        std::string s = "[";
        for (size_t i = 0; i < v.size(); ++i)
        {
            if (i)
                s += ",";
            s += jv(v[i]);
        }
        return s + "]";
    }

    // ── Main pre-processor ────────────────────────────────────────────────────────

    std::string preprocessExpression(const std::string &rawExpr)
    {
        // ── Pass-through: already has a routing prefix ────────────────────────────
        static const char *prefixes[] = {
            "calc:", "la:", "de:", "stat:", "am:", "nt:", "dm:",
            "ca:", "na:", "aa:", "prob:", "geo:", "matrix:", nullptr};
        for (int i = 0; prefixes[i]; ++i)
            if (rawExpr.rfind(prefixes[i], 0) == 0)
                return rawExpr;

        std::string e = pp_trim(rawExpr);

        // ── "expr as var->point[+/-]" limit syntax ────────────────────────────────
        {
            size_t asPos = e.find(" as ");
            if (asPos != std::string::npos)
            {
                size_t arrowPos = e.find("->", asPos);
                if (arrowPos != std::string::npos)
                {
                    std::string f = pp_trim(e.substr(0, asPos));
                    std::string var = pp_trim(e.substr(asPos + 4, arrowPos - asPos - 4));
                    std::string rest = pp_trim(e.substr(arrowPos + 2));
                    std::string op = "limit";
                    std::string pt = rest;
                    if (!pt.empty() && pt.back() == '+')
                    {
                        op = "limit_right";
                        pt.pop_back();
                        pt = pp_trim(pt);
                    }
                    else if (!pt.empty() && pt.back() == '-')
                    {
                        op = "limit_left";
                        pt.pop_back();
                        pt = pp_trim(pt);
                    }
                    std::string pl = pp_lower(pt);
                    if (pl == "inf" || pl == "infinity" || pl == "+inf")
                        op = "limit_inf";
                    else if (pl == "-inf" || pl == "-infinity")
                        op = "limit_neginf";
                    return "calc:" + op + "|{\"expr\":" + jv(f) + ",\"var\":" + jv(var) + ",\"point\":" + jv(pt) + "}";
                }
            }
        }

        // ── "expr, a, b"  →  numerical_int (plot/integrate request) ─────────────
        // Pattern: no op name before bracket, just "expression, number, number"
        {
            auto topParts = splitArgs(e, ',');
            if (topParts.size() == 3 &&
                !topParts[0].empty() &&
                isNumOrConst(topParts[1]) &&
                isNumOrConst(topParts[2]))
            {
                // Looks like "f(x), a, b" — numerical integration
                std::string var = firstVar(topParts[0]);
                return "calc:numerical_int|{\"expr\":" + jv(topParts[0]) + ",\"var\":" + jv(var) + ",\"a\":" + topParts[1] + ",\"b\":" + topParts[2] + ",\"method\":\"romberg\",\"n\":1000}";
            }
        }

        // ── "d/dx expr" and "d2/dx2 expr" shorthand ───────────────────────────────
        {
            std::string el = pp_lower(e);
            // d2/dx2 or d^2/dx^2
            size_t d2pos = el.find("d2/dx2");
            if (d2pos == std::string::npos)
                d2pos = el.find("d^2/dx^2");
            if (d2pos != std::string::npos)
            {
                // Extract var from denominator and expression after the pattern
                // Simplified: assume var=x for now; expression is the rest
                std::string rest = pp_trim(e.substr(d2pos + (el[d2pos + 1] == '2' ? 6 : 8)));
                // Strip surrounding [ ] if present
                if (!rest.empty() && rest.front() == '[')
                {
                    auto cl = rest.find(']');
                    if (cl != std::string::npos)
                        rest = pp_trim(rest.substr(1, cl - 1));
                }
                return "calc:diff|{\"expr\":" + jv(rest) + ",\"var\":\"x\",\"order\":2}";
            }
            size_t d1pos = el.find("d/dx");
            if (d1pos != std::string::npos)
            {
                std::string rest = pp_trim(e.substr(d1pos + 4));
                if (!rest.empty() && rest.front() == '[')
                {
                    auto cl = rest.find(']');
                    if (cl != std::string::npos)
                        rest = pp_trim(rest.substr(1, cl - 1));
                }
                return "calc:diff|{\"expr\":" + jv(rest) + ",\"var\":\"x\",\"order\":1}";
            }
        }

        // ── Colon-syntax shortcuts: "saddle:expr" and "gradient_descent:expr,a,b" ─
        // These are typed directly by users without bracket notation.
        {
            auto tryColonOp = [&](const std::string &prefix, const std::string &mappedOp) -> std::string {
                if (e.size() > prefix.size() && e.substr(0, prefix.size()) == prefix) {
                    std::string rest = pp_trim(e.substr(prefix.size()));
                    auto parts = splitArgs(rest, ',');
                    std::string expr_ = parts.size() > 0 ? parts[0] : "";
                    std::vector<std::string> varsList;
                    for (size_t i = 1; i < parts.size(); ++i)
                        if (isVarName(parts[i]))
                            varsList.push_back(parts[i]);
                    if (varsList.empty()) {
                        std::string v1 = firstVar(expr_);
                        varsList = {v1};
                        std::string tmp = expr_;
                        for (char &c : tmp) if (c == v1[0]) c = ' ';
                        std::string v2 = firstVar(tmp);
                        if (v2 != v1 && !v2.empty()) varsList.push_back(v2);
                    }
                    return "calc:optimize_nd|{\"expr\":" + jv(expr_) + ",\"vars\":" + jsonStrArr(varsList) + ",\"min\":[-10,-10],\"max\":[10,10]}";
                }
                return "";
            };
            std::string res;
            if (!(res = tryColonOp("saddle:", "optimize_nd")).empty()) return res;
            if (!(res = tryColonOp("gradient_descent:", "optimize_nd")).empty()) return res;
            if (!(res = tryColonOp("critical:", "optimize_nd")).empty()) return res;
        }

        // ── Extract "opname[inner]" pattern ───────────────────────────────────────
        std::string op, inner;
        if (!extractBracketOp(e, op, inner))
            return e; // no bracket → pass through

        auto pipeArgs = splitArgs(inner, '|');
        auto commaArgs = splitArgs(inner, ',');

        // ═══════════════════════════════════════════════════════════════════════════
        // CALCULUS OPERATIONS
        // ═══════════════════════════════════════════════════════════════════════════

        // ── diff / differentiate ─────────────────────────────────────────────────
        if (op == "diff" || op == "differentiate" || op == "derivative")
        {
            // diff[expr, var]  or  diff[expr, var, order]
            std::string expr_ = commaArgs.size() > 0 ? commaArgs[0] : "";
            std::string var_ = commaArgs.size() > 1 ? commaArgs[1] : firstVar(expr_);
            int order = (commaArgs.size() > 2) ? std::stoi(commaArgs[2]) : 1;
            return "calc:diff|{\"expr\":" + jv(expr_) + ",\"var\":" + jv(var_) + ",\"order\":" + std::to_string(order) + "}";
        }

        // ── partial ──────────────────────────────────────────────────────────────
        if (op == "partial")
        {
            std::string expr_ = commaArgs.size() > 0 ? commaArgs[0] : "";
            std::string var_ = commaArgs.size() > 1 ? commaArgs[1] : "x";
            int order = (commaArgs.size() > 2) ? std::stoi(commaArgs[2]) : 1;
            return "calc:partial|{\"expr\":" + jv(expr_) + ",\"var\":" + jv(var_) + ",\"order\":" + std::to_string(order) + "}";
        }

        // ── gradient ─────────────────────────────────────────────────────────────
        if (op == "gradient" || op == "grad")
        {
            // gradient[expr, var1, var2, ...]
            if (commaArgs.size() < 2)
                return e;
            std::string expr_ = commaArgs[0];
            std::vector<std::string> vars(commaArgs.begin() + 1, commaArgs.end());
            return "calc:gradient|{\"expr\":" + jv(expr_) + ",\"vars\":" + jsonStrArr(vars) + "}";
        }

        // ── jacobian ─────────────────────────────────────────────────────────────
        if (op == "jacobian")
        {
            // jacobian[f1, f2, ..., var1, var2, ...]
            // Heuristic: scan from end, collect items that look like variable names
            if (commaArgs.size() < 2)
                return e;
            std::vector<std::string> vars, exprs;
            size_t splitPoint = commaArgs.size();
            for (int i = (int)commaArgs.size() - 1; i >= 0; --i)
            {
                if (isVarName(commaArgs[i]) && commaArgs[i].size() <= 2)
                    splitPoint = i;
                else
                    break;
            }
            exprs = std::vector<std::string>(commaArgs.begin(), commaArgs.begin() + splitPoint);
            vars = std::vector<std::string>(commaArgs.begin() + splitPoint, commaArgs.end());
            if (vars.empty())
            {
                vars = {exprs.back()};
                exprs.pop_back();
            }
            return "calc:jacobian|{\"exprs\":" + jsonStrArr(exprs) + ",\"vars\":" + jsonStrArr(vars) + "}";
        }

        // ── hessian ──────────────────────────────────────────────────────────────
        if (op == "hessian")
        {
            std::string expr_ = commaArgs.size() > 0 ? commaArgs[0] : "";
            std::vector<std::string> vars(commaArgs.begin() + 1, commaArgs.end());
            if (vars.empty())
                vars = {"x", "y"};
            return "calc:hessian|{\"expr\":" + jv(expr_) + ",\"vars\":" + jsonStrArr(vars) + "}";
        }

        // ── laplacian ────────────────────────────────────────────────────────────
        if (op == "laplacian")
        {
            std::string expr_ = commaArgs.size() > 0 ? commaArgs[0] : "";
            std::vector<std::string> vars(commaArgs.begin() + 1, commaArgs.end());
            if (vars.empty())
                vars = {"x", "y"};
            return "calc:laplacian|{\"expr\":" + jv(expr_) + ",\"vars\":" + jsonStrArr(vars) + "}";
        }

        // ── limit ────────────────────────────────────────────────────────────────
        if (op == "limit" || op == "lim")
        {
            // limit[expr, var, point]  or  limit[expr, var, point, direction]
            std::string f = commaArgs.size() > 0 ? commaArgs[0] : "";
            std::string var = commaArgs.size() > 1 ? commaArgs[1] : "x";
            std::string pt = commaArgs.size() > 2 ? commaArgs[2] : "0";
            std::string dir = commaArgs.size() > 3 ? pp_lower(commaArgs[3]) : "both";
            std::string limOp = "limit";
            if (dir == "left" || dir == "-")
                limOp = "limit_left";
            else if (dir == "right" || dir == "+")
                limOp = "limit_right";
            else if (dir == "inf" || dir == "+inf")
                limOp = "limit_inf";
            else if (dir == "-inf")
                limOp = "limit_neginf";
            return "calc:" + limOp + "|{\"expr\":" + jv(f) + ",\"var\":" + jv(var) + ",\"point\":" + jv(pt) + "}";
        }

        // ── integrate / indefinite integral ──────────────────────────────────────
        if (op == "integrate" || op == "antiderivative" || op == "indefinite_int")
        {
            std::string expr_ = commaArgs.size() > 0 ? commaArgs[0] : "";
            std::string var_ = commaArgs.size() > 1 ? commaArgs[1] : firstVar(expr_);
            return "calc:integrate|{\"expr\":" + jv(expr_) + ",\"var\":" + jv(var_) + "}";
        }

        // ── definite_int → always go numerical ───────────────────────────────────
        if (op == "definite_int" || op == "definite")
        {
            std::string expr_ = commaArgs.size() > 0 ? commaArgs[0] : "";
            std::string var_ = commaArgs.size() > 1 ? commaArgs[1] : firstVar(expr_);
            std::string a = commaArgs.size() > 2 ? commaArgs[2] : "0";
            std::string b = commaArgs.size() > 3 ? commaArgs[3] : "1";
            return "calc:numerical_int|{\"expr\":" + jv(expr_) + ",\"var\":" + jv(var_) + ",\"a\":" + a + ",\"b\":" + b + ",\"method\":\"romberg\",\"n\":1000}";
        }

        // ── numerical_int / romberg (handles | separator) ─────────────────────────
        if (op == "numerical_int" || op == "romberg" || op == "simpson" || op == "trapezoid")
        {
            // Format: expr|var|a|b  or  expr, var, a, b
            std::vector<std::string> args = (pipeArgs.size() >= 3) ? pipeArgs : commaArgs;
            std::string expr_ = args.size() > 0 ? args[0] : "";
            std::string var_ = args.size() > 1 ? args[1] : firstVar(expr_);
            std::string a = args.size() > 2 ? args[2] : "0";
            std::string b = args.size() > 3 ? args[3] : "1";
            std::string meth = (op == "romberg") ? "romberg" : (op == "simpson") ? "simpson"
                                                                                 : "romberg";
            return "calc:numerical_int|{\"expr\":" + jv(expr_) + ",\"var\":" + jv(var_) + ",\"a\":" + a + ",\"b\":" + b + ",\"method\":\"" + meth + "\",\"n\":1000}";
        }

        // ── double integral ───────────────────────────────────────────────────────
        if (op == "double" || op == "double_int" || op == "dblint")
        {
            // double[expr, varX, varY, ax, bx, ay, by]
            std::string expr_ = commaArgs.size() > 0 ? commaArgs[0] : "";
            std::string vX = commaArgs.size() > 1 ? commaArgs[1] : "x";
            std::string vY = commaArgs.size() > 2 ? commaArgs[2] : "y";
            std::string ax = commaArgs.size() > 3 ? commaArgs[3] : "0";
            std::string bx = commaArgs.size() > 4 ? commaArgs[4] : "1";
            std::string ay = commaArgs.size() > 5 ? commaArgs[5] : "0";
            std::string by = commaArgs.size() > 6 ? commaArgs[6] : "1";
            return "calc:double_int|{\"expr\":" + jv(expr_) + ",\"varX\":" + jv(vX) + ",\"varY\":" + jv(vY) + ",\"ax\":" + ax + ",\"bx\":" + bx + ",\"ay\":" + ay + ",\"by\":" + by + "}";
        }

        // ── triple integral ───────────────────────────────────────────────────────
        if (op == "triple" || op == "triple_int")
        {
            // triple[expr, vX, vY, vZ, ax, bx, ay, by, az, bz]
            std::string expr_ = commaArgs.size() > 0 ? commaArgs[0] : "";
            std::string vX = commaArgs.size() > 1 ? commaArgs[1] : "x";
            std::string vY = commaArgs.size() > 2 ? commaArgs[2] : "y";
            std::string vZ = commaArgs.size() > 3 ? commaArgs[3] : "z";
            std::string ax = commaArgs.size() > 4 ? commaArgs[4] : "0";
            std::string bx = commaArgs.size() > 5 ? commaArgs[5] : "1";
            std::string ay = commaArgs.size() > 6 ? commaArgs[6] : "0";
            std::string by = commaArgs.size() > 7 ? commaArgs[7] : "1";
            std::string az = commaArgs.size() > 8 ? commaArgs[8] : "0";
            std::string bz = commaArgs.size() > 9 ? commaArgs[9] : "1";
            return "calc:triple_int|{\"expr\":" + jv(expr_) + ",\"varX\":" + jv(vX) + ",\"varY\":" + jv(vY) + ",\"varZ\":" + jv(vZ) + ",\"ax\":" + ax + ",\"bx\":" + bx + ",\"ay\":" + ay + ",\"by\":" + by + ",\"az\":" + az + ",\"bz\":" + bz + "}";
        }

        // ── Taylor series ─────────────────────────────────────────────────────────
        if (op == "taylor")
        {
            // taylor[expr, center, order]  OR  taylor[expr, var, center, order]
            std::string expr_ = commaArgs.size() > 0 ? commaArgs[0] : "";
            std::string var_, center, orderStr;
            if (commaArgs.size() >= 4)
            {
                // 4-arg: expr, var, center, order
                var_ = commaArgs[1];
                center = commaArgs[2];
                orderStr = commaArgs[3];
            }
            else if (commaArgs.size() == 3)
            {
                // 3-arg: expr, center, order  (var inferred)
                var_ = firstVar(expr_);
                center = commaArgs[1];
                orderStr = commaArgs[2];
            }
            else
            {
                var_ = firstVar(expr_);
                center = "0";
                orderStr = "6";
            }
            return "calc:taylor|{\"expr\":" + jv(expr_) + ",\"var\":" + jv(var_) + ",\"center\":" + center + ",\"order\":" + orderStr + "}";
        }

        // ── Maclaurin series ──────────────────────────────────────────────────────
        if (op == "maclaurin")
        {
            // maclaurin[expr, order]  OR  maclaurin[expr, var, order]
            std::string expr_ = commaArgs.size() > 0 ? commaArgs[0] : "";
            std::string var_, orderStr;
            if (commaArgs.size() >= 3)
            {
                var_ = commaArgs[1];
                orderStr = commaArgs[2];
            }
            else
            {
                var_ = firstVar(expr_);
                orderStr = commaArgs.size() > 1 ? commaArgs[1] : "6";
            }
            return "calc:maclaurin|{\"expr\":" + jv(expr_) + ",\"var\":" + jv(var_) + ",\"order\":" + orderStr + "}";
        }

        // ── Fourier series ────────────────────────────────────────────────────────
        if (op == "fourier")
        {
            // fourier[expr, period, terms]
            std::string expr_ = commaArgs.size() > 0 ? commaArgs[0] : "";
            std::string period = commaArgs.size() > 1 ? commaArgs[1] : "3.14159265358979";
            std::string terms = commaArgs.size() > 2 ? commaArgs[2] : "6";
            std::string var_ = firstVar(expr_);
            // Route through series (if fourier is implemented) or taylor as approximation
            return "calc:fourier|{\"expr\":" + jv(expr_) + ",\"var\":" + jv(var_) + ",\"period\":" + period + ",\"terms\":" + terms + "}";
        }

        // ── Vector calculus: div, curl, laplacian ─────────────────────────────────
        if (op == "div" || op == "divergence")
        {
            // div[f1, f2, ..., var1, var2, ...]
            size_t sp = commaArgs.size();
            for (int i = (int)commaArgs.size() - 1; i >= 0; --i)
            {
                if (isVarName(commaArgs[i]) && commaArgs[i].size() <= 2)
                    sp = i;
                else
                    break;
            }
            std::vector<std::string> exprs(commaArgs.begin(), commaArgs.begin() + sp);
            std::vector<std::string> vars(commaArgs.begin() + sp, commaArgs.end());
            return "calc:div|{\"exprs\":" + jsonStrArr(exprs) + ",\"vars\":" + jsonStrArr(vars) + "}";
        }
        if (op == "curl")
        {
            size_t sp = commaArgs.size();
            for (int i = (int)commaArgs.size() - 1; i >= 0; --i)
            {
                if (isVarName(commaArgs[i]) && commaArgs[i].size() <= 2)
                    sp = i;
                else
                    break;
            }
            std::vector<std::string> exprs(commaArgs.begin(), commaArgs.begin() + sp);
            std::vector<std::string> vars(commaArgs.begin() + sp, commaArgs.end());
            return "calc:curl|{\"exprs\":" + jsonStrArr(exprs) + ",\"vars\":" + jsonStrArr(vars) + "}";
        }

        // ── Line integral ─────────────────────────────────────────────────────────
        if (op == "line" || op == "line_integral")
        {
            // line[expr, varX, varY, paramExprX, paramExprY, paramVar, a, b]
            // where paramExprX = x(t), paramExprY = y(t), paramVar = t
            std::string expr_ = commaArgs.size() > 0 ? commaArgs[0] : "";
            std::string vX = commaArgs.size() > 1 ? commaArgs[1] : "x";
            std::string vY = commaArgs.size() > 2 ? commaArgs[2] : "y";
            std::string xp = commaArgs.size() > 3 ? commaArgs[3] : "t";
            std::string yp = commaArgs.size() > 4 ? commaArgs[4] : "t";
            std::string pvar = commaArgs.size() > 5 ? commaArgs[5] : "t";
            std::string a = commaArgs.size() > 6 ? commaArgs[6] : "0";
            std::string b = commaArgs.size() > 7 ? commaArgs[7] : "1";
            return "calc:line_integral|{\"expr\":" + jv(expr_) + ",\"varX\":" + jv(vX) + ",\"varY\":" + jv(vY) + ",\"paramX\":" + jv(xp) + ",\"paramY\":" + jv(yp) + ",\"param\":" + jv(pvar) + ",\"a\":" + a + ",\"b\":" + b + "}";
        }

        // ── Surface integral ──────────────────────────────────────────────────────
        if (op == "surface" || op == "surface_int")
        {
            // surface[expr, varX, varY, varZ]  →  symbolic surface setup
            std::string expr_ = commaArgs.size() > 0 ? commaArgs[0] : "";
            std::string vX = commaArgs.size() > 1 ? commaArgs[1] : "x";
            std::string vY = commaArgs.size() > 2 ? commaArgs[2] : "y";
            std::string vZ = commaArgs.size() > 3 ? commaArgs[3] : "z";
            return "calc:surface_int|{\"expr\":" + jv(expr_) + ",\"varX\":" + jv(vX) + ",\"varY\":" + jv(vY) + ",\"varZ\":" + jv(vZ) + "}";
        }

        // ── Green's theorem ───────────────────────────────────────────────────────
        if (op == "greens" || op == "greens_theorem")
        {
            // greens[P, Q, ax, bx, ay, by]
            std::string P = commaArgs.size() > 0 ? commaArgs[0] : "0";
            std::string Q = commaArgs.size() > 1 ? commaArgs[1] : "0";
            std::string ax = commaArgs.size() > 2 ? commaArgs[2] : "0";
            std::string bx = commaArgs.size() > 3 ? commaArgs[3] : "1";
            std::string ay = commaArgs.size() > 4 ? commaArgs[4] : "0";
            std::string by = commaArgs.size() > 5 ? commaArgs[5] : "1";
            return "calc:greens|{\"P\":" + jv(P) + ",\"Q\":" + jv(Q) + ",\"ax\":" + ax + ",\"bx\":" + bx + ",\"ay\":" + ay + ",\"by\":" + by + "}";
        }

        // ── Stokes' theorem ───────────────────────────────────────────────────────
        if (op == "stokes" || op == "stokes_theorem")
        {
            // stokes[P, Q, R, varX, varY, varZ]
            std::string P = commaArgs.size() > 0 ? commaArgs[0] : "0";
            std::string Q = commaArgs.size() > 1 ? commaArgs[1] : "0";
            std::string R = commaArgs.size() > 2 ? commaArgs[2] : "0";
            std::string vX = commaArgs.size() > 3 ? commaArgs[3] : "x";
            std::string vY = commaArgs.size() > 4 ? commaArgs[4] : "y";
            std::string vZ = commaArgs.size() > 5 ? commaArgs[5] : "z";
            return "calc:stokes|{\"P\":" + jv(P) + ",\"Q\":" + jv(Q) + ",\"R\":" + jv(R) + ",\"varX\":" + jv(vX) + ",\"varY\":" + jv(vY) + ",\"varZ\":" + jv(vZ) + "}";
        }

        // ── Optimization ──────────────────────────────────────────────────────────
        if (op == "max" || op == "min" || op == "optimize" || op == "optimize_1d" || op == "golden_section" || op == "golden_search")
        {
            // max[expr, a, b]
            std::string expr_ = commaArgs.size() > 0 ? commaArgs[0] : "";
            std::string var_ = firstVar(expr_);
            std::string a = commaArgs.size() > 1 ? commaArgs[1] : "-10";
            std::string b = commaArgs.size() > 2 ? commaArgs[2] : "10";
            return "calc:optimize_1d|{\"expr\":" + jv(expr_) + ",\"var\":" + jv(var_) + ",\"a\":" + a + ",\"b\":" + b + "}";
        }
        if (op == "saddle" || op == "critical" || op == "optimize_nd" || op == "gradient_descent" || op == "gradient_descent_nd")
        {
            // saddle[expr]  or  saddle[expr, var1, var2, ...]
            std::string expr_ = commaArgs.size() > 0 ? commaArgs[0] : "";
            std::vector<std::string> vars(commaArgs.begin() + 1, commaArgs.end());
            if (vars.empty())
            {
                // Infer vars from expression
                std::string v1 = firstVar(expr_);
                vars = {v1};
                // Try to find a second var
                std::string tmp = expr_;
                for (char &c : tmp)
                    if (c == v1[0])
                        c = ' ';
                std::string v2 = firstVar(tmp);
                if (v2 != v1 && !v2.empty())
                    vars.push_back(v2);
            }
            // Build min/max bounds from commaArgs if available
            std::string minArr = "[-10,-10]", maxArr = "[10,10]";
            return "calc:optimize_nd|{\"expr\":" + jv(expr_) + ",\"vars\":" + jsonStrArr(vars) + ",\"min\":" + minArr + ",\"max\":" + maxArr + "}";
        }

        // ═══════════════════════════════════════════════════════════════════════════
        // APPLIED MATH OPNS  (→ am:op|{json})
        // ═══════════════════════════════════════════════════════════════════════════

        if (op == "sir" || op == "sir_epidemic")
        {
            // sir[beta, gamma, S0, I0, R0, T, n]
            std::string beta = commaArgs.size() > 0 ? commaArgs[0] : "0.3";
            std::string gamma = commaArgs.size() > 1 ? commaArgs[1] : "0.1";
	    std::string S0 = commaArgs.size() > 2 ? commaArgs[2] : "990";
            std::string I0 = commaArgs.size() > 3 ? commaArgs[3] : "10";
            std::string R0 = commaArgs.size() > 4 ? commaArgs[4] : "0";
            std::string T = commaArgs.size() > 5 ? commaArgs[5] : "100";
            std::string n = commaArgs.size() > 6 ? commaArgs[6] : "500";
            return "am:sir|{\"beta\":" + beta + ",\"gamma\":" + gamma + ",\"S0\":" + S0 + ",\"I0\":" + I0 + ",\"R0\":" + R0 + ",\"T\":" + T + ",\"n\":" + n + "}";
        }

        if (op == "lotka_volterra")
        {
            // lotka_volterra[alpha, beta, delta, gamma, x0, y0, T]
            std::string alpha = commaArgs.size() > 0 ? commaArgs[0] : "1";
            std::string beta = commaArgs.size() > 1 ? commaArgs[1] : "0.1";
            std::string delta = commaArgs.size() > 2 ? commaArgs[2] : "0.075";
            std::string gamma = commaArgs.size() > 3 ? commaArgs[3] : "1.5";
            std::string x0 = commaArgs.size() > 4 ? commaArgs[4] : "10";
            std::string y0 = commaArgs.size() > 5 ? commaArgs[5] : "5";
            std::string T = commaArgs.size() > 6 ? commaArgs[6] : "50";
            std::string n = commaArgs.size() > 7 ? commaArgs[7] : "1000";
            return "am:lotka_volterra|{\"alpha\":" + alpha + ",\"beta\":" + beta + ",\"delta\":" + delta + ",\"gamma\":" + gamma + ",\"x0\":" + x0 + ",\"y0\":" + y0 + ",\"T\":" + T + ",\"n\":" + n + "}";
        }

        if (op == "logistic" || op == "logistic_growth")
        {
            // logistic[r, K, x0, T]  or  logistic_map[r, x0, n]
            std::string r = commaArgs.size() > 0 ? commaArgs[0] : "2";
            std::string K = commaArgs.size() > 1 ? commaArgs[1] : "100";
            std::string x0 = commaArgs.size() > 2 ? commaArgs[2] : "10";
            std::string T = commaArgs.size() > 3 ? commaArgs[3] : "50";
            return "am:logistic|{\"r\":" + r + ",\"K\":" + K + ",\"x0\":" + x0 + ",\"T\":" + T + "}";
        }

        if (op == "logistic_map")
        {
            std::string r = commaArgs.size() > 0 ? commaArgs[0] : "3.5";
            std::string x0 = commaArgs.size() > 1 ? commaArgs[1] : "0.5";
            std::string n = commaArgs.size() > 2 ? commaArgs[2] : "200";
            return "am:logistic_map|{\"r\":" + r + ",\"x0\":" + x0 + ",\"n\":" + n + "}";
        }

        if (op == "perturbation" || op == "regular_perturbation")
        {
            std::string eq = commaArgs.size() > 0 ? commaArgs[0] : "";
            std::string var = commaArgs.size() > 1 ? commaArgs[1] : "x";
            std::string eps = commaArgs.size() > 2 ? commaArgs[2] : "0.1";
            std::string ord = commaArgs.size() > 3 ? commaArgs[3] : "3";
            return "am:perturbation|{\"eq\":" + jv(eq) + ",\"var\":" + jv(var) + ",\"epsilon\":" + eps + ",\"order\":" + ord + "}";
        }

        if (op == "buckingham_pi" || op == "buckingham")
        {
            // buckingham[var1, var2, ...|dim_matrix]
            // If pipe separator: vars|matrix
            if (pipeArgs.size() >= 2)
            {
                return "am:buckingham|{\"vars\":" + jv(pipeArgs[0]) + ",\"D\":" + jv(pipeArgs[1]) + "}";
            }
            return "am:buckingham|{\"vars\":" + jv(inner) + ",\"D\":\"[]\"}";
        }

        if (op == "phase_portrait")
        {
            // phase_portrait[f, g, xmin, xmax, ymin, ymax]
            std::string f = commaArgs.size() > 0 ? commaArgs[0] : "x";
            std::string g = commaArgs.size() > 1 ? commaArgs[1] : "y";
            std::string xmin = commaArgs.size() > 2 ? commaArgs[2] : "-5";
            std::string xmax = commaArgs.size() > 3 ? commaArgs[3] : "5";
            std::string ymin = commaArgs.size() > 4 ? commaArgs[4] : "-5";
            std::string ymax = commaArgs.size() > 5 ? commaArgs[5] : "5";
            return "am:phase_portrait|{\"f\":" + jv(f) + ",\"g\":" + jv(g) + ",\"xmin\":" + xmin + ",\"xmax\":" + xmax + ",\"ymin\":" + ymin + ",\"ymax\":" + ymax + "}";
        }

        if (op == "bifurcation")
        {
            // bifurcation[f, stateVar, paramVar, muMin, muMax]
            std::string f = commaArgs.size() > 0 ? commaArgs[0] : "";
            std::string sv = commaArgs.size() > 1 ? commaArgs[1] : "x";
            std::string pv = commaArgs.size() > 2 ? commaArgs[2] : "mu";
            std::string mumin = commaArgs.size() > 3 ? commaArgs[3] : "-3";
            std::string mumax = commaArgs.size() > 4 ? commaArgs[4] : "3";
            return "am:bifurcation|{\"f\":" + jv(f) + ",\"x\":" + jv(sv) + ",\"mu\":" + jv(pv) + ",\"mumin\":" + mumin + ",\"mumax\":" + mumax + "}";
        }

        if (op == "turing" || op == "turing_instability")
        {
            // turing[D1, D2, fu, fv, gu, gv]
            std::string D1 = commaArgs.size() > 0 ? commaArgs[0] : "1";
            std::string D2 = commaArgs.size() > 1 ? commaArgs[1] : "10";
            std::string fu = commaArgs.size() > 2 ? commaArgs[2] : "0";
            std::string fv = commaArgs.size() > 3 ? commaArgs[3] : "0";
            std::string gu = commaArgs.size() > 4 ? commaArgs[4] : "0";
            std::string gv = commaArgs.size() > 5 ? commaArgs[5] : "0";
            return "am:turing|{\"D1\":" + D1 + ",\"D2\":" + D2 + ",\"fu\":" + fu + ",\"fv\":" + fv + ",\"gu\":" + gu + ",\"gv\":" + gv + "}";
        }

        // ═══════════════════════════════════════════════════════════════════════════
        // PROBABILITY THEORprob:op|{json})
        // ═══════════════════════════════════════════════════════════════════════════

        if (op == "markov")
        {
            // markov[matrix|initial_dist|steps]
            // Matrix rows separated by ;  e.g.  0.7,0.3;0.4,0.6
            if (pipeArgs.size() >= 2)
            {
                std::string mat = pipeArgs[0];
          std::string init = pipeArgs[1];
                std::string steps = pipeArgs.size() > 2 ? pipeArgs[2] : "5";
                return "prob:markov|{\"matrix\":" + jv(mat) + ",\"initial\":" + jv(init) + ",\"steps\":" + steps + "}";
            }
            return "prob:markov|{\"matrix\":" + jv(inner) + ",\"steps\":5}";
        }

        // ═══════════════════════════════════════════════════════â═══
        // DISCRETE MATH  (→ dm:op|{json})
        // ═══════════════════════════════════════════════════════════════════════════

        if (op == "combinations" || op == "choose" || op == "nCr")
        {
            std::string n = commaArgs.size() > 0 ? commaArgs[0] : "0";
            std::string r = commaArgs.size() > 1 ? commaArgs[1] : "0";
            return "dm:combinations|{\"n\":" + n + ",\"r\":" + r + "}";
        }

        if (op == "permutations" || op == "nPr")
        {
            std::string n = commaArgs.size() > 0 ? commaArgs[0] : "0";
            std::string r = commaArgs.size() > 1 ? commaArgs[1] : "0";
            return "dm:permutations|{\"n\":" + n + ",\"r\":" + r + "}";
        }

        if (op == "recurrence")
        {
            // recurrence[[coeffs]|[initial]|n]
            if (pipeArgs.size() >= 2)
            {
                std::string coeffs = pipeArgs[0]; // "[1,1]"
                std::string init = pipeArgs[1];   // "[0,1]"
                std::string n = pipeArgs.size() > 2 ? pipeArgs[2] : "10";
                return "dm:recurrence|{\"coeffs\":" + coeffs + ",\"initial\":" + init + ",\"n\":" + n + "}";
            }
            // comma-separated fallback
            return "dm:recurrence|{\"coeffs\":[1,1],\"initial\":[0,1],\"n\":10}";
        }

        if (op == "derangements")
        {
            std::string n = commaArgs.size() > 0 ? commaArgs[0] : "5";
            return "dm:derangements|{\"n\":" + n + "}";
        }

        if (op == "multiset")
        {
            std::string n = commaArgs.size() > 0 ? commaArgs[0] : "0";
            std::string r = commaArgs.size() > 1 ? commaArgs[1] : "0";
            return "dm:multiset|{\"n\":" + n + ",\"r\":" + r + "}";
        }

        // ── Geometry: dot_product, cross_product, convex_hull ────────────────────
	if (op == "dot_product" || op == "dot")
        {
            // dot_product[v1|v2]  pipe-separated vectors
            std::string v1 = pipeArgs.size() > 0 ? pipeArgs[0] : commaArgs.size() > 0 ? commaArgs[0] : "";
            std::string v2 = pipeArgs.size() > 1 ? pipeArgs[1] : "";
            return "la:dot_product|" + v1 + "|" + v2;
        }
        if (op == "cross_product" || op == "cross")
        {
            std::string v1 = pipeArgs.size() > 0 ? pipeArgs[0] : "";
            std::string v2 = pipeArgs.size() > 1 ? pipeArgs[1] : "";
            return "la:crpss_product|" + v1 + "|" + v2;
        }
        if (op == "convex_hull")
        {
            // convex_hull[x1,y1,x2,y2,...] or convex_hull[x1,y1|x2,y2|...]
            return "geo:convex_hull|{\"points\":" + jv(inner) + "}";
        }
        if (op == "triangle_area")
        {
            // triangle_area[x1,y1|x2,y2|x3,y3]
            double x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0;
            if (pipeArgs.size() >= 3)
            {
                auto p1 = splitArgs(pipeArgs[0], ',');
                auto p2 = splitArgs(pipeArgs[1], ',');
                auto p3 = splitArgs(pipeArgs[2], ',');
                std::string s = "{\"x1\":" + (p1.size()>0?p1[0]:"0") + ",\"y1\":" + (p1.size()>1?p1[1]:"0") +
                                ",\"x2\":" + (p2.size()>0?p2[0]:"0") + ",\"y2\":" + (p2.size()>1?p2[1]:"0") +
                                ",\"x3\":" + (p3.size()>0?p3[0]:"0") + ",\"y3\":" + (p3.size()>1?p3[1]:"0") + "}";
                return "geo:triangle_area|" + s;
            }
            if (commaArgs.size() >= 6)
                return "geo:triangle_area|{\"x1\":" + commaArgs[0] + ",\"y1\":" + commaArgs[1] +
                       ",\"x2\":" + commaArgs[2] + ",\"y2\":" + commaArgs[3] +
                       ",\"x3\":" + commaArgs[4] + ",\"y3\":" + commaArgs[5] + "}";
            return "geo:triangle_area|{\"x1\":0,\"y1\":0,\"x2\":1,\"y2\":0,\"x3\":0,\"y3\":1}";
        }

        // ── Fast Fouriernsform ──────────────────────────────────────────────────
        if (op == "fft" || op == "ifft")
        {
            // fft[1,1,0,-1,-1,-1,0,1]  — comma-separated real-valued samples
            return "na:" + op + "|{\"data\":" + jv(inner) + "}";
        }

        // ── Spline interpolation ────────────────────────────────────────────────
        if (op == "spline" || op == "cubic_spline" || op == "linear_spline")
        {
            // spline[x0,x1,...|y0,y1,...|xeval]  or  spline[x0,x1,...,y0,y1,...,xeval]
            std::string splineOp = (op == "linear_spline") ? "linear_spline" : "cubic_spline";
            std::string xsStr, ysStr, xevalStr;
            if (pipeArgs.size() >= 3)
            {
                xsStr = pipeArgs[0];
                ysStr = pipeArgs[1];
                xevalStr = pipeArgs[2];
            }
            else if (pipeArgs.size() == 2)
            {
                // Two pipe-separated parts: xs and ys|xeval
                xsStr = pipeArgs[0];
                auto inner2 = splitArgs(pipeArgs[1], ',');
                if (!inner2.empty()) { xevalStr = inner2.back(); inner2.pop_back(); }
                for (size_t i = 0; i < inner2.size(); ++i) { if (i) ysStr += ","; ysStr += inner2[i]; }
            }
            else
            {
                // comma-separated: x0,...,xn,y0,...,yn,xeval — split in half
                size_t half = commaArgs.size() / 2;
                for (size_t i = 0; i < half; ++i) { if (i) xsStr += ","; xsStr += commaArgs[i]; }
                for (size_t i = half; i < commaArgs.size() - 1; ++i) { if (i > half) ysStr += ","; ysStr += commaArgs[i]; }
                if (!commaArgs.empty()) xevalStr = commaArgs.back();
            }
            return "na:" + splineOp + "|{\"xs\":" + jv(xsStr) + ",\"ys\":" + jv(ysStr) + ",\"xeval\":" + xevalStr + "}";
      }

        // Unknown bracket op — return original so week4 has a chance or gives a proper error
        return e;
    }

} // namespace CoreEngine
