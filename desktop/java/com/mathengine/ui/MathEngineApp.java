package com.mathengine.ui;

import javafx.animation.FadeTransition;
import javafx.application.Application;
import javafx.scene.Scene;
import javafx.stage.Stage;
import javafx.util.Duration;

/**
 * MathEngineApp — JavaFX application entry point.
 *
 * Flow:
 *   1. Show LoginScreen (login / register / guest)
 *   2. On success → fade into MainLayout
 *
 * The scene is created once and its root is swapped to avoid
 * re-registering stylesheets with ThemeManager on every transition.
 */
public class MathEngineApp extends Application {

    private Scene scene;
    @Override
    public void start(Stage stage) {
        // Create the single Scene with the login screen as initial root
        LoginScreen loginScreen = new LoginScreen();
        scene = new Scene(loginScreen, 1100, 720);

        // Apply theme before first paint
        ThemeManager.getInstance().addScene(scene);

        loginScreen.setOnSuccess(username -> transitionToMain(username));
        loginScreen.setOnGuest(() -> transitionToMain(null));

        stage.setTitle("Math Engine");
        stage.setScene(scene);
        stage.setMinWidth(780);
        stage.setMinHeight(520);
        stage.show();
    }

    private void transitionToMain(String username) {
        MainLayout main = new MainLayout(username);

        // Fade out current root, swap, fade in
        FadeTransition out = new FadeTransition(Duration.millis(180), scene.getRoot());
        out.setFromValue(1); out.setToValue(0);
        out.setOnFinished(e -> {
            scene.setRoot(main);
            main.setOpacity(0);
            FadeTransition in = new FadeTransition(Duration.millis(220), main);
            in.setFromValue(0); in.setToValue(1);
            in.play();
        });
        out.play();
    }

    @Override
    public void stop() {
        // Cleanly terminate the chatbot's Python subprocess, if it was started.
        com.mathengine.chatbot.ChatbotBridge.getInstance().shutdown();
    }

    public static void main(String[] args) {
        launch(args);
    }
}
