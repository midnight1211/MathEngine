package com.mathengine.ui;

import javafx.animation.FadeTransition;
import javafx.application.Platform;
import javafx.geometry.Pos;
import javafx.scene.control.*;
import javafx.scene.layout.*;
import javafx.util.Duration;
import java.util.function.Consumer;

/**
 * LoginScreen
 * ────────────
 * Full-window login / create-account screen shown before the main app.
 *
 * Layout:
 *   ┌──────────────────────────────────────┐
 *   │          M  Math Engine              │  ← logo
 *   │     A symbolic mathematics suite     │  ← tagline
 *   │                                      │
 *   │   ┌──── Log In ──┬── Create Account ─┤  ← tab bar
 *   │   │  Username    │                   │
 *   │   │  Password    │                   │
 *   │   │  [Log In]    │                   │
 *   │   └──────────────┘                   │
 *   │       [Continue as Guest]            │
 *   └──────────────────────────────────────┘
 *
 * Calls onSuccess(username) when the user logs in or registers.
 * Calls onGuest() when the user clicks "Continue as Guest".
 *
 * Wire in MathEngineApp:
 *   LoginScreen ls = new LoginScreen();
 *   ls.setOnSuccess(user -> transitionToMain(stage, user));
 *   ls.setOnGuest(() -> transitionToMain(stage, null));
 */
public class LoginScreen extends StackPane {

    // ── Callbacks ─────────────────────────────────────────────────────────────

    private Consumer<String> onSuccess;  // receives username
    private Runnable         onGuest;

    // ── Widgets (login tab) ───────────────────────────────────────────────────

    private final TextField     loginUser  = new TextField();
    private final PasswordField loginPass  = new PasswordField();
    private final Button        loginBtn   = new Button("Log In");
    private final Label         loginMsg   = new Label();

    // ── Widgets (register tab) ────────────────────────────────────────────────

    private final TextField     regUser    = new TextField();
    private final PasswordField regPass    = new PasswordField();
    private final PasswordField regPass2   = new PasswordField();
    private final TextField     regEmail   = new TextField();
    private final Button        regBtn     = new Button("Create Account");
    private final Label         regMsg     = new Label();

    // ── Tab state ─────────────────────────────────────────────────────────────

    private final Button tabLogin  = new Button("Log In");
    private final Button tabReg    = new Button("Create Account");
    private final StackPane formStack = new StackPane();

    // ── Services ──────────────────────────────────────────────────────────────

    private final AuthService auth  = AuthService.getInstance();


    // ── Constructor ───────────────────────────────────────────────────────────

    public LoginScreen() {
        getStyleClass().add("auth-root");
        VBox.setVgrow(this, Priority.ALWAYS);

        // Center card
        VBox card = buildCard();
        card.setMaxWidth(420);
        card.setMaxHeight(Double.MAX_VALUE);

        setAlignment(Pos.CENTER);
        getChildren().add(card);
    }

    // ── Public API ────────────────────────────────────────────────────────────

    public void setOnSuccess(Consumer<String> cb) { onSuccess = cb; }
    public void setOnGuest(Runnable cb)           { onGuest   = cb; }

    // ── Build ─────────────────────────────────────────────────────────────────

    private VBox buildCard() {
        VBox card = new VBox(20);
        card.getStyleClass().add("auth-card");
        card.setAlignment(Pos.TOP_CENTER);

        card.getChildren().addAll(
            buildLogoRow(),
            buildTabBar(),
            formStack,
            buildGuestRow()
        );

        // Init form stack
        formStack.getChildren().addAll(buildRegisterForm(), buildLoginForm());
        selectTab(true);

        return card;
    }

    private HBox buildLogoRow() {
        Label mark = new Label("M");
        mark.getStyleClass().add("auth-logo-mark");

        Label name   = new Label("Math");   name.getStyleClass().add("auth-app-name");
        Label engine = new Label("Engine"); engine.getStyleClass().add("auth-app-name");
        engine.setStyle("-fx-text-fill: -accent-primary;");

        VBox nameBox = new VBox(2,
            new HBox(6, name, engine),
            makeLabel("A symbolic mathematics suite", "auth-app-tagline"));

        HBox row = new HBox(14, mark, nameBox);
        row.setAlignment(Pos.CENTER_LEFT);
        return row;
    }

    private HBox buildTabBar() {
        tabLogin.getStyleClass().addAll("auth-tab-btn");
        tabReg.getStyleClass().addAll("auth-tab-btn");

        HBox bar = new HBox(4, tabLogin, tabReg);
        bar.getStyleClass().add("auth-tab-bar");
        bar.setAlignment(Pos.CENTER);
        HBox.setHgrow(tabLogin, Priority.ALWAYS);
        HBox.setHgrow(tabReg,   Priority.ALWAYS);
        tabLogin.setMaxWidth(Double.MAX_VALUE);
        tabReg.setMaxWidth(Double.MAX_VALUE);

        tabLogin.setOnAction(e -> selectTab(true));
        tabReg.setOnAction(e -> selectTab(false));
        return bar;
    }

    private VBox buildLoginForm() {
        VBox form = new VBox(12);
        form.setAlignment(Pos.TOP_LEFT);

        loginUser.setPromptText("Username");
        loginUser.getStyleClass().add("auth-field");
        loginPass.setPromptText("Password");
        loginPass.getStyleClass().add("auth-field");

        loginBtn.getStyleClass().add("auth-submit-btn");
        loginBtn.setMaxWidth(Double.MAX_VALUE);
        loginBtn.setDefaultButton(false);

        loginMsg.getStyleClass().add("auth-error");
        loginMsg.setWrapText(true);
        loginMsg.setVisible(false);
        loginMsg.setManaged(false);

        loginBtn.setOnAction(e -> doLogin());
        loginUser.setOnAction(e -> loginPass.requestFocus());
        loginPass.setOnAction(e -> doLogin());

        form.getChildren().addAll(
            makeLabel("Username", "auth-field-label"), loginUser,
            makeLabel("Password", "auth-field-label"), loginPass,
            loginMsg,
            loginBtn
        );
        return form;
    }

    private VBox buildRegisterForm() {
        VBox form = new VBox(12);
        form.setAlignment(Pos.TOP_LEFT);

        regUser.setPromptText("Choose a username");
        regUser.getStyleClass().add("auth-field");
        regPass.setPromptText("Password (min 8 chars)");
        regPass.getStyleClass().add("auth-field");
        regPass2.setPromptText("Confirm password");
        regPass2.getStyleClass().add("auth-field");
        regEmail.setPromptText("Email (optional)");
        regEmail.getStyleClass().add("auth-field");

        regBtn.getStyleClass().add("auth-submit-btn");
        regBtn.setMaxWidth(Double.MAX_VALUE);

        regMsg.setWrapText(true);
        regMsg.setVisible(false);
        regMsg.setManaged(false);

        regBtn.setOnAction(e -> doRegister());
        regUser.setOnAction(e -> regPass.requestFocus());
        regPass.setOnAction(e -> regPass2.requestFocus());
        regPass2.setOnAction(e -> regEmail.requestFocus());
        regEmail.setOnAction(e -> doRegister());

        form.getChildren().addAll(
            makeLabel("Username", "auth-field-label"), regUser,
            makeLabel("Password", "auth-field-label"), regPass,
            makeLabel("Confirm Password", "auth-field-label"), regPass2,
            makeLabel("Email (optional)", "auth-field-label"), regEmail,
            regMsg,
            regBtn
        );
        return form;
    }

    private HBox buildGuestRow() {
        // Horizontal divider with "or" label
        Region left = new Region(); HBox.setHgrow(left,  Priority.ALWAYS);
        left.setStyle("-fx-border-color: -brd-subtle; -fx-border-width: 0 0 1 0; -fx-pref-height: 1;");
        Region right = new Region(); HBox.setHgrow(right, Priority.ALWAYS);
        right.setStyle("-fx-border-color: -brd-subtle; -fx-border-width: 0 0 1 0; -fx-pref-height: 1;");
        Label or = new Label("or"); or.getStyleClass().add("auth-divider-label");

        Button guestBtn = new Button("Continue as Guest");
        guestBtn.getStyleClass().add("auth-guest-btn");
        guestBtn.setMaxWidth(Double.MAX_VALUE);
        guestBtn.setOnAction(e -> { if (onGuest != null) onGuest.run(); });

        HBox divider = new HBox(left, or, right);
        divider.setAlignment(Pos.CENTER);

        VBox wrapper = new VBox(8, divider, guestBtn);
        wrapper.setAlignment(Pos.CENTER);
        HBox box = new HBox(wrapper);
        HBox.setHgrow(wrapper, Priority.ALWAYS);
        return box;
    }

    // ── Actions ───────────────────────────────────────────────────────────────

    private void doLogin() {
        String user = loginUser.getText().trim();
        String pass = loginPass.getText();
        if (user.isEmpty() || pass.isEmpty()) {
            showMessage(loginMsg, "Please enter your username and password.", false);
            return;
        }
        loginBtn.setDisable(true);
        loginBtn.setText("Logging in…");

        // Run on background thread so UI stays responsive
        Thread t = new Thread(() -> {
            AuthService.AuthResult result = auth.login(user, pass);
            Platform.runLater(() -> {
                loginBtn.setDisable(false);
                loginBtn.setText("Log In");
                if (result.success()) {
                    if (onSuccess != null) onSuccess.accept(auth.getCurrentUser());
                } else {
                    showMessage(loginMsg, result.message(), false);
                    loginPass.clear();
                    loginPass.requestFocus();
                }
            });
        });
        t.setDaemon(true);
        t.start();
    }

    private void doRegister() {
        String user   = regUser.getText().trim();
        String pass   = regPass.getText();
        String pass2  = regPass2.getText();
        String email  = regEmail.getText().trim();

        if (user.isEmpty() || pass.isEmpty()) {
            showMessage(regMsg, "Username and password are required.", false);
            return;
        }
        if (!pass.equals(pass2)) {
            showMessage(regMsg, "Passwords do not match.", false);
            regPass2.clear();
            regPass2.requestFocus();
            return;
        }

        regBtn.setDisable(true);
        regBtn.setText("Creating account…");

        Thread t = new Thread(() -> {
            AuthService.AuthResult result = auth.register(user, pass, email);
            Platform.runLater(() -> {
                regBtn.setDisable(false);
                regBtn.setText("Create Account");
                if (result.success()) {
                    // Auto-login after registration
                    auth.login(user, pass);
                    if (onSuccess != null) onSuccess.accept(auth.getCurrentUser());
                } else {
                    showMessage(regMsg, result.message(), false);
                }
            });
        });
        t.setDaemon(true);
        t.start();
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    private void selectTab(boolean loginTab) {
        // Index 0 = register form, Index 1 = login form (added in that order)
        javafx.scene.Node loginForm = formStack.getChildren().get(1);
        javafx.scene.Node regForm   = formStack.getChildren().get(0);
        if (loginTab) {
            tabLogin.getStyleClass().add("auth-tab-btn-active");
            tabReg.getStyleClass().remove("auth-tab-btn-active");
            loginForm.setVisible(true);  loginForm.setManaged(true);
            regForm.setVisible(false);   regForm.setManaged(false);
            Platform.runLater(loginUser::requestFocus);
        } else {
            tabReg.getStyleClass().add("auth-tab-btn-active");
            tabLogin.getStyleClass().remove("auth-tab-btn-active");
            regForm.setVisible(true);    regForm.setManaged(true);
            loginForm.setVisible(false); loginForm.setManaged(false);
            Platform.runLater(regUser::requestFocus);
        }
    }

    private void showMessage(Label lbl, String text, boolean isSuccess) {
        lbl.setText(text);
        lbl.getStyleClass().removeAll("auth-error", "auth-success");
        lbl.getStyleClass().add(isSuccess ? "auth-success" : "auth-error");
        lbl.setVisible(true);
        lbl.setManaged(true);

        // Fade in
        FadeTransition ft = new FadeTransition(Duration.millis(150), lbl);
        ft.setFromValue(0); ft.setToValue(1);
        ft.play();
    }

    private static Label makeLabel(String text, String styleClass) {
        Label l = new Label(text);
        l.getStyleClass().add(styleClass);
        return l;
    }
}