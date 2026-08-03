package com.mathengine.ui;

import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.layout.*;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Consumer;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.control.ScrollPane;
import javafx.scene.control.Tooltip;
import javafx.scene.input.KeyCode;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.VBox;

/**
 * OperationPanel
 * ───────────────
 * Left sidebar with two sections:
 *
 *  1. OPERATION — 18 operations grouped into 5 collapsible categories.
 *     All operations are reachable via a ScrollPane; groups collapse/expand
 *     with an animated chevron.  Keyboard: arrow keys to navigate, Enter to select.
 *
 *  2. HISTORY — scrollable list of the last 20 computations.
 */
public class OperationPanel extends VBox {

    // ── Operation enum ────────────────────────────────────────────────────────

    public enum Operation {
        // Calculus
        ARITHMETIC ("Arithmetic", "±",    "arithmetic-icon", "2+3*sin(pi/4)"),
        DERIVATIVE ("Derivative", "d/dx", "derivative-icon", "x^3+sin(x)"),
        INTEGRAL   ("Integral",   "∫",    "integral-icon",   "x^2, 0, 1"),
        LIMIT      ("Limit",      "lim",  "limit-icon",      "sin(x)/x as x->0"),
        SERIES     ("Series",     "Σ",    "series-icon",     "taylor[sin(x),0,6]"),
        VECTOR     ("Vector",     "∇",    "vector-icon",     "gradient[x^2+y^2, x, y]"),
        OPTIMIZE   ("Optimize",   "min",  "optimize-icon",   "x^3-3*x, -3, 3"),
        // Applied
        DIFFEQ     ("Diff Eq",    "dy",   "diffeq-icon",     "rk4[t*y,0,2,1,100]"),
        APPLIED    ("Applied",    "Φ",    "applied-icon",    "sir[0.3,0.1,990,10,0,100,200]"),
        // Data
        STATISTICS ("Statistics", "Σx",   "statistics-icon", "summarize[1,2,3,4,5]"),
        PROBABILITY("Probability","P",    "prob-icon",       "mgf_normal[0,1,0.5]"),
        // Algebra & Number Theory
        NUMBER_THY ("Number Thy", "ℕ",    "nt-icon",         "prime_factors[360]"),
        DISCRETE   ("Discrete",   "∅",    "dm-icon",         "combinations[10,3]"),
        ALGEBRA    ("Algebra",    "𝔾",    "aa-icon",         "cyclic[6]"),
        COMPLEX    ("Complex",    "ℂ",    "ca-icon",         "polar[3,4]"),
        // Geometry & Numerical
        GEOMETRY   ("Geometry",   "△",    "geo-icon",        "circle[0,0,5]"),
        NUMERICAL  ("Numerical",  "≈",    "na-icon",         "newton[x^3-x-2|x|1.5]"),
        // Matrix
        MATRIX     ("Matrix",     "[]",   "matrix-icon",     "[[1,2],[3,4]]");

        public final String label, symbol, iconStyle, placeholder;
        Operation(String l, String s, String i, String p) {
            label = l; symbol = s; iconStyle = i; placeholder = p;
        }
    }

    // ── Groups (ordered; each maps to its operations) ─────────────────────────

    private static final Map<String, Operation[]> GROUPS = new LinkedHashMap<>();
    static {
        GROUPS.put("Calculus",       new Operation[]{
            Operation.ARITHMETIC, Operation.DERIVATIVE, Operation.INTEGRAL,
            Operation.LIMIT,      Operation.SERIES,     Operation.VECTOR,
            Operation.OPTIMIZE });
        GROUPS.put("Differential Eq & Applied", new Operation[]{
            Operation.DIFFEQ, Operation.APPLIED });
        GROUPS.put("Data & Probability", new Operation[]{
            Operation.STATISTICS, Operation.PROBABILITY });
        GROUPS.put("Algebra & Number Theory", new Operation[]{
            Operation.NUMBER_THY, Operation.DISCRETE,
            Operation.ALGEBRA,    Operation.COMPLEX });
        GROUPS.put("Geometry & Numerical", new Operation[]{
            Operation.GEOMETRY, Operation.NUMERICAL, Operation.MATRIX });
    }

    // ── History entry ─────────────────────────────────────────────────────────

    public record HistoryEntry(String expression, String result, Operation op, String mode) {}

    // ── State ─────────────────────────────────────────────────────────────────

    private Operation               currentOp = Operation.ARITHMETIC;
    private final List<HistoryEntry> history   = new ArrayList<>();
    private final List<Button>       allOpBtns = new ArrayList<>();  // flat list for arrow nav

    private Consumer<Operation>    onOperationChanged;
    private Consumer<HistoryEntry> onHistoryItemLoaded;

    private final VBox historyList = new VBox(4);

    // ── Constructor ───────────────────────────────────────────────────────────

    public OperationPanel() {
        setSpacing(12);
        setPadding(new Insets(16));
        setPrefWidth(224);
        getStyleClass().add("sidebar");
        getChildren().addAll(buildOperationSection(), buildHistorySection());
        VBox.setVgrow(this, Priority.ALWAYS);
    }

    // ── Build sections ────────────────────────────────────────────────────────

    private VBox buildOperationSection() {
        Label title = new Label("OPERATION");
        title.getStyleClass().add("sidebar-section-title");

        // Inner VBox holding all group panes
        VBox groupsBox = new VBox(2);
        groupsBox.getStyleClass().add("op-groups");

        GROUPS.forEach((groupName, ops) ->
            groupsBox.getChildren().add(buildGroup(groupName, ops)));

        // ScrollPane wraps all groups so nothing is hidden
        ScrollPane scroll = new ScrollPane(groupsBox);
        scroll.setFitToWidth(true);
        scroll.setHbarPolicy(ScrollPane.ScrollBarPolicy.NEVER);
        scroll.setVbarPolicy(ScrollPane.ScrollBarPolicy.AS_NEEDED);
        scroll.getStyleClass().add("op-scroll");
        scroll.setFocusTraversable(false);
        VBox.setVgrow(scroll, Priority.ALWAYS);

        // Wire global arrow-key nav across all buttons
        for (int i = 0; i < allOpBtns.size(); i++) {
            final int idx = i;
            allOpBtns.get(i).setOnKeyPressed(e -> {
                if (e.getCode() == KeyCode.UP   && idx > 0)
                    allOpBtns.get(idx - 1).requestFocus();
                if (e.getCode() == KeyCode.DOWN  && idx < allOpBtns.size() - 1)
                    allOpBtns.get(idx + 1).requestFocus();
            });
        }

        setActive(currentOp);

        VBox section = new VBox(8, title, scroll);
        VBox.setVgrow(section, Priority.ALWAYS);
        section.getStyleClass().add("sidebar-section");
        return section;
    }

    /** Build one collapsible group header + its operation buttons. */
    private VBox buildGroup(String name, Operation[] ops) {
        // ── Group header ──────────────────────────────────────────────────────
        Label chevron = new Label("▾");
        chevron.getStyleClass().add("group-chevron");

        Label groupLabel = new Label(name);
        groupLabel.getStyleClass().add("group-header-label");

        HBox header = new HBox(6, chevron, groupLabel);
        header.setAlignment(Pos.CENTER_LEFT);
        header.setPadding(new Insets(6, 8, 6, 8));
        header.getStyleClass().add("group-header");
        header.setFocusTraversable(true);
        header.setAccessibleText(name + " group — click to collapse/expand");

        // ── Operation buttons ─────────────────────────────────────────────────
        VBox opBox = new VBox(2);
        opBox.getStyleClass().add("op-list");
        opBox.setPadding(new Insets(2, 0, 4, 0));

        for (Operation op : ops) {
            Button btn = buildOpButton(op);
            allOpBtns.add(btn);
            opBox.getChildren().add(btn);
        }

        // ── Collapse toggle ───────────────────────────────────────────────────
        final boolean[] expanded = {true};
        Runnable toggle = () -> {
            expanded[0] = !expanded[0];
            opBox.setVisible(expanded[0]);
            opBox.setManaged(expanded[0]);
            chevron.setText(expanded[0] ? "▾" : "▸");
        };
        header.setOnMouseClicked(e -> toggle.run());
        header.setOnKeyPressed(e -> {
            if (e.getCode() == KeyCode.ENTER || e.getCode() == KeyCode.SPACE) toggle.run();
        });

        // Auto-expand the group containing the active operation
        boolean containsActive = false;
        for (Operation op : ops) if (op == currentOp) { containsActive = true; break; }
        if (!containsActive) {
            expanded[0] = false;
            opBox.setVisible(false);
            opBox.setManaged(false);
            chevron.setText("▸");
        }

        VBox group = new VBox(0, header, opBox);
        group.getStyleClass().add("op-group");
        return group;
    }

    private Button buildOpButton(Operation op) {
        Label icon = new Label(op.symbol);
        icon.getStyleClass().addAll("op-icon", op.iconStyle);
        icon.setMinWidth(26);
        icon.setAlignment(Pos.CENTER);

        Label text = new Label(op.label);
        text.getStyleClass().add("op-label");

        HBox content = new HBox(10, icon, text);
        content.setAlignment(Pos.CENTER_LEFT);

        Button btn = new Button();
        btn.setGraphic(content);
        btn.setMaxWidth(Double.MAX_VALUE);
        btn.getStyleClass().add("op-btn");
        btn.setFocusTraversable(true);
        btn.setAccessibleText(op.label + " operation");
        btn.setTooltip(new Tooltip(op.label + "\n" + op.placeholder));
        btn.setOnAction(e -> selectOperation(op));
        return btn;
    }

    private VBox buildHistorySection() {
        Label title = new Label("HISTORY");
        title.getStyleClass().add("sidebar-section-title");

        Button clearBtn = new Button("Clear");
        clearBtn.getStyleClass().add("history-clear-btn");
        clearBtn.setAccessibleText("Clear calculation history");
        clearBtn.setOnAction(e -> clearHistory());

        HBox titleRow = new HBox(title, new Spacer(), clearBtn);
        titleRow.setAlignment(Pos.CENTER_LEFT);

        ScrollPane scroll = new ScrollPane(historyList);
        scroll.setFitToWidth(true);
        scroll.setMaxHeight(200);
        scroll.getStyleClass().add("history-scroll");
        scroll.setFocusTraversable(false);

        historyList.setPadding(new Insets(4, 0, 4, 0));
        renderHistoryEmpty();

        VBox section = new VBox(8, titleRow, scroll);
        section.getStyleClass().add("sidebar-section");
        return section;
    }

    // ── Operation selection ───────────────────────────────────────────────────

    /** Also callable from MainLayout when a QuickFunction forces a mode switch. */
    void selectOperation(Operation op) {
        currentOp = op;
        setActive(op);
        if (onOperationChanged != null) onOperationChanged.accept(op);
    }

    private void setActive(Operation op) {
        for (int i = 0; i < allOpBtns.size(); i++) {
            Button btn = allOpBtns.get(i);
            // Find which operation this button corresponds to
            Operation btnOp = opForButton(i);
            boolean active = (btnOp == op);
            btn.getStyleClass().removeAll("op-btn-active");
            if (active) {
                btn.getStyleClass().add("op-btn-active");
                // Ensure the group containing this button is expanded
                ensureGroupExpanded(op);
            }
            btn.setAccessibleRoleDescription(active ? "selected" : "");
        }
    }

    /** Walk groups in order to find which Operation corresponds to button index i. */
    private Operation opForButton(int idx) {
        int count = 0;
        for (Operation[] ops : GROUPS.values()) {
            for (Operation op : ops) {
                if (count == idx) return op;
                count++;
            }
        }
        return Operation.ARITHMETIC;
    }

    /** Expand the group containing the given operation (used when history re-selects). */
    private void ensureGroupExpanded(Operation target) {
        // Locate the group VBox in the scroll's content
        ScrollPane scroll = findOpScroll();
        if (scroll == null) return;
        VBox groupsBox = (VBox) scroll.getContent();
        int groupIdx = 0;
        for (Operation[] ops : GROUPS.values()) {
            for (Operation op : ops) {
                if (op == target) {
                    VBox group = (VBox) groupsBox.getChildren().get(groupIdx);
                    VBox opBox = (VBox) group.getChildren().get(1);
                    HBox header = (HBox) group.getChildren().get(0);
                    Label chevron = (Label) header.getChildren().get(0);
                    if (!opBox.isVisible()) {
                        opBox.setVisible(true);
                        opBox.setManaged(true);
                        chevron.setText("▾");
                    }
                    return;
                }
            }
            groupIdx++;
        }
    }

    private ScrollPane findOpScroll() {
        // The ScrollPane is the second child of the first section (VBox)
        if (getChildren().size() > 0 && getChildren().get(0) instanceof VBox section) {
            for (var child : section.getChildren()) {
                if (child instanceof ScrollPane sp) return sp;
            }
        }
        return null;
    }

    // ── History ───────────────────────────────────────────────────────────────

    public void addHistory(HistoryEntry entry) {
        history.add(0, entry);
        if (history.size() > 20) history.remove(history.size() - 1);
        renderHistory();
    }

    private void clearHistory() {
        history.clear();
        renderHistoryEmpty();
    }

    private void renderHistoryEmpty() {
        historyList.getChildren().clear();
        Label empty = new Label("No history yet");
        empty.getStyleClass().add("history-empty");
        historyList.getChildren().add(empty);
    }

    private void renderHistory() {
        historyList.getChildren().clear();
        if (history.isEmpty()) { renderHistoryEmpty(); return; }

        for (HistoryEntry entry : history) {
            VBox item = new VBox(2);
            item.getStyleClass().add("history-item");
            item.setFocusTraversable(true);

            String displayExpr = entry.expression().length() > 32
                ? entry.expression().substring(0, 29) + "…"
                : entry.expression();

            Label expr = new Label(displayExpr);
            expr.getStyleClass().add("history-expr");

            Label meta = new Label(entry.op().label + "  ·  " + entry.mode());
            meta.getStyleClass().add("history-meta");

            item.getChildren().addAll(expr, meta);
            item.setAccessibleText("Load: " + entry.expression());
            item.setOnMouseClicked(e -> loadHistoryItem(entry));
            item.setOnKeyPressed(e -> {
                if (e.getCode() == KeyCode.ENTER || e.getCode() == KeyCode.SPACE)
                    loadHistoryItem(entry);
            });
            historyList.getChildren().add(item);
        }
    }

    private void loadHistoryItem(HistoryEntry entry) {
        if (onHistoryItemLoaded != null) onHistoryItemLoaded.accept(entry);
        selectOperation(entry.op());
    }

    // ── Callbacks ─────────────────────────────────────────────────────────────

    public void setOnOperationChanged(Consumer<Operation> cb)     { onOperationChanged  = cb; }
    public void setOnHistoryItemLoaded(Consumer<HistoryEntry> cb) { onHistoryItemLoaded = cb; }
    public Operation getCurrentOperation()                        { return currentOp; }

    /** Feature 1 (Workspace Sync): lets ChatbotPanel snapshot what the
     * Compute tab was last showing, so a fresh chat message can reference
     * "this matrix"/"that result" without having typed it into the chat. */
    public java.util.Optional<HistoryEntry> getLastEntry() { return history.isEmpty() ? java.util.Optional.empty() : java.util.Optional.of(history.get(0)); }

    // ── Helpers ───────────────────────────────────────────────────────────────

    private static class Spacer extends Region {
        Spacer() { HBox.setHgrow(this, Priority.ALWAYS); }
    }
}
