package com.mathengine.ui;

import javafx.application.Platform;
import javafx.scene.Scene;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
import java.util.prefs.Preferences;

/**
 * ThemeManager
 * ─────────────
 * Manages application-wide theming and font scaling for ADA compliance.
 *
 * Themes:
 *   LIGHT         — warm off-white surfaces, dark text
 *   DARK          — deep charcoal surfaces, light text
 *   SYSTEM        — follows the OS preference (detected via screen brightness
 *                   heuristic; JavaFX has no native prefers-color-scheme API)
 *   HIGH_CONTRAST — pure black background, white text, yellow accents
 *
 * Font sizes:
 *   NORMAL (13px base), LARGE (16px), X_LARGE (20px)
 *
 * Preferences are persisted via java.util.prefs so the user's
 * choice survives app restarts.
 *
 * CSS is loaded from /css/<theme>.css resources.
 * Every Scene registered with addScene() is updated on theme change.
 */
public class ThemeManager {

    // ── Enums ─────────────────────────────────────────────────────────────────

    public enum Theme {
        LIGHT("light", "☀  Light"),
        DARK("dark", "◑  Dark"),
        SYSTEM("system", "⊙  System"),
        HIGH_CONTRAST("high-contrast", "◈  High Contrast");

        public final String cssName;
        public final String label;
        Theme(String cssName, String label) {
            this.cssName = cssName;
            this.label   = label;
        }
    }

    public enum FontSize {
        NORMAL  ("normal",  13.0, "A"),
        LARGE   ("large",   16.0, "A+"),
        X_LARGE ("x-large", 20.0, "A++");

        public final String key;
        public final double basePx;
        public final String label;
        FontSize(String key, double basePx, String label) {
            this.key    = key;
            this.basePx = basePx;
            this.label  = label;
        }
    }

    // ── Singleton ─────────────────────────────────────────────────────────────

    private static ThemeManager INSTANCE;
    public static ThemeManager getInstance() {
        if (INSTANCE == null) INSTANCE = new ThemeManager();
        return INSTANCE;
    }

    // ── State ─────────────────────────────────────────────────────────────────

    private Theme    currentTheme;
    private FontSize currentFontSize;

    private final List<Scene>              scenes    = new ArrayList<>();
    private final List<Consumer<Theme>>    listeners = new ArrayList<>();
    private final Preferences              prefs     = Preferences.userNodeForPackage(ThemeManager.class);

    // ── Constructor ───────────────────────────────────────────────────────────

    private ThemeManager() {
        // Restore persisted preferences
        String savedTheme = prefs.get("theme", Theme.LIGHT.cssName);
        String savedFont  = prefs.get("fontSize", FontSize.NORMAL.key);

        currentTheme    = themeFromKey(savedTheme);
        currentFontSize = fontSizeFromKey(savedFont);
    }

    // ── Public API ────────────────────────────────────────────────────────────

    /** Register a scene to receive stylesheet updates on every theme change. */
    public void addScene(Scene scene) {
        scenes.add(scene);
        applyTheme(scene, resolveTheme(currentTheme));
        applyFontSize(scene, currentFontSize);
    }

    /** Add a listener called after every theme switch (for controls to update their state). */
    public void addThemeListener(Consumer<Theme> listener) {
        listeners.add(listener);
    }

    public void setTheme(Theme theme) {
        currentTheme = theme;
        prefs.put("theme", theme.cssName);

        // FIX: Pass the .key string instead of the enum object
        AuthService.getInstance().pushPreferencesAsync(theme.cssName, currentFontSize.key);

        Theme resolved = resolveTheme(theme);
        scenes.forEach(s -> applyTheme(s, resolved));
        listeners.forEach(l -> l.accept(theme));
    }

    public void setFontSize(FontSize size) {
        currentFontSize = size;
        prefs.put("fontSize", size.key);
        // Sync to server if logged in
        AuthService.getInstance().pushPreferencesAsync(currentTheme.cssName, size.key);
        scenes.forEach(s -> applyFontSize(s, size));
        listeners.forEach(l -> l.accept(currentTheme)); // triggers re-render
    }

    public Theme    getCurrentTheme()    { return currentTheme;    }
    public FontSize getCurrentFontSize() { return currentFontSize; }

    /** Returns the effective theme (resolves SYSTEM → LIGHT or DARK). */
    public Theme resolveTheme(Theme preference) {
        if (preference != Theme.SYSTEM) return preference;
        // JavaFX has no CSS media query API, so we detect via platform brightness.
        // Falls back to LIGHT if detection is unavailable.
        String osName = System.getProperty("os.name", "").toLowerCase();
        if (osName.contains("mac")) {
            String appearance = System.getProperty("apple.awt.application.appearance", "");
            return appearance.toLowerCase().contains("dark") ? Theme.DARK : Theme.LIGHT;
        }
        // Windows: check registry setting (best-effort)
        if (osName.contains("win")) {
            try {
                @SuppressWarnings("deprecation")
                Process p = Runtime.getRuntime().exec(
                    "reg query HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize /v AppsUseLightTheme");
                String out = new String(p.getInputStream().readAllBytes());
                return out.contains("0x0") ? Theme.DARK : Theme.LIGHT;
            } catch (Exception ignored) {}
        }
        return Theme.LIGHT;
    }

    // ── Private helpers ───────────────────────────────────────────────────────

    private void applyTheme(Scene scene, Theme resolved) {
        Platform.runLater(() -> {
            scene.getStylesheets().clear();
            // Base stylesheet always loads first
            addSheet(scene, "/css/base.css");
            // Then the theme-specific overrides
            addSheet(scene, "/css/" + resolved.cssName + ".css");
            // Then font-size sheet
            addSheet(scene, "/css/font-" + currentFontSize.key + ".css");
        });
    }

    private void applyFontSize(Scene scene, FontSize size) {
        Platform.runLater(() -> {
            // Remove existing font sheets, re-add correct one
            scene.getStylesheets().removeIf(s -> s.contains("/css/font-"));
            addSheet(scene, "/css/font-" + size.key + ".css");
        });
    }

    private void addSheet(Scene scene, String resource) {
        var url = getClass().getResource(resource);
        if (url != null) {
            scene.getStylesheets().add(url.toExternalForm());
        } else {
            System.err.println("[ThemeManager] Missing stylesheet: " + resource);
        }
    }

    private Theme themeFromKey(String key) {
        for (Theme t : Theme.values()) if (t.cssName.equals(key)) return t;
        return Theme.LIGHT;
    }

    private FontSize fontSizeFromKey(String key) {
        for (FontSize f : FontSize.values()) if (f.key.equals(key)) return f;
        return FontSize.NORMAL;
    }
}