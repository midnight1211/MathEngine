package com.mathengine.ui;

// CalcExpressionBuilder
// Translates a user-typed expression and a selected operation into the
// "calc:<op>|{json}" protocol string that CoreEngine.cpp understands.
//
// This is the only place in the Java codebase that knows about the wire
// format. If the C++ operation names ever change, only this file changes.
//
// Supported operations and their expected input formats:
//
//   ARITHMETIC  → passed straight to the week_4 pipeline (no calc: prefix)
//   MATRIX      → passed as "matrix:..." (handled separately)
//
//   DERIVATIVE  → "d/dx[f]"         or just "f"  (variable inferred as x)
//                 "d2/dx2[f]"        second order
//                 "partial[f,x]"     partial w.r.t. x
//                 "gradient[f,x,y]"  gradient
//
//   INTEGRAL    → "f"               indefinite ∫f dx
//                 "f,a,b"           definite ∫_a^b f dx
//                 "f,a,b,method"    with explicit numerical method
//
//   LIMIT       → "f as x->a"       two-sided
//                 "f as x->a+"      right-sided
//                 "f as x->a-"      left-sided
//                 "f as x->inf"     limit at +∞
//
//   SERIES      → "taylor[f,center,order]"
//                 "maclaurin[f,order]"
//                 "convergence[a_n]"
//
// If the expression doesn't match any special syntax it is wrapped in a
// default operation for its type (e.g. DERIVATIVE → diff w.r.t. x).

public final class CalcExpressionBuilder {

    private CalcExpressionBuilder() {
    }

    // Main entry point called by MainLayout.dispatchSolve()
    public static String build(String expression,
            OperationPanel.Operation op) {
        String e = normaliseLatex(expression.trim(), op);
        return switch (op) {
            case ARITHMETIC -> e;
            case MATRIX -> buildMatrix(e);
            case DERIVATIVE -> buildDerivative(e);
            case INTEGRAL -> buildIntegral(e);
            case LIMIT -> buildLimit(e);
            case SERIES -> buildSeries(e);
            case VECTOR -> buildVector(e);
            case OPTIMIZE -> buildOptimize(e);
            case DIFFEQ -> buildDiffEq(e);
            case STATISTICS -> buildStat(e);
            case APPLIED -> buildApplied(e);
            case NUMBER_THY -> buildNumberTheory(e);
            case DISCRETE -> buildDiscrete(e);
            case COMPLEX -> buildComplex(e);
            case NUMERICAL -> buildNumerical(e);
            case ALGEBRA -> buildAlgebra(e);
            case PROBABILITY -> buildProbability(e);
            case GEOMETRY -> buildGeometry(e);
            default -> buildExtended(e, op);
        };
    }

    // ── DERIVATIVE ────────────────────────────────────────────────────────────

    private static String buildDerivative(String e) {

        // "partial[f,x,y]" → mixed partial ∂²f/∂x∂y
        if (e.startsWith("partial[") && e.endsWith("]")) {
            String inner = e.substring(8, e.length() - 1);
            String[] parts = inner.split(",", 3);
            if (parts.length == 3) {
                return calc("partial_mixed", json(
                        "expr", parts[0].trim(),
                        "var1", parts[1].trim(),
                        "var2", parts[2].trim()));
            }
            if (parts.length == 2) {
                return calc("partial", json(
                        "expr", parts[0].trim(),
                        "var", parts[1].trim(),
                        "order", "1"));
            }
        }

        // "gradient[f,x,y,z]" → ∇f
        if (e.startsWith("gradient[") && e.endsWith("]")) {
            String inner = e.substring(9, e.length() - 1);
            String[] parts = inner.split(",");
            if (parts.length >= 2) {
                StringBuilder sb = new StringBuilder();
                sb.append("{\"expr\":\"").append(parts[0].trim())
                        .append("\",\"vars\":[");
                for (int i = 1; i < parts.length; i++) {
                    if (i > 1)
                        sb.append(',');
                    sb.append('"').append(parts[i].trim()).append('"');
                }
                sb.append("]}");
                return "calc:gradient|" + sb;
            }
        }

        // "hessian[f,x,y]"
        if (e.startsWith("hessian[") && e.endsWith("]")) {
            String inner = e.substring(8, e.length() - 1);
            String[] parts = inner.split(",");
            if (parts.length >= 2) {
                return calc("hessian", jsonWithVarArray(parts));
            }
        }

        // "implicit[F,x,y]" → implicit dy/dx
        if (e.startsWith("implicit[") && e.endsWith("]")) {
            String inner = e.substring(9, e.length() - 1);
            String[] parts = inner.split(",", 3);
            if (parts.length == 3) {
                return calc("implicit_diff", json(
                        "expr", parts[0].trim(),
                        "indep", parts[1].trim(),
                        "dep", parts[2].trim()));
            }
        }

        // "logdiff[f,x]" → logarithmic differentiation
        if (e.startsWith("logdiff[") && e.endsWith("]")) {
            String inner = e.substring(8, e.length() - 1);
            String[] parts = inner.split(",", 2);
            if (parts.length == 2) {
                return calc("log_diff", json("expr", parts[0].trim(), "var", parts[1].trim()));
            }
        }

        // "d2/dx2[f]" or "d3/dx3[f]" → nth derivative
        if (e.startsWith("d") && e.contains("/d") && e.contains("[")) {
            try {
                int orderEnd = e.indexOf('/');
                int orderNum = orderEnd > 1
                        ? Integer.parseInt(e.substring(1, orderEnd))
                        : 1;
                int varStart = e.indexOf("d", orderEnd) + 1;
                int varEnd = e.indexOf('[', varStart);
                // Strip trailing order digit: "x2" from "d2/dx2[...]" → "x"
                String var = e.substring(varStart, varEnd).trim()
                              .replaceAll("\\d+$", "");
                if (var.isEmpty()) var = "x";
                String expr = e.substring(varEnd + 1, e.length() - 1);
                return calc("diff", json("expr", expr, "var", var, "order",
                        String.valueOf(orderNum)));
            } catch (Exception ignored) {
            }
        }

        // "d/dx[f]" shorthand
        if (e.startsWith("d/d") && e.contains("[")) {
            int varEnd = e.indexOf('[');
            String var = e.substring(3, varEnd).trim();
            String expr = e.substring(varEnd + 1, e.length() - 1);
            return calc("diff", json("expr", expr, "var", var, "order", "1"));
        }

        // Plain expression → diff w.r.t. x, order 1
        String var = inferVariable(e);
        return calc("diff", json("expr", e, "var", var, "order", "1"));
    }

    // ── INTEGRAL ──────────────────────────────────────────────────────────────

    private static String buildIntegral(String e) {

        // "taylor[f,center,order]" or "maclaurin[f,order]" — series via integral panel
        if (e.startsWith("taylor[") && e.endsWith("]")) {
            String inner = e.substring(7, e.length() - 1);
            String[] parts = inner.split(",");
            if (parts.length >= 3) {
                String var = inferVariable(parts[0].trim());
                return calc("taylor", json(
                        "expr", parts[0].trim(),
                        "var", var,
                        "center", parts[1].trim(),
                        "order", parts[2].trim()));
            }
        }
        if (e.startsWith("maclaurin[") && e.endsWith("]")) {
            String inner = e.substring(10, e.length() - 1);
            String[] parts = inner.split(",", 2);
            if (parts.length == 2) {
                String var = inferVariable(parts[0].trim());
                return calc("maclaurin", json(
                        "expr", parts[0].trim(),
                        "var", var,
                        "order", parts[1].trim()));
            }
        }

        // "double[f,x,y,ax,bx,ay,by]" → double integral
        if (e.startsWith("double[") && e.endsWith("]")) {
            String inner = e.substring(7, e.length() - 1);
            String[] p = inner.split(",");
            if (p.length == 7) {
                return calc("double_int", json(
                        "expr", p[0].trim(), "varX", p[1].trim(), "varY", p[2].trim(),
                        "ax", p[3].trim(), "bx", p[4].trim(),
                        "ay", p[5].trim(), "by", p[6].trim()));
            }
        }

        // "triple[f,x,y,z,ax,bx,ay,by,az,bz]"
        if (e.startsWith("triple[") && e.endsWith("]")) {
            String[] p = e.substring(7, e.length() - 1).split(",");
            if (p.length == 10) {
                return calc("triple_int", json(
                        "expr", p[0].trim(), "varX", p[1].trim(),
                        "varY", p[2].trim(), "varZ", p[3].trim(),
                        "ax", p[4].trim(), "bx", p[5].trim(),
                        "ay", p[6].trim(), "by", p[7].trim(),
                        "az", p[8].trim(), "bz", p[9].trim()));
            }
        }

        // "polar[f,r,t,r1,r2,t1,t2]"
        if (e.startsWith("polar[") && e.endsWith("]")) {
            String[] p = e.substring(6, e.length() - 1).split(",");
            if (p.length == 7) {
                return calc("polar_int", json(
                        "expr", p[0].trim(), "varR", p[1].trim(), "varT", p[2].trim(),
                        "r1", p[3].trim(), "r2", p[4].trim(),
                        "t1", p[5].trim(), "t2", p[6].trim()));
            }
        }

        // "f,a,b" or "f,a,b,method" → definite integral
        String[] parts = e.split(",");
        if (parts.length >= 3) {
            String expr = parts[0].trim();
            String a = parts[1].trim();
            String b = parts[2].trim();
            String method = parts.length >= 4 ? parts[3].trim() : "auto";
            String var = inferVariable(expr);

            if (method.equals("auto") || method.equals("symbolic")) {
                return calc("definite_int", json("expr", expr, "var", var, "a", a, "b", b));
            }
            // Explicit numerical method (romberg, gauss, simpson, trapezoid, etc.)
            return calc("numerical_int", json(
                    "expr", expr, "var", var, "a", a, "b", b, "method", method));
        }

        // Plain expression → indefinite integral
        String var = inferVariable(e);
        return calc("integrate", json("expr", e, "var", var));
    }

    // ── LIMIT ─────────────────────────────────────────────────────────────────

    private static String buildLimit(String e) {
        // Expected: "f as x->point" or "f as x->point+" / "point-"
        String lower = e.toLowerCase();
        int asIdx = lower.indexOf(" as ");
        if (asIdx == -1) {
            // No "as" keyword — guess: just wrap as two-sided limit at 0
            String var = inferVariable(e);
            return calc("limit", json("expr", e, "var", var, "point", "0"));
        }

        String expr = e.substring(0, asIdx).trim();
        String arrowStr = e.substring(asIdx + 4).trim(); // "x->point" or "x->point+"

        int arrowIdx = arrowStr.indexOf("->");
        if (arrowIdx == -1)
            return calc("limit", json("expr", e, "var", "x", "point", "0"));

        String var = arrowStr.substring(0, arrowIdx).trim();
        String pointRaw = arrowStr.substring(arrowIdx + 2).trim();
        String op = "limit";
        String point = pointRaw;

        if (pointRaw.endsWith("+")) {
            op = "limit_right";
            point = pointRaw.substring(0, pointRaw.length() - 1).trim();
        } else if (pointRaw.endsWith("-")) {
            op = "limit_left";
            point = pointRaw.substring(0, pointRaw.length() - 1).trim();
        } else if (point.equals("inf") || point.equals("+inf")) {
            op = "limit_inf";
        } else if (point.equals("-inf")) {
            op = "limit_neginf";
        }

        return calc(op, json("expr", expr, "var", var, "point", point));
    }

    // ── Extended operations (series, vector calc, optimization) ───────────────

    private static String buildExtended(String e, OperationPanel.Operation op) {
        // Series convergence
        if (e.startsWith("convergence[") && e.endsWith("]")) {
            String inner = e.substring(12, e.length() - 1).trim();
            String var = inferVariable(inner);
            return calc("convergence", json("expr", inner, "var", var));
        }

        // Vector operations: div[P,Q,R,x,y,z]
        if (e.startsWith("div[") && e.endsWith("]")) {
            String[] p = e.substring(4, e.length() - 1).split(",");
            return buildVecOpWithExprs("div", p);
        }
        if (e.startsWith("curl[") && e.endsWith("]")) {
            java.util.List<String> pList = splitArgs(e.substring(5, e.length() - 1), ',');
            String[] p = pList.stream().map(String::trim).toArray(String[]::new);
            return buildVecOpWithExprs("curl", p);
        }

        // Optimization
        if (e.startsWith("optimize[") && e.endsWith("]")) {
            String inner = e.substring(9, e.length() - 1);
            String[] p = inner.split(",", 4);
            if (p.length == 3) {
                return calc("optimize_1d", json(
                        "expr", p[0].trim(), "var", inferVariable(p[0].trim()),
                        "a", p[1].trim(), "b", p[2].trim()));
            }
        }
        if (e.startsWith("lagrange[") && e.endsWith("]")) {
            String inner = e.substring(9, e.length() - 1);
            String[] p = inner.split(",", 6);
            if (p.length >= 4) {
                inferVariable(p[0].trim());
                StringBuilder sb = new StringBuilder();
                sb.append("{\"f\":\"").append(p[0].trim())
                        .append("\",\"g\":\"").append(p[1].trim())
                        .append("\",\"vars\":[\"x\",\"y\"]")
                        .append(",\"min\":[").append(p[2].trim()).append(",").append(p[3].trim()).append("]")
                        .append(",\"max\":[").append(p.length > 4 ? p[4].trim() : "2").append(",")
                        .append(p.length > 5 ? p[5].trim() : "2").append("]}");
                return "calc:lagrange|" + sb;
            }
        }

        // Fallback: treat as derivative w.r.t. x
        return buildDerivative(e);
    }

    // ── Series ────────────────────────────────────────────────────────────────

    private static String buildSeries(String e) {
        if (e.startsWith("convergence[") && e.endsWith("]")) {
            String inner = e.substring(12, e.length() - 1).trim();
            return calc("convergence", json("expr", inner, "var", inferVariable(inner)));
        }
        if (e.startsWith("taylor[") && e.endsWith("]")) {
            String[] p = e.substring(7, e.length() - 1).split(",");
            if (p.length >= 3)
                return calc("taylor", json("expr", p[0].trim(), "var", inferVariable(p[0].trim()),
                        "center", p[1].trim(), "order", p[2].trim()));
        }
        if (e.startsWith("maclaurin[") && e.endsWith("]")) {
            String[] p = e.substring(10, e.length() - 1).split(",", 2);
            if (p.length == 2)
                return calc("maclaurin", json("expr", p[0].trim(), "var", inferVariable(p[0].trim()),
                        "order", p[1].trim()));
        }
        // Fourier series
        if (e.startsWith("fourier[") && e.endsWith("]")) {
            String[] p = e.substring(8, e.length() - 1).split(",");
            if (p.length >= 3)
                return calc("taylor", json("expr", p[0].trim(), "var", inferVariable(p[0].trim()),
                        "center", "0", "order", p[2].trim()));
        }
        // Default: Maclaurin order 6
        return calc("maclaurin", json("expr", e, "var", inferVariable(e), "order", "6"));
    }

    // ── Vector calculus ────────────────────────────────────────────────────────

    private static String buildVector(String e) {
        // Gradient: gradient[f,x,y,...] →
        // calc:gradient|{"expr":"f","vars":["x","y",...]}
        if (e.startsWith("gradient[") && e.endsWith("]")) {
            String inner = e.substring(9, e.length() - 1);
            String[] p = inner.split(",");
            if (p.length >= 2) {
                StringBuilder sb = new StringBuilder("{\"expr\":\"")
                        .append(p[0].trim()).append("\",\"vars\":[");
                for (int i = 1; i < p.length; i++) {
                    if (i > 1)
                        sb.append(',');
                    sb.append("\"").append(p[i].trim()).append("\"");
                }
                sb.append("]}");
                return "calc:gradient|" + sb;
            }
        }
        // Divergence: div[P,Q,R,x,y,z]
        if (e.startsWith("div[") && e.endsWith("]"))
            return buildVecOp("div", e.substring(4, e.length() - 1).split(","));
        // Curl: curl[P,Q,R,x,y,z]
        if (e.startsWith("curl[") && e.endsWith("]"))
            return buildVecOp("curl", e.substring(5, e.length() - 1).split(","));
        // Laplacian: laplacian[f,x,y,...]
        if (e.startsWith("laplacian[") && e.endsWith("]")) {
            String[] p = e.substring(10, e.length() - 1).split(",");
            return "calc:laplacian|" + jsonWithVarArray(p);
        }
        // Jacobian: jacobian[f1,f2,...,x,y,...] (half exprs, half vars)
        if (e.startsWith("jacobian[") && e.endsWith("]")) {
            String[] p = e.substring(9, e.length() - 1).split(",");
            int half = p.length / 2;
            StringBuilder sb = new StringBuilder("{\"exprs\":[");
            for (int i = 0; i < half; i++) {
                if (i > 0)
                    sb.append(',');
                sb.append("\"").append(p[i].trim()).append("\"");
            }
            sb.append("],\"vars\":[");
            for (int i = half; i < p.length; i++) {
                if (i > half)
                    sb.append(',');
                sb.append("\"").append(p[i].trim()).append("\"");
            }
            sb.append("]}");
            return "calc:jacobian|" + sb;
        }
        // Dot product: dot_product[a1,a2,a3|b1,b2,b3]
        if (e.startsWith("dot_product[") && e.endsWith("]"))
            return "la:dot_product|" + e.substring(12, e.length() - 1);
        // Cross product: cross_product[a1,a2,a3|b1,b2,b3]
        if (e.startsWith("cross_product[") && e.endsWith("]"))
            return "la:cross_product|" + e.substring(14, e.length() - 1);
        // Vector norm: norm[v1,v2,v3]
        if (e.startsWith("norm[") && e.endsWith("]"))
            return "la:vector_norm|" + e.substring(5, e.length() - 1) + "|2";
        // Line integral: line[f,g,x,y,t,t0,t1] — compute ∫(f*dx/dt + g*dy/dt)dt
        // numerically
        // Maps to calc:numerical_int which IS in the C++ dispatch
        if (e.startsWith("line[") && e.endsWith("]")) {
            String[] p = e.substring(5, e.length() - 1).split(",");
            if (p.length >= 7) {
                // F=(p[0],p[1]), curve=(p[2],p[3]), t range p[4]..p[5]
                // Build integrand F·dr/dt = f*x'(t)+g*y'(t) — user provides the integrand
                // directly
                String integrand = p[0].trim(); // user should supply the full integrand
                return calc("numerical_int", json(
                        "expr", integrand, "var", "t",
                        "a", p[p.length - 2].trim(), "b", p[p.length - 1].trim(), "method", "gauss"));
            }
        }
        // Surface integral: surface[f,x,y,ax,bx,ay,by] → calc:double_int
        if (e.startsWith("surface[") && e.endsWith("]")) {
            String[] p = e.substring(8, e.length() - 1).split(",");
            if (p.length >= 7)
                return calc("double_int", json(
                        "expr", p[0].trim(), "varX", p[1].trim(), "varY", p[2].trim(),
                        "ax", p[3].trim(), "bx", p[4].trim(),
                        "ay", p[5].trim(), "by", p[6].trim()));
            // Short form: surface[f,x,y] → integrate over [-1,1]x[-1,1]
            return calc("double_int", json(
                    "expr", p[0].trim(),
                    "varX", p.length > 1 ? p[1].trim() : "x", "varY", p.length > 2 ? p[2].trim() : "y",
                    "ax", "-1", "bx", "1", "ay", "-1", "by", "1"));
        }
        // Green's theorem: greens[P,Q,x0,x1,y0,y1]
        // ∬_D (∂Q/∂x - ∂P/∂y) dA — route as double integral of the curl
        if (e.startsWith("greens[") && e.endsWith("]")) {
            String[] p = e.substring(7, e.length() - 1).split(",");
            if (p.length >= 4) {
                // We can't easily compute ∂Q/∂x - ∂P/∂y symbolically here.
                // Route as double_int of the 2D curl expression provided by user.
                // Format: greens[curl_expr,x,y,x0,x1,y0,y1]
                return calc("double_int", json(
                        "expr", p[0].trim(),
                        "varX", p.length > 5 ? p[1].trim() : "x",
                        "varY", p.length > 5 ? p[2].trim() : "y",
                        "ax", p.length > 5 ? p[3].trim() : p[1].trim(),
                        "bx", p.length > 5 ? p[4].trim() : p[2].trim(),
                        "ay", p.length > 5 ? p[5].trim() : "0",
                        "by", p.length > 5 ? p[6].trim() : "1"));
            }
        }
        // Stokes: stokes[curl_z_expr,x,y,x0,x1,y0,y1] → double_int of Fz component
        if (e.startsWith("stokes[") && e.endsWith("]")) {
            String[] p = e.substring(7, e.length() - 1).split(",");
            if (p.length >= 4)
                return calc("double_int", json(
                        "expr", p[0].trim(),
                        "varX", p.length > 3 ? p[1].trim() : "x", "varY", p.length > 3 ? p[2].trim() : "y",
                        "ax", p.length > 5 ? p[3].trim() : "-1", "bx", p.length > 5 ? p[4].trim() : "1",
                        "ay", p.length > 5 ? p[5].trim() : "-1", "by", p.length > 5 ? p[6].trim() : "1"));
        }
        // Default: gradient w.r.t. first variable found
        String var = inferVariable(e);
        return calc("gradient", "{\"expr\":\"" + e + "\",\"vars\":[\"" + var + "\"]}");
    }

    private static String buildVecOp(String op, String[] p) {
        // p = [expr0, expr1, ..., var0, var1, ...] (first half exprs, second half vars)
        int half = p.length / 2;
        StringBuilder sb = new StringBuilder("{\"exprs\":[");
        for (int i = 0; i < half; i++) {
            if (i > 0)
                sb.append(',');
            sb.append("\"").append(p[i].trim()).append("\"");
        }
        sb.append("],\"vars\":[");
        for (int i = half; i < p.length; i++) {
            if (i > half)
                sb.append(',');
            sb.append("\"").append(p[i].trim()).append("\"");
        }
        sb.append("]}");
        return "calc:" + op + "|" + sb;
    }

    // ── Optimization ──────────────────────────────────────────────────────────

    private static String buildOptimize(String e) {
        // lagrange[f,g,x,y,xmin,ymin,xmax,ymax]
        if (e.startsWith("lagrange[") && e.endsWith("]")) {
            String[] p = e.substring(9, e.length() - 1).split(",");
            if (p.length >= 4) {
                String f = p[0].trim(), g = p[1].trim();
                String x = p[2].trim(), y = p.length > 3 ? p[3].trim() : "y";
                String xmin = p.length > 4 ? p[4].trim() : "-5";
                String ymin = p.length > 5 ? p[5].trim() : "-5";
                String xmax = p.length > 6 ? p[6].trim() : "5";
                String ymax = p.length > 7 ? p[7].trim() : "5";
                StringBuilder sb = new StringBuilder("{\"f\":\"").append(f)
                        .append("\",\"g\":\"").append(g)
                        .append("\",\"vars\":[\"").append(x).append("\",\"").append(y).append("\"]")
                        .append(",\"min\":[").append(xmin).append(",").append(ymin).append("]")
                        .append(",\"max\":[").append(xmax).append(",").append(ymax).append("]}");
                return "calc:lagrange|" + sb;
            }
        }
        // optimize_nd[f,x,y,...] — multi-variable
        if (e.startsWith("optimize_nd[") && e.endsWith("]")) {
            String inner = e.substring(12, e.length() - 1);
            String[] p = inner.split(",");
            StringBuilder sb = new StringBuilder("{\"f\":\"").append(p[0].trim())
                    .append("\",\"vars\":[");
            for (int i = 1; i < p.length; i++) {
                if (i > 1)
                    sb.append(',');
                sb.append("\"").append(p[i].trim()).append("\"");
            }
            sb.append("]}");
            return "calc:optimize_nd|" + sb;
        }
        // gradient_descent[f,x,y,...] — same as optimize_nd
        if (e.startsWith("gradient_descent[") && e.endsWith("]")) {
            String inner = e.substring(17, e.length() - 1);
            String[] p = inner.split(",");
            StringBuilder sb = new StringBuilder("{\"f\":\"").append(p[0].trim())
                    .append("\",\"vars\":[");
            for (int i = 1; i < p.length; i++) {
                if (i > 1)
                    sb.append(',');
                sb.append("\"").append(p[i].trim()).append("\"");
            }
            sb.append("]}");
            return "calc:optimize_nd|" + sb;
        }
        // Default: 1D optimize with range "expr, a, b"
        String[] p = e.split(",");
        if (p.length >= 3)
            return calc("optimize_1d", json(
                    "expr", p[0].trim(), "var", inferVariable(p[0].trim()),
                    "a", p[1].trim(), "b", p[2].trim()));
        if (p.length == 2)
            return calc("optimize_1d", json(
                    "expr", p[0].trim(), "var", inferVariable(p[0].trim()),
                    "a", p[1].trim(), "b", "10"));
        return calc("optimize_1d", json(
                "expr", e, "var", inferVariable(e), "a", "-10", "b", "10"));
    }

    // ── Differential equations (DiffEq tab syntax) ────────────────────────────

    static String buildDiffEq(String e) {
        // Applications
        if (e.startsWith("mixing[") && e.endsWith("]")) {
            String[] p = e.substring(7, e.length() - 1).split(",");
            if (p.length >= 5)
                return de("mixing", json("V", p[0].trim(), "cin", p[1].trim(),
                        "rin", p[2].trim(), "rout", p[3].trim(), "c0", p[4].trim(),
                        "tend", p.length > 5 ? p[5].trim() : "100"));
        }
        if (e.startsWith("cooling[") && e.endsWith("]")) {
            String[] p = e.substring(8, e.length() - 1).split(",");
            if (p.length >= 3)
                return de("cooling", json("k", p[0].trim(), "Tenv", p[1].trim(),
                        "T0", p[2].trim(), "tend", p.length > 3 ? p[3].trim() : "50"));
        }
        if (e.startsWith("population[") && e.endsWith("]")) {
            String[] p = e.substring(11, e.length() - 1).split(",");
            if (p.length >= 3)
                return de("population", json("r", p[0].trim(), "P0", p[1].trim(),
                        "tend", p[2].trim(), "logistic", p.length > 3 ? "true" : "false",
                        "K", p.length > 4 ? p[4].trim() : "1000"));
        }
        if (e.startsWith("logistic[") && e.endsWith("]")) {
            String[] p = e.substring(9, e.length() - 1).split(",");
            if (p.length >= 4)
                return de("population", json("r", p[0].trim(), "P0", p[1].trim(),
                        "tend", p[2].trim(), "logistic", "true", "K", p[3].trim()));
        }
        if (e.startsWith("terminal_v[") && e.endsWith("]")) {
            String[] p = e.substring(11, e.length() - 1).split(",");
            if (p.length >= 3)
                return de("terminal_velocity", json("m", p[0].trim(),
                        "g", "9.81", "k", p[1].trim(), "v0", "0", "tend", p[2].trim()));
        }
        // Phase portrait classify_linear[a,b,c,d]
        if (e.startsWith("phase_portrait[") && e.endsWith("]")) {
            String[] p = e.substring(15, e.length() - 1).split(",");
            if (p.length == 4)
                return de("phase_portrait", json("a", p[0].trim(), "b", p[1].trim(),
                        "c", p[2].trim(), "d", p[3].trim()));
        }
        // Fourier series: fourier[f,L,N]
        if (e.startsWith("fourier[") && e.endsWith("]")) {
            String[] p = e.substring(8, e.length() - 1).split(",");
            if (p.length >= 1)
                return de("fourier_series", json("f", p[0].trim(),
                        "x", "x", "L", p.length > 1 ? p[1].trim() : "3.14159", "N", p.length > 2 ? p[2].trim() : "10"));
        }
        if (e.startsWith("fourier_sine[") && e.endsWith("]")) {
            String[] p = e.substring(13, e.length() - 1).split(",");
            return de("fourier_sine", json("f", p[0].trim(), "x", "x",
                    "L", p.length > 1 ? p[1].trim() : "3.14159", "N", p.length > 2 ? p[2].trim() : "10"));
        }
        // Dynamical systems
        if (e.startsWith("lyapunov[") && e.endsWith("]")) {
            String[] p = e.substring(9, e.length() - 1).split(",");
            if (p.length >= 3)
                return de("lyapunov_fn", json("f1", p[0].trim(),
                        "f2", p[1].trim(), "V", p[2].trim(), "x1", "x", "x2", "y", "xs", "0", "ys", "0"));
        }
        if (e.startsWith("dulac[") && e.endsWith("]")) {
            String[] p = e.substring(6, e.length() - 1).split(",");
            if (p.length >= 2)
                return de("dulac", json("f1", p[0].trim(),
                        "f2", p[1].trim(), "B", p.length > 2 ? p[2].trim() : "1", "x1", "x", "x2", "y"));
        }
        // Bessel / Legendre
        if (e.startsWith("bessel[") && e.endsWith("]")) {
            String[] p = e.substring(7, e.length() - 1).split(",");
            if (p.length >= 2)
                return de("bessel", json("nu", p[0].trim(), "x", p[1].trim()));
        }
        if (e.startsWith("legendre[") && e.endsWith("]")) {
            String[] p = e.substring(9, e.length() - 1).split(",");
            if (p.length >= 2)
                return de("legendre", json("n", p[0].trim(), "x", p[1].trim()));
        }
        // Implicit Euler / Crank-Nicolson
        if (e.startsWith("implicit_euler[") && e.endsWith("]")) {
            String[] p = e.substring(15, e.length() - 1).split(",");
            if (p.length >= 4)
                return de("implicit_euler", json("f", p[0].trim(),
                        "t0", p[1].trim(), "t1", p[2].trim(), "y0", p[3].trim(), "n",
                        p.length > 4 ? p[4].trim() : "100"));
        }
        if (e.startsWith("crank_nicolson[") && e.endsWith("]")) {
            String[] p = e.substring(15, e.length() - 1).split(",");
            if (p.length >= 4)
                return de("crank_nicolson", json("f", p[0].trim(),
                        "t0", p[1].trim(), "t1", p[2].trim(), "y0", p[3].trim(), "n",
                        p.length > 4 ? p[4].trim() : "100"));
        }
        // Characteristics: characteristics[a,b,c,ic]
        if (e.startsWith("characteristics[") && e.endsWith("]")) {
            String[] p = e.substring(16, e.length() - 1).split(",");
            if (p.length >= 4)
                return de("characteristics_1st", json("a", p[0].trim(),
                        "b", p[1].trim(), "c", p[2].trim(), "ic", p[3].trim()));
        }
        // SL eigenvalues: sl_eigen[p,q,w,a,b,N]
        if (e.startsWith("sl_eigen[") && e.endsWith("]")) {
            String[] p = e.substring(9, e.length() - 1).split(",");
            if (p.length >= 6)
                return de("sl_eigen", json("p", p[0].trim(), "q", p[1].trim(),
                        "w", p[2].trim(), "a", p[3].trim(), "b", p[4].trim(), "N", p[5].trim()));
        }
        // Nonlinear 2D system: nonlinear[f1,f2]
        if (e.startsWith("nonlinear[") && e.endsWith("]")) {
            String[] p = e.substring(10, e.length() - 1).split(",");
            if (p.length >= 2)
                return de("nonlinear_2d", json("f1", p[0].trim(), "f2", p[1].trim()));
        }

        // "rk4[f(t,y), t0, t1, y0, n]"
        for (String method : new String[] { "rk4", "rk45", "euler", "improved_euler", "rk2", "adams_bashforth" }) {
            if (e.startsWith(method + "[") && e.endsWith("]")) {
                String[] p = e.substring(method.length() + 1, e.length() - 1).split(",");
                if (p.length >= 4) {
                    return "de:" + method + "|" + json("f", p[0].trim(),
                            "t0", p[1].trim(), "t1", p[2].trim(), "y0", p[3].trim(),
                            "n", p.length > 4 ? p[4].trim() : "100");
                }
            }
        }
        // "homogeneous[a,b,c]"
        if (e.startsWith("homogeneous[") && e.endsWith("]")) {
            String[] p = e.substring(12, e.length() - 1).split(",");
            if (p.length == 3)
                return "de:homogeneous2nd|" + json("a", p[0].trim(), "b", p[1].trim(), "c", p[2].trim());
        }
        // "cauchy[a,b,c]"
        if (e.startsWith("cauchy[") && e.endsWith("]")) {
            String[] p = e.substring(7, e.length() - 1).split(",");
            if (p.length == 3)
                return "de:cauchy_euler|" + json("a", p[0].trim(), "b", p[1].trim(), "c", p[2].trim());
        }
        // "heat[alpha,L,ic,terms]"
        if (e.startsWith("heat[") && e.endsWith("]")) {
            String[] p = e.substring(5, e.length() - 1).split(",");
            if (p.length >= 3)
                return "de:heat_pde|" + json("alpha", p[0].trim(), "L", p[1].trim(), "ic", p[2].trim(), "terms",
                        p.length > 3 ? p[3].trim() : "5");
        }
        return "de:homogeneous2nd|" + json("a", "1", "b", "0", "c", "1");
    }

    // ── Statistics (Stat tab syntax) ──────────────────────────────────────────

    static String buildStat(String e) {
        // summarize[1,2,3,4,5]
        if (e.startsWith("summarize[") && e.endsWith("]"))
            return stat("summarize", "{\"x\":[" + e.substring(10, e.length() - 1) + "]}");
        // t_test[x|y] or t_test[x]
        if (e.startsWith("t_test[") && e.endsWith("]")) {
            String[] g = e.substring(7, e.length() - 1).split("\\|");
            if (g.length == 2)
                return stat("t_test_two", "{\"x\":[" + g[0] + "],\"y\":[" + g[1] + "]}");
            return stat("t_test_one", "{\"x\":[" + g[0] + "],\"mu0\":0}");
        }
        // regression[x|y]
        if (e.startsWith("regression[") && e.endsWith("]")) {
            String[] g = e.substring(11, e.length() - 1).split("\\|");
            if (g.length == 2)
                return stat("linear_reg", "{\"x\":[" + g[0] + "],\"y\":[" + g[1] + "]}");
        }
        // anova[g1|g2|g3]
        if (e.startsWith("anova[") && e.endsWith("]")) {
            String[] g = e.substring(6, e.length() - 1).split("\\|");
            StringBuilder sb = new StringBuilder("{\"groups\":[");
            for (int i = 0; i < g.length; i++) {
                if (i > 0)
                    sb.append(",");
                sb.append("[").append(g[i]).append("]");
            }
            return stat("anova_one", sb + "]}");
        }
        // tukey[g1|g2|g3]
        if (e.startsWith("tukey[") && e.endsWith("]")) {
            String[] g = e.substring(6, e.length() - 1).split("\\|");
            StringBuilder sb = new StringBuilder("{\"groups\":[");
            for (int i = 0; i < g.length; i++) {
                if (i > 0)
                    sb.append(",");
                sb.append("[").append(g[i]).append("]");
            }
            return stat("tukey", sb + "]}");
        }
        // fisher[a,b,c,d]
        if (e.startsWith("fisher[") && e.endsWith("]")) {
            String[] p = e.substring(7, e.length() - 1).split(",");
            if (p.length == 4)
                return stat("fisher_exact",
                        json("a", p[0].trim(), "b", p[1].trim(), "c", p[2].trim(), "d", p[3].trim()));
        }
        // mcnemar[a,b,c,d]
        if (e.startsWith("mcnemar[") && e.endsWith("]")) {
            String[] p = e.substring(8, e.length() - 1).split(",");
            if (p.length == 4)
                return stat("mcnemar", json("a", p[0].trim(), "b", p[1].trim(), "c", p[2].trim(), "d", p[3].trim()));
        }
        // odds_ratio[a,b,c,d]
        if (e.startsWith("odds_ratio[") && e.endsWith("]")) {
            String[] p = e.substring(11, e.length() - 1).split(",");
            if (p.length == 4)
                return stat("odds_ratio", json("a", p[0].trim(), "b", p[1].trim(), "c", p[2].trim(), "d", p[3].trim()));
        }
        // kaplan_meier[times|status]
        if (e.startsWith("kaplan_meier[") && e.endsWith("]")) {
            String[] g = e.substring(13, e.length() - 1).split("\\|");
            if (g.length == 2)
                return stat("kaplan_meier", "{\"x\":[" + g[0] + "],\"status\":[" + g[1] + "]}");
        }
        // moving_avg[data|window]
        if (e.startsWith("moving_avg[") && e.endsWith("]")) {
            String[] g = e.substring(11, e.length() - 1).split("\\|");
            if (g.length == 2)
                return stat("moving_avg", "{\"x\":[" + g[0] + "],\"window\":" + g[1].trim() + "}");
        }
        // exp_smooth[data|alpha]
        if (e.startsWith("exp_smooth[") && e.endsWith("]")) {
            String[] g = e.substring(11, e.length() - 1).split("\\|");
            if (g.length == 2)
                return stat("exp_smooth", "{\"x\":[" + g[0] + "],\"alpha\":" + g[1].trim() + "}");
        }
        // acf[data] or acf[data|maxlag]
        if (e.startsWith("acf[") && e.endsWith("]")) {
            String[] g = e.substring(4, e.length() - 1).split("\\|");
            String lag = g.length > 1 ? g[1].trim() : "20";
            return stat("acf", "{\"x\":[" + g[0] + "],\"maxlag\":" + lag + "}");
        }
        // xbar_chart[row|row|row]
        if (e.startsWith("xbar_chart[") && e.endsWith("]")) {
            String[] g = e.substring(11, e.length() - 1).split("\\|");
            StringBuilder sb = new StringBuilder("{\"subgroups\":[");
            for (int i = 0; i < g.length; i++) {
                if (i > 0)
                    sb.append(",");
                sb.append("[").append(g[i]).append("]");
            }
            return stat("xbar_chart", sb + "]}");
        }
        // process_cap[data|LSL,USL]
        if (e.startsWith("process_cap[") && e.endsWith("]")) {
            String[] g = e.substring(12, e.length() - 1).split("\\|");
            if (g.length == 2) {
                String[] b = g[1].split(",");
                if (b.length == 2)
                    return stat("process_cap",
                            "{\"x\":[" + g[0] + "],\"LSL\":" + b[0].trim() + ",\"USL\":" + b[1].trim() + "}");
            }
        }
        // shapiro[data]
        if (e.startsWith("shapiro[") && e.endsWith("]"))
            return stat("shapiro_wilk", "{\"x\":[" + e.substring(8, e.length() - 1) + "]}");
        // normal_cdf[x,mu,sigma]
        if (e.startsWith("normal_cdf[") && e.endsWith("]")) {
            String[] p = e.substring(11, e.length() - 1).split(",");
            if (p.length == 3)
                return stat("normal_cdf", json("x", p[0].trim(), "mu", p[1].trim(), "sigma", p[2].trim()));
        }
        // C(n,r) combinations P(n,r) permutations
        if (e.startsWith("C(") && e.endsWith(")")) {
            String[] p = e.substring(2, e.length() - 1).split(",");
            if (p.length == 2)
                return stat("combinations", json("n", p[0].trim(), "r", p[1].trim()));
        }
        if (e.startsWith("P(") && e.endsWith(")")) {
            String[] p = e.substring(2, e.length() - 1).split(",");
            if (p.length == 2)
                return stat("permutations", json("n", p[0].trim(), "r", p[1].trim()));
        }
        // freq_table[data]
        if (e.startsWith("freq_table[") && e.endsWith("]"))
            return stat("freq_table", "{\"x\":[" + e.substring(11, e.length() - 1) + "]}");
        // bootstrap[data]
        if (e.startsWith("bootstrap[") && e.endsWith("]"))
            return stat("bootstrap_ci", "{\"x\":[" + e.substring(10, e.length() - 1) + "],\"alpha\":0.05}");
        // Default: summarize
        return stat("summarize", "{\"x\":[" + e + "]}");
    }

    private static String stat(String op, String jsonPayload) {
        return "stat:" + op + "|" + jsonPayload;
    }

    // ── Applied Mathematics ───────────────────────────────────────────────────

    static String buildApplied(String e) {
        // SIR epidemic model: sir[beta, gamma, S0, I0, R0, tEnd, steps]
        if (e.startsWith("sir[") && e.endsWith("]")) {
            String[] p = e.substring(4, e.length() - 1).split(",");
            if (p.length >= 6)
                return am("sir", json(
                        "beta", p[0].trim(), "gamma", p[1].trim(),
                        "S0", p[2].trim(), "I0", p[3].trim(), "R0", p.length > 4 ? p[4].trim() : "0",
                        "tend", p.length > 5 ? p[5].trim() : "100", "n", p.length > 6 ? p[6].trim() : "500"));
        }
        // SEIR: seir[beta, sigma, gamma, S0, E0, I0, R0, tEnd]
        if (e.startsWith("seir[") && e.endsWith("]")) {
            String[] p = e.substring(5, e.length() - 1).split(",");
            if (p.length >= 7)
                return am("seir", json(
                        "beta", p[0].trim(), "sigma", p[1].trim(), "gamma", p[2].trim(),
                        "S0", p[3].trim(), "E0", p[4].trim(), "I0", p[5].trim(), "R0", p[6].trim(),
                        "tend", p.length > 7 ? p[7].trim() : "100", "n", "500"));
        }
        // Lotka-Volterra: lotka_volterra[alpha,beta,delta,gamma,x0,y0,tEnd]
        if (e.startsWith("lotka_volterra[") && e.endsWith("]")) {
            String[] p = e.substring(15, e.length() - 1).split(",");
            if (p.length >= 6)
                return am("lotka_volterra", json(
                        "alpha", p[0].trim(), "beta", p[1].trim(), "delta", p[2].trim(),
                        "gamma", p[3].trim(), "x0", p[4].trim(), "y0", p[5].trim(),
                        "tend", p.length > 6 ? p[6].trim() : "50", "n", "5000"));
        }
        // Logistic map: logistic[r, x0, n]
        if (e.startsWith("logistic[") && e.endsWith("]")) {
            String[] p = e.substring(9, e.length() - 1).split(",");
            if (p.length >= 2)
                return am("logistic_map", json(
                        "r", p[0].trim(), "x0", p[1].trim(), "n", p.length > 2 ? p[2].trim() : "200"));
        }
        // Euler-Lagrange: euler_lagrange[L, x, y, a, b]
        if (e.startsWith("euler_lagrange[") && e.endsWith("]")) {
            String[] p = e.substring(15, e.length() - 1).split(",");
            if (p.length >= 3)
                return am("euler_lagrange", json(
                        "L", p[0].trim(), "x", p[1].trim(), "y", p[2].trim(),
                        "a", p.length > 3 ? p[3].trim() : "0", "b", p.length > 4 ? p[4].trim() : "1"));
        }
        // Brachistochrone: brachistochrone[x1, y1]
        if (e.startsWith("brachistochrone[") && e.endsWith("]")) {
            String[] p = e.substring(16, e.length() - 1).split(",");
            if (p.length == 2)
                return am("brachistochrone", json(
                        "x1", p[0].trim(), "y1", p[1].trim(), "x0", "0", "y0", "0"));
        }
        // Classify linear system: classify_linear[a,b,c,d]
        if (e.startsWith("classify_linear[") && e.endsWith("]")) {
            String[] p = e.substring(16, e.length() - 1).split(",");
            if (p.length == 4)
                return am("classify_linear", json(
                        "a11", p[0].trim(), "a12", p[1].trim(), "a21", p[2].trim(), "a22", p[3].trim()));
        }
        // SIR R0: r0[beta, gamma, N]
        if (e.startsWith("r0[") && e.endsWith("]")) {
            String[] p = e.substring(3, e.length() - 1).split(",");
            if (p.length == 3)
                return am("R0", json("beta", p[0].trim(), "gamma", p[1].trim(), "N", p[2].trim()));
        }
        // Burgers: burgers[nu, u0, L, T]
        if (e.startsWith("burgers[") && e.endsWith("]")) {
            String[] p = e.substring(8, e.length() - 1).split(",");
            if (p.length >= 2)
                return am("burgers", json(
                        "nu", p[0].trim(), "u0", p[1].trim(),
                        "L", p.length > 2 ? p[2].trim() : "10", "T", p.length > 3 ? p[3].trim() : "1",
                        "nx", "100", "nt", "1000"));
        }
        // D'Alembert: dalembert[f0, g0, c, x, t]
        if (e.startsWith("dalembert[") && e.endsWith("]")) {
            String[] p = e.substring(10, e.length() - 1).split(",");
            if (p.length >= 3)
                return am("dalembert", json(
                        "f0", p[0].trim(), "g0", p[1].trim(), "c", p[2].trim(),
                        "x", p.length > 3 ? p[3].trim() : "1", "t", p.length > 4 ? p[4].trim() : "1"));
        }
        // Sturm-Liouville: sl[p, q, w, a, b, N]
        if (e.startsWith("sl[") && e.endsWith("]")) {
            String[] p = e.substring(3, e.length() - 1).split(",");
            if (p.length >= 6)
                return am("sturm_liouville", json(
                        "p", p[0].trim(), "q", p[1].trim(), "w", p[2].trim(),
                        "a", p[3].trim(), "b", p[4].trim(), "N", p[5].trim()));
        }
        // Reaction-diffusion: reaction_diffusion[D, R, L, n]
        if (e.startsWith("reaction_diffusion[") && e.endsWith("]")) {
            String[] p = e.substring(19, e.length() - 1).split(",");
            if (p.length >= 2)
                return am("reaction_diffusion", json(
                        "D", p[0].trim(), "R", p[1].trim(),
                        "L", p.length > 2 ? p[2].trim() : "1", "n", "100"));
        }
        // Gas shock: gas_shock[gamma, M1]
        if (e.startsWith("gas_shock[") && e.endsWith("]")) {
            String[] p = e.substring(10, e.length() - 1).split(",");
            if (p.length >= 2)
                return am("gas_shock", json(
                        "gamma", p[0].trim(), "M1", p[1].trim(), "p1", "101325", "rho1", "1.225", "T1", "288"));
        }
        // Galton-Watson: galton_watson[p0, p1, p2, ...; generations]
        if (e.startsWith("galton_watson[") && e.endsWith("]")) {
            String inner = e.substring(14, e.length() - 1);
            String[] parts = inner.split(";");
            String pk = parts[0].trim();
            String gen = parts.length > 1 ? parts[1].trim() : "10";
            return "am:galton_watson|{\"pk\":[" + pk + "],\"generations\":" + gen + "}";
        }
        // Default: treat as SIR with guessed params
        return am("sir",
                json("beta", "0.3", "gamma", "0.1", "S0", "990", "I0", "10", "R0", "0", "tend", "100", "n", "500"));
    }

    private static String am(String op, String jsonPayload) {
        return "am:" + op + "|" + jsonPayload;
    }

    private static String de(String op, String jsonPayload) {
        return "de:" + op + "|" + jsonPayload;
    }

    // ── Number Theory ─────────────────────────────────────────────────────────
    static String buildNumberTheory(String e) {
        if (e.startsWith("prime_factors[") && e.endsWith("]"))
            return nt("prime_factors", json("n", e.substring(14, e.length() - 1).trim()));
        if (e.startsWith("gcd[") && e.endsWith("]")) {
            String[] p = e.substring(4, e.length() - 1).split(",");
            return nt("gcd", json("a", p[0].trim(), "b", p[1].trim()));
        }
        if (e.startsWith("lcm[") && e.endsWith("]")) {
            String[] p = e.substring(4, e.length() - 1).split(",");
            return nt("lcm", json("a", p[0].trim(), "b", p[1].trim()));
        }
        if (e.startsWith("mod_pow[") && e.endsWith("]")) {
            String[] p = e.substring(8, e.length() - 1).split(",");
            return nt("mod_pow", json("base", p[0].trim(), "exp", p[1].trim(), "mod", p[2].trim()));
        }
        if (e.startsWith("mod_inv[") && e.endsWith("]")) {
            String[] p = e.substring(8, e.length() - 1).split(",");
            return nt("mod_inverse", json("a", p[0].trim(), "mod", p[1].trim()));
        }
        if (e.startsWith("fibonacci[") && e.endsWith("]"))
            return nt("fibonacci", json("n", e.substring(10, e.length() - 1).trim()));
        if (e.startsWith("is_prime[") && e.endsWith("]"))
            return nt("is_prime", json("n", e.substring(9, e.length() - 1).trim()));
        if (e.startsWith("euler_phi[") && e.endsWith("]"))
            return nt("euler_phi", json("n", e.substring(10, e.length() - 1).trim()));
        if (e.startsWith("crt[") && e.endsWith("]")) {
            String inner = e.substring(4, e.length() - 1);
            String[] parts = inner.split("\\|");
            return "nt:crt|{\"r\":[" + parts[0] + "],\"m\":[" + parts[1] + "]}";
        }
        if (e.startsWith("miller_rabin[") && e.endsWith("]"))
            return nt("miller_rabin", json("n", e.substring(13, e.length() - 1).trim()));
        if (e.startsWith("catalan[") && e.endsWith("]"))
            return nt("catalan", json("n", e.substring(8, e.length() - 1).trim()));
        if (e.startsWith("stirling2[") && e.endsWith("]")) {
            String[] p = e.substring(10, e.length() - 1).split(",");
            return nt("stirling2", json("n", p[0].trim(), "k", p[1].trim()));
        }
        if (e.startsWith("linear_dioph[") && e.endsWith("]")) {
            String[] p = e.substring(13, e.length() - 1).split(",");
            return nt("linear_dioph", json("a", p[0].trim(), "b", p[1].trim(), "c", p[2].trim()));
        }
        if (e.startsWith("primitive_root[") && e.endsWith("]"))
            return nt("primitive_root", json("p", e.substring(15, e.length() - 1).trim()));
        return nt("prime_factors", json("n", e));
    }

    private static String nt(String op, String j) {
        return "nt:" + op + "|" + j;
    }

    // ── Discrete Mathematics ──────────────────────────────────────────────────

    private static String dm(String op, String j) {
        return "dm:" + op + "|" + j;
    }

    // ── Complex Analysis ──────────────────────────────────────────────────────
    static String buildComplex(String e) {
        if (e.startsWith("polar[") && e.endsWith("]")) {
            String[] p = e.substring(6, e.length() - 1).split(",");
            return ca("polar", json("re", p[0].trim(), "im", p[1].trim()));
        }
        if (e.startsWith("cauchy_riemann[") && e.endsWith("]")) {
            String[] p = e.substring(15, e.length() - 1).split("\\|");
            return ca("cauchy_riemann", json("u", p[0].trim(), "v", p[1].trim(), "x", p.length > 2 ? p[2].trim() : "x",
                    "y", p.length > 3 ? p[3].trim() : "y", "x0", "0", "y0", "0"));
        }
        if (e.startsWith("residue[") && e.endsWith("]")) {
            String[] p = e.substring(8, e.length() - 1).split("\\|");
            return ca("residue",
                    json("f", p[0].trim(), "re", p[1].trim(), "im", p.length > 2 ? p[2].trim() : "0", "order", "1"));
        }
        if (e.startsWith("zeta[") && e.endsWith("]")) {
            String[] p = e.substring(5, e.length() - 1).split(",");
            return ca("zeta", json("re", p[0].trim(), "im", p.length > 1 ? p[1].trim() : "0", "terms", "1000"));
        }
        if (e.startsWith("mobius[") && e.endsWith("]")) {
            // Format: mobius[a,b,c,d|re,im]  — matrix params | point to map
            String[] pipes = e.substring(7, e.length() - 1).split("\\|");
            String[] abcd  = pipes[0].split(",");
            String[] z     = pipes.length > 1 ? pipes[1].split(",") : new String[]{"0","0"};
            if (abcd.length >= 4 && z.length >= 2)
                return ca("mobius", json("a", abcd[0].trim(), "b", abcd[1].trim(),
                        "c", abcd[2].trim(), "d", abcd[3].trim(),
                        "re", z[0].trim(), "im", z.length > 1 ? z[1].trim() : "0"));
        }
        if (e.startsWith("complex_exp[") && e.endsWith("]")) {
            String[] p = e.substring(12, e.length() - 1).split(",");
            return ca("complex_exp", json("re", p[0].trim(), "im", p[1].trim()));
        }
        if (e.startsWith("complex_log[") && e.endsWith("]")) {
            String[] p = e.substring(12, e.length() - 1).split(",");
            return ca("complex_log", json("re", p[0].trim(), "im", p[1].trim()));
        }
        if (e.startsWith("all_roots[") && e.endsWith("]")) {
            String[] p = e.substring(10, e.length() - 1).split(",");
            return ca("all_roots", json("re", p[0].trim(), "im", p[1].trim(), "n", p[2].trim()));
        }
        if (e.startsWith("harmonic_conj[") && e.endsWith("]"))
            return ca("harmonic_conj", json("u", e.substring(14, e.length() - 1).trim(), "x", "x", "y", "y"));
        if (e.startsWith("laplacian[") && e.endsWith("]"))
            return ca("laplacian_check", json("u", e.substring(10, e.length() - 1).trim(), "x", "x", "y", "y"));
        if (e.startsWith("gamma_c[") && e.endsWith("]")) {
            String[] p = e.substring(8, e.length() - 1).split(",");
            return ca("gamma_c", json("re", p[0].trim(), "im", p.length > 1 ? p[1].trim() : "0"));
        }
        return ca("polar", json("re", "3", "im", "4"));
    }

    private static String ca(String op, String j) {
        return "ca:" + op + "|" + j;
    }

    // ── Numerical Analysis ────────────────────────────────────────────────────
    static String buildNumerical(String e) {
        if (e.startsWith("newton[") && e.endsWith("]")) {
            String[] p = e.substring(7, e.length() - 1).split("\\|");
            return na("newton", json("f", p[0].trim(), "x", p.length > 1 ? p[1].trim() : "x", "x0",
                    p.length > 2 ? p[2].trim() : "1"));
        }
        if (e.startsWith("bisection[") && e.endsWith("]")) {
            String[] p = e.substring(10, e.length() - 1).split("\\|");
            return na("bisection", json("f", p[0].trim(), "x", p.length > 1 ? p[1].trim() : "x", "a",
                    p.length > 2 ? p[2].trim() : "0", "b", p.length > 3 ? p[3].trim() : "2"));
        }
        if (e.startsWith("secant[") && e.endsWith("]")) {
            String[] p = e.substring(7, e.length() - 1).split("\\|");
            return na("secant", json("f", p[0].trim(), "x", p.length > 1 ? p[1].trim() : "x", "x0",
                    p.length > 2 ? p[2].trim() : "0", "x1", p.length > 3 ? p[3].trim() : "1"));
        }
        if (e.startsWith("brent[") && e.endsWith("]")) {
            String[] p = e.substring(6, e.length() - 1).split("\\|");
            return na("brent", json("f", p[0].trim(), "x", p.length > 1 ? p[1].trim() : "x", "a",
                    p.length > 2 ? p[2].trim() : "0", "b", p.length > 3 ? p[3].trim() : "2"));
        }
        if (e.startsWith("lagrange[") && e.endsWith("]")) {
            String[] g = e.substring(9, e.length() - 1).split("\\|");
            return "na:lagrange|{\"xs\":" + g[0] + ",\"ys\":" + g[1] + ",\"xeval\":" + g[2] + "}";
        }
        if (e.startsWith("spline[") && e.endsWith("]")) {
            String[] g = e.substring(7, e.length() - 1).split("\\|");
            return "na:cubic_spline|{\"xs\":" + g[0] + ",\"ys\":" + g[1] + ",\"xeval\":" + g[2] + "}";
        }
        if (e.startsWith("romberg[") && e.endsWith("]")) {
            String[] p = e.substring(8, e.length() - 1).split("\\|");
            return na("romberg", json("f", p[0].trim(), "x", p.length > 1 ? p[1].trim() : "x", "a",
                    p.length > 2 ? p[2].trim() : "0", "b", p.length > 3 ? p[3].trim() : "1"));
        }
        if (e.startsWith("simpsons[") && e.endsWith("]")) {
            String[] p = e.substring(9, e.length() - 1).split("\\|");
            return na("simpsons",
                    json("f", p[0].trim(), "x", p.length > 1 ? p[1].trim() : "x", "a", p.length > 2 ? p[2].trim() : "0",
                            "b", p.length > 3 ? p[3].trim() : "1", "n", p.length > 4 ? p[4].trim() : "100"));
        }
        if (e.startsWith("gauss_leg[") && e.endsWith("]")) {
            String[] p = e.substring(10, e.length() - 1).split("\\|");
            return na("gauss_legendre", json("f", p[0].trim(), "x", "x", "a", p.length > 1 ? p[1].trim() : "0", "b",
                    p.length > 2 ? p[2].trim() : "1"));
        }
        if (e.startsWith("gauss_elim[") && e.endsWith("]")) {
            String[] g = e.substring(11, e.length() - 1).split("\\|");
            return "na:gauss_elim|{\"A\":" + g[0] + ",\"b\":" + g[1] + "}";
        }
        if (e.startsWith("jacobi[") && e.endsWith("]")) {
            String[] g = e.substring(7, e.length() - 1).split("\\|");
            return "na:jacobi_iter|{\"A\":" + g[0] + ",\"b\":" + g[1] + "}";
        }
        if (e.startsWith("central_diff[") && e.endsWith("]")) {
            String[] p = e.substring(13, e.length() - 1).split("\\|");
            return na("central_diff", json("f", p[0].trim(), "x", "x", "x0", p.length > 1 ? p[1].trim() : "1"));
        }
        // numerical_int[f|x|a|b] — direct calc: dispatch
        if (e.startsWith("numerical_int[") && e.endsWith("]")) {
            String[] p = e.substring(14, e.length() - 1).split("\\|");
            if (p.length >= 4)
                return calc("definite_int",
                        json("expr", p[0].trim(), "var", p[1].trim(), "a", p[2].trim(), "b", p[3].trim()));
        }
        return na("newton", json("f", "x^3-x-2", "x", "x", "x0", "1.5"));
    }

    private static String na(String op, String j) {
        return "na:" + op + "|" + j;
    }

    // ── Abstract Algebra ──────────────────────────────────────────────────────
    static String buildAlgebra(String e) {
        if (e.startsWith("cyclic[") && e.endsWith("]"))
            return aa("cyclic", json("n", e.substring(7, e.length() - 1).trim()));
        if (e.startsWith("dihedral[") && e.endsWith("]"))
            return aa("dihedral", json("n", e.substring(9, e.length() - 1).trim()));
        if (e.startsWith("symmetric[") && e.endsWith("]"))
            return aa("symmetric", json("n", e.substring(10, e.length() - 1).trim()));
        if (e.startsWith("poly_add[") && e.endsWith("]")) {
            String[] g = e.substring(9, e.length() - 1).split("\\|");
            return "aa:poly_add|{\"A\":" + g[0] + ",\"B\":" + g[1] + (g.length > 2 ? ",\"mod\":" + g[2] : "") + "}";
        }
        if (e.startsWith("poly_mul[") && e.endsWith("]")) {
            String[] g = e.substring(9, e.length() - 1).split("\\|");
            return "aa:poly_mul|{\"A\":" + g[0] + ",\"B\":" + g[1] + (g.length > 2 ? ",\"mod\":" + g[2] : "") + "}";
        }
        if (e.startsWith("poly_div[") && e.endsWith("]")) {
            String[] g = e.substring(9, e.length() - 1).split("\\|");
            return "aa:poly_div|{\"A\":" + g[0] + ",\"B\":" + g[1] + (g.length > 2 ? ",\"mod\":" + g[2] : "") + "}";
        }
        if (e.startsWith("poly_gcd[") && e.endsWith("]")) {
            String[] g = e.substring(9, e.length() - 1).split("\\|");
            return "aa:poly_gcd|{\"A\":" + g[0] + ",\"B\":" + g[1] + (g.length > 2 ? ",\"mod\":" + g[2] : "") + "}";
        }
        if (e.startsWith("poly_eval[") && e.endsWith("]")) {
            String[] g = e.substring(10, e.length() - 1).split("\\|");
            return "aa:poly_eval|{\"poly\":" + g[0] + ",\"x\":" + g[1] + (g.length > 2 ? ",\"mod\":" + g[2] : "") + "}";
        }
        if (e.startsWith("galois_field[") && e.endsWith("]")) {
            String[] p = e.substring(13, e.length() - 1).split(",");
            return aa("galois_field", json("p", p[0].trim(), "n", p.length > 1 ? p[1].trim() : "1"));
        }
        if (e.startsWith("perm_cycle[") && e.endsWith("]"))
            return "aa:perm_cycle|{\"perm\":" + e.substring(11, e.length() - 1) + "}";
        if (e.startsWith("perm_order[") && e.endsWith("]"))
            return "aa:perm_order|{\"perm\":" + e.substring(11, e.length() - 1) + "}";
        if (e.startsWith("perm_compose[") && e.endsWith("]")) {
            String[] g = e.substring(13, e.length() - 1).split("\\|");
            return "aa:perm_compose|{\"p\":" + g[0] + ",\"q\":" + g[1] + "}";
        }
        if (e.startsWith("sylow[") && e.endsWith("]")) {
            String[] p = e.substring(6, e.length() - 1).split(",");
            return aa("sylow", json("p", p[0].trim(), "n", p[1].trim()));
        }
        if (e.startsWith("ring_zn[") && e.endsWith("]"))
            return aa("ring_zn", json("n", e.substring(8, e.length() - 1).trim()));
        if (e.startsWith("cyclotomic[") && e.endsWith("]"))
            return aa("cyclotomic", json("n", e.substring(11, e.length() - 1).trim()));
        return aa("cyclic", json("n", "6"));
    }

    static String buildMatrix(String e) {
        // Direct la: dispatch — pass straight through
        if (e.startsWith("la:"))
            return e;

        // Named operations — map to correct C++ op names (see LinearAlgebra.cpp
        // dispatch)
        // Format sent to C++: la:<op>|<raw_input>
        // LA input uses | as separator between multiple matrices/vectors
        if (e.startsWith("inverse[") || e.startsWith("inv["))
            return "la:inverse|" + stripBrackets(e);
        if (e.startsWith("eigen["))
            return "la:eigen_full|" + stripBrackets(e);
        if (e.startsWith("eigenvalues["))
            return "la:eigenvalues|" + stripBrackets(e);
        if (e.startsWith("eigenvectors["))
            return "la:eigenvectors|" + stripBrackets(e);
        if (e.startsWith("svd[") || e.startsWith("decomp_svd["))
            return "la:decomp_svd|" + stripBrackets(e);
        if (e.startsWith("lu[") || e.startsWith("decomp_lu["))
            return "la:decomp_lu|" + stripBrackets(e);
        if (e.startsWith("qr[") || e.startsWith("decomp_qr["))
            return "la:decomp_qr_hh|" + stripBrackets(e);
        if (e.startsWith("cholesky["))
            return "la:decomp_cholesky|" + stripBrackets(e);
        if (e.startsWith("null_space[") || e.startsWith("null["))
            return "la:null_space|" + stripBrackets(e);
        if (e.startsWith("rank["))
            return "la:rank|" + stripBrackets(e);
        if (e.startsWith("rref["))
            return "la:rref|" + stripBrackets(e);
        if (e.startsWith("ref["))
            return "la:ref|" + stripBrackets(e);
        if (e.startsWith("det[") || e.startsWith("determinant["))
            return "la:determinant|" + stripBrackets(e);
        if (e.startsWith("trace["))
            return "la:trace|" + stripBrackets(e);
        if (e.startsWith("transpose["))
            return "la:matrix_transpose|" + stripBrackets(e);
        if (e.startsWith("solve[")) {
            // solve[A|b]
            String inner = e.substring(6, e.length() - 1);
            return "la:solve|" + inner;
        }
        if (e.startsWith("dot[") || e.startsWith("dot_product[")) {
            String inner = stripBrackets(e);
            return "la:dot_product|" + inner;
        }
        if (e.startsWith("cross[") || e.startsWith("cross_product[")) {
            String inner = stripBrackets(e);
            return "la:cross_product|" + inner;
        }
        if (e.startsWith("norm[") || e.startsWith("vector_norm[")) {
            String inner = stripBrackets(e);
            return "la:vector_norm|" + inner + "|2"; // default L2
        }
        if (e.startsWith("diagonalize["))
            return "la:diagonalize|" + stripBrackets(e);
        if (e.startsWith("pseudoinverse["))
            return "la:pseudoinverse|" + stripBrackets(e);
        if (e.startsWith("projection[")) {
            String inner = e.substring(11, e.length() - 1);
            return "la:projection|" + inner;
        }
        // Default: full classify (det + inv + eigen + properties)
        return "la:classify|" + e;
    }

    private static String stripBrackets(String e) {
        // "op[...content...]" -> "content"
        int ob = e.indexOf('[');
        return ob >= 0 ? e.substring(ob + 1, e.length() - 1) : e;
    }

    private static String aa(String op, String j) {
        return "aa:" + op + "|" + j;
    }

    // ── Probability Theory ────────────────────────────────────────────────────
    static String buildProbability(String e) {
        if (e.startsWith("mgf_normal[") && e.endsWith("]")) {
            String[] p = e.substring(11, e.length() - 1).split(",");
            return prob("mgf_normal", json("mu", p[0].trim(), "sigma", p[1].trim(), "t", p[2].trim()));
        }
        if (e.startsWith("mgf_poisson[") && e.endsWith("]")) {
            String[] p = e.substring(12, e.length() - 1).split(",");
            return prob("mgf_poisson", json("lambda", p[0].trim(), "t", p[1].trim()));
        }
        if (e.startsWith("mgf_exp[") && e.endsWith("]")) {
            String[] p = e.substring(8, e.length() - 1).split(",");
            return prob("mgf_exp", json("lambda", p[0].trim(), "t", p[1].trim()));
        }
        if (e.startsWith("berry_esseen[") && e.endsWith("]")) {
            String[] p = e.substring(13, e.length() - 1).split(",");
            return prob("berry_esseen",
                    json("mu", p[0].trim(), "sigma", p[1].trim(), "rho", p[2].trim(), "n", p[3].trim()));
        }
        if (e.startsWith("clt[") && e.endsWith("]")) {
            String[] p = e.substring(4, e.length() - 1).split(",");
            return prob("clt_approx",
                    json("mu", p[0].trim(), "sigma", p[1].trim(), "n", p[2].trim(), "x", p[3].trim()));
        }
        if (e.startsWith("gbm[") && e.endsWith("]")) {
            String[] p = e.substring(4, e.length() - 1).split(",");
            return prob("gbm",
                    json("S0", p[0].trim(), "mu", p[1].trim(), "sigma", p[2].trim(), "t", p[3].trim(), "steps", "252"));
        }
        if (e.startsWith("gamblers_ruin[") && e.endsWith("]")) {
            String[] p = e.substring(14, e.length() - 1).split(",");
            return prob("gamblers_ruin", json("p", p[0].trim(), "start", p[1].trim(), "target", p[2].trim()));
        }
        if (e.startsWith("poisson_proc[") && e.endsWith("]")) {
            String[] p = e.substring(13, e.length() - 1).split(",");
            return prob("poisson_proc", json("lambda", p[0].trim(), "t", p[1].trim(), "k", "10"));
        }
        if (e.startsWith("brownian[") && e.endsWith("]"))
            return prob("brownian", json("t", e.substring(9, e.length() - 1).trim(), "steps", "100"));
        if (e.startsWith("chebyshev[") && e.endsWith("]")) {
            String[] p = e.substring(10, e.length() - 1).split(",");
            return prob("chebyshev_ineq", json("mu", p[0].trim(), "sigma", p[1].trim(), "k", p[2].trim()));
        }
        if (e.startsWith("markov_ineq[") && e.endsWith("]")) {
            String[] p = e.substring(12, e.length() - 1).split(",");
            return prob("markov_ineq", json("mu", p[0].trim(), "a", p[1].trim()));
        }
        if (e.startsWith("joint_normal[") && e.endsWith("]")) {
            String[] p = e.substring(13, e.length() - 1).split(",");
            if (p.length >= 7)
                return prob("joint_normal", json("mu1", p[0].trim(), "mu2", p[1].trim(), "s1", p[2].trim(), "s2",
                        p[3].trim(), "rho", p[4].trim(), "x1", p[5].trim(), "x2", p[6].trim()));
        }
        if (e.startsWith("mc_int[") && e.endsWith("]")) {
            String[] p = e.substring(7, e.length() - 1).split("\\|");
            return prob("mc_integrate", json("f", p[0].trim(), "a", p[1].trim(), "b", p[2].trim(), "n", "100000"));
        }
        // markov[P|init|steps] — Markov chain steady-state / n-step
        if (e.startsWith("markov[") && e.endsWith("]")) {
            String[] g = e.substring(7, e.length() - 1).split("\\|");
            if (g.length >= 2) {
                String steps = g.length > 2 ? ",\"steps\":" + g[2].trim() : "";
                return "prob:markov_steady|{\"P\":" + g[0].trim() + ",\"init\":" + g[1].trim() + steps + "}";
            }
        }
        // normal_pdf/cdf/binomial_pmf/poisson_pmf — route through stat module
        if (e.startsWith("normal_pdf[") && e.endsWith("]")) {
            String[] p = e.substring(11, e.length() - 1).split(",");
            return stat("normal_pdf", json("x", p[0].trim(), "mu", p[1].trim(), "sigma", p[2].trim()));
        }
        if (e.startsWith("normal_cdf[") && e.endsWith("]")) {
            String[] p = e.substring(11, e.length() - 1).split(",");
            return stat("normal_cdf", json("x", p[0].trim(), "mu", p[1].trim(), "sigma", p[2].trim()));
        }
        if (e.startsWith("binomial_pmf[") && e.endsWith("]")) {
            String[] p = e.substring(13, e.length() - 1).split(",");
            return stat("binomial_pmf", json("k", p[0].trim(), "n", p[1].trim(), "p", p[2].trim()));
        }
        if (e.startsWith("poisson_pmf[") && e.endsWith("]")) {
            String[] p = e.substring(12, e.length() - 1).split(",");
            return stat("poisson_pmf", json("k", p[0].trim(), "lambda", p[1].trim()));
        }
        return prob("mgf_normal", json("mu", "0", "sigma", "1", "t", "0.5"));
    }

    private static String prob(String op, String j) {
        return "prob:" + op + "|" + j;
    }

    // ── Geometry ──────────────────────────────────────────────────────────────
    static String buildGeometry(String e) {
        if (e.startsWith("circle[") && e.endsWith("]")) {
            String[] p = e.substring(7, e.length() - 1).split(",");
            return geo("circle", json("cx", p[0].trim(), "cy", p[1].trim(), "r", p[2].trim()));
        }
        if (e.startsWith("ellipse[") && e.endsWith("]")) {
            String[] p = e.substring(8, e.length() - 1).split(",");
            return geo("ellipse", json("cx", p[0].trim(), "cy", p[1].trim(), "a", p[2].trim(), "b", p[3].trim(),
                    "theta", p.length > 4 ? p[4].trim() : "0"));
        }
        if (e.startsWith("hyperbola[") && e.endsWith("]")) {
            String[] p = e.substring(10, e.length() - 1).split(",");
            return geo("hyperbola",
                    json("cx", p[0].trim(), "cy", p[1].trim(), "a", p[2].trim(), "b", p[3].trim(), "dir", "h"));
        }
        if (e.startsWith("parabola[") && e.endsWith("]")) {
            String[] p = e.substring(9, e.length() - 1).split(",");
            return geo("parabola", json("h", p[0].trim(), "k", p[1].trim(), "p", p[2].trim()));
        }
        if (e.startsWith("distance_2d[") && e.endsWith("]")) {
            String[] p = e.substring(12, e.length() - 1).split(",");
            return geo("distance_2d", json("x1", p[0].trim(), "y1", p[1].trim(), "x2", p[2].trim(), "y2", p[3].trim()));
        }
        if (e.startsWith("distance_3d[") && e.endsWith("]")) {
            String[] p = e.substring(12, e.length() - 1).split("\\|");
            String[] a = p[0].split(","), b = p[1].split(",");
            return geo("distance_3d", json("x1", a[0].trim(), "y1", a[1].trim(), "z1", a[2].trim(), "x2", b[0].trim(),
                    "y2", b[1].trim(), "z2", b[2].trim()));
        }
        if (e.startsWith("cross_product[") && e.endsWith("]")) {
            String[] p = e.substring(14, e.length() - 1).split("\\|");
            String[] a = p[0].split(","), b = p[1].split(",");
            return geo("cross_product", json("ax", a[0].trim(), "ay", a[1].trim(), "az", a[2].trim(), "bx", b[0].trim(),
                    "by", b[1].trim(), "bz", b[2].trim()));
        }
        if (e.startsWith("plane_3pts[") && e.endsWith("]")) {
            String[] g = e.substring(11, e.length() - 1).split("\\|");
            String[] a = g[0].split(","), b = g[1].split(","), c = g[2].split(",");
            return geo("plane_3pts", json("x1", a[0].trim(), "y1", a[1].trim(), "z1", a[2].trim(), "x2", b[0].trim(),
                    "y2", b[1].trim(), "z2", b[2].trim(), "x3", c[0].trim(), "y3", c[1].trim(), "z3", c[2].trim()));
        }
        if (e.startsWith("sphere[") && e.endsWith("]")) {
            String[] p = e.substring(7, e.length() - 1).split(",");
            if (p.length == 1)
                return geo("sphere", json("cx", "0", "cy", "0", "cz", "0", "r", p[0].trim()));
            if (p.length >= 4)
                return geo("sphere", json("cx", p[0].trim(), "cy", p[1].trim(), "cz", p[2].trim(), "r", p[3].trim()));
            return geo("sphere", json("cx", "0", "cy", "0", "cz", "0", "r", p[0].trim()));
        }
        if (e.startsWith("param_length[") && e.endsWith("]")) {
            String[] g = e.substring(13, e.length() - 1).split("\\|");
            return geo("param_length",
                    json("x", g[0].trim(), "y", g[1].trim(), "t", "t", "t0", g[2].trim(), "t1", g[3].trim()));
        }
        if (e.startsWith("polar_to_rect[") && e.endsWith("]")) {
            String[] p = e.substring(14, e.length() - 1).split(",");
            return geo("polar_to_rect", json("r", p[0].trim(), "theta", p[1].trim()));
        }
        if (e.startsWith("polar_area[") && e.endsWith("]")) {
            String[] g = e.substring(11, e.length() - 1).split("\\|");
            return geo("polar_area", json("r", g[0].trim(), "theta", "theta", "t0", g[1].trim(), "t1", g[2].trim()));
        }
        if (e.startsWith("rotate[") && e.endsWith("]")) {
            String[] p = e.substring(7, e.length() - 1).split(",");
            return geo("rotate_2d", json("px", p[0].trim(), "py", p[1].trim(), "cx", p[2].trim(), "cy", p[3].trim(),
                    "angle", p[4].trim()));
        }
        if (e.startsWith("classify_conic[") && e.endsWith("]")) {
            String[] p = e.substring(15, e.length() - 1).split(",");
            if (p.length >= 6)
                return geo("classify_conic", json("A", p[0].trim(), "B", p[1].trim(), "C", p[2].trim(), "D",
                        p[3].trim(), "E", p[4].trim(), "F", p[5].trim()));
        }
        if (e.startsWith("triangle_area[") && e.endsWith("]")) {
            String[] g = e.substring(14, e.length() - 1).split("\\|");
            String[] a = g[0].split(","), b = g[1].split(","), c = g[2].split(",");
            return geo("triangle_area", json("x1", a[0].trim(), "y1", a[1].trim(), "x2", b[0].trim(), "y2", b[1].trim(),
                    "x3", c[0].trim(), "y3", c[1].trim()));
        }
        return geo("circle", json("cx", "0", "cy", "0", "r", "1"));
    }

    private static String geo(String op, String j) {
        return "geo:" + op + "|" + j;
    }

    // ── LaTeX normaliser ─────────────────────────────────────────────────────
    /**
     * Converts LaTeX notation into the plain-math format the C++ engine accepts.
     *
     * INTEGRAL:
     * \int_0^8 x^2\,dx → x^2, 0, 8
     * \int_{-1}^{1} e^x dx → e^x, -1, 1
     * \int x^2 dx → x^2
     *
     * DERIVATIVE:
     * \frac{d}{dx}[x^3] → d/dx[x^3]
     * \frac{d^2}{dx^2}[f] → d2/dx2[f]
     *
     * ANY:
     * \frac{a}{b} → (a)/(b) \sqrt{x} → sqrt(x) \pi → pi
     */
    static String normaliseLatex(String raw, OperationPanel.Operation op) {
        if (raw == null)
            return "";
        String s = raw.trim();

        // Strip surrounding $...$ or \[...\]
        if (s.startsWith("$") && s.endsWith("$") && s.length() > 2)
            s = s.substring(1, s.length() - 1).trim();
        if (s.startsWith("\\[") && s.endsWith("\\]") && s.length() > 4)
            s = s.substring(3, s.length() - 3).trim();

        // ── Definite integral: \int_{a}^{b} expr d<var> ─────────────────────
        if (op == OperationPanel.Operation.INTEGRAL) {
            // Handles \int_0^8, \int_{0}^{8}, \int_{-1}^{\pi}
            java.util.regex.Matcher m = java.util.regex.Pattern.compile(
                    "\\\\int\\s*_\\s*\\{?([^}^\\s,]+)\\}?\\s*\\^\\s*\\{?([^}\\s,]+)\\}?\\s*(.+?)\\s*(?:\\\\[,;])?\\s*d([a-zA-Z])\\s*$",
                    java.util.regex.Pattern.DOTALL).matcher(s);
            if (m.find()) {
                String lo = cleanExprOnly(m.group(1));
                String hi = cleanExprOnly(m.group(2));
                String expr = cleanExprOnly(m.group(3).trim());
                return expr + ", " + lo + ", " + hi;
            }
            // Indefinite: \int expr dx
            m = java.util.regex.Pattern.compile(
                    "\\\\int\\s*(.+?)\\s*(?:\\\\[,;])?\\s*d([a-zA-Z])\\s*$",
                    java.util.regex.Pattern.DOTALL).matcher(s);
            if (m.find()) {
                return cleanExprOnly(m.group(1).trim());
            }
            // Bare \int with nothing else parseable — strip and pass through
            if (s.startsWith("\\int")) {
                s = cleanExprOnly(s.substring(4).trim());
            }
        }

        // ── Derivative: \frac{d}{dx}[f] or \frac{d^n}{dx^n}[f] ───────────
        if (op == OperationPanel.Operation.DERIVATIVE) {
            java.util.regex.Matcher m = java.util.regex.Pattern.compile(
                    "\\\\frac\\{d(?:\\^(\\d+))?\\}\\{d([a-zA-Z])(?:\\^(\\d+))?\\}\\s*\\[(.+)\\]",
                    java.util.regex.Pattern.DOTALL).matcher(s);
            if (m.find()) {
                String order = m.group(1) != null ? m.group(1) : "1";
                String var = m.group(2);
                String body = cleanExprOnly(m.group(4).trim());
                return order.equals("1")
                        ? "d/d" + var + "[" + body + "]"
                        : "d" + order + "/d" + var + order + "[" + body + "]";
            }
            // \frac{d}{dx} f (no brackets — applies to following expression)
            m = java.util.regex.Pattern.compile(
                    "\\\\frac\\{d\\}\\{d([a-zA-Z])\\}\\s*(.+)",
                    java.util.regex.Pattern.DOTALL).matcher(s);
            if (m.find())
                return "d/d" + m.group(1) + "[" + cleanExprOnly(m.group(2).trim()) + "]";
        }

        // ── Limit: \lim_{x \to a} expr ─────────────────────────────────────
        if (op == OperationPanel.Operation.LIMIT) {
            java.util.regex.Matcher m = java.util.regex.Pattern.compile(
                    "\\\\lim\\s*_\\s*\\{?([a-zA-Z])\\s*\\\\to\\s*([^}\\s]+)\\}?\\s*(.+)",
                    java.util.regex.Pattern.DOTALL).matcher(s);
            if (m.find()) {
                String var = m.group(1);
                String to = cleanExprOnly(m.group(2).trim());
                String expr = cleanExprOnly(m.group(3).trim());
                return expr + " as " + var + "->" + to;
            }
        }

        // ── Universal symbol cleanup (all modes) ──────────────────────────────
        return cleanExprOnly(s);
    }

    /**
     * Replace LaTeX commands with plain-math equivalents the C++ parser accepts.
     * Every replaceAll first arg is a Java String literal → regex.
     * Rule: to match one LaTeX backslash in input, write \\\\ in Java source
     * (= string \\ = regex \ = matches one literal \).
     * No: to match one \ we need Java source \\ → string \ → regex \ ✓
     */
    static String cleanExprOnly(String s) {
        if (s == null)
            return "";
        s = s.trim();

        // \frac{a}{b} → (a)/(b) [iterate for nested fracs]
        java.util.regex.Pattern fracPat = java.util.regex.Pattern.compile("\\\\frac\\{([^{}]+)\\}\\{([^{}]+)\\}");
        java.util.regex.Matcher fm;
        int safety = 0;
        while ((fm = fracPat.matcher(s)).find() && safety++ < 20)
            s = fm.replaceFirst("($1)/($2)");

        // \sqrt{x} → sqrt(x), \sqrt x → sqrt(x)
        s = s.replaceAll("\\\\sqrt\\{([^}]+)\\}", "sqrt($1)");
        s = s.replaceAll("\\\\sqrt\\s+([^{\\s]+)", "sqrt($1)");

        // \left( \right) → ( ) etc.
        s = s.replaceAll("\\\\left\\s*\\(", "(")
                .replaceAll("\\\\right\\s*\\)", ")");
        s = s.replaceAll("\\\\left\\s*\\[", "[")
                .replaceAll("\\\\right\\s*\\]", "]");
        s = s.replaceAll("\\\\left\\s*\\|", "abs(")
                .replaceAll("\\\\right\\s*\\|", ")");
        s = s.replaceAll("\\\\left\\s*\\\\\\{", "{")
                .replaceAll("\\\\right\\s*\\\\\\}", "}");

        // Operators
        s = s.replaceAll("\\\\cdot", "*")
                .replaceAll("\\\\times", "*")
                .replaceAll("\\\\pm", "+-")
                .replaceAll("\\\\div", "/");

        // Greek letters (use negative lookahead so \pi doesn't eat \pi_n)
        s = s.replaceAll("\\\\pi(?![a-zA-Z])", "pi")
                .replaceAll("\\\\infty(?![a-zA-Z])", "inf")
                .replaceAll("\\\\alpha(?![a-zA-Z])", "alpha")
                .replaceAll("\\\\beta(?![a-zA-Z])", "beta")
                .replaceAll("\\\\gamma(?![a-zA-Z])", "gamma")
                .replaceAll("\\\\delta(?![a-zA-Z])", "delta")
                .replaceAll("\\\\theta(?![a-zA-Z])", "theta")
                .replaceAll("\\\\phi(?![a-zA-Z])", "phi")
                .replaceAll("\\\\psi(?![a-zA-Z])", "psi")
                .replaceAll("\\\\omega(?![a-zA-Z])", "omega")
                .replaceAll("\\\\sigma(?![a-zA-Z])", "sigma")
                .replaceAll("\\\\lambda(?![a-zA-Z])", "lambda")
                .replaceAll("\\\\epsilon(?![a-zA-Z])", "epsilon")
                .replaceAll("\\\\mu(?![a-zA-Z])", "mu")
                .replaceAll("\\\\eta(?![a-zA-Z])", "eta")
                .replaceAll("\\\\rho(?![a-zA-Z])", "rho")
                .replaceAll("\\\\tau(?![a-zA-Z])", "tau")
                .replaceAll("\\\\xi(?![a-zA-Z])", "xi")
                .replaceAll("\\\\nu(?![a-zA-Z])", "nu")
                .replaceAll("\\\\kappa(?![a-zA-Z])", "kappa")
                .replaceAll("\\\\zeta(?![a-zA-Z])", "zeta");

        // Trig / log (remove the backslash, keep the function name)
        s = s.replaceAll("\\\\(arcsin|arccos|arctan|arcsinh|arccosh|arctanh)", "$1")
                .replaceAll("\\\\(sinh|cosh|tanh|coth|sech|csch)", "$1")
                .replaceAll("\\\\(sin|cos|tan|cot|sec|csc)(?![a-zA-Z])", "$1")
                .replaceAll("\\\\ln(?![a-zA-Z])", "ln")
                .replaceAll("\\\\log(?![a-zA-Z])", "log")
                .replaceAll("\\\\exp(?![a-zA-Z])", "exp")
                .replaceAll("\\\\abs(?![a-zA-Z])", "abs")
                .replaceAll("\\\\max(?![a-zA-Z])", "max")
                .replaceAll("\\\\min(?![a-zA-Z])", "min")
                .replaceAll("\\\\gcd(?![a-zA-Z])", "gcd")
                .replaceAll("\\\\det(?![a-zA-Z])", "det")
                .replaceAll("\\\\lim(?![a-zA-Z])", "lim");

        // LaTeX spacing commands: \, \; \: \! → remove
        s = s.replaceAll("\\\\[,;:! ]", "");

        // ^{expr} → ^(expr)
        s = s.replaceAll("\\^\\{([^}]+)\\}", "^($1)");

        // _{sub} → strip
        s = s.replaceAll("_\\{[^}]+\\}", "");
        s = s.replaceAll("_[a-zA-Z0-9](?![a-zA-Z0-9])", "");

        // Remove remaining lone backslash-letter sequences (unknown LaTeX commands)
        s = s.replaceAll("\\\\[a-zA-Z]+", "");
        // Remove stray lone backslashes not followed by a letter
        s = s.replaceAll("\\\\(?![a-zA-Z])", "");

        return s.trim();
    }

    // ── Helpers
    // ───────────────────────────────────────────────────────────────────────────

    // Build "calc:<op>|<json>"
    private static String calc(String op, String jsonPayload) {
        return "calc:" + op + "|" + jsonPayload;
    }

    // Build a JSON object from alternating key/value strings
    // json("expr","x^2","var","x","order","1") →
    // {"expr":"x^2","var":"x","order":"1"}
    // Numeric-looking values are written without quotes.
    private static String json(String... kvs) {
        StringBuilder sb = new StringBuilder("{");
        for (int i = 0; i + 1 < kvs.length; i += 2) {
            if (i > 0)
                sb.append(',');
            sb.append('"').append(kvs[i]).append("\":");
            String v = kvs[i + 1];
            // Write without quotes if it looks like a number
            if (v.matches("-?\\d+(\\.\\d+)?"))
                sb.append(v);
            else
                sb.append('"').append(v).append('"');
        }
        sb.append('}');
        return sb.toString();
    }

    // Build JSON with first element as "expr" and rest as vars array
    private static String jsonWithVarArray(String[] parts) {
        StringBuilder sb = new StringBuilder();
        sb.append("{\"expr\":\"").append(parts[0].trim()).append("\",\"vars\":[");
        for (int i = 1; i < parts.length; i++) {
            if (i > 1)
                sb.append(',');
            sb.append('"').append(parts[i].trim()).append('"');
        }
        sb.append("]}");
        return sb.toString();
    }

    // Build a vector field JSON: first n/2 elements are exprs, rest are vars
    private static String buildVecOpWithExprs(String op, String[] p) {
        int half = p.length / 2;
        StringBuilder sb = new StringBuilder();
        sb.append("{\"exprs\":[");
        for (int i = 0; i < half; i++) {
            if (i > 0)
                sb.append(',');
            sb.append('"').append(p[i].trim()).append('"');
        }
        sb.append("],\"vars\":[");
        for (int i = half; i < p.length; i++) {
            if (i > half)
                sb.append(',');
            sb.append('"').append(p[i].trim()).append('"');
        }
        sb.append("]}");
        return "calc:" + op + "|" + sb;
    }

    // ── Decimal → Fraction conversion ────────────────────────────────────────
    /**
     * Convert a decimal string to its simplest fraction.
     * e.g. "0.5" → "1/2", "0.333..." → "1/3", "1.25" → "5/4"
     * Integers and already-fraction strings are returned unchanged.
     * Used in OutputPanel to display clean fractions instead of decimals.
     */
    public static String toFraction(String s) {
        if (s == null)
            return "0";
        s = s.trim();
        // Already a fraction or integer
        if (s.matches("-?\\d+(/\\d+)?"))
            return s;
        // Contains a decimal point
        if (!s.contains("."))
            return s;
        try {
            double d = Double.parseDouble(s);
            if (Double.isInfinite(d) || Double.isNaN(d))
                return s;
            // Special values
            if (d == 0.0)
                return "0";
            boolean neg = d < 0;
            d = Math.abs(d);
            // Try denominators up to 10000
            long bestNum = Math.round(d), bestDen = 1;
            double bestErr = Math.abs(d - bestNum);
            for (long den = 2; den <= 10000; den++) {
                long num = Math.round(d * den);
                double err = Math.abs(d - (double) num / den);
                if (err < bestErr) {
                    bestErr = err;
                    bestNum = num;
                    bestDen = den;
                }
                if (bestErr < 1e-9)
                    break;
            }
            if (bestErr > 1e-7)
                return s; // not representable as simple fraction
            long g = gcd(bestNum, bestDen);
            bestNum /= g;
            bestDen /= g;
            if (neg)
                bestNum = -bestNum;
            return bestDen == 1 ? String.valueOf(bestNum) : bestNum + "/" + bestDen;
        } catch (NumberFormatException e) {
            return s;
        }
    }

    private static long gcd(long a, long b) {
        return b == 0 ? a : gcd(b, a % b);
    }

    static java.util.List<String> splitArgs(String s, char delim) {
        java.util.List<String> result = new java.util.ArrayList<>();
        int depth = 0;
        StringBuilder cur = new StringBuilder();
        for (char c : s.toCharArray()) {
            if (c == '(' || c == '[' || c == '{') depth++;
            else if (c == ')' || c == ']' || c == '}') depth--;
            if (c == delim && depth == 0) { result.add(cur.toString().trim()); cur.setLength(0); }
            else cur.append(c);
        }
        if (cur.length() > 0) result.add(cur.toString().trim());
        return result;
    }

    // Guess the primary variable name from an expression string.
    // Looks for single letters that aren't known constants.
    static String inferVariable(String expr) {
        // Known math function/constant identifiers to skip over entirely
        java.util.Set<String> SKIP = java.util.Set.of(
            "sin","cos","tan","cot","sec","csc",
            "asin","acos","atan","acot","asec","acsc",
            "sinh","cosh","tanh","asinh","acosh","atanh",
            "exp","log","ln","log2","log10","sqrt","cbrt","abs",
            "floor","ceil","round","sign","signum",
            "min","max","gcd","lcm","mod",
            "pi","inf","infinity","nan","e");
        int i = 0;
        while (i < expr.length()) {
            char c = expr.charAt(i);
            if (Character.isLetter(c) || c == '_') {
                int start = i;
                while (i < expr.length() &&
                       (Character.isLetterOrDigit(expr.charAt(i)) || expr.charAt(i) == '_'))
                    i++;
                String id = expr.substring(start, i);
                // Return it only if it is NOT a known function/constant
                // and looks like a variable (1-2 chars, or single-letter identifiers)
                if (!SKIP.contains(id.toLowerCase())) {
                    return id.length() <= 2 ? id : String.valueOf(id.charAt(0));
                }
            } else {
                i++;
            }
        }
        return "x";
    }

    private static String buildDiscrete(String e) {
        // ── Graph operations: need edge-list → adjacency-matrix conversion ──────
        if (e.startsWith("bfs[") && e.endsWith("]")) {
            String[] g = e.substring(4, e.length() - 1).split("\\|");
            String adj = edgeListToAdjMatrix(g[0], false); // unweighted
            String start = g.length > 1 ? g[1].trim() : "0";
            return "dm:bfs|{\"adj\":\"" + adj + "\",\"start\":" + start + "}";
        }
        if (e.startsWith("dfs[") && e.endsWith("]")) {
            String[] g = e.substring(4, e.length() - 1).split("\\|");
            String adj = edgeListToAdjMatrix(g[0], false);
            String start = g.length > 1 ? g[1].trim() : "0";
            return "dm:dfs|{\"adj\":\"" + adj + "\",\"start\":" + start + "}";
        }
        if (e.startsWith("dijkstra[") && e.endsWith("]")) {
            String[] g = e.substring(9, e.length() - 1).split("\\|");
            String adj = edgeListToAdjMatrix(g[0], true); // weighted
            String src = g.length > 1 ? g[1].trim() : "0";
            return "dm:dijkstra|{\"adj\":\"" + adj + "\",\"src\":" + src + "}";
        }
        // Matrix-based graph ops (user passes adjacency matrix directly [[...]])
        if (e.startsWith("topo_sort[") && e.endsWith("]"))
            return "dm:topo_sort|{\"adj\":\"" + e.substring(10, e.length() - 1) + "\"}";
        if (e.startsWith("components[") && e.endsWith("]"))
            return "dm:components|{\"adj\":\"" + e.substring(11, e.length() - 1) + "\"}";
        if (e.startsWith("bipartite[") && e.endsWith("]"))
            return "dm:bipartite|{\"adj\":\"" + e.substring(10, e.length() - 1) + "\"}";
        if (e.startsWith("euler_circuit[") && e.endsWith("]"))
            return "dm:euler_circuit|{\"adj\":\"" + e.substring(14, e.length() - 1) + "\"}";
        if (e.startsWith("chromatic[") && e.endsWith("]"))
            return "dm:chromatic|{\"adj\":\"" + e.substring(10, e.length() - 1) + "\"}";

        // ── Combinatorics ──────────────────────────────────────────────────────
        if (e.startsWith("combinations[") && e.endsWith("]")) {
            String[] p = e.substring(13, e.length() - 1).split(",");
            return dm("combinations", json("n", p[0].trim(), "r", p[1].trim()));
        }
        if (e.startsWith("permutations[") && e.endsWith("]")) {
            String[] p = e.substring(13, e.length() - 1).split(",");
            return dm("permutations", json("n", p[0].trim(), "r", p[1].trim()));
        }
        if (e.startsWith("recurrence[") && e.endsWith("]")) {
            String inner = e.substring(11, e.length() - 1);
            String[] g = inner.split("\\|");
            if (g.length == 3)
                return dm("recurrence", json("coeffs", g[0].trim(),
                        "initial", g[1].trim(), "n", g[2].trim()));
        }
        if (e.startsWith("master[") && e.endsWith("]")) {
            String[] p = e.substring(7, e.length() - 1).split(",");
            if (p.length >= 3)
                return dm("master_theorem", json("a", p[0].trim(), "b", p[1].trim(), "p", p[2].trim()));
        }
        if (e.startsWith("pigeonhole[") && e.endsWith("]")) {
            String[] p = e.substring(11, e.length() - 1).split(",");
            return dm("pigeonhole", json("items", p[0].trim(), "holes", p[1].trim()));
        }
        if (e.startsWith("inclusion_exclusion[") && e.endsWith("]")) {
            // No direct C++ handler — compute using set sizes via combinations formula
            String inner = e.substring(20, e.length() - 1);
            String[] p = inner.split(",");
            // Use the largest two sets for a two-set PIE: |A∪B| = |A|+|B|-|A∩B|
            // For the quick function template, route to permutations as a placeholder
            if (p.length >= 1)
                return dm("inclusion_excl", json("sizes", inner));
            return dm("combinations", json("n", "10", "r", "2"));
        }
        // Default: combinations
        String[] p = e.split(",");
        if (p.length >= 2)
            return dm("combinations", json("n", p[0].trim(), "r", p[1].trim()));
        return dm("combinations", json("n", e.trim(), "k", "2"));
    }

    /**
     * Convert a semicolon-separated edge list to a JSON-safe adjacency matrix
     * string.
     *
     * Unweighted format: "u,v;u,v;..." → [[0,1,0,...],[...]]
     * Weighted format: "u,v,w;u,v,w;..." → [[0,w,0,...],[...]]
     *
     * Node count is inferred from the highest node index found.
     */
    private static String edgeListToAdjMatrix(String edgeList, boolean weighted) {
        if (edgeList == null || edgeList.isBlank())
            return "[[0]]";
        String[] edges = edgeList.trim().split(";");
        // Find max node index
        int n = 0;
        for (String edge : edges) {
            String[] p = edge.trim().split(",");
            if (p.length >= 2) {
                try {
                    n = Math.max(n, Math.max(
                            Integer.parseInt(p[0].trim()),
                            Integer.parseInt(p[1].trim())));
                } catch (NumberFormatException ignored) {
                }
            }
        }
        n++; // size = max_index + 1

        double[][] mat = new double[n][n];
        for (String edge : edges) {
            String[] p = edge.trim().split(",");
            if (p.length < 2)
                continue;
            try {
                int u = Integer.parseInt(p[0].trim());
                int v = Integer.parseInt(p[1].trim());
                double w = (weighted && p.length >= 3)
                        ? Double.parseDouble(p[2].trim())
                        : 1.0;
                if (u >= 0 && u < n && v >= 0 && v < n) {
                    mat[u][v] = w;
                    if (!weighted)
                        mat[v][u] = w; // undirected for BFS/DFS
                }
            } catch (NumberFormatException ignored) {
            }
        }

        // Serialise to [[a,b,...],[c,d,...]] format
        StringBuilder sb = new StringBuilder("[");
        for (int i = 0; i < n; i++) {
            if (i > 0)
                sb.append(',');
            sb.append('[');
            for (int j = 0; j < n; j++) {
                if (j > 0)
                    sb.append(',');
                // Use int for clean values
                sb.append(mat[i][j] == (long) mat[i][j]
                        ? String.valueOf((long) mat[i][j])
                        : String.valueOf(mat[i][j]));
            }
            sb.append(']');
        }
        sb.append(']');
        return sb.toString();
    }
}