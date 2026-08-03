package com.mathengine.ui;

import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.control.Tooltip;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Region;

/**
 * AccessibilityToolbar
 * ─────────────────────
 * A JavaFX HBox containing:
 *
 * Font size buttons: [A] [A+] [A++]
 * Separator
 * Theme buttons: [☀ Light] [◑ Dark] [⊙ System] [◈ High Contrast]
 *
 * All buttons are keyboard-focusable and have tooltips.
 * Active state is shown with the "toolbar-btn-active" CSS class.
 *
 * Wires directly into ThemeManager so changes propagate to every
 * registered Scene immediately.
 */
public class AccessibilityToolbar extends HBox {

    private final ThemeManager tm = ThemeManager.getInstance();

    // Font size button references (to update active state)
    private final Button btnNormal = makeBtn(ThemeManager.FontSize.NORMAL.label, "Normal text size");
    private final Button btnLarge = makeBtn(ThemeManager.FontSize.LARGE.label, "Large text size");
    private final Button btnXLarge = makeBtn(ThemeManager.FontSize.X_LARGE.label, "Extra-large text size");

    // Theme button references
    private final Button btnLight = makeBtn(ThemeManager.Theme.LIGHT.label, "Switch to Light theme");
    private final Button btnDark = makeBtn(ThemeManager.Theme.DARK.label, "Switch to Dark theme");
    private final Button btnSystem = makeBtn(ThemeManager.Theme.SYSTEM.label, "Follow system color scheme");
    private final Button btnHC = makeBtn(ThemeManager.Theme.HIGH_CONTRAST.label, "High Contrast theme (ADA)");

    public AccessibilityToolbar() {
        setSpacing(6);
        setAlignment(Pos.CENTER_RIGHT);
        setPadding(new Insets(0, 0, 0, 0));
        getStyleClass().add("accessibility-toolbar");

        // ── Font size group ───────────────────────────────────────────────────
        Label fontLabel = new Label("Size:");
        fontLabel.getStyleClass().add("toolbar-group-label");

        btnNormal.setOnAction(e -> {
            tm.setFontSize(ThemeManager.FontSize.NORMAL);
            refreshFontState();
        });
        btnLarge.setOnAction(e -> {
            tm.setFontSize(ThemeManager.FontSize.LARGE);
            refreshFontState();
        });
        btnXLarge.setOnAction(e -> {
            tm.setFontSize(ThemeManager.FontSize.X_LARGE);
            refreshFontState();
        });

        // ── Theme group ───────────────────────────────────────────────────────
        Label themeLabel = new Label("Theme:");
        themeLabel.getStyleClass().add("toolbar-group-label");

        btnLight.setOnAction(e -> {
            tm.setTheme(ThemeManager.Theme.LIGHT);
            refreshThemeState();
        });
        btnDark.setOnAction(e -> {
            tm.setTheme(ThemeManager.Theme.DARK);
            refreshThemeState();
        });
        btnSystem.setOnAction(e -> {
            tm.setTheme(ThemeManager.Theme.SYSTEM);
            refreshThemeState();
        });
        btnHC.setOnAction(e -> {
            tm.setTheme(ThemeManager.Theme.HIGH_CONTRAST);
            refreshThemeState();
        });

        getChildren().addAll(
                fontLabel, btnNormal, btnLarge, btnXLarge,
                separator(),
                themeLabel, btnLight, btnDark, btnSystem, btnHC);

        // Reflect current persisted state on first paint
        refreshFontState();
        refreshThemeState();

        // Keep in sync if theme is changed from elsewhere
        tm.addThemeListener(t -> {
            refreshThemeState();
            refreshFontState();
        });
    }

    // ── Refresh active states ─────────────────────────────────────────────────

    private void refreshFontState() {
        ThemeManager.FontSize cur = tm.getCurrentFontSize();
        setActive(btnNormal, cur == ThemeManager.FontSize.NORMAL);
        setActive(btnLarge, cur == ThemeManager.FontSize.LARGE);
        setActive(btnXLarge, cur == ThemeManager.FontSize.X_LARGE);
    }

    private void refreshThemeState() {
        ThemeManager.Theme cur = tm.getCurrentTheme();
        setActive(btnLight, cur == ThemeManager.Theme.LIGHT);
        setActive(btnDark, cur == ThemeManager.Theme.DARK);
        setActive(btnSystem, cur == ThemeManager.Theme.SYSTEM);
        setActive(btnHC, cur == ThemeManager.Theme.HIGH_CONTRAST);
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    private static Button makeBtn(String text, String tooltip) {
        Button b = new Button(text);
        b.getStyleClass().add("toolbar-btn");
        b.setTooltip(new Tooltip(tooltip));
        // ADA: accessible text
        b.setAccessibleText(tooltip);
        b.setFocusTraversable(true);
        return b;
    }

    private static void setActive(Button b, boolean active) {
        if (active) {
            if (!b.getStyleClass().contains("toolbar-btn-active"))
                b.getStyleClass().add("toolbar-btn-active");
        } else {
            b.getStyleClass().remove("toolbar-btn-active");
        }
    }

    private static Region separator() {
        Region sep = new Region();
        sep.setPrefWidth(1);
        sep.setPrefHeight(20);
        sep.getStyleClass().add("toolbar-separator");
        sep.setFocusTraversable(false);
        return sep;
    }
}