package com.mathengine.ui;

import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.image.ImageView;
import javafx.scene.input.KeyCode;
import javafx.scene.input.KeyCombination;
import javafx.scene.input.KeyCodeCombination;
import javafx.scene.layout.*;
import java.util.function.BiConsumer;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.control.ScrollPane;
import javafx.scene.control.TextField;
import javafx.scene.control.Tooltip;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.VBox;

/**
 * InputPanel
 * ───────────
 * The main input area. Contains:
 *
 *   ┌──────────────────────────────────────────────────────────┐
 *   │  Operation badge          [Symbolic] [Numerical]        │  ← header bar
 *   ├──────────────────────────────────────────────────────────┤
 *   │  $ __________________________________________ $         │  ← LaTeX field
 *   ├──────────────────────────────────────────────────────────┤
 *   │  RENDERED INPUT PREVIEW (JLaTeXMath ImageView)          │  ← live preview
 *   ├──────────────────────────────────────────────────────────┤
 *   │               [Clear]           [Solve ▶]               │  ← action bar
 *   └──────────────────────────────────────────────────────────┘
 *
 * Keyboard:
 *   Ctrl+Enter  — Solve
 *   Escape      — Clear
 *   Tab         — focus moves to Solve button
 *
 * Communicates upward via a single BiConsumer:
 *   onSolve(expression: String, precisionFlag: int)
 *     where precisionFlag 0 = Symbolic, 1 = Numerical
 */
public class InputPanel extends VBox {

    // ── Widgets ───────────────────────────────────────────────────────────────

    private final TextField  latexField     = new TextField();
    private final Label      opBadge        = new Label("Arithmetic");
    private final Button     btnSymbolic    = new Button("Symbolic");
    private final Button     btnNumerical   = new Button("Numerical");
    private final Button     btnSolve       = new Button("Solve  ▶");
    private final Button     btnClear       = new Button("Clear");
    private final VBox       previewBox     = new VBox();
    private final Label      previewLabel   = new Label("Preview");
    private final ScrollPane previewScroll  = new ScrollPane();

    // ── State ─────────────────────────────────────────────────────────────────

    private String  currentPrecision = "symbolic";   // "symbolic" | "numerical"
    private BiConsumer<String, Integer> onSolve;
    private MathKeyboard mathKeyboard;

    private final ThemeManager tm = ThemeManager.getInstance();

    // ── Preview debounce ──────────────────────────────────────────────────────

    private final javafx.animation.PauseTransition previewDebounce =
        new javafx.animation.PauseTransition(javafx.util.Duration.millis(200));

    // ── Constructor ───────────────────────────────────────────────────────────

    public InputPanel() {
        setSpacing(0);
        getStyleClass().add("input-card");

        getChildren().addAll(
            buildHeader(),
            buildLatexRow(),
            buildPreviewArea(),
            buildActionBar()
        );

        wireEvents();
        mathKeyboard = new MathKeyboard(latexField);
    }

    // ── Build sub-sections ────────────────────────────────────────────────────

    /** Header: operation badge on the left, precision toggle on the right. */
    private HBox buildHeader() {
        opBadge.getStyleClass().addAll("op-badge", "badge-arithmetic");

        btnSymbolic .getStyleClass().addAll("precision-btn", "precision-active");
        btnNumerical.getStyleClass().add("precision-btn");

        btnSymbolic .setFocusTraversable(true);
        btnNumerical.setFocusTraversable(true);

        // ADA
        btnSymbolic .setAccessibleText("Symbolic output mode — returns exact forms like π, √2");
        btnNumerical.setAccessibleText("Numerical output mode — returns floating-point approximations");
        btnSymbolic .setTooltip(new Tooltip("Return exact symbolic forms (π, √2, x³/3 + C)"));
        btnNumerical.setTooltip(new Tooltip("Return floating-point approximations (3.14159…)"));

        Label precLabel = new Label("Output:");
        precLabel.getStyleClass().add("precision-label");

        HBox precGroup = new HBox(4, precLabel, btnSymbolic, btnNumerical);
        precGroup.setAlignment(Pos.CENTER_RIGHT);

        HBox header = new HBox(opBadge, new Spacer(), precGroup);
        header.setAlignment(Pos.CENTER_LEFT);
        header.setPadding(new Insets(12, 16, 12, 16));
        header.getStyleClass().add("input-card-header");
        return header;
    }

    /** LaTeX input row: sigil $ — text field — sigil $ */
    private HBox buildLatexRow() {
        Label sigilL = new Label("$");
        Label sigilR = new Label("$");
        sigilL.getStyleClass().add("latex-sigil");
        sigilR.getStyleClass().add("latex-sigil");
        sigilL.setFocusTraversable(false);
        sigilR.setFocusTraversable(false);

        latexField.getStyleClass().add("latex-field");
        latexField.setPromptText("Enter LaTeX expression…  e.g.  \\int_0^8 x^2\\,dx");
        latexField.setMaxWidth(Double.MAX_VALUE);
        HBox.setHgrow(latexField, Priority.ALWAYS);

        // ADA
        latexField.setAccessibleText("LaTeX expression input");
        latexField.setTooltip(new Tooltip(
            "Enter a LaTeX expression.\n" +
            "Examples:\n" +
            "  \\int_0^8 x^2\\,dx\n" +
            "  sin(pi/2) + cos(pi/3)\n" +
            "  \\frac{d}{dx}[x^3]\n" +
            "Press Ctrl+Enter to solve."
        ));

        HBox row = new HBox(8, sigilL, latexField, sigilR);
        row.setAlignment(Pos.CENTER);
        row.setPadding(new Insets(14, 16, 10, 16));
        row.getStyleClass().add("latex-row");
        return row;
    }

    /** Rendered LaTeX preview using JLaTeXMath. Updates 200 ms after typing stops. */
    private VBox buildPreviewArea() {
        previewLabel.getStyleClass().add("preview-label");

        // Placeholder shown before the user types anything
        Label placeholder = new Label("Your expression will render here…");
        placeholder.getStyleClass().add("preview-placeholder");
        placeholder.setId("preview-placeholder");

        previewBox.getChildren().add(placeholder);
        previewBox.setPadding(new Insets(8, 0, 8, 0));
        previewBox.setAlignment(Pos.CENTER_LEFT);

        previewScroll.setContent(previewBox);
        previewScroll.setFitToHeight(true);
        previewScroll.setPrefHeight(80);
        previewScroll.setMinHeight(60);
        previewScroll.getStyleClass().add("preview-scroll");
        previewScroll.setFocusTraversable(false);
        // ADA: the preview is decorative — screen reader reads the raw text field
        previewScroll.setAccessibleText("Rendered math preview");

        VBox container = new VBox(6, previewLabel, previewScroll);
        container.setPadding(new Insets(4, 16, 10, 16));
        container.getStyleClass().add("preview-container");
        return container;
    }

    /** Action bar: Clear button on the left, Solve button on the right. */
    private HBox buildActionBar() {
        btnClear.getStyleClass().add("btn-clear");
        btnSolve.getStyleClass().add("btn-solve");

        btnClear.setFocusTraversable(true);
        btnSolve.setFocusTraversable(true);
        btnSolve.setDefaultButton(true);

        btnClear.setAccessibleText("Clear the input field");
        btnSolve.setAccessibleText("Solve the expression (Ctrl+Enter)");

        btnClear.setTooltip(new Tooltip("Clear input (Escape)"));
        btnSolve.setTooltip(new Tooltip("Solve (Ctrl+Enter)"));

        Button btnKeyboard = new Button("⌨");
        btnKeyboard.getStyleClass().add("btn-keyboard");
        btnKeyboard.setTooltip(new Tooltip("Math keyboard (Ctrl+K)"));
        btnKeyboard.setFocusTraversable(true);
        btnKeyboard.setAccessibleText("Toggle math keyboard");
        btnKeyboard.setOnAction(e -> mathKeyboard.toggle(btnKeyboard));

        HBox bar = new HBox(10, btnClear, new Spacer(), btnKeyboard, btnSolve);
        bar.setAlignment(Pos.CENTER_RIGHT);
        bar.setPadding(new Insets(12, 16, 12, 16));
        bar.getStyleClass().add("action-bar");
        return bar;
    }

    // ── Event wiring ──────────────────────────────────────────────────────────

    private void wireEvents() {
        // Precision toggle
        btnSymbolic.setOnAction(e  -> setPrecision("symbolic"));
        btnNumerical.setOnAction(e -> setPrecision("numerical"));

        // Solve / Clear
        btnSolve.setOnAction(e -> handleSolve());
        btnClear.setOnAction(e -> clearInput());

        // Keyboard shortcuts on the text field
        latexField.setOnKeyPressed(e -> {
            if (new KeyCodeCombination(KeyCode.ENTER, KeyCombination.CONTROL_DOWN).match(e)) {
                e.consume();
                handleSolve();
            } else if (e.getCode() == KeyCode.ESCAPE) {
                e.consume();
                clearInput();
            } else if (e.getCode() == KeyCode.K &&
                       e.isControlDown()) {
                e.consume();
                // find the keyboard button and toggle via it
                getScene().lookup(".btn-keyboard").fireEvent(
                    new javafx.event.ActionEvent());
            }
        });

        // Live preview: debounce 200 ms after last keystroke
        latexField.textProperty().addListener((obs, old, text) -> {
            previewDebounce.setOnFinished(ev -> updatePreview(text));
            previewDebounce.playFromStart();
        });
    }

    // ── Preview rendering ─────────────────────────────────────────────────────

    private void updatePreview(String latex) {
        previewBox.getChildren().clear();

        if (latex == null || latex.isBlank()) {
            Label ph = new Label("Your expression will render here…");
            ph.getStyleClass().add("preview-placeholder");
            previewBox.getChildren().add(ph);
            return;
        }

        ImageView rendered = LatexRenderer.render(latex, tm);
        if (rendered.getImage() != null) {
            previewBox.getChildren().add(rendered);
        } else {
            // JLaTeXMath couldn't parse it yet (user still typing)
            Label partial = new Label(latex);
            partial.getStyleClass().add("preview-raw");
            previewBox.getChildren().add(partial);
        }
    }

    // ── Actions ───────────────────────────────────────────────────────────────

    private void handleSolve() {
        String expression = latexField.getText().trim();
        if (expression.isEmpty()) return;
        int flag = currentPrecision.equals("symbolic") ? 0 : 1;
        if (onSolve != null) onSolve.accept(expression, flag);
    }

    public void clearInput() {
        latexField.clear();
        previewBox.getChildren().clear();
        Label ph = new Label("Your expression will render here…");
        ph.getStyleClass().add("preview-placeholder");
        previewBox.getChildren().add(ph);
        latexField.requestFocus();
    }

    // ── Public API ────────────────────────────────────────────────────────────

    /** Load an expression into the field (e.g. from history). */
    public void loadExpression(String latex) {
        latexField.setText(latex);
        latexField.positionCaret(latex.length());
        latexField.requestFocus();
    }

    /** Update the operation badge text and style. */
    public void setOperationBadge(String name, String styleClass) {
        opBadge.setText(name);
        opBadge.getStyleClass().removeIf(c -> c.startsWith("badge-"));
        opBadge.getStyleClass().add(styleClass);
        // Update placeholder based on operation
        latexField.setPromptText(promptForOp(name));
    }

    // Called by MainLayout when operation changes — sets prompt text directly
    public void setPlaceholder(String prompt) {
        latexField.setPromptText(prompt);
    }

    public void setOnSolve(BiConsumer<String, Integer> cb) { onSolve = cb; }

    public String  getCurrentExpression() { return latexField.getText().trim(); }
    public String  getCurrentPrecision()  { return currentPrecision; }
    public void    focusInput()           { latexField.requestFocus(); }

    // ── Precision toggle ──────────────────────────────────────────────────────

    private void setPrecision(String mode) {
        currentPrecision = mode;
        btnSymbolic .getStyleClass().removeAll("precision-active");
        btnNumerical.getStyleClass().removeAll("precision-active");
        if (mode.equals("symbolic"))  btnSymbolic .getStyleClass().add("precision-active");
        else                          btnNumerical.getStyleClass().add("precision-active");
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    private static String promptForOp(String op) {
        return switch (op) {
            case "Derivative"  -> "x^3 + sin(x)   or   d/dx[f]   or   partial[x^2*y, x]";
            case "Integral"    -> "x^2   or   x^2, 0, 1   or   double[x+y, x, y, 0,1, 0,1]";
            case "Limit"       -> "sin(x)/x as x->0   or   1/x as x->0+";
            case "Series"      -> "taylor[sin(x),0,6]   or   convergence[1/n^2]";
            case "Vector"      -> "gradient[x^2+y^2, x, y]   or   curl[y,-x,0, x,y,z]";
            case "Optimize"    -> "x^3-3*x, -3, 3   or   lagrange[x+y, x^2+y^2-1, x,y, -2,-2, 2,2]";
            case "Diff Eq"     -> "homogeneous[1,0,1]   or   rk4[t*y,0,2,1,100]   or   mixing[100,0.5,2,2,0,100]   or   cooling[0.1,20,100,50]   or   fourier[sin(x),3.14,8]   or   bessel[0,2.4]";
            case "Statistics"  -> "summarize[1,2,3,4,5]   or   t_test[1,2,3|4,5,6]   or   kaplan_meier[1,2,3,4|1,0,1,1]   or   xbar_chart[r1|r2|r3]   or   shapiro[data]";
            case "Applied"     -> "sir[0.3,0.1,990,10,0,100,200]   or   lotka_volterra[1,0.1,0.075,1.5,10,5,50]";
            case "Matrix"      -> "[[1,2],[3,4]]";
            case "Number Thy"  -> "prime_factors[360]   or   gcd[48,18]   or   fibonacci[20]   or   mod_pow[3,100,1000000007]";
            case "Discrete"    -> "combinations[10,3]   or   dfs[adj|0]   or   recurrence[[1,1]|[0,1]|10]   or   master[2,2,1]";
            case "Complex"     -> "polar[3,4]   or   cauchy_riemann[x^2-y^2|2*x*y|x|y]   or   residue[1/(z^2+1)|0|1]   or   zeta[2,0]";
            case "Numerical"   -> "newton[x^3-x-2|x|1.5]   or   romberg[sin(x)|x|0|3.14159]   or   bisection[x^3-x-2|x|1|2]   or   spline[xs|ys|xeval]";
            case "Algebra"     -> "cyclic[6]   or   poly_mul[[1,0,1]|[1,1]]   or   perm_cycle[[1,2,0,3]]   or   galois_field[2,4]";
            case "Probability" -> "mgf_normal[0,1,0.5]   or   gbm[100,0.05,0.2,1]   or   gamblers_ruin[0.5,5,10]   or   poisson_proc[2,5]";
            case "Geometry"    -> "circle[0,0,5]   or   ellipse[0,0,3,2,0]   or   distance_3d[1,2,3|4,5,6]   or   cross_product[1,0,0|0,1,0]";
            default            -> "sin(pi/2) + 5   or   sqrt(2^2 + 3^2)";
        };
    }

    private static class Spacer extends Region {
        Spacer() { HBox.setHgrow(this, Priority.ALWAYS); }
    }
}