package com.mathengine.ui;

import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.Node;
import javafx.scene.control.Label;
import javafx.scene.control.Separator;
import javafx.scene.input.KeyCode;
import javafx.scene.layout.*;
import javafx.stage.Popup;

/**
 * SettingsPanel
 * ──────────────
 * A floating popover anchored to the ⚙ button that exposes
 * theme and font-size controls, replacing the AccessibilityToolbar
 * in the header (those controls now live here and in the toolbar).
 *
 * The panel is a Popup (non-modal, auto-hides on outside click).
 */
public class SettingsPanel {

    private final Popup         popup = new Popup();
    private final ThemeManager  tm    = ThemeManager.getInstance();
    private boolean built = false;

    public void toggle(Node anchor) {
        if (popup.isShowing()) {
            popup.hide();
            return;
        }
        // Rebuild every time so active state is always current
        built = false;
        popup.getContent().clear();
        ensureBuilt();
        javafx.geometry.Bounds b = anchor.localToScreen(anchor.getBoundsInLocal());
        if (b == null) return;
        popup.show(anchor.getScene().getWindow(), b.getMaxX() - 280, b.getMaxY() + 6);
    }

    public void hide() { popup.hide(); }

    private void ensureBuilt() {
        if (built) return;
        built = true;

        VBox root = new VBox(0);
        root.getStyleClass().add("settings-popup");
        root.setPrefWidth(280);
        root.setOnKeyPressed(e -> { if (e.getCode() == KeyCode.ESCAPE) popup.hide(); });

        // ── Header ────────────────────────────────────────────────────────────
        Label title = new Label("Settings");
        title.getStyleClass().add("settings-title");
        HBox header = new HBox(title);
        header.getStyleClass().add("settings-header");
        header.setPadding(new Insets(12, 16, 12, 16));

        // ── Theme section ─────────────────────────────────────────────────────
        Label themeLabel = new Label("THEME");
        themeLabel.getStyleClass().add("settings-section-label");

        HBox themeRow = new HBox(6);
        themeRow.setPadding(new Insets(0, 16, 0, 16));
        themeRow.setAlignment(Pos.CENTER_LEFT);

        for (ThemeManager.Theme t : ThemeManager.Theme.values()) {
            javafx.scene.control.Button btn = new javafx.scene.control.Button(t.label);
            btn.getStyleClass().add("settings-theme-btn");
            btn.setMaxWidth(Double.MAX_VALUE);
            HBox.setHgrow(btn, Priority.ALWAYS);
            if (tm.resolveTheme(tm.getCurrentTheme()) == t ||
                tm.getCurrentTheme() == t) {
                btn.getStyleClass().add("settings-theme-btn-active");
            }
            btn.setOnAction(e -> {
                tm.setTheme(t);
                // Refresh active states
                themeRow.getChildren().forEach(n ->
                    n.getStyleClass().remove("settings-theme-btn-active"));
                btn.getStyleClass().add("settings-theme-btn-active");
            });
            themeRow.getChildren().add(btn);
        }

        VBox themeSection = new VBox(8, themeLabel, themeRow);
        themeSection.setPadding(new Insets(12, 0, 12, 0));

        // ── Font size section ─────────────────────────────────────────────────
        Label fontLabel = new Label("FONT SIZE");
        fontLabel.getStyleClass().add("settings-section-label");

        HBox fontRow = new HBox(6);
        fontRow.setPadding(new Insets(0, 16, 0, 16));
        fontRow.setAlignment(Pos.CENTER_LEFT);

        for (ThemeManager.FontSize fs : ThemeManager.FontSize.values()) {
            javafx.scene.control.Button btn = new javafx.scene.control.Button(fs.label);
            btn.getStyleClass().add("settings-font-btn");
            btn.setMinWidth(60);
            if (tm.getCurrentFontSize() == fs) {
                btn.getStyleClass().add("settings-font-btn-active");
            }
            btn.setOnAction(e -> {
                tm.setFontSize(fs);
                fontRow.getChildren().forEach(n ->
                    n.getStyleClass().remove("settings-font-btn-active"));
                btn.getStyleClass().add("settings-font-btn-active");
            });
            fontRow.getChildren().add(btn);
        }

        VBox fontSection = new VBox(8, fontLabel, fontRow);
        fontSection.setPadding(new Insets(0, 0, 12, 0));

        // ── Version info ──────────────────────────────────────────────────────
        Label version = new Label("Math Engine  ·  C++ math backend  ·  JavaFX 21");
        version.getStyleClass().add("settings-version");
        version.setWrapText(true);
        HBox versionRow = new HBox(version);
        versionRow.setPadding(new Insets(10, 16, 12, 16));
        versionRow.getStyleClass().add("settings-version-row");

        root.getChildren().addAll(
            header,
            new Separator(),
            themeSection,
            new Separator(),
            fontSection,
            new Separator(),
            versionRow
        );

        popup.setAutoHide(true);
        popup.setHideOnEscape(true);
        popup.getContent().add(root);
    }
}