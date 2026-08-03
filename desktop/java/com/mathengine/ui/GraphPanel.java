package com.mathengine.ui;

import com.mathengine.jni.MathBridge;
import com.mathengine.model.PrecisionMode;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.canvas.Canvas;
import javafx.scene.canvas.GraphicsContext;
import javafx.scene.input.MouseEvent;
import javafx.scene.input.ScrollEvent;
import javafx.scene.paint.Color;
import javafx.scene.text.Font;

import java.util.ArrayList;
import java.util.List;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.control.Separator;
import javafx.scene.control.TextField;
import javafx.scene.control.ToggleButton;
import javafx.scene.control.Tooltip;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.StackPane;
import javafx.scene.layout.VBox;

/**
 * GraphPanel
 * ───────────
 * Embedded 2D/3D function plotter.
 *
 * 2D mode: plots y=f(x) expressions using the calculus expression evaluator.
 * 3D mode: plots z=f(x,y) as a wireframe/heatmap projection.
 *
 * Controls:
 *   - Scroll to zoom
 *   - Drag to pan
 *   - Add multiple functions (each a different colour)
 *   - Toggle 2D/3D
 *   - Range inputs (xMin, xMax, yMin, yMax)
 */
public class GraphPanel extends VBox {

    // ── Graph state ───────────────────────────────────────────────────────────

    private double xMin = -10, xMax = 10;
    private double yMin = -6,  yMax = 6;
    private double zMin = -10, zMax = 10; // for 3D

    private boolean is3D = false;

    // Drag state
    private double dragStartX, dragStartY;
    private double panXmin, panXmax, panYmin, panYmax;

    // Plotted functions
    private final List<PlotEntry> entries = new ArrayList<>();

    private static final Color[] PALETTE = {
        Color.web("#e8962e"), Color.web("#4dabf7"), Color.web("#69db7c"),
        Color.web("#f783ac"), Color.web("#cc5de8"), Color.web("#ff6b6b"),
        Color.web("#ffd43b"), Color.web("#38d9a9")
    };

    // ── UI components ─────────────────────────────────────────────────────────

    private final Canvas        canvas      = new Canvas(600, 400);
    private final ThemeManager  tm          = ThemeManager.getInstance();
    private final TextField     exprField   = new TextField();
    private final TextField     xMinField   = new TextField("-10");
    private final TextField     xMaxField   = new TextField("10");
    private final TextField     yMinField   = new TextField("-6");
    private final TextField     yMaxField   = new TextField("6");
    private final VBox          legendBox   = new VBox(4);
    private final Label         statusLabel = new Label("Ready");
    private final ToggleButton  btn3D       = new ToggleButton("3D");
    private final MathBridge    bridge      = MathBridge.getInstance();

    // ── Constructor ───────────────────────────────────────────────────────────

    public GraphPanel() {
        setSpacing(0);
        getStyleClass().add("graph-panel");
        getChildren().addAll(buildToolbar(), buildCanvasArea(), buildStatusBar());
        tm.addThemeListener(theme -> redraw());
        setupInteraction();
        drawAxes();
    }

    // ── Build UI ──────────────────────────────────────────────────────────────

    private HBox buildToolbar() {
        // Expression input
        exprField.setPromptText("y = f(x)  e.g.  sin(x)  or  x^2 - 3");
        exprField.setPrefWidth(220);
        exprField.getStyleClass().add("graph-expr-input");
        exprField.setOnAction(e -> addFunction());

        Button addBtn = new Button("Plot");
        addBtn.getStyleClass().add("btn-primary");
        addBtn.setOnAction(e -> addFunction());

        Button clearBtn = new Button("Clear");
        clearBtn.getStyleClass().add("btn-secondary");
        clearBtn.setOnAction(e -> clearAll());

        // Range inputs
        Label xLbl = new Label("x:");  xLbl.getStyleClass().add("graph-range-label");
        Label toLbl = new Label("to"); toLbl.getStyleClass().add("graph-range-label");
        Label yLbl = new Label("y:");  yLbl.getStyleClass().add("graph-range-label");
        Label toLbl2= new Label("to"); toLbl2.getStyleClass().add("graph-range-label");

        for (TextField f : new TextField[]{xMinField, xMaxField, yMinField, yMaxField}) {
            f.setPrefWidth(52);
            f.getStyleClass().add("graph-range-input");
            f.setOnAction(e -> applyRange());
        }

        Button resetBtn = new Button("⟳");
        resetBtn.setTooltip(new Tooltip("Reset view"));
        resetBtn.getStyleClass().add("icon-btn");
        resetBtn.setOnAction(e -> resetView());

        btn3D.getStyleClass().add("icon-btn");
        btn3D.setTooltip(new Tooltip("Toggle 3D wireframe mode"));
        btn3D.setOnAction(e -> { is3D = btn3D.isSelected(); redraw(); });

        HBox toolbar = new HBox(8,
            exprField, addBtn, clearBtn,
            new Separator(), xLbl, xMinField, toLbl, xMaxField,
            new Separator(), yLbl, yMinField, toLbl2, yMaxField,
            resetBtn, btn3D);
        toolbar.setAlignment(Pos.CENTER_LEFT);
        toolbar.setPadding(new Insets(8, 12, 8, 12));
        toolbar.getStyleClass().add("graph-toolbar");
        return toolbar;
    }

    private HBox buildCanvasArea() {
        canvas.setWidth(600);
        canvas.setHeight(400);
        canvas.getStyleClass().add("graph-canvas");

        // Canvas resizes with parent
        StackPane canvasPane = new StackPane(canvas);
        canvasPane.getStyleClass().add("graph-canvas-pane");
        HBox.setHgrow(canvasPane, Priority.ALWAYS);

        canvasPane.widthProperty().addListener((obs, o, n) -> {
            canvas.setWidth(n.doubleValue());
            redraw();
        });
        canvasPane.heightProperty().addListener((obs, o, n) -> {
            canvas.setHeight(n.doubleValue());
            redraw();
        });

        // Legend sidebar
        legendBox.setPrefWidth(150);
        legendBox.setPadding(new Insets(8));
        legendBox.getStyleClass().add("graph-legend");

        Label legendTitle = new Label("FUNCTIONS");
        legendTitle.getStyleClass().add("graph-legend-title");
        legendBox.getChildren().add(legendTitle);

        HBox area = new HBox(0, canvasPane, legendBox);
        HBox.setHgrow(canvasPane, Priority.ALWAYS);
        VBox.setVgrow(area, Priority.ALWAYS);
        return area;
    }

    private HBox buildStatusBar() {
        statusLabel.getStyleClass().add("graph-status");
        HBox bar = new HBox(statusLabel);
        bar.setPadding(new Insets(4, 12, 4, 12));
        bar.getStyleClass().add("graph-status-bar");
        return bar;
    }

    // ── Canvas interaction ────────────────────────────────────────────────────

    private void setupInteraction() {
        // Zoom on scroll
        canvas.addEventHandler(ScrollEvent.SCROLL, e -> {
            double factor = e.getDeltaY() > 0 ? 0.85 : 1.0/0.85;
            double cx = toMathX(e.getX()), cy = toMathY(e.getY());
            xMin = cx + (xMin - cx) * factor;
            xMax = cx + (xMax - cx) * factor;
            yMin = cy + (yMin - cy) * factor;
            yMax = cy + (yMax - cy) * factor;
            syncRangeFields();
            redraw();
        });

        // Pan on drag
        canvas.addEventHandler(MouseEvent.MOUSE_PRESSED, e -> {
            dragStartX = e.getX(); dragStartY = e.getY();
            panXmin = xMin; panXmax = xMax; panYmin = yMin; panYmax = yMax;
        });
        canvas.addEventHandler(MouseEvent.MOUSE_DRAGGED, e -> {
            double dx = (e.getX() - dragStartX) / canvas.getWidth()  * (xMax - xMin);
            double dy = (e.getY() - dragStartY) / canvas.getHeight() * (yMax - yMin);
            xMin = panXmin - dx; xMax = panXmax - dx;
            yMin = panYmin + dy; yMax = panYmax + dy;
            syncRangeFields();
            redraw();
        });

        // Crosshair coordinates on hover
        canvas.addEventHandler(MouseEvent.MOUSE_MOVED, e -> {
            double mx = toMathX(e.getX()), my = toMathY(e.getY());
            statusLabel.setText(String.format("x = %.4f,  y = %.4f", mx, my));
        });
    }

    // ── Coordinate transforms ─────────────────────────────────────────────────

    private double toScreenX(double mathX) {
        return (mathX - xMin) / (xMax - xMin) * canvas.getWidth();
    }
    private double toScreenY(double mathY) {
        return (1.0 - (mathY - yMin) / (yMax - yMin)) * canvas.getHeight();
    }
    private double toMathX(double sx) {
        return sx / canvas.getWidth()  * (xMax - xMin) + xMin;
    }
    private double toMathY(double sy) {
        return (1.0 - sy / canvas.getHeight()) * (yMax - yMin) + yMin;
    }

    // ── Drawing helpers ───────────────────────────────────────────────────────

    private boolean isDark() {
        ThemeManager.Theme resolved = tm.resolveTheme(tm.getCurrentTheme());
        return resolved == ThemeManager.Theme.DARK || resolved == ThemeManager.Theme.HIGH_CONTRAST;
    }

    private Color axisColor() {
        return isDark() ? Color.web("#E5E1DC") : Color.web("#1A1714");
    }

    private Color gridColor() {
        return isDark() ? Color.web("#3B3732") : Color.web("#D0CCC7");
    }

    private Color labelColor() {
        return isDark() ? Color.web("#CFC8BF") : Color.web("#7a7268");
    }

    private void drawAxes() {
        GraphicsContext gc = canvas.getGraphicsContext2D();
        double w = canvas.getWidth(), h = canvas.getHeight();

        // Background from theme
        Color bg = isDark() ? Color.web("#1e1b17") : Color.web("#f7f6f2");
        gc.setFill(bg);
        gc.fillRect(0, 0, w, h);

        // Grid
        gc.setStroke(gridColor());
        gc.setLineWidth(1);
        drawGrid(gc, w, h);

        // Axes
        double ox = toScreenX(0), oy = toScreenY(0);
        gc.setStroke(axisColor());
        gc.setLineWidth(1.5);
        if (oy >= 0 && oy <= h) { gc.strokeLine(0, oy, w, oy); } // x-axis
        if (ox >= 0 && ox <= w) { gc.strokeLine(ox, 0, ox, h); } // y-axis

        // Axis labels
        gc.setFill(labelColor());
        gc.setFont(Font.font("JetBrains Mono", 10));
        drawAxisLabels(gc, w, h);
    }

    private void drawGrid(GraphicsContext gc, double w, double h) {
        double range = Math.max(xMax - xMin, yMax - yMin);
        double step  = Math.pow(10, Math.floor(Math.log10(range / 5)));
        if (range / step > 12) step *= 2;
        if (range / step < 4)  step /= 2;

        double xStart = Math.floor(xMin / step) * step;
        for (double x = xStart; x <= xMax; x += step) {
            double sx = toScreenX(x);
            gc.strokeLine(sx, 0, sx, h);
        }
        double yStart = Math.floor(yMin / step) * step;
        for (double y = yStart; y <= yMax; y += step) {
            double sy = toScreenY(y);
            gc.strokeLine(0, sy, w, sy);
        }
    }

    private void drawAxisLabels(GraphicsContext gc, double w, double h) {
        double range = xMax - xMin;
        double step  = Math.pow(10, Math.floor(Math.log10(range / 5)));
        if (range / step > 12) step *= 2;
        if (range / step < 4)  step /= 2;

        double oy = Math.max(4, Math.min(h - 14, toScreenY(0)));
        double ox = Math.max(4, Math.min(w - 30, toScreenX(0)));

        double xStart = Math.floor(xMin / step) * step;
        for (double x = xStart; x <= xMax; x += step) {
            if (Math.abs(x) < step * 0.1) continue;
            double sx = toScreenX(x);
            String lbl = formatAxisNum(x);
            gc.fillText(lbl, sx - lbl.length() * 3, oy + 12);
        }
        double yStart = Math.floor(yMin / step) * step;
        for (double y = yStart; y <= yMax; y += step) {
            if (Math.abs(y) < step * 0.1) continue;
            double sy = toScreenY(y);
            gc.fillText(formatAxisNum(y), ox + 4, sy + 4);
        }
    }

    private String formatAxisNum(double v) {
        if (v == Math.floor(v) && Math.abs(v) < 1e6)
            return String.valueOf((long) v);
        return String.format("%.2g", v);
    }

    private void redraw() {
        drawAxes();
        if (is3D) {
            for (PlotEntry e : entries) drawFunction3D(e);
        } else {
            for (PlotEntry e : entries) drawFunction2D(e);
        }
    }

    private void drawFunction2D(PlotEntry entry) {
        GraphicsContext gc = canvas.getGraphicsContext2D();
        double w = canvas.getWidth();
        int steps = (int) w * 2;

        gc.setStroke(entry.color);
        gc.setLineWidth(2.0);
        gc.setLineDashes();
        gc.beginPath();

        boolean penDown = false;
        double prevY = Double.NaN;

        for (int i = 0; i <= steps; i++) {
            double x = xMin + (double) i / steps * (xMax - xMin);
            double y = evalFunction(entry.expression, x);

            if (!Double.isFinite(y) || Math.abs(y - prevY) > (yMax - yMin) * 5) {
                penDown = false;
                prevY = y;
                continue;
            }
            double sx = toScreenX(x), sy = toScreenY(y);
            if (!penDown) {
                gc.moveTo(sx, sy);
                penDown = true;
            } else {
                gc.lineTo(sx, sy);
            }
            prevY = y;
        }
        gc.stroke();
    }

    private void drawFunction3D(PlotEntry entry) {
        // Wireframe projection: isometric-style parallel projection of z=f(x,y)
        GraphicsContext gc = canvas.getGraphicsContext2D();
        double w = canvas.getWidth(), h = canvas.getHeight();
        int gridN = 30;

        double cx = (xMin + xMax) / 2, cy = (yMin + yMax) / 2;
        double scaleX = w * 0.35 / (xMax - xMin);
        double scaleY = h * 0.35 / (yMax - yMin);

        // Project 3D (x,y,z) → screen (sx,sy) using oblique projection
        double angle = Math.toRadians(30);
        java.util.function.BiFunction<Double[], Double[], double[]> project = (pt, range) -> {
            double px = pt[0], py = pt[1], pz = pt[2];
            double sx = w/2 + (px - cx) * scaleX - (py - cy) * scaleY * Math.cos(angle) * 0.5;
            double sy = h/2 - pz * h * 0.3 / (zMax - zMin) + (py - cy) * scaleY * Math.sin(angle) * 0.5;
            return new double[]{sx, sy};
        };

        gc.setLineWidth(0.7);

        // Draw x-direction lines
        for (int j = 0; j <= gridN; j++) {
            double y = yMin + (double)j/gridN * (yMax - yMin);
            boolean first = true;
            for (int i = 0; i <= gridN; i++) {
                double x = xMin + (double)i/gridN * (xMax - xMin);
                double z = evalFunction2D(entry.expression, x, y);
                if (!Double.isFinite(z)) { first = true; continue; }
                double t = Math.max(0, Math.min(1, (z - zMin) / (zMax - zMin)));
                gc.setStroke(interpolateColor(entry.color, t));
                double[] s = project.apply(new Double[]{x,y,z}, null);
                if (first) { gc.beginPath(); gc.moveTo(s[0],s[1]); first=false; }
                else { gc.lineTo(s[0],s[1]); gc.stroke(); gc.beginPath(); gc.moveTo(s[0],s[1]); }
            }
        }
        // Draw y-direction lines
        for (int i = 0; i <= gridN; i++) {
            double x = xMin + (double)i/gridN * (xMax - xMin);
            boolean first = true;
            for (int j = 0; j <= gridN; j++) {
                double y = yMin + (double)j/gridN * (yMax - yMin);
                double z = evalFunction2D(entry.expression, x, y);
                if (!Double.isFinite(z)) { first = true; continue; }
                double t = Math.max(0, Math.min(1, (z - zMin) / (zMax - zMin)));
                gc.setStroke(interpolateColor(entry.color, t));
                double[] s = project.apply(new Double[]{x,y,z}, null);
                if (first) { gc.beginPath(); gc.moveTo(s[0],s[1]); first=false; }
                else { gc.lineTo(s[0],s[1]); gc.stroke(); gc.beginPath(); gc.moveTo(s[0],s[1]); }
            }
        }
    }

    private Color interpolateColor(Color base, double t) {
        // Blue (cold) → base → red (hot)
        Color cold = Color.web("#4dabf7"), hot = Color.web("#ff6b6b");
        if (t < 0.5) return cold.interpolate(base, t * 2);
        return base.interpolate(hot, (t - 0.5) * 2);
    }

    // ── Function evaluation ───────────────────────────────────────────────────

    /**
     * Evaluate a 1D function at x.
     * Supports inputs like "y = f(x)" or just "f(x)".
     * If the engine returns "symbolic  ~  numeric", we parse the numeric part.
     */
    /**
     * Evaluate f(x) at a given x by sending a numerical_int request over a
     * tiny interval, then dividing by the width — effectively a point eval.
     *
     * The old approach used String.replace("x", "(val)") which corrupted
     * function names like "exp", "max", "cbrt" that contain the letter x.
     * The new approach uses word-boundary regex so only standalone variable
     * occurrences are replaced.
     */
    private double evalFunction(String expr, double x) {
        try {
            String core = stripAssignment(expr);
            // Replace standalone 'x' only — negative lookbehind/lookahead
            // prevents replacing 'x' inside identifiers like exp, max, sqrt.
            String xVal = "(" + x + ")";
            String substituted = core.replaceAll(
                "(?<![a-zA-Z0-9_])x(?![a-zA-Z0-9_])", xVal);
            String result = bridge.compute(substituted, PrecisionMode.NUMERICAL);
            return parseNumericResult(result);
        } catch (Exception e) {
            return Double.NaN;
        }
    }

    /**
     * Evaluate a 2D function z = f(x,y).
     * Supports inputs like "z = f(x,y)" or just "f(x,y)".
     */
    private double evalFunction2D(String expr, double x, double y) {
        try {
            String core = stripAssignment(expr);
            // Replace standalone 'y' first, then 'x' — both with word-boundary regex
            // to avoid corrupting function names (exp, xyz, max, etc.)
            String substituted = core
                .replaceAll("(?<![a-zA-Z0-9_])y(?![a-zA-Z0-9_])", "(" + y + ")")
                .replaceAll("(?<![a-zA-Z0-9_])x(?![a-zA-Z0-9_])", "(" + x + ")");
            String result = bridge.compute(substituted, PrecisionMode.NUMERICAL);
            return parseNumericResult(result);
        } catch (Exception e) {
            return Double.NaN;
        }
    }

    /** Strip leading "y =" / "z =" / "f(x) =" style assignments. */
    private String stripAssignment(String expr) {
        String s = expr.trim();
        int eq = s.indexOf('=');
        if (eq >= 0 && eq < s.length() - 1) {
            return s.substring(eq + 1).trim();
        }
        return s;
    }

    /**
     * Handle engine outputs like:
     *   "3.14159"
     *   "pi  ~  3.14159"
     *   "512/3  ~  170.6666666667"
     * We always try to use the numeric part.
     */
    private double parseNumericResult(String result) {
        if (result == null) return Double.NaN;
        String s = result.trim();
        if (s.contains("~")) {
            String[] parts = s.split("~");
            s = parts[parts.length - 1].trim();
        }
        // If there's still extra text, take the last token
        String[] tokens = s.split("\\s+");
        s = tokens[tokens.length - 1];
        return Double.parseDouble(s);
    }

    // ── Actions ───────────────────────────────────────────────────────────────

    private void addFunction() {
        String expr = exprField.getText().trim();
        if (expr.isEmpty()) return;

        Color color = PALETTE[entries.size() % PALETTE.length];
        PlotEntry entry = new PlotEntry(expr, color);
        entries.add(entry);

        // Update legend
        HBox row = new HBox(6);
        row.setAlignment(Pos.CENTER_LEFT);
        javafx.scene.shape.Rectangle swatch = new javafx.scene.shape.Rectangle(12, 12, color);
        swatch.setArcWidth(3); swatch.setArcHeight(3);
        Label lbl = new Label(expr.length() > 18 ? expr.substring(0, 15) + "…" : expr);
        lbl.getStyleClass().add("graph-legend-entry");
        lbl.setTooltip(new Tooltip(expr));
        Button del = new Button("×");
        del.getStyleClass().add("graph-legend-delete");
        del.setOnAction(e -> {
            entries.remove(entry);
            legendBox.getChildren().remove(row);
            redraw();
        });
        row.getChildren().addAll(swatch, lbl, del);
        legendBox.getChildren().add(row);

        exprField.clear();
        redraw();
    }

    private void clearAll() {
        entries.clear();
        // Remove all legend rows except the title
        while (legendBox.getChildren().size() > 1)
            legendBox.getChildren().remove(legendBox.getChildren().size() - 1);
        redraw();
    }

    private void applyRange() {
        try {
            xMin = Double.parseDouble(xMinField.getText().trim());
            xMax = Double.parseDouble(xMaxField.getText().trim());
            yMin = Double.parseDouble(yMinField.getText().trim());
            yMax = Double.parseDouble(yMaxField.getText().trim());
            if (xMin >= xMax || yMin >= yMax) throw new NumberFormatException();
            redraw();
        } catch (NumberFormatException e) {
            statusLabel.setText("Invalid range — enter valid numbers");
        }
    }

    private void resetView() {
        xMin = -10; xMax = 10; yMin = -6; yMax = 6;
        syncRangeFields();
        redraw();
    }

    private void syncRangeFields() {
        xMinField.setText(formatAxisNum(xMin));
        xMaxField.setText(formatAxisNum(xMax));
        yMinField.setText(formatAxisNum(yMin));
        yMaxField.setText(formatAxisNum(yMax));
    }

    // ── Plot entry record ─────────────────────────────────────────────────────

    private static class PlotEntry {
        final String expression;
        final Color  color;
        PlotEntry(String e, Color c) { expression = e; color = c; }
    }

    // ── Public API ────────────────────────────────────────────────────────────

    /** Plot a function from outside (e.g. clicking a result) */
    public void plotExpression(String expr) {
        exprField.setText(expr);
        addFunction();
    }

    /** Plot ODE solution points returned from DifferentialEquations module */
    public void plotPoints(String pointsJson, String label, Color color) {
        // pointsJson format: [[t0,y0],[t1,y1],...]
        try {
            List<double[]> pts = parsePointsJson(pointsJson);
            if (pts.isEmpty()) return;

            // Adjust range to fit
            double tmin = pts.stream().mapToDouble(p->p[0]).min().orElse(-10);
            double tmax = pts.stream().mapToDouble(p->p[0]).max().orElse(10);
            double ymin = pts.stream().mapToDouble(p->p[1]).min().orElse(-6);
            double ymax = pts.stream().mapToDouble(p->p[1]).max().orElse(6);
            double pad = (ymax - ymin) * 0.1 + 0.5;
            xMin=tmin; xMax=tmax; yMin=ymin-pad; yMax=ymax+pad;
            syncRangeFields();

            drawAxes();
            GraphicsContext gc = canvas.getGraphicsContext2D();
            gc.setStroke(color != null ? color : PALETTE[entries.size() % PALETTE.length]);
            gc.setLineWidth(2.0);
            gc.beginPath();
            for (int i = 0; i < pts.size(); i++) {
                double sx = toScreenX(pts.get(i)[0]);
                double sy = toScreenY(pts.get(i)[1]);
                if (i == 0) gc.moveTo(sx, sy); else gc.lineTo(sx, sy);
            }
            gc.stroke();
            statusLabel.setText("ODE: " + label + " (" + pts.size() + " points)");
        } catch (Exception e) {
            statusLabel.setText("Could not plot: " + e.getMessage());
        }
    }

    private List<double[]> parsePointsJson(String json) {
        List<double[]> pts = new ArrayList<>();
        int i = json.indexOf("[[");
        if (i == -1) i = json.indexOf('[');
        if (i == -1) return pts;
        int depth = 0;
        StringBuilder cur = new StringBuilder();
        for (int j = i; j < json.length(); j++) {
            char c = json.charAt(j);
            if (c == '[') {
                depth++;
                if (depth == 2) cur = new StringBuilder();
            } else if (c == ']') {
                if (depth == 2) {
                    String[] parts = cur.toString().split(",");
                    if (parts.length == 2) {
                        try { pts.add(new double[]{Double.parseDouble(parts[0].trim()),
                                                   Double.parseDouble(parts[1].trim())}); }
                        catch (Exception ignored) {}
                    }
                }
                depth--;
                if (depth == 0) break;
            } else if (depth == 2) {
                cur.append(c);
            }
        }
        return pts;
    }
}