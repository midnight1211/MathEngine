package com.mathengine.ui;

import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.canvas.Canvas;
import javafx.scene.canvas.GraphicsContext;
import javafx.scene.control.Label;
import javafx.scene.control.ScrollPane;
import javafx.scene.image.ImageView;
import javafx.scene.layout.*;
import javafx.scene.paint.Color;
import java.util.regex.Pattern;

/**
 * OutputPanel — displays computation results with:
 *   • Inline Canvas graph auto-plotted for calculus/analysis operations
 *   • Rich educational "Show Work" panel with concept explanations,
 *     method descriptions, result interpretation, and study tips
 *   • LaTeX rendering for symbolic answers
 *   • Clean multi-line preformatted blocks for module output
 *   • Actionable error suggestions
 */
public class OutputPanel extends VBox {

    // ── Regex patterns ─────────────────────────────────────────────────────────
    private static final Pattern TEXT_HEADER = Pattern.compile(
            "^(Step|Rule|Method|Strategy|Formula|Note|Check|Verify|"
            + "Subgroup|Generator|Element|Coset|Order|Group|Cycle|Class|"
            + "Conjugacy|Ring|Field|Factor|Cayley|Truth|Node|Edge|Adj|Component|"
            + "Clique|Path|Mean|Variance|Std|Median|Mode|Skew|Kurt|Corr|Reg|"
            + "PDF|CDF|PMF|Bin|Poi|Norm|Exp|Chi|Beta|Gamma|Test|ANOVA|"
            + "Prob|Markov|Brownian|GBM|CLT|Berry|Monte|Gamblers|"
            + "Combinations|Permutations|Derangements|Inclusion|Pigeonhole|Master|"
            + "Necklace|Ramsey|Partition|Subsets|Power set|Bijection|"
            + "DFS|BFS|Dijkstra|Floyd|Prim|Kruskal|Topo|Euler|Hamilton|"
            + "Fourier|Taylor|Maclaurin|Series|Convergent|Divergent|"
            + "Limit|lim|Integral|Derivative|Gradient|Divergence|Curl|Laplacian|"
            + "SIR|SEIR|Lotka|Volterra|Logistic|Bifurcation|Turing|Lyapunov|"
            + "Circle|Ellipse|Sphere|Plane|Triangle|Area|Volume|Distance|Angle|"
            + "Eigen|Rank|Determinant|Trace|Null|Row|Column|Basis|Span|Orthogon|"
            + "Time|Infected|Susceptible|Recovered|Population|Density|"
            + "Characteristic|Solution|Result|Parameters|Domain|Range|"
            + "Initial|Carrying|Half|Derived|Isentropic|Mach|Speed).*",
            Pattern.CASE_INSENSITIVE);

    // ── Widgets ────────────────────────────────────────────────────────────────
    private final Label      headerTitle   = new Label("RESULT");
    private final Label      statusBadge   = new Label();
    private final VBox       contentBox    = new VBox(14);
    private final ScrollPane contentScroll = new ScrollPane(contentBox);
    private final ThemeManager tm          = ThemeManager.getInstance();

    // ── Constructor ────────────────────────────────────────────────────────────
    public OutputPanel() {
        setSpacing(0);
        getStyleClass().add("output-card");
        getChildren().addAll(buildHeader(), contentScroll);
        contentBox.setPadding(new Insets(20, 20, 20, 20));
        contentScroll.setFitToWidth(true);
        contentScroll.setPrefHeight(300);
        contentScroll.getStyleClass().add("output-scroll");
        contentScroll.setFocusTraversable(false);
        showEmpty();
    }

    private HBox buildHeader() {
        headerTitle.getStyleClass().add("output-title");
        statusBadge.getStyleClass().add("output-status-badge");
        statusBadge.setVisible(false);
        HBox h = new HBox(headerTitle, new Spacer(), statusBadge);
        h.setAlignment(Pos.CENTER_LEFT);
        h.setPadding(new Insets(12, 16, 12, 16));
        h.getStyleClass().add("output-card-header");
        return h;
    }

    // ── Public API ─────────────────────────────────────────────────────────────
    public void showResult(String inputLatex, String result,
                           String opName, String precisionMode) {
        showResult(inputLatex, result, null, opName, precisionMode, false);
    }

    public void showResult(String inputLatex, String result, String steps,
                           String opName, String precisionMode, boolean isArithmetic) {
        contentBox.getChildren().clear();
        getStyleClass().removeAll("output-has-result", "output-has-error");
        getStyleClass().add("output-has-result");
        contentBox.setAlignment(Pos.TOP_LEFT);

        boolean symbolicMode = !"numerical".equalsIgnoreCase(precisionMode);
        statusBadge.setText(capitalize(precisionMode) + "  ·  " + opName);
        statusBadge.getStyleClass().removeAll("badge-error", "badge-success");
        statusBadge.getStyleClass().add("badge-success");
        statusBadge.setVisible(true);

        String raw = normalise(result != null ? result : "");
        String answerText, autoSteps;
        int dnl = raw.indexOf("\n\n");
        if (dnl >= 0) { answerText = raw.substring(0, dnl).trim(); autoSteps = raw.substring(dnl+2).trim(); }
        else           { answerText = raw.trim(); autoSteps = null; }

        String symbolicPart = answerText, numericPart = null;
        for (String sep : new String[]{"  ~  ", "  ≈  "}) {
            if (answerText.contains(sep)) {
                String[] p = answerText.split(sep, 2);
                symbolicPart = p[0].trim(); numericPart = p[1].trim(); break;
            }
        }

        String displayAnswer;
        if (!symbolicMode && numericPart != null && !numericPart.isBlank())
            displayAnswer = numericPart;
        else {
            displayAnswer = symbolicMode
                    ? (symbolicPart.isBlank() ? numericPart : symbolicPart)
                    : (numericPart != null ? numericPart : symbolicPart);
            if (symbolicMode) displayAnswer = fractionalise(displayAnswer);
        }

        // Input row
        Label inputLabel = new Label("Input");
        inputLabel.getStyleClass().add("result-row-label");
        VBox inputRow = new VBox(4, inputLabel, renderAnswerLine(inputLatex));
        inputRow.getStyleClass().add("result-row");

        // Result row
        Label resultLabel = new Label("Result");
        resultLabel.getStyleClass().add("result-row-label");
        Label equals = new Label("=");
        equals.getStyleClass().add("result-equals");
        javafx.scene.Node resultNode = renderResult(displayAnswer);
        HBox mainResult = new HBox(12, equals, resultNode);
        mainResult.setAlignment(Pos.TOP_LEFT);
        VBox resultContent = new VBox(6, mainResult);

        if (symbolicMode && numericPart != null && !numericPart.isBlank()
                && !numericPart.equals(symbolicPart)) {
            Label approxSign = new Label("≈");
            approxSign.getStyleClass().add("result-approx-sign");
            Label approxLbl  = new Label(numericPart);
            approxLbl.getStyleClass().add("result-approx-value");
            HBox approxRow = new HBox(10, approxSign, approxLbl);
            approxRow.setAlignment(Pos.CENTER_LEFT);
            approxRow.getStyleClass().add("result-approx-row");
            resultContent.getChildren().add(approxRow);
        }

        VBox resultRow = new VBox(4, resultLabel, resultContent);
        resultRow.getStyleClass().add("result-row");
        contentBox.getChildren().addAll(inputRow, resultRow);

        // Inline graph
        Canvas graph = tryBuildGraph(inputLatex, opName, autoSteps);
        if (graph != null) {
            Label graphLabel = new Label("Graph");
            graphLabel.getStyleClass().add("result-row-label");
            VBox graphRow = new VBox(6, graphLabel, graph);
            graphRow.getStyleClass().add("result-row");
            contentBox.getChildren().add(graphRow);
        }

        // Show Work (educational)
        String stepsText = (steps != null && !steps.isBlank()) ? steps
                : (autoSteps != null && !autoSteps.isBlank()) ? autoSteps : null;
        String edu = buildEducationalSteps(opName, inputLatex, symbolicPart, numericPart, stepsText);
        if (!isArithmetic && edu != null && !edu.isBlank())
            contentBox.getChildren().add(buildStepsPanel(edu));

        contentBox.setAccessibleText("Result: " + displayAnswer);
    }

    // ── Inline graph ───────────────────────────────────────────────────────────
    private Canvas tryBuildGraph(String expr, String opName, String steps) {
        if (expr == null || expr.isBlank()) return null;
        String op = opName == null ? "" : opName.toLowerCase();
        if (op.contains("matrix") || op.contains("algebra") || op.contains("discrete")
                || op.contains("number") || op.contains("statistic")
                || op.contains("probability") || op.contains("geometry")) return null;

        double a = -5, b = 5;
        String plotExpr = expr.replaceAll("^(taylor|maclaurin|fourier)\\[([^,\\]]+).*","$2")
                              .replaceAll("^(diff|partial|gradient|curl|div)\\[([^,\\]]+).*","$2")
                              .replaceAll("^([^,\\[|]+),.*$","$1").trim();
        if (plotExpr.contains("[") || plotExpr.contains("|")) return null;
        if (!plotExpr.contains("x") && !plotExpr.contains("sin") && !plotExpr.contains("cos")
                && !plotExpr.contains("exp") && !plotExpr.contains("log")) return null;

        // Extract bounds if present in original expression
        java.util.regex.Matcher m = java.util.regex.Pattern
                .compile(",\\s*([\\-\\d.]+)\\s*,\\s*([\\-\\d.]+)").matcher(expr);
        if (m.find()) {
            try { a = Double.parseDouble(m.group(1)); b = Double.parseDouble(m.group(2)); } catch (Exception ignored) {}
        }
        return drawGraph(plotExpr, a, b, op);
    }

    private Canvas drawGraph(String exprStr, double a, double b, String opLabel) {
        final int W = 420, H = 170;
        Canvas canvas = new Canvas(W, H);
        GraphicsContext gc = canvas.getGraphicsContext2D();
        boolean dark = isDark();

        gc.setFill(dark ? Color.rgb(28,28,36) : Color.rgb(248,247,244));
        gc.fillRect(0, 0, W, H);

        final int N = 320;
        double[] xs = new double[N], ys = new double[N];
        double yMin = 1e18, yMax = -1e18;
        double step = (b - a) / (N - 1);
        for (int i = 0; i < N; i++) {
            xs[i] = a + i * step;
            ys[i] = evalAt(exprStr, xs[i]);
            if (Double.isFinite(ys[i])) { yMin = Math.min(yMin, ys[i]); yMax = Math.max(yMax, ys[i]); }
        }
        if (!Double.isFinite(yMin) || yMax <= yMin) return null;
        double pad = 0.15*(yMax-yMin); yMin -= pad; yMax += pad;

        double mx = W/(b-a), my = -H/(yMax-yMin);
        double ox = -a*mx, oy = H+yMin*my;  // oy = H - (-yMin * (-H/(yMax-yMin)))

        // Axes
        gc.setLineWidth(1);
        gc.setStroke(dark ? Color.rgb(55,55,70) : Color.rgb(215,210,205));
        if (oy >= 0 && oy <= H) gc.strokeLine(0, oy, W, oy);
        if (ox >= 0 && ox <= W) gc.strokeLine(ox, 0, ox, H);

        // Tick labels
        gc.setFill(dark ? Color.rgb(130,130,150) : Color.rgb(110,105,98));
        gc.setFont(javafx.scene.text.Font.font("System", 9));
        for (int tx = (int)Math.ceil(a); tx <= (int)Math.floor(b); tx++) {
            double px = ox + tx*mx;
            if (tx != 0) gc.fillText(String.valueOf(tx), px-4, Math.min(oy+12, H-3));
        }

        // Fill area for integrals
        if (opLabel.contains("integral") || opLabel.contains("integrate")) {
            gc.setFill(dark ? Color.rgb(100,170,255,0.22) : Color.rgb(70,130,220,0.16));
            gc.beginPath(); boolean s = false;
            for (int i = 0; i < N; i++) {
                double px = ox+xs[i]*mx, py = oy+ys[i]*my;
                if (!Double.isFinite(ys[i])) { s = false; continue; }
                if (!s) { gc.moveTo(px, oy); gc.lineTo(px, py); s = true; } else gc.lineTo(px, py);
            }
            if (s) { gc.lineTo(ox+xs[N-1]*mx, oy); gc.closePath(); gc.fill(); }
        }

        // Plot curve
        gc.setStroke(dark ? Color.rgb(100,185,255) : Color.rgb(35,105,210));
        gc.setLineWidth(2);
        boolean pen = false; double prevY = Double.NaN;
        gc.beginPath();
        for (int i = 0; i < N; i++) {
            double y = ys[i]; if (!Double.isFinite(y)) { pen = false; prevY = Double.NaN; continue; }
            if (Double.isFinite(prevY) && Math.abs(y - prevY) > (yMax-yMin)*2) { pen = false; }
            double px = ox+xs[i]*mx, py = oy+y*my;
            if (px < -8 || px > W+8 || py < -80 || py > H+80) { pen = false; prevY = y; continue; }
            if (!pen) { gc.moveTo(px, py); pen = true; } else gc.lineTo(px, py);
            prevY = y;
        }
        gc.stroke();

        // Label
        gc.setFill(dark ? Color.rgb(160,160,185) : Color.rgb(90,85,78));
        gc.setFont(javafx.scene.text.Font.font("System", 10));
        String lbl = "y = " + exprStr; if (lbl.length() > 42) lbl = lbl.substring(0, 42) + "…";
        gc.fillText(lbl, 7, 13);

        canvas.getStyleClass().add("output-graph");
        return canvas;
    }

    private static double evalAt(String expr, double x) {
        try {
            javax.script.ScriptEngineManager mgr = new javax.script.ScriptEngineManager();
            javax.script.ScriptEngine eng = mgr.getEngineByName("JavaScript");
            if (eng == null) eng = mgr.getEngineByName("Nashorn");
            if (eng == null) return Double.NaN;
            String safe = expr.replace("^","**")
                    .replaceAll("(?<![a-zA-Z0-9_])x(?![a-zA-Z0-9_])", "("+x+")");
            String js = "var pi=Math.PI,e=Math.E;"
                + "function sin(v){return Math.sin(v);}function cos(v){return Math.cos(v);}"
                + "function tan(v){return Math.tan(v);}function exp(v){return Math.exp(v);}"
                + "function log(v){return Math.log(v);}function ln(v){return Math.log(v);}"
                + "function sqrt(v){return Math.sqrt(v);}function abs(v){return Math.abs(v);}"
                + "function pow(a,b){return Math.pow(a,b);}function sinh(v){return Math.sinh(v);}"
                + "function cosh(v){return Math.cosh(v);}function tanh(v){return Math.tanh(v);}("
                + safe + ")";
            Object r = eng.eval(js);
            return r instanceof Number ? ((Number)r).doubleValue() : Double.NaN;
        } catch (Exception ex) { return Double.NaN; }
    }

    private boolean isDark() {
        ThemeManager.Theme t = tm.resolveTheme(tm.getCurrentTheme());
        return t == ThemeManager.Theme.DARK || t == ThemeManager.Theme.HIGH_CONTRAST;
    }

    // ── Educational steps builder ───────────────────────────────────────────────
    private String buildEducationalSteps(String opName, String input,
                                          String sym, String num, String engineSteps) {
        if (opName == null) return engineSteps;
        String op = opName.toLowerCase();
        StringBuilder sb = new StringBuilder();

        String concept = concept(op);
        if (concept != null) { sb.append("📖 What this means\n─────────────────────────────\n").append(concept).append("\n\n"); }

        String method = method(op, engineSteps);
        if (method != null) { sb.append("⚙  Method\n─────────────────────────────\n").append(method).append("\n\n"); }

        if (engineSteps != null && !engineSteps.isBlank())
            sb.append("🔢 Computation\n─────────────────────────────\n").append(engineSteps).append("\n\n");

        String interp = interp(op, sym, num);
        if (interp != null) { sb.append("✓  Result\n─────────────────────────────\n").append(interp).append("\n\n"); }

        String tip = tip(op);
        if (tip != null) { sb.append("💡 Tip\n─────────────────────────────\n").append(tip); }

        return sb.length() > 0 ? sb.toString() : engineSteps;
    }

    private static String concept(String op) {
        if (op.contains("diff")||op.contains("deriv"))
            return "The derivative f'(x) measures the instantaneous rate of change of f at x — the slope of the tangent line. f'(x) > 0 means f is increasing; f'(x) < 0 means decreasing. f'(x) = 0 at critical points (potential maxima, minima, or saddle points).";
        if (op.contains("partial"))
            return "∂f/∂x measures how f changes as x varies while all other variables are held fixed. Partial derivatives build the gradient vector ∇f = [∂f/∂x, ∂f/∂y, …], which points in the direction of steepest ascent.";
        if (op.contains("gradient"))
            return "The gradient ∇f = [∂f/∂x, ∂f/∂y, …] points in the direction of steepest ascent. |∇f| gives the rate of increase in that direction. ∇f is perpendicular to the level curves (2D) or level surfaces (3D) of f.";
        if (op.contains("curl"))
            return "The curl ∇×F measures the rotational tendency of a vector field F at each point. In fluid mechanics, it gives twice the local angular velocity (vorticity). curl F = 0 means the field is irrotational (conservative).";
        if (op.contains("integral")||op.contains("integrate"))
            return "The definite integral ∫ₐᵇ f(x)dx = F(b) − F(a) gives the signed area between f and the x-axis on [a,b]. The Fundamental Theorem of Calculus connects integration (area) to differentiation (slope): they are inverses.";
        if (op.contains("limit"))
            return "lim_{x→c} f(x) = L means f(x) → L as x → c, even if f(c) is undefined. Limits define continuity, derivatives (as the difference quotient lim_{h→0} [f(x+h)−f(x)]/h), and integrals (as Riemann sums).";
        if (op.contains("taylor")||op.contains("maclaurin"))
            return "f(x) = Σ f⁽ⁿ⁾(a)/n! · (x−a)ⁿ — any infinitely differentiable function equals its Taylor series within the radius of convergence. The Maclaurin series is the special case a=0. Truncating after N terms gives an N-th degree polynomial approximation.";
        if (op.contains("fourier"))
            return "f(x) = a₀/2 + Σ[aₙcos(nωx) + bₙsin(nωx)] decomposes any periodic function into its frequency components. aₙ = (1/L)∫f·cos(nωx)dx, bₙ = (1/L)∫f·sin(nωx)dx. Used everywhere in signal processing, PDEs, and quantum mechanics.";
        if (op.contains("greens"))
            return "Green's theorem: ∮_C(P dx + Q dy) = ∬_D(∂Q/∂x − ∂P/∂y)dA converts a line integral around curve C into a double integral over the enclosed region D. It is the 2D version of Stokes' theorem.";
        if (op.contains("sir"))
            return "SIR model: dS/dt = −βSI/N, dI/dt = βSI/N − γI, dR/dt = γI. β=transmission rate, γ=recovery rate. The basic reproduction number R₀ = β/γ determines if an epidemic occurs (R₀>1) or dies out (R₀<1). Herd immunity requires vaccinating fraction 1−1/R₀.";
        if (op.contains("lotka"))
            return "Predator-prey: dx/dt = αx−βxy (prey), dy/dt = δxy−γy (predator). Equilibrium at (γ/δ, α/β). Solutions are closed periodic orbits — populations oscillate forever, neither going extinct in the idealized model.";
        if (op.contains("logistic"))
            return "Logistic growth: dN/dt = rN(1−N/K). Solution: N(t) = K/[1+((K−N₀)/N₀)e^{−rt}]. Population grows exponentially when small, then slows as it approaches carrying capacity K. The inflection point (fastest growth) occurs at N=K/2.";
        if (op.contains("combin"))
            return "C(n,r) = n!/[r!(n−r)!] counts unordered subsets of size r from n elements. It is the binomial coefficient — it appears in (a+b)ⁿ = Σ C(n,k) aᵏ bⁿ⁻ᵏ and in probability as P(X=k) for binomial distributions.";
        if (op.contains("permut"))
            return "P(n,r) = n!/(n−r)! counts ordered arrangements of r items from n. Order matters: ABC ≠ BAC. P(n,r) = r! × C(n,r).";
        if (op.contains("eigen"))
            return "Eigenvalues λ and eigenvectors v satisfy Av = λv: the matrix A stretches v by λ without changing its direction. Eigenvalues solve det(A−λI)=0. They reveal the principal axes of transformations, stability of systems, and natural frequencies.";
        return null;
    }

    private static String method(String op, String steps) {
        if (steps != null) {
            if (steps.contains("Romberg")) return "Romberg integration: applies Richardson extrapolation to the trapezoidal rule. Each level doubles the subintervals, achieving O(h^{2k}) accuracy with k levels.";
            if (steps.contains("Runge-Kutta")||steps.contains("RK4")) return "RK4: evaluates f at 4 points per step (start, two midpoints, end) and takes a weighted average. Local error O(h⁵), global error O(h⁴).";
            if (steps.contains("Newton-Raphson")||steps.contains("Newton")) return "Newton-Raphson: xₙ₊₁ = xₙ − f(xₙ)/f'(xₙ). Quadratic convergence near simple roots (doubles correct digits each iteration).";
        }
        if (op.contains("diff")||op.contains("deriv"))
            return "Symbolic differentiation: power rule d/dx[xⁿ]=nxⁿ⁻¹, chain rule d/dx[f(g(x))]=f'(g)·g', product rule d/dx[fg]=f'g+fg', quotient rule d/dx[f/g]=(f'g−fg')/g², plus the derivatives of sin, cos, exp, ln, etc.";
        if (op.contains("integral"))
            return "Romberg numerical integration (8 levels, ~256 function evaluations). Falls back to adaptive Simpson for discontinuous integrands.";
        if (op.contains("limit"))
            return "Richardson extrapolation on values f(c±h) for h = 10⁻², …, 10⁻⁸. The extrapolated limit converges O(h⁸). Values exceeding 10¹⁰ are reported as ±∞.";
        if (op.contains("eigen"))
            return "QR algorithm: repeatedly factor A = QR, then set A ← RQ (QR iteration). Converges to Schur form; eigenvalues appear on the diagonal. O(n³) per iteration.";
        if (op.contains("greens")||op.contains("stokes"))
            return "Symbolically computes ∂Q/∂x and ∂P/∂y via the symbolic differentiation engine, then numerically integrates their difference over the rectangular domain using iterated Romberg.";
        return null;
    }

    private static String interp(String op, String sym, String num) {
        if (sym == null || sym.isBlank()) return null;
        if (op.contains("diff")||op.contains("deriv")) {
            if ("0".equals(sym)) return "f'(x) = 0 everywhere: the function is constant with respect to this variable (or the differentiation variable was not found in the expression — check your variable name).";
            return "f'(x) = " + sym + "\n\nCritical points: solve " + sym + " = 0.\n"
                 + "Where f'(x) > 0: f is increasing.  Where f'(x) < 0: f is decreasing.";
        }
        if (op.contains("integral")) {
            String val = num != null ? num : sym;
            return "∫ = " + val + "\n\nIf positive: net area lies above the x-axis.\n"
                 + "If negative: net area lies below the x-axis.\n"
                 + "If zero: positive and negative areas cancelled exactly.";
        }
        if (op.contains("limit")) {
            if (sym.equals("+∞")) return "The limit is +∞: f(x) grows without bound from above as x approaches the given point.";
            if (sym.equals("-∞")) return "The limit is -∞: f(x) decreases without bound as x approaches the given point.";
            if (sym.contains("DNE")) return "The limit Does Not Exist: the left-hand and right-hand limits differ, or the function oscillates without settling.";
            return "lim = " + sym + (num != null ? " ≈ " + num : "");
        }
        if (op.contains("taylor")||op.contains("maclaurin"))
            return sym + "\n\nThe radius of convergence R determines where this series converges to f(x). For |x−a| < R the series equals f; for |x−a| > R it diverges.";
        return null;
    }

    private static String tip(String op) {
        if (op.contains("diff")||op.contains("deriv"))
            return "Drill: xⁿ, sin x, cos x, eˣ, ln x. Then compose them. The chain rule is the most-used rule in practice: d/dx[f(g(x))] = f'(g(x))·g'(x).";
        if (op.contains("integral"))
            return "Strategy order: (1) u-substitution — look for f'(x)·f(g(x)). (2) Integration by parts — ∫u dv = uv − ∫v du; use LIATE to pick u. (3) Partial fractions — for rational functions. (4) Trig identities — for powers of sin/cos.";
        if (op.contains("limit"))
            return "L'Hôpital's rule: if lim f/g gives 0/0 or ∞/∞, then lim f/g = lim f'/g'. Squeeze theorem: if g ≤ f ≤ h and lim g = lim h = L, then lim f = L.";
        if (op.contains("taylor")||op.contains("maclaurin"))
            return "Memorize: eˣ = Σxⁿ/n!   sin x = Σ(−1)ⁿx^{2n+1}/(2n+1)!   cos x = Σ(−1)ⁿx^{2n}/(2n)!   1/(1−x) = Σxⁿ  (|x|<1). Build others by composing/differentiating/integrating these.";
        if (op.contains("eigen"))
            return "det(A−λI)=0 gives the characteristic polynomial; its roots are the eigenvalues. Then solve (A−λI)v=0 for each eigenvector. A matrix is diagonalizable iff it has n linearly independent eigenvectors.";
        if (op.contains("combin")||op.contains("permut"))
            return "C(n,r) = C(n,n−r) (choosing r is the same as rejecting n−r). Pascal's identity: C(n,r) = C(n−1,r−1) + C(n−1,r). P(n,r) = r!·C(n,r).";
        if (op.contains("sir")||op.contains("lotka")||op.contains("logistic"))
            return "These systems are nonlinear ODEs — exact solutions rarely exist. Qualitative analysis (equilibria, stability, phase portraits) often tells you more than numerical solutions alone.";
        return null;
    }

    // ── Result rendering ───────────────────────────────────────────────────────
    private javafx.scene.Node renderResult(String text) {
        if (text == null || text.isBlank()) return rawLabel("", "result-value");
        String norm = normalise(text);
        String[] lines = norm.split("\n", -1);
        if (lines.length == 1) return renderAnswerLine(norm.trim());

        VBox stack = new VBox(2);
        stack.getStyleClass().add("result-preformatted-block");
        stack.setPadding(new Insets(10, 12, 10, 12));
        boolean first = true;
        for (String line : lines) {
            String t = line.trim();
            if (t.isEmpty()) { Region g = new Region(); g.setPrefHeight(4); stack.getChildren().add(g); continue; }
            if (first && !TEXT_HEADER.matcher(t).matches() && !looksLikePlainText(t)) {
                ImageView iv = LatexRenderer.render(t, tm);
                if (iv != null && iv.getImage() != null) { stack.getChildren().add(iv); first = false; continue; }
            }
            first = false;
            stack.getChildren().add(rawLabel(line, isHeader(t) ? "result-section-header" : "result-preformatted"));
        }
        return stack;
    }

    private javafx.scene.Node renderAnswerLine(String text) {
        if (text == null || text.isBlank()) return rawLabel("", "result-value");
        String t = text.trim();
        if (!TEXT_HEADER.matcher(t).matches() && !looksLikePlainText(t)) {
            ImageView iv = LatexRenderer.render(t, tm);
            if (iv != null && iv.getImage() != null) return iv;
        }
        return rawLabel(t, "result-value");
    }

    // ── Steps panel ────────────────────────────────────────────────────────────
    private VBox buildStepsPanel(String stepsText) {
        Label stepsHeader = new Label("▸  Show Work");
        stepsHeader.getStyleClass().add("steps-header");
        stepsHeader.setFocusTraversable(true);

        VBox stepsBody = new VBox(3);
        stepsBody.getStyleClass().add("steps-body");
        stepsBody.setPadding(new Insets(12, 16, 12, 16));
        stepsBody.setVisible(false);
        stepsBody.setManaged(false);

        for (String rawLine : normalise(stepsText).split("\n", -1)) {
            String t = rawLine.trim();
            if (t.isEmpty()) { Region g = new Region(); g.setPrefHeight(5); stepsBody.getChildren().add(g); continue; }

            if (t.startsWith("📖")||t.startsWith("⚙")||t.startsWith("🔢")||t.startsWith("✓")||t.startsWith("💡")) {
                stepsBody.getChildren().add(rawLabel(rawLine, "steps-emoji-header")); continue;
            }
            if (t.startsWith("─────")) {
                Label sep = new Label(); sep.setPrefHeight(1); sep.setMaxWidth(Double.MAX_VALUE);
                sep.getStyleClass().add("steps-separator-line"); stepsBody.getChildren().add(sep); continue;
            }
            if (!TEXT_HEADER.matcher(t).matches() && !looksLikePlainText(t)) {
                ImageView iv = LatexRenderer.render(t, tm);
                if (iv != null && iv.getImage() != null) { stepsBody.getChildren().add(iv); continue; }
            }
            stepsBody.getChildren().add(rawLabel(rawLine, isHeader(t) ? "steps-section-header" : "steps-content"));
        }

        stepsHeader.setOnMouseClicked(e -> { boolean show = !stepsBody.isVisible(); stepsBody.setVisible(show); stepsBody.setManaged(show); stepsHeader.setText((show?"▾":"▸")+"  Show Work"); });
        stepsHeader.setOnKeyPressed(e -> { if (e.getCode()==javafx.scene.input.KeyCode.ENTER) stepsHeader.fireEvent(new javafx.scene.input.MouseEvent(javafx.scene.input.MouseEvent.MOUSE_CLICKED,0,0,0,0,javafx.scene.input.MouseButton.PRIMARY,1,false,false,false,false,true,false,false,true,false,false,null)); });

        VBox panel = new VBox(0, stepsHeader, stepsBody);
        panel.getStyleClass().add("steps-panel");
        return panel;
    }

    // ── Error / Empty ──────────────────────────────────────────────────────────
    public void showError(String inputLatex, String errorMessage) {
        contentBox.getChildren().clear();
        getStyleClass().removeAll("output-has-result","output-has-error");
        getStyleClass().add("output-has-error");
        statusBadge.setText("Error");
        statusBadge.getStyleClass().removeAll("badge-success","badge-error");
        statusBadge.getStyleClass().add("badge-error");
        statusBadge.setVisible(true);

        Label errTitle = new Label("Computation Error"); errTitle.getStyleClass().add("error-title");
        Label errMsg   = new Label(errorMessage);        errMsg.getStyleClass().add("error-message"); errMsg.setWrapText(true);
        Label errHint  = new Label("Input: "+inputLatex); errHint.getStyleClass().add("error-hint"); errHint.setWrapText(true);
        VBox errBox    = new VBox(8, errTitle, errMsg, errHint);

        String sugg = fix(errorMessage);
        if (sugg != null) { Label s = new Label("💡 "+sugg); s.getStyleClass().add("error-suggestion"); s.setWrapText(true); errBox.getChildren().add(s); }
        errBox.getStyleClass().add("error-box"); errBox.setPadding(new Insets(14));
        contentBox.getChildren().add(errBox);
    }

    private static String fix(String e) {
        if (e == null) return null;
        String l = e.toLowerCase();
        if (l.contains("undefined variable"))  return "The variable name in your expression doesn't match what the engine expects. Use 'x' for single-variable expressions. Multi-variable: list all vars explicitly, e.g. gradient[x^2+y^2, x, y].";
        if (l.contains("unexpected token")||l.contains("unexpected char")) return "Syntax error. Use ^ for powers, * for multiplication. Format: opname[expr, var, ...]. Example: diff[sin(x), x] or taylor[exp(x), 0, 6].";
        if (l.contains("at least 3")||l.contains("data points"))  return "Spline needs ≥ 3 points. Example: spline[0,1,2,3|0,1,4,9|1.5]";
        if (l.contains("unknown")&&l.contains("operation"))       return "Unknown operation name. Check the Quick Functions panel for correct syntax.";
        if (l.contains("division by zero"))                        return "Division by zero at the evaluation point. Try a slightly offset value or check the domain.";
        return null;
    }

    public void showEmpty() {
        contentBox.getChildren().clear();
        getStyleClass().removeAll("output-has-result","output-has-error");
        statusBadge.setVisible(false);
        Label icon = new Label("◌"); icon.getStyleClass().add("empty-icon");
        Label msg  = new Label("Enter an expression and press Solve  (Ctrl+Enter)"); msg.getStyleClass().add("empty-message"); msg.setWrapText(true);
        VBox empty = new VBox(8, icon, msg); empty.setAlignment(Pos.CENTER); empty.setPadding(new Insets(20));
        contentBox.getChildren().add(empty);
        contentBox.setAlignment(Pos.CENTER);
    }

    // ── Helpers ────────────────────────────────────────────────────────────────
    private static String normalise(String s) {
        if (s==null) return "";
        return s.replace("\r\n","\n").replace("\r","\n").replace("\\n","\n");
    }
    private static boolean isHeader(String t) {
        return t.endsWith(":")||t.endsWith("—")||TEXT_HEADER.matcher(t).matches();
    }

    /**
     * B00003 fix: Returns true when text is clearly plain-prose output that
     * should NOT be fed to the LaTeX renderer. Criteria:
     *  - Contains a colon followed by space (sentence-like label)
     *  - Contains multiple consecutive words (≥2 alpha-only tokens)
     *  - Contains square-bracket lists like "[0.5, 0.5]"
     *  - Starts with a word that looks like a sentence (capital letter followed
     *    by lowercase letters and spaces, not a math identifier)
     */
    private static boolean looksLikePlainText(String t) {
        if (t == null || t.isBlank()) return false;
        // Contains a labeled colon like "After 5 steps: [...]"
        if (java.util.regex.Pattern.compile("[A-Za-z][A-Za-z0-9 ]+:\\s").matcher(t).find()) return true;
        // Sentence starting with capital word followed by space and another word
        if (java.util.regex.Pattern.compile("^[A-Z][a-z]{2,}\\s+[A-Za-z0-9]").matcher(t).find() &&
            !t.contains("\\") && !t.contains("^") && !t.contains("_")) return true;
        // Contains square bracket arrays with commas like "[0.1, 0.2, 0.7]"
        if (java.util.regex.Pattern.compile("\\[[0-9., -]+\\]").matcher(t).find() &&
            !t.contains("\\") && !t.contains("^")) return true;
        return false;
    }
    private static String fractionalise(String s) {
        if (s==null) return "";
        return java.util.regex.Pattern.compile("-?\\d+\\.\\d+").matcher(s).replaceAll(m -> {
            String f = CalcExpressionBuilder.toFraction(m.group());
            return f.equals(m.group()) ? m.group() : f;
        });
    }
    private static Label rawLabel(String text, String style) {
        Label l = new Label(text); l.getStyleClass().add(style); l.setWrapText(true); return l;
    }
    private static String capitalize(String s) {
        if (s==null||s.isEmpty()) return s;
        return Character.toUpperCase(s.charAt(0))+s.substring(1);
    }
    private static class Spacer extends Region { Spacer() { HBox.setHgrow(this, Priority.ALWAYS); } }
}
