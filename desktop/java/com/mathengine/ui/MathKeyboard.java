package com.mathengine.ui;

import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.control.*;
import javafx.scene.input.KeyCode;
import javafx.scene.layout.*;
import javafx.stage.Popup;

/**
 * MathKeyboard
 * ─────────────
 * A Symbolab / Desmos-style popup math keyboard that inserts LaTeX snippets
 * into a target TextField.
 *
 * Layout (tabbed):
 *   [Basics]  [Greek]  [Functions]  [Calculus]  [Matrix]  [Logic]
 *
 * Each tab is a grid of clickable keys.  Keys with a leading cursor marker "│"
 * place the cursor inside the inserted text (e.g. \frac{│}{} ).
 *
 * Usage:
 *   MathKeyboard kb = new MathKeyboard(latexField);
 *   Button toggleBtn = new Button("⌨");
 *   toggleBtn.setOnAction(e -> kb.toggle(toggleBtn));
 */
public class MathKeyboard {

    // ── Key definition ────────────────────────────────────────────────────────

    /**
     * @param display  Label shown on the key button
     * @param latex    Text inserted into the field; "│" marks cursor position
     * @param tooltip  Tooltip description
     */
    private record Key(String display, String latex, String tooltip) {}

    // ── Key tables ────────────────────────────────────────────────────────────

    private static final Key[][] BASICS = {
        // Row 0: digits + arithmetic
        { key("7","7"), key("8","8"), key("9","9"), key("÷","/ "), key("x²","x^{2}"),key("xⁿ","x^{│}"),  key("√","\\sqrt{│}"),   key("ⁿ√","\\sqrt[│]{}") },
        { key("4","4"), key("5","5"), key("6","6"), key("×","* "), key("1/x","\\frac{1}{│}"), key("a/b","\\frac{│}{}"), key("|x|","\\left|│\\right|"), key("(·)","\\left(│\\right)") },
        { key("1","1"), key("2","2"), key("3","3"), key("−","- "), key("e^x","e^{│}"),  key("10^x","10^{│}"), key("²","^{2}"),      key("³","^{3}") },
        { key("0","0"), key(".","." ), key("±","\\pm "), key("+","+ "), key(",",", "), key("=","= "), key("≠","\\neq "),  key("≈","\\approx ") },
        { key("(","( "), key(")",") "), key("[","[ "), key("]","] "), key("{","\\{ "), key("}","\\} "), key("∞","\\infty"), key("π","\\pi") },
    };

    private static final Key[][] GREEK = {
        { key("α","\\alpha"),   key("β","\\beta"),  key("γ","\\gamma"),  key("δ","\\delta"),
          key("ε","\\epsilon"), key("ζ","\\zeta"),  key("η","\\eta"),    key("θ","\\theta") },
        { key("ι","\\iota"),    key("κ","\\kappa"),  key("λ","\\lambda"), key("μ","\\mu"),
          key("ν","\\nu"),      key("ξ","\\xi"),     key("π","\\pi"),     key("ρ","\\rho") },
        { key("σ","\\sigma"),   key("τ","\\tau"),    key("υ","\\upsilon"),key("φ","\\phi"),
          key("χ","\\chi"),     key("ψ","\\psi"),    key("ω","\\omega"),  key("∂","\\partial") },
        { key("Γ","\\Gamma"),   key("Δ","\\Delta"),  key("Θ","\\Theta"),  key("Λ","\\Lambda"),
          key("Ξ","\\Xi"),      key("Σ","\\Sigma"),  key("Φ","\\Phi"),    key("Ψ","\\Psi") },
        { key("Ω","\\Omega"),   key("∇","\\nabla"),  key("ℵ","\\aleph"),  key("ℏ","\\hbar"),
          key("∞","\\infty"),   key("∅","\\emptyset"),key("…","\\ldots"), key("·","\\cdot") },
    };

    private static final Key[][] FUNCTIONS = {
        { key("sin","\\sin(│)"),   key("cos","\\cos(│)"),  key("tan","\\tan(│)"),  key("cot","\\cot(│)"),
          key("sec","\\sec(│)"),   key("csc","\\csc(│)"),  key("sinh","\\sinh(│)"),key("cosh","\\cosh(│)") },
        { key("sin⁻¹","\\arcsin(│)"),key("cos⁻¹","\\arccos(│)"),key("tan⁻¹","\\arctan(│)"),key("tanh","\\tanh(│)"),
          key("ln","\\ln(│)"),     key("log","\\log(│)"),  key("log₂","\\log_2(│)"),key("logₙ","\\log_{│}()") },
        { key("exp","e^{│}"),      key("√","\\sqrt{│}"),   key("∛","\\sqrt[3]{│}"),key("|·|","\\left|│\\right|"),
          key("⌊·⌋","\\lfloor │\\rfloor"),key("⌈·⌉","\\lceil │\\rceil"),key("Γ","\\Gamma(│)"),key("n!","│!") },
        { key("max","\\max(│,)"),  key("min","\\min(│,)"), key("gcd","\\gcd(│,)"), key("lcm","\\text{lcm}(│,)"),
          key("Re","\\text{Re}(│)"),key("Im","\\text{Im}(│)"),key("arg","\\arg(│)"),key("sgn","\\text{sgn}(│)") },
        { key("sum","\\sum_{│}^{}"),key("prod","\\prod_{│}^{}"),key("lim","\\lim_{│\\to}"),key("binom","\\binom{│}{}"),
          key("f∘g","f\\circ g"),  key("f⁻¹","f^{-1}(│)"),key("‖·‖","\\|│\\|"),  key("⟨·,·⟩","\\langle│,\\rangle") },
    };

    private static final Key[][] CALCULUS = {
        { key("d/dx","\\frac{d}{dx}│"),        key("∂/∂x","\\frac{\\partial}{\\partial x}│"),
          key("d²/dx²","\\frac{d^2}{dx^2}│"),  key("dy/dx","\\frac{dy}{dx}"),
          key("∫","\\int│\\,dx"),               key("∫ab","\\int_{│}^{}\\,dx"),
          key("∬","\\iint│\\,dA"),              key("∭","\\iiint│\\,dV") },
        { key("∮","\\oint_{│}"),                key("∑","\\sum_{n=│}^{\\infty}"),
          key("∏","\\prod_{n=│}^{\\infty}"),    key("lim","\\lim_{x \\to │}"),
          key("lim 0⁺","\\lim_{x \\to 0^+}"),  key("lim ∞","\\lim_{x \\to \\infty}"),
          key("→0","\\to 0"),                   key("→∞","\\to \\infty") },
        { key("∇","\\nabla"),                   key("∇²","\\nabla^2"),
          key("∇·F","\\nabla \\cdot \\mathbf{F}"),key("∇×F","\\nabla \\times \\mathbf{F}"),
          key("grad","\\nabla │"),              key("div","\\nabla \\cdot │"),
          key("curl","\\nabla \\times │"),      key("Lap","\\Delta │") },
        { key("Taylor","\\sum_{n=0}^{\\infty}\\frac{f^{(n)}(a)}{n!}(x-a)^n"),
          key("L{f}","\\mathcal{L}\\{│\\}"),   key("F{f}","\\mathcal{F}\\{│\\}"),
          key("δ(x)","\\delta(x)"),             key("H(x)","H(│)"),
          key("*","*"),                          key("→","\\rightarrow"),
          key("⇒","\\Rightarrow") },
        { key("dx","\\,dx"),   key("dy","\\,dy"),  key("dt","\\,dt"),  key("ds","\\,ds"),
          key("dA","\\,dA"),   key("dV","\\,dV"),  key("dθ","\\,d\\theta"),key("dr","\\,dr") },
    };

    private static final Key[][] MATRIX = {
        { key("2×2","\\begin{pmatrix}│ & \\\\ & \\end{pmatrix}"),
          key("2×3","\\begin{pmatrix}│ & & \\\\ & & \\end{pmatrix}"),
          key("3×3","\\begin{pmatrix}│ & & \\\\ & & \\\\ & & \\end{pmatrix}"),
          key("mxn","\\begin{pmatrix}│\\end{pmatrix}"),
          key("det","\\det\\begin{pmatrix}│\\end{pmatrix}"),
          key("tr","\\text{tr}(│)"),
          key("Aᵀ","│^T"),
          key("A⁻¹","│^{-1}") },
        { key("·","\\cdot"),     key("⊗","\\otimes"),  key("⊕","\\oplus"),  key("⊂","\\subset"),
          key("⊆","\\subseteq"), key("∈","\\in"),       key("∉","\\notin"),  key("∩","\\cap") },
        { key("∪","\\cup"),      key("\\","\\setminus"),key("Ø","\\emptyset"),key("∀","\\forall"),
          key("∃","\\exists"),   key("¬","\\neg"),      key("∧","\\wedge"),  key("∨","\\vee") },
        { key("[a]","\\begin{bmatrix}│\\end{bmatrix}"),
          key("|a|","\\begin{vmatrix}│\\end{vmatrix}"),
          key("||a||","\\begin{Vmatrix}│\\end{Vmatrix}"),
          key("→v","\\vec{│}"),
          key("v̂","\\hat{│}"),
          key("‾v","\\bar{│}"),
          key("v⃗","\\overrightarrow{│}"),
          key("‖v‖","\\|│\\|") },
        { key("i","\\mathbf{i}"), key("j","\\mathbf{j}"),key("k","\\mathbf{k}"),key("0","\\mathbf{0}"),
          key("I","\\mathbf{I}"), key("A","\\mathbf{A}"),key("λ","\\lambda"),  key("rank","\\text{rank}(│)") },
    };

    private static final Key[][] LOGIC = {
        { key("∀","\\forall"),  key("∃","\\exists"),  key("¬","\\neg"),     key("∧","\\wedge"),
          key("∨","\\vee"),     key("⇒","\\Rightarrow"),key("⇔","\\Leftrightarrow"),key("⊢","\\vdash") },
        { key("∈","\\in"),      key("∉","\\notin"),   key("⊂","\\subset"),  key("⊆","\\subseteq"),
          key("∪","\\cup"),     key("∩","\\cap"),     key("\\","\\setminus"),key("×","\\times") },
        { key("ℕ","\\mathbb{N}"),key("ℤ","\\mathbb{Z}"),key("ℚ","\\mathbb{Q}"),key("ℝ","\\mathbb{R}"),
          key("ℂ","\\mathbb{C}"),key("ℙ","\\mathbb{P}"),key("∅","\\emptyset"),key("∞","\\infty") },
        { key("≤","\\leq"),     key("≥","\\geq"),     key("≠","\\neq"),     key("≈","\\approx"),
          key("≡","\\equiv"),   key("≅","\\cong"),    key("∼","\\sim"),     key("∝","\\propto") },
        { key("↦","\\mapsto"),  key("→","\\to"),      key("←","\\leftarrow"),key("↔","\\leftrightarrow"),
          key("↑","\\uparrow"), key("↓","\\downarrow"),key("⊥","\\perp"),  key("∥","\\parallel") },
    };

    // ── Helper to build a Key ─────────────────────────────────────────────────
    private static Key key(String d, String l) { return new Key(d, l, l); }


    // ── Instance state ────────────────────────────────────────────────────────

    private final javafx.scene.control.TextField target;
    private final Popup popup = new Popup();
    private boolean built = false;

    // ── Constructor ───────────────────────────────────────────────────────────

    public MathKeyboard(javafx.scene.control.TextField targetField) {
        this.target = targetField;
    }

    // ── Public API ────────────────────────────────────────────────────────────

    /** Show or hide the keyboard anchored below anchorNode. */
    public void toggle(javafx.scene.Node anchorNode) {
        if (popup.isShowing()) {
            popup.hide();
        } else {
            ensureBuilt();
            javafx.geometry.Bounds bounds = anchorNode.localToScreen(anchorNode.getBoundsInLocal());
            if (bounds == null) return;
            popup.show(anchorNode.getScene().getWindow(),
                       bounds.getMinX(),
                       bounds.getMaxY() + 4);
        }
    }

    public void hide() { popup.hide(); }
    public boolean isShowing() { return popup.isShowing(); }

    // ── Build keyboard UI (lazy) ──────────────────────────────────────────────

    private void ensureBuilt() {
        if (built) return;
        built = true;

        VBox root = new VBox(0);
        root.getStyleClass().add("math-keyboard");

        // Dismiss on Escape
        root.setOnKeyPressed(e -> { if (e.getCode() == KeyCode.ESCAPE) popup.hide(); });

        TabPane tabs = new TabPane();
        tabs.getStyleClass().add("math-keyboard-tabs");
        tabs.setTabClosingPolicy(TabPane.TabClosingPolicy.UNAVAILABLE);
        tabs.setTabMinWidth(62);

        tabs.getTabs().addAll(
            buildTab("Basics",    BASICS),
            buildTab("Greek",     GREEK),
            buildTab("Functions", FUNCTIONS),
            buildTab("Calculus",  CALCULUS),
            buildTab("Matrix",    MATRIX),
            buildTab("Logic",     LOGIC)
        );

        // Backspace + close row
        Button bsBtn = new Button("⌫  Backspace");
        bsBtn.getStyleClass().add("kb-backspace");
        bsBtn.setOnAction(e -> doBackspace());

        Button closeBtn = new Button("✕ Close");
        closeBtn.getStyleClass().add("kb-close");
        closeBtn.setOnAction(e -> popup.hide());

        Region spacer = new Region();
        HBox.setHgrow(spacer, Priority.ALWAYS);

        HBox bottomBar = new HBox(8, bsBtn, spacer, closeBtn);
        bottomBar.getStyleClass().add("kb-bottom-bar");
        bottomBar.setPadding(new Insets(6, 10, 6, 10));
        bottomBar.setAlignment(Pos.CENTER_LEFT);

        root.getChildren().addAll(tabs, bottomBar);

        popup.setAutoHide(true);
        popup.setHideOnEscape(true);
        popup.getContent().add(root);
    }

    private Tab buildTab(String name, Key[][] rows) {
        VBox grid = new VBox(4);
        grid.getStyleClass().add("kb-grid");
        grid.setPadding(new Insets(8, 10, 8, 10));

        for (Key[] row : rows) {
            HBox hrow = new HBox(4);
            hrow.setAlignment(Pos.CENTER_LEFT);
            for (Key k : row) {
                if (k == null) continue;
                Button btn = new Button(k.display());
                btn.getStyleClass().add("kb-key");
                btn.setMinWidth(50);
                btn.setPrefHeight(36);
                btn.setTooltip(new Tooltip(k.tooltip()));
                btn.setFocusTraversable(false);  // keep focus in target field
                btn.setOnAction(e -> insert(k.latex()));
                hrow.getChildren().add(btn);
            }
            grid.getChildren().add(hrow);
        }

        ScrollPane scroll = new ScrollPane(grid);
        scroll.setFitToWidth(true);
        scroll.setHbarPolicy(ScrollPane.ScrollBarPolicy.NEVER);
        scroll.setVbarPolicy(ScrollPane.ScrollBarPolicy.NEVER);
        scroll.getStyleClass().add("kb-tab-scroll");

        Tab tab = new Tab(name, scroll);
        tab.setClosable(false);
        return tab;
    }

    // ── Insertion logic ───────────────────────────────────────────────────────

    /**
     * Insert latex at the current caret position.
     * If latex contains "│", the cursor is placed at that position;
     * otherwise the cursor is placed after the inserted text.
     */
    private void insert(String latex) {
        int caretPos = target.getCaretPosition();
        String text  = target.getText();

        if (text == null) text = "";

        boolean hasCursor = latex.contains("│");
        String clean      = latex.replace("│", "");

        String newText = text.substring(0, caretPos) + clean + text.substring(caretPos);
        target.setText(newText);

        if (hasCursor) {
            int cursorOffset = latex.indexOf("│");
            target.positionCaret(caretPos + cursorOffset);
        } else {
            target.positionCaret(caretPos + clean.length());
        }

        target.requestFocus();
    }

    private void doBackspace() {
        int caret = target.getCaretPosition();
        if (caret == 0) return;
        target.getText();
        // If selection, delete it; otherwise delete char before caret
        if (target.getSelection().getLength() > 0) {
            target.deleteText(target.getSelection());
        } else {
            target.deleteText(caret - 1, caret);
        }
        target.requestFocus();
    }
}