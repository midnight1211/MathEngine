package com.mathengine.ui;

import javafx.application.Platform;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.control.*;
import javafx.scene.layout.*;
import java.io.*;
import java.net.HttpURLConnection;
import java.net.URI;
import java.nio.file.Files;
import java.util.*;
import java.util.function.Consumer;
import javafx.stage.FileChooser;
import javafx.stage.Window;

/**
 * DatasetImportPanel
 * ───────────────────
 * Collapsible panel for the Statistics operation that lets the user
 * import a dataset from:
 *   • Local file  (CSV, TSV, TXT)
 *   • URL         (Google Sheets export link, GitHub raw, any public CSV)
 *   • Paste       (raw comma or tab-separated text)
 *
 * On successful import it fires onDatasetLoaded(columns) where columns is a
 * Map<String, List<Double>> keyed by column name.
 * The caller (MainLayout / InputPanel) translates that into a summarize[] or
 * regression[] expression and loads it into the input field.
 *
 * ┌─────────────────────────────────────────────────────────┐
 * │  ▸  IMPORT DATASET                                      │
 * ├─────────────────────────────────────────────────────────┤
 * │  [📂 Local file]  [🔗 From URL]  [📋 Paste]            │
 * │  ─────────────────────────────────────────────────────  │
 * │  Status: "No dataset loaded"                            │
 * │  [Summarize all] [Column selector ▾]  [Clear]           │
 * └─────────────────────────────────────────────────────────┘
 */
public class DatasetImportPanel extends VBox {

    // ── Callbacks ─────────────────────────────────────────────────────────────

    /** Fired when a dataset is loaded; key = column name, value = numeric data. */
    private Consumer<Map<String, List<Double>>> onDatasetLoaded;
    /** Fired when user clicks a quick action (e.g. "Summarize column X"). */
    private Consumer<String> onExpressionRequested;

    // ── State ─────────────────────────────────────────────────────────────────

    private Map<String, List<Double>> currentDataset = null;
    private final Label  statusLabel   = new Label("No dataset loaded");
    private final VBox   body          = new VBox(10);
    private final Label  chevron       = new Label("▸");
    private final ComboBox<String> colSelector = new ComboBox<>();
    private Window ownerWindow;

    // ── Constructor ───────────────────────────────────────────────────────────

    public DatasetImportPanel() {
        getStyleClass().add("dataset-import-panel");
        setSpacing(0);

        HBox header = buildHeader();
        body.setPadding(new Insets(10, 12, 10, 12));
        body.setVisible(false);
        body.setManaged(false);

        body.getChildren().addAll(buildSourceRow(), buildStatusRow(), buildActionRow());
        getChildren().addAll(header, body);
    }

    // ── Public API ────────────────────────────────────────────────────────────

    public void setOnDatasetLoaded(Consumer<Map<String, List<Double>>> cb) {
        onDatasetLoaded = cb;
    }

    public void setOnExpressionRequested(Consumer<String> cb) {
        onExpressionRequested = cb;
    }

    public void setOwnerWindow(Window w) { ownerWindow = w; }

    public boolean hasDataset() { return currentDataset != null && !currentDataset.isEmpty(); }

    // ── Build ─────────────────────────────────────────────────────────────────

    private HBox buildHeader() {
        chevron.getStyleClass().add("quick-panel-chevron");
        Label title = new Label("IMPORT DATASET");
        title.getStyleClass().add("quick-panel-title");
        Region spacer = new Region(); HBox.setHgrow(spacer, Priority.ALWAYS);

        HBox hdr = new HBox(6, chevron, title, spacer);
        hdr.setAlignment(Pos.CENTER_LEFT);
        hdr.getStyleClass().add("quick-panel-header");
        hdr.setOnMouseClicked(e -> toggleExpanded());
        hdr.setFocusTraversable(true);
        hdr.setOnKeyPressed(e -> {
            if (e.getCode() == javafx.scene.input.KeyCode.ENTER) toggleExpanded();
        });
        return hdr;
    }

    private HBox buildSourceRow() {
        Button localBtn = iconButton("📂 Local file", this::importFromLocal);
        Button urlBtn   = iconButton("🔗 From URL",   this::importFromUrl);
        Button pasteBtn = iconButton("📋 Paste data", this::importFromPaste);
        localBtn.getStyleClass().add("dataset-source-btn");
        urlBtn.getStyleClass().add("dataset-source-btn");
        pasteBtn.getStyleClass().add("dataset-source-btn");

        HBox row = new HBox(8, localBtn, urlBtn, pasteBtn);
        row.setAlignment(Pos.CENTER_LEFT);
        return row;
    }

    private HBox buildStatusRow() {
        statusLabel.getStyleClass().add("dataset-status");
        statusLabel.setWrapText(true);
        return new HBox(statusLabel);
    }

    private HBox buildActionRow() {
        Button summarizeAll = new Button("Summarize all");
        summarizeAll.getStyleClass().add("quick-btn");
        summarizeAll.setOnAction(e -> {
            if (!hasDataset()) return;
            // Build a summarize[] call for the first column
            String first = currentDataset.keySet().iterator().next();
            fireExpr(buildSummarize(first));
        });

        colSelector.setPromptText("Choose column…");
        colSelector.getStyleClass().add("dataset-col-selector");
        colSelector.setPrefWidth(160);

        Button analyzeCol = new Button("Analyze column");
        analyzeCol.getStyleClass().add("quick-btn");
        analyzeCol.setOnAction(e -> {
            String col = colSelector.getValue();
            if (col != null && hasDataset()) fireExpr(buildSummarize(col));
        });

        Button regBtn = new Button("Regression (2 cols)");
        regBtn.getStyleClass().add("quick-btn");
        regBtn.setOnAction(e -> fireRegression());

        Button clearBtn = new Button("Clear");
        clearBtn.getStyleClass().add("header-logout-btn");
        clearBtn.setOnAction(e -> clearDataset());

        HBox row = new HBox(8, summarizeAll, colSelector, analyzeCol, regBtn, new Spacer(), clearBtn);
        row.setAlignment(Pos.CENTER_LEFT);
        return row;
    }

    // ── Import sources ────────────────────────────────────────────────────────

    private void importFromLocal() {
        FileChooser chooser = new FileChooser();
        chooser.setTitle("Open dataset file");
        chooser.getExtensionFilters().addAll(
            new FileChooser.ExtensionFilter("Data files", "*.csv","*.tsv","*.txt","*.dat"),
            new FileChooser.ExtensionFilter("All files", "*.*"));
        File file = chooser.showOpenDialog(ownerWindow);
        if (file == null) return;

        setStatus("Loading " + file.getName() + "…");
        Thread t = new Thread(() -> {
            try {
                String content = Files.readString(file.toPath());
                loadCsv(content, file.getName());
            } catch (Exception ex) {
                Platform.runLater(() -> setStatus("Error: " + ex.getMessage()));
            }
        });
        t.setDaemon(true); t.start();
    }

    private void importFromUrl() {
        TextInputDialog dlg = new TextInputDialog();
        dlg.setTitle("Import from URL");
        dlg.setHeaderText("Enter a URL to a public CSV or TSV file.\n" +
            "For Google Sheets: File → Share → Publish to web → CSV format.");
        dlg.setContentText("URL:");
        dlg.getEditor().setPrefWidth(420);
        dlg.showAndWait().ifPresent(url -> {
            if (url.isBlank()) return;
            // Convert Google Sheets edit URL to CSV export URL automatically
            String fetchUrl = convertGSheetsUrl(url.trim());
            setStatus("Fetching " + fetchUrl.substring(0, Math.min(50, fetchUrl.length())) + "…");
            Thread t = new Thread(() -> {
                try {
                    HttpURLConnection conn = (HttpURLConnection) URI.create(fetchUrl).toURL().openConnection();
                    conn.setRequestProperty("User-Agent", "MathEngine/1.0");
                    conn.setConnectTimeout(10_000);
                    conn.setReadTimeout(20_000);
                    String content;
                    try (InputStream is = conn.getInputStream();
                         BufferedReader br = new BufferedReader(new InputStreamReader(is))) {
                        content = br.lines().collect(java.util.stream.Collectors.joining("\n"));
                    }
                    loadCsv(content, fetchUrl);
                } catch (Exception ex) {
                    Platform.runLater(() -> setStatus("Fetch failed: " + ex.getMessage()));
                }
            });
            t.setDaemon(true); t.start();
        });
    }

    private void importFromPaste() {
        Dialog<String> dlg = new Dialog<>();
        dlg.setTitle("Paste dataset");
        dlg.setHeaderText("Paste comma- or tab-separated data.\nFirst row can contain column headers.");
        ButtonType okType = new ButtonType("Load", ButtonBar.ButtonData.OK_DONE);
        dlg.getDialogPane().getButtonTypes().addAll(okType, ButtonType.CANCEL);

        TextArea ta = new TextArea();
        ta.setPrefRowCount(10);
        ta.setPrefColumnCount(60);
        ta.setPromptText("x, y, z\n1, 2, 3\n4, 5, 6\n…");
        dlg.getDialogPane().setContent(ta);
        dlg.setResultConverter(bt -> bt == okType ? ta.getText() : null);

        dlg.showAndWait().ifPresent(text -> {
            if (text != null && !text.isBlank()) loadCsv(text, "paste");
        });
    }

    // ── CSV parsing ───────────────────────────────────────────────────────────

    private void loadCsv(String raw, String source) {
        try {
            Map<String, List<Double>> data = parseCsv(raw);
            if (data.isEmpty()) {
                Platform.runLater(() -> setStatus("No numeric columns found in data."));
                return;
            }
            Platform.runLater(() -> {
                currentDataset = data;
                colSelector.getItems().setAll(data.keySet());
                colSelector.getSelectionModel().selectFirst();
                int rows = data.values().iterator().next().size();
                setStatus("✓ Loaded " + data.size() + " column(s), " + rows + " row(s)  ←  " +
                    source.substring(Math.max(0, source.length()-40)));
                if (onDatasetLoaded != null) onDatasetLoaded.accept(data);
            });
        } catch (Exception ex) {
            Platform.runLater(() -> setStatus("Parse error: " + ex.getMessage()));
        }
    }

    private static Map<String, List<Double>> parseCsv(String raw) {
        Map<String, List<Double>> result = new LinkedHashMap<>();
        String[] lines = raw.replace("\r\n", "\n").replace("\r", "\n").split("\n");
        if (lines.length == 0) return result;

        // Detect separator: tab > comma > semicolon > space
        String sep = lines[0].contains("\t") ? "\t"
                   : lines[0].contains(",")  ? ","
                   : lines[0].contains(";")  ? ";"
                   : " +";

        // First row — try as header
        String[] firstRow = lines[0].split(sep, -1);
        boolean hasHeader = false;
        for (String cell : firstRow) {
            try { Double.parseDouble(cell.trim()); }
            catch (NumberFormatException e) { hasHeader = true; break; }
        }

        String[] headers;
        int dataStart;
        if (hasHeader) {
            headers = firstRow;
            for (int i = 0; i < headers.length; i++)
                headers[i] = headers[i].trim().replaceAll("^\"|\"$", "");
            dataStart = 1;
        } else {
            headers = new String[firstRow.length];
            for (int i = 0; i < firstRow.length; i++) headers[i] = "col" + (i + 1);
            dataStart = 0;
        }

        List<List<Double>> cols = new ArrayList<>();
        for (int i = 0; i < headers.length; i++) cols.add(new ArrayList<>());

        for (int r = dataStart; r < lines.length; r++) {
            String line = lines[r].trim();
            if (line.isEmpty()) continue;
            String[] cells = line.split(sep, -1);
            for (int c = 0; c < Math.min(cells.length, headers.length); c++) {
                try { cols.get(c).add(Double.parseDouble(cells[c].trim())); }
                catch (NumberFormatException ignored) {}
            }
        }

        for (int i = 0; i < headers.length; i++) {
            if (!cols.get(i).isEmpty()) result.put(headers[i], cols.get(i));
        }
        return result;
    }

    private static String convertGSheetsUrl(String url) {
        // https://docs.google.com/spreadsheets/d/ID/edit → export as CSV
        if (url.contains("docs.google.com/spreadsheets")) {
            String base = url.replaceAll("/edit.*$", "").replaceAll("/pub.*$", "");
            return base + "/export?format=csv";
        }
        return url;
    }

    // ── Expression builders ───────────────────────────────────────────────────

    private String buildSummarize(String col) {
        List<Double> vals = currentDataset.get(col);
        if (vals == null || vals.isEmpty()) return "summarize[1,2,3,4,5]";
        StringBuilder sb = new StringBuilder("summarize[");
        for (int i = 0; i < vals.size(); i++) {
            if (i > 0) sb.append(',');
            sb.append(vals.get(i) % 1 == 0 ? String.valueOf(vals.get(i).longValue()) : vals.get(i));
        }
        return sb.append(']').toString();
    }

    private void fireRegression() {
        if (!hasDataset() || currentDataset.size() < 2) {
            setStatus("Need at least 2 columns for regression.");
            return;
        }
        List<String> keys = new ArrayList<>(currentDataset.keySet());
        String xKey = keys.get(0), yKey = keys.get(1);
        List<Double> xs = currentDataset.get(xKey);
        List<Double> ys = currentDataset.get(yKey);
        int n = Math.min(xs.size(), ys.size());
        StringBuilder xb = new StringBuilder(), yb = new StringBuilder();
        for (int i = 0; i < n; i++) {
            if (i > 0) { xb.append(','); yb.append(','); }
            xb.append(xs.get(i) % 1 == 0 ? String.valueOf(xs.get(i).longValue()) : xs.get(i));
            yb.append(ys.get(i) % 1 == 0 ? String.valueOf(ys.get(i).longValue()) : ys.get(i));
        }
        fireExpr("regression[" + xb + "|" + yb + "]");
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    private void toggleExpanded() {
        boolean show = !body.isVisible();
        body.setVisible(show); body.setManaged(show);
        chevron.setText(show ? "▾" : "▸");
    }

    private void setStatus(String msg) {
        statusLabel.setText(msg);
    }

    private void clearDataset() {
        currentDataset = null;
        colSelector.getItems().clear();
        setStatus("No dataset loaded");
    }

    private void fireExpr(String expr) {
        if (onExpressionRequested != null) onExpressionRequested.accept(expr);
    }

    private static Button iconButton(String label, Runnable action) {
        Button b = new Button(label);
        b.setOnAction(e -> action.run());
        return b;
    }

    private static class Spacer extends Region {
        Spacer() { HBox.setHgrow(this, Priority.ALWAYS); }
    }
}