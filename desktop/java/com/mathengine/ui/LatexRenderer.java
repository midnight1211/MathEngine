package com.mathengine.ui;

import javafx.embed.swing.SwingFXUtils;
import javafx.scene.image.Image;
import javafx.scene.image.ImageView;
import org.scilab.forge.jlatexmath.TeXConstants;
import org.scilab.forge.jlatexmath.TeXFormula;
import org.scilab.forge.jlatexmath.TeXIcon;

import javax.swing.*;
import java.awt.*;
import java.awt.image.BufferedImage;

/**
 * LatexRenderer
 * ──────────────
 * Converts a LaTeX string into a JavaFX {@link ImageView} using JLaTeXMath.
 *
 * JLaTeXMath renders LaTeX to a Swing {@link TeXIcon}, which we then paint
 * onto a {@link BufferedImage} and convert to a JavaFX {@link Image} via
 * {@link SwingFXUtils#toFXImage}.
 *
 * Usage:
 * ImageView view = LatexRenderer.render("\\int_0^8 x^2\\,dx", 24, false);
 * outputPane.getChildren().add(view);
 *
 * The caller is responsible for wrapping in a ScrollPane if the rendered
 * image might be wider than the available space.
 */
public class LatexRenderer {

    /**
     * Renders a LaTeX string to a JavaFX ImageView.
     *
     * @param latex    LaTeX source (no surrounding $ or \[ needed)
     * @param sizePt   Font size in points — scale with ThemeManager font setting
     * @param darkMode If true, renders white text on transparent background;
     *                 if false, renders dark text on transparent background
     * @return An ImageView containing the rendered math, or an
     *         error ImageView if parsing fails
     */
    public static ImageView render(String latex, float sizePt, boolean darkMode) {
        if (latex == null || latex.isBlank()) {
            return new ImageView();
        }

        try {
            // Clean up common input patterns:
            // "$\int_0^8 x^2 dx$" → "\int_0^8 x^2 dx"
            String cleaned = latex.trim();
            if (cleaned.startsWith("$") && cleaned.endsWith("$") && cleaned.length() > 1) {
                cleaned = cleaned.substring(1, cleaned.length() - 1);
            }
            if (cleaned.startsWith("\\[") && cleaned.endsWith("\\]")) {
                cleaned = cleaned.substring(2, cleaned.length() - 2).trim();
            }

            // Build TeXFormula
            TeXFormula formula = new TeXFormula(cleaned);

            // DISPLAY style gives full-size integral/sum symbols
            TeXIcon icon = formula.createTeXIcon(TeXConstants.STYLE_DISPLAY, sizePt);
            icon.setInsets(new Insets(4, 4, 4, 4));

            int w = icon.getIconWidth();
            int h = icon.getIconHeight();
            if (w <= 0 || h <= 0)
                return new ImageView();

            // Paint onto BufferedImage
            BufferedImage bi = new BufferedImage(w, h, BufferedImage.TYPE_INT_ARGB);
            Graphics2D g2 = bi.createGraphics();
            g2.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
            g2.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);

            // Transparent background
            g2.setComposite(AlphaComposite.Clear);
            g2.fillRect(0, 0, w, h);
            g2.setComposite(AlphaComposite.SrcOver);

            // Choose foreground color
            Color fg = darkMode ? Color.WHITE : new Color(26, 23, 20);
            icon.setForeground(fg);

            // JLaTeXMath requires a JLabel as the component
            JLabel label = new JLabel();
            label.setForeground(fg);
            icon.paintIcon(label, g2, 0, 0);
            g2.dispose();

            Image fxImage = SwingFXUtils.toFXImage(bi, null);
            ImageView view = new ImageView(fxImage);
            view.setPreserveRatio(true);
            view.setSmooth(true);

            // ADA: set accessible text to the raw LaTeX string
            view.setAccessibleHelp("Rendered LaTeX: " + cleaned);
            view.setAccessibleText(cleaned);

            return view;

        } catch (Exception e) {
            // Return a small error marker rather than crashing
            System.err.println("[LatexRenderer] Failed to render: " + latex + " — " + e.getMessage());
            return makeErrorView("⚠ Cannot render: " + latex);
        }
    }

    /**
     * Convenience overload — uses the current ThemeManager font size and
     * dark-mode setting automatically.
     */
    public static ImageView render(String latex, ThemeManager tm) {
        boolean dark = isDarkTheme(tm);
        float size = (float) tm.getCurrentFontSize().basePx * 1.6f;
        return render(latex, size, dark);
    }

    private static boolean isDarkTheme(ThemeManager tm) {
        ThemeManager.Theme resolved = tm.resolveTheme(tm.getCurrentTheme());
        return resolved == ThemeManager.Theme.DARK
                || resolved == ThemeManager.Theme.HIGH_CONTRAST;
    }

    private static ImageView makeErrorView(String message) {
        // A tiny 1×1 placeholder; the error will surface via the status bar
        return new ImageView();
    }
}