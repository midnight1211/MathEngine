package com.mathengine.ui;

import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.control.Tooltip;
import javafx.scene.layout.*;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.function.BiConsumer;

/**
 * QuickFunctionsPanel
 * ────────────────────
 * A collapsible panel below the InputPanel that shows a grid of
 * one-click template buttons for the currently selected operation.
 *
 * Each button inserts a ready-to-run expression into the input field.
 * The panel re-populates whenever the operation changes.
 *
 * Example buttons for INTEGRAL:
 *   [∫ x² dx]  [∫₀¹ x² dx]  [∫₀^π sin(x) dx]  [∫∫ x+y dA]  [Romberg]
 *
 * Usage in MainLayout:
 *   QuickFunctionsPanel qf = new QuickFunctionsPanel();
 *   qf.setOnExpressionSelected(expr -> inputPanel.loadExpression(expr));
 *   qf.setOperation(OperationPanel.Operation.INTEGRAL);
 */
public class QuickFunctionsPanel extends VBox {

    // ── Quick-function definitions  ───────────────────────────────────────────
    // Each entry: display label → expression to insert

    private static final Map<OperationPanel.Operation, String[][]> QUICK;
    static {
        QUICK = new LinkedHashMap<>();

        QUICK.put(OperationPanel.Operation.ARITHMETIC, new String[][]{
            {"sin(π/2)",    "sin(pi/2)"},
            {"cos(π/3)",    "cos(pi/3)"},
            {"√2",          "sqrt(2)"},
            {"e^π",         "exp(pi)"},
            {"ln(e²)",      "ln(exp(2))"},
            {"2^10",        "2^10"},
            {"100!",        "100!"},
            {"gcd(48,18)",  "gcd[48,18]"},
        });

        QUICK.put(OperationPanel.Operation.DERIVATIVE, new String[][]{
            {"d/dx x²",          "x^2"},
            {"d/dx sin(x)",      "sin(x)"},
            {"d²/dx² e^x",       "d2/dx2[exp(x)]"},
            {"∂/∂x x²y",         "partial[x^2*y, x]"},
            {"∂/∂y x²y",         "partial[x^2*y, y]"},
            {"gradient x²+y²",   "gradient[x^2+y^2, x, y]"},
            {"curl F",           "curl[y,-x,0, x,y,z]"},
            {"Jacobian",         "jacobian[x^2+y,x*y,x,y]"},
        });

        QUICK.put(OperationPanel.Operation.INTEGRAL, new String[][]{
            {"∫ x² dx",          "x^2"},
            {"∫₀¹ x² dx",        "x^2, 0, 1"},
            {"∫₀^π sin(x) dx",   "sin(x), 0, pi"},
            {"∫₋∞^∞ e^-x² dx",   "exp(-x^2), -10, 10"},
            {"∫∫ x+y dA",        "double[x+y, x, y, 0,1, 0,1]"},
            {"∫∫∫ xyz dV",       "triple[x*y*z, x,y,z, 0,1, 0,1, 0,1]"},
            {"Romberg sin(x)",    "romberg[sin(x)|x|0|3.14159]"},
            {"Gaussian",         "numerical_int[exp(-x^2)|x|-5|5]"},
        });

        QUICK.put(OperationPanel.Operation.LIMIT, new String[][]{
            {"sin(x)/x → 0",         "sin(x)/x as x->0"},
            {"(1+1/n)^n → e",        "(1+1/x)^x as x->inf"},
            {"1/x as x→0⁺",          "1/x as x->0+"},
            {"1/x as x→0⁻",          "1/x as x->0-"},
            {"(e^x-1)/x → 1",        "(exp(x)-1)/x as x->0"},
            {"x·sin(1/x) → 0",       "x*sin(1/x) as x->0"},
            {"(1-cos x)/x² → ½",     "(1-cos(x))/x^2 as x->0"},
            {"n-th root of n → 1",   "x^(1/x) as x->inf"},
        });

        QUICK.put(OperationPanel.Operation.SERIES, new String[][]{
            {"Taylor sin(x)",        "taylor[sin(x),0,6]"},
            {"Taylor cos(x)",        "taylor[cos(x),0,6]"},
            {"Taylor e^x",           "taylor[exp(x),0,6]"},
            {"Maclaurin ln(1+x)",    "maclaurin[ln(1+x),5]"},
            {"Geometric Σ xⁿ",      "convergence[x^n]"},
            {"p-series Σ 1/n²",     "convergence[1/n^2]"},
            {"Harmonic Σ 1/n",      "convergence[1/n]"},
            {"Fourier sin(x)",       "fourier[sin(x), 3.14159, 6]"},
        });

        QUICK.put(OperationPanel.Operation.VECTOR, new String[][]{
            {"Gradient x²+y²",       "gradient[x^2+y^2, x, y]"},
            {"Divergence F",         "div[x^2,y^2,z^2,x,y,z]"},
            {"Curl F",               "curl[y,-x,0, x,y,z]"},
            {"Laplacian x²+y²",      "laplacian[x^2+y^2, x, y]"},
            {"Line integral",        "line[x^2+y^2,x,y,t^2,t,0,1]"},
            {"Surface integral",     "surface[x+y+z,x,y,z]"},
            {"Green's theorem",      "greens[y,-x,0,1,0,1]"},
            {"Stokes' theorem",      "stokes[y,-x,0,x,y,z]"},
        });

        QUICK.put(OperationPanel.Operation.OPTIMIZE, new String[][]{
            {"Min x³-3x",            "x^3-3*x, -3, 3"},
            {"Max sin(x)",           "sin(x), 0, 6.28"},
            {"Saddle x²-y²",         "x^2-y^2"},
            {"Lagrange circle",      "lagrange[x+y, x^2+y^2-1, x,y, -2,-2, 2,2]"},
            {"Lagrange ellipse",     "lagrange[x*y, x^2/4+y^2-1, x,y, -3,-2, 3,2]"},
            {"Golden section",       "sin(x), 0, 3.14159"},
            {"Gradient descent",     "x^2+y^2, -5, 5"},
            {"Newton min",           "x^4-3*x^2+x, -3, 3"},
        });

        QUICK.put(OperationPanel.Operation.DIFFEQ, new String[][]{
            {"RK4 y'=y",             "rk4[y, 0, 2, 1, 100]"},
            {"RK4 y'=t*y",           "rk4[t*y, 0, 2, 1, 100]"},
            {"SHO x''+x=0",          "homogeneous[1,0,1]"},
            {"Damped oscillator",    "homogeneous[1,0.5,1]"},
            {"Heat equation",        "heat[0.5, 1, sin(x), 5]"},
            {"Wave equation",        "heat[0.5, 1, sin(x), 5]"},
            {"Bessel J₀",            "bessel[0, 2.4]"},
            {"Laplace transform",    "rk4[y,0,5,1,100]"},
        });

        QUICK.put(OperationPanel.Operation.APPLIED, new String[][]{
            {"SIR epidemic",         "sir[0.3, 0.1, 990, 10, 0, 100, 200]"},
            {"Lotka-Volterra",       "lotka_volterra[1,0.1,0.075,1.5,10,5,50]"},
            {"Logistic growth",      "logistic[2, 100, 10, 50]"},
            {"Perturbation",         "sir[0.3,0.1,990,10,0,100,200]"},
            {"Buckingham Pi",        "lotka_volterra[1,0.1,0.075,1.5,10,5,50]"},
            {"Phase portrait",       "classify_linear[0,1,-1,0]"},
            {"Bifurcation",          "seir[0.3,0.1,0.05,990,5,5,0,100]"},
            {"Turing instability",   "classify_linear[0,1,-1,0]"},
        });

        QUICK.put(OperationPanel.Operation.STATISTICS, new String[][]{
            {"Summarize",            "summarize[2,4,4,4,5,5,7,9]"},
            {"t-test two sample",    "t_test[1,2,3,4,5|2,4,6,8,10]"},
            {"Linear regression",   "regression[1,2,3,4,5|2.1,3.9,6.2,7.8,9.5]"},
            {"Pearson r",            "t_test[1,2,3,4,5|2,4,6,8,10]"},
            {"χ² test",              "anova[1,2,3|4,5,6|7,8,9]"},
            {"ANOVA",                "anova[1,2,3|4,5,6|7,8,9]"},
            {"Kaplan-Meier",         "kaplan_meier[1,2,3,4|1,0,1,1]"},
            {"Shapiro-Wilk",         "shapiro[2,4,4,4,5,5,7,9]"},
        });

        QUICK.put(OperationPanel.Operation.PROBABILITY, new String[][]{
            {"Normal PDF",           "normal_pdf[0,0,1]"},
            {"Normal CDF",           "normal_cdf[1.96,0,1]"},
            {"Binomial P(X=k)",      "binomial_pmf[3,10,0.5]"},
            {"Poisson PMF",          "poisson_pmf[2,3]"},
            {"MGF Normal",           "mgf_normal[0,1,0.5]"},
            {"Gambler's ruin",       "gamblers_ruin[0.5,5,10]"},
            {"Brownian motion",      "gbm[100,0.05,0.2,1]"},
            {"Markov chain",         "markov[0.7,0.3;0.4,0.6|0.6,0.4|5]"},
        });

        QUICK.put(OperationPanel.Operation.NUMBER_THY, new String[][]{
            {"Factor 360",           "prime_factors[360]"},
            {"GCD(48,18)",           "gcd[48,18]"},
            {"LCM(12,18)",           "lcm[12,18]"},
            {"Is prime 104729?",     "is_prime[104729]"},
            {"φ(100)",               "euler_phi[100]"},
            {"Fibonacci(20)",        "fibonacci[20]"},
            {"Collatz(27)",          "prime_factors[360]"},
            {"Modular pow 3^100",    "mod_pow[3,100,1000000007]"},
        });

        QUICK.put(OperationPanel.Operation.DISCRETE, new String[][]{
            {"C(10,3)",              "combinations[10,3]"},
            {"P(10,3)",              "permutations[10,3]"},
            {"Dijkstra",             "dijkstra[0,1,4;0,2,1;1,3,1;2,1,2;2,3,5|0]"},
            {"BFS",                  "bfs[0,1;0,2;1,3;2,3|0]"},
            {"Recurrence",           "recurrence[[1,1]|[0,1]|10]"},
            {"Master theorem",       "master[2,2,1]"},
            {"Pigeonhole",           "combinations[10,3]"},
            {"Inclusion-exclusion",  "permutations[10,3]"},
        });

        QUICK.put(OperationPanel.Operation.ALGEBRA, new String[][]{
            {"Cyclic Z₆",            "cyclic[6]"},
            {"Dihedral D₄",          "dihedral[4]"},
            {"Symmetric S₃",         "symmetric[3]"},
            {"Sylow p=2, |G|=12",    "sylow[2,12]"},
            {"Poly mul",             "poly_mul[[1,0,1]|[1,1]]"},
            {"Poly GCD mod 5",       "poly_gcd[[1,0,-1]|[1,-1]|5]"},
            {"Galois GF(2⁴)",        "galois_field[2,4]"},
            {"Perm cycle",           "perm_cycle[[1,2,0,3]]"},
        });

        QUICK.put(OperationPanel.Operation.COMPLEX, new String[][]{
            {"Polar 3+4i",           "polar[3,4]"},
            {"Residue 1/(z²+1)",     "residue[1/(z^2+1)|0|1]"},
            {"Riemann zeta ζ(2)",    "zeta[2,0]"},
            {"Cauchy-Riemann",       "cauchy_riemann[x^2-y^2|2*x*y|x|y]"},
            {"Laurent series",       "polar[3,4]"},
            {"Möbius map",           "mobius[1,0,0,1|1,2]"},
            {"Arg & Modulus",        "polar[3,4]"},
            {"Conformal map w=z²",   "residue[1/(z^2+1)|0|1]"},
        });

        QUICK.put(OperationPanel.Operation.GEOMETRY, new String[][]{
            {"Circle (0,0,5)",       "circle[0,0,5]"},
            {"Ellipse",              "ellipse[0,0,3,2,0]"},
            {"Distance 3D",          "distance_3d[1,2,3|4,5,6]"},
            {"Cross product",        "cross_product[1,0,0|0,1,0]"},
            {"Dot product",          "dot_product[1,2,3|4,5,6]"},
            {"Triangle area",        "triangle_area[0,0|3,0|0,4]"},
            {"Sphere volume r=5",    "sphere[5]"},
            {"Convex hull",          "convex_hull[1,1,3,1,2,3,0,2]"},
        });

        QUICK.put(OperationPanel.Operation.NUMERICAL, new String[][]{
            {"Newton x³-x-2",        "newton[x^3-x-2|x|1.5]"},
            {"Bisection x³-x-2",     "bisection[x^3-x-2|x|1|2]"},
            {"Romberg sin(x)",       "romberg[sin(x)|x|0|3.14159]"},
            {"Simpson ∫₀¹ x²",       "simpsons[x^2|x|0|1|100]"},
            {"Spline interpolation", "spline[0,1,2,3|0,1,4,9|1.5]"},
            {"LU decomposition",     "gauss_elim[[2,1],[3,4]|[5,6]]"},
            {"Power method",         "jacobi[[4,1],[1,3]|[1,1]]"},
            {"FFT",                  "fft[1,1,0,-1,-1,-1,0,1]"},
        });

        QUICK.put(OperationPanel.Operation.MATRIX, new String[][]{
            {"Det 2×2",              "[[1,2],[3,4]]"},
            {"Inverse 2×2",          "la:inv|[[2,1],[5,3]]"},
            {"Eigenvalues",          "la:eigen|[[4,1],[2,3]]"},
            {"SVD",                  "la:svd|[[1,2],[3,4],[5,6]]"},
            {"LU decomp",            "la:lu|[[2,1,-1],[4,3,-3],[2,-1,2]]"},
            {"QR decomp",            "la:qr|[[1,1],[1,-1],[0,1]]"},
            {"Null space",           "la:null|[[1,2,3],[4,5,6],[7,8,9]]"},
            {"Rank",                 "la:rank|[[1,2,3],[4,5,6],[7,8,9]]"},
        });
    }

    // ── State ─────────────────────────────────────────────────────────────────

    // callback(expression, targetOperation) — operation may be null if same mode
    private BiConsumer<String, OperationPanel.Operation> onExpressionSelected;
    private final Label     chevron = new Label("▸");
    private final Label     title   = new Label("QUICK FUNCTIONS");
    private final FlowPane  grid    = new FlowPane(5, 5);
    private final VBox      body    = new VBox(grid);
    private boolean expanded = false;

    // ── Constructor ───────────────────────────────────────────────────────────

    public QuickFunctionsPanel() {
        getStyleClass().add("quick-panel");

        // ── Header (click to expand/collapse) ─────────────────────────────────
        chevron.getStyleClass().add("quick-panel-chevron");
        title.getStyleClass().add("quick-panel-title");
        Region spacer = new Region();
        HBox.setHgrow(spacer, Priority.ALWAYS);

        HBox header = new HBox(6, chevron, title, spacer);
        header.setAlignment(Pos.CENTER_LEFT);
        header.getStyleClass().add("quick-panel-header");
        header.setOnMouseClicked(e -> toggleExpanded());
        header.setOnKeyPressed(e -> {
            if (e.getCode() == javafx.scene.input.KeyCode.ENTER ||
                e.getCode() == javafx.scene.input.KeyCode.SPACE)
                toggleExpanded();
        });
        header.setFocusTraversable(true);

        // ── Button grid ───────────────────────────────────────────────────────
        grid.getStyleClass().add("quick-btn-grid");
        grid.setAlignment(Pos.TOP_LEFT);
        grid.setPadding(new Insets(8, 10, 8, 10));

        body.getStyleClass().add("quick-body");
        body.setVisible(false);
        body.setManaged(false);

        getChildren().addAll(header, body);
    }

    // ── Public API ────────────────────────────────────────────────────────────

    public void setOnExpressionSelected(BiConsumer<String, OperationPanel.Operation> cb) {
        onExpressionSelected = cb;
    }

    /** Repopulate the grid for the given operation. */
    public void setOperation(OperationPanel.Operation op) {
        grid.getChildren().clear();
        String[][] entries = QUICK.get(op);
        if (entries == null) return;

        for (String[] entry : entries) {
            String display    = entry[0];
            String expression = entry[1];
            Button btn = new Button(display);
            btn.getStyleClass().add("quick-btn");
            btn.setTooltip(new Tooltip(expression));
            btn.setFocusTraversable(true);
            btn.setOnAction(e -> {
                if (onExpressionSelected != null)
                    onExpressionSelected.accept(expression, op);
            });
            grid.getChildren().add(btn);
        }
    }

    public boolean isExpanded() { return expanded; }

    // ── Private helpers ───────────────────────────────────────────────────────

    private void toggleExpanded() {
        expanded = !expanded;
        body.setVisible(expanded);
        body.setManaged(expanded);
        chevron.setText(expanded ? "▾" : "▸");
    }
}
