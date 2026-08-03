package com.mathengine.ui;

import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.util.Base64;
import java.util.prefs.Preferences;

/**
 * AuthService — server-first, local fallback.
 *
 * When the Spring Boot server (SERVER_URL) is reachable:
 * - Login / register via POST /api/auth/login|register → receives JWT
 * - JWT is stored in Preferences and sent with every API call
 * - History pushed to server after every solve (pushHistoryAsync)
 * - Preferences (theme, font) pushed on change (pushPreferencesAsync)
 * - On next login from mobile or another desktop, everything is restored
 *
 * When the server is unreachable:
 * - Falls back to local SHA-256-salted credential store in Java Preferences
 * - Session and local history still work; server sync resumes on reconnect
 */
public class AuthService {

    public static final String SERVER_URL = "http://localhost:8080";
    private static final int CONNECT_MS = 4_000;
    private static final int READ_MS = 8_000;

    private static AuthService INSTANCE;

    public static AuthService getInstance() {
        if (INSTANCE == null)
            INSTANCE = new AuthService();
        return INSTANCE;
    }

    private final Preferences root = Preferences.userRoot().node("com/mathengine");
    private final Preferences session = root.node("session");

    private String currentUser = null;
    private String jwtToken = null;
    private boolean serverOnline = false;

    public record AuthResult(boolean success, String message, boolean online) {
    }

    private AuthService() {
        String u = session.get("user", "");
        String t = session.get("jwt", "");
        if (!u.isBlank()) {
            currentUser = u;
            jwtToken = t.isBlank() ? null : t;
        }
    }

    // ── Registration ──────────────────────────────────────────────────────────

    public AuthResult register(String username, String password, String email) {
        username = username.trim().toLowerCase();
        if (!username.matches("[a-z0-9_]{3,32}"))
            return new AuthResult(false, "Username must be 3–32 alphanumeric/underscore chars.", false);
        if (password.length() < 8)
            return new AuthResult(false, "Password must be at least 8 characters.", false);
        try {
            String body = kv("username", username, "password", password,
                    "email", email == null ? "" : email.trim());
            ServerResp r = post("/api/auth/register", body, null);
            if (r.status == 200) {
                persist(extract(r.body, "username"), extract(r.body, "token"));
                serverOnline = true;
                return new AuthResult(true, "Account created and synced with server.", true);
            }
            return new AuthResult(false, or(extract(r.body, "error"), "Registration failed."), true);
        } catch (Exception ignored) {
        }
        return localRegister(username, password, email);
    }

    // ── Login ─────────────────────────────────────────────────────────────────

    public AuthResult login(String username, String password) {
        username = username.trim().toLowerCase();
        try {
            ServerResp r = post("/api/auth/login", kv("username", username, "password", password), null);
            if (r.status == 200) {
                String uname = extract(r.body, "username");
                String token = extract(r.body, "token");
                String theme = extract(r.body, "theme");
                persist(uname, token);
                serverOnline = true;
                if (!theme.isBlank()) {
                    try {
                        ThemeManager.getInstance()
                                .setTheme(ThemeManager.Theme.valueOf(theme.toUpperCase()));
                    } catch (Exception ignored) {
                    }
                }
                return new AuthResult(true, "Welcome back, " + uname + "!  (history synced)", true);
            }
            return new AuthResult(false, or(extract(r.body, "error"), "Login failed."), true);
        } catch (Exception ignored) {
        }
        return localLogin(username, password);
    }

    // ── Logout ────────────────────────────────────────────────────────────────

    public void logout() {
        currentUser = null;
        jwtToken = null;
        serverOnline = false;
        session.remove("user");
        session.remove("jwt");
        try {
            session.flush();
        } catch (Exception ignored) {
        }
    }

    // ── Server sync (fire-and-forget) ─────────────────────────────────────────

    public void pushHistoryAsync(String expression, String result, String operation) {
        if (jwtToken == null)
            return;
        async(() -> {
            try {
                post("/api/history",
                        kv("expression", expression, "result", result, "operation", operation),
                        jwtToken);
            } catch (Exception ignored) {
            }
        });
    }

    public void pushPreferencesAsync(String theme, String fontSize) {
        if (jwtToken == null)
            return;
        async(() -> {
            try {
                put("/api/preferences", kv("theme", theme, "fontSize", fontSize), jwtToken);
            } catch (Exception ignored) {
            }
        });
    }

    public String fetchHistoryJson() {
        if (jwtToken == null)
            return null;
        try {
            ServerResp r = get("/api/history?limit=100", jwtToken);
            return r.status == 200 ? r.body : null;
        } catch (Exception e) {
            return null;
        }
    }

    // ── Accessors ─────────────────────────────────────────────────────────────

    public String getCurrentUser() {
        return currentUser;
    }

    public String getJwtToken() {
        return jwtToken;
    }

    public boolean isLoggedIn() {
        return currentUser != null;
    }

    public boolean isServerOnline() {
        return serverOnline;
    }

    public String getCurrentEmail() {
        if (currentUser == null)
            return "";
        return root.node("users/" + currentUser).get("email", "");
    }

    // ── Local fallback ────────────────────────────────────────────────────────

    private AuthResult localRegister(String username, String password, String email) {
        Preferences n = root.node("users/" + username);
        if (!n.get("hash", "").isEmpty())
            return new AuthResult(false, "Username taken locally.", false);
        try {
            byte[] salt = salt();
            byte[] hash = hash(salt, password);
            n.put("salt", b64(salt));
            n.put("hash", b64(hash));
            n.put("email", email == null ? "" : email.trim());
            n.flush();
            persist(username, null);
            return new AuthResult(true, "Account created locally (server offline).", false);
        } catch (Exception e) {
            return new AuthResult(false, "Failed: " + e.getMessage(), false);
        }
    }

    private AuthResult localLogin(String username, String password) {
        Preferences n = root.node("users/" + username);
        String sB64 = n.get("salt", ""), hB64 = n.get("hash", "");
        if (sB64.isEmpty() || hB64.isEmpty())
            return new AuthResult(false, "No account for " + username + " (server offline).", false);
        try {
            byte[] salt = Base64.getDecoder().decode(sB64);
            byte[] stored = Base64.getDecoder().decode(hB64);
            if (!MessageDigest.isEqual(hash(salt, password), stored))
                return new AuthResult(false, "Incorrect password.", false);
            persist(username, null);
            return new AuthResult(true, "Logged in locally (server offline — history won't sync yet).", false);
        } catch (Exception e) {
            return new AuthResult(false, "Login error: " + e.getMessage(), false);
        }
    }

    private void persist(String user, String token) {
        currentUser = user;
        jwtToken = token;
        session.put("user", user);
        if (token != null)
            session.put("jwt", token);
        else
            session.remove("jwt");
        try {
            session.flush();
        } catch (Exception ignored) {
        }
    }

    // ── HTTP ──────────────────────────────────────────────────────────────────

    private record ServerResp(int status, String body) {
    }

    private ServerResp post(String p, String body, String tok) throws IOException {
        return req("POST", p, body, tok);
    }

    private ServerResp put(String p, String body, String tok) throws IOException {
        return req("PUT", p, body, tok);
    }

    private ServerResp get(String p, String tok) throws IOException {
        return req("GET", p, null, tok);
    }

    private ServerResp req(String method, String path, String body, String token) throws IOException {
        HttpURLConnection c = (HttpURLConnection) URI.create(SERVER_URL + path).toURL().openConnection();
        c.setRequestMethod(method);
        c.setConnectTimeout(CONNECT_MS);
        c.setReadTimeout(READ_MS);
        c.setRequestProperty("Content-Type", "application/json");
        c.setRequestProperty("Accept", "application/json");
        if (token != null)
            c.setRequestProperty("Authorization", "Bearer " + token);
        if (body != null) {
            c.setDoOutput(true);
            try (OutputStream os = c.getOutputStream()) {
                os.write(body.getBytes(StandardCharsets.UTF_8));
            }
        }
        int status = c.getResponseCode();
        InputStream is = status < 400 ? c.getInputStream() : c.getErrorStream();
        String rb = is == null ? "" : new String(is.readAllBytes(), StandardCharsets.UTF_8);
        return new ServerResp(status, rb);
    }

    // ── Tiny JSON/crypto helpers ───────────────────────────────────────────────

    private static String kv(String... pairs) {
        StringBuilder sb = new StringBuilder("{");
        for (int i = 0; i + 1 < pairs.length; i += 2) {
            if (i > 0)
                sb.append(',');
            sb.append('"').append(pairs[i]).append("\":\"")
                    .append(pairs[i + 1].replace("\"", "\\\"")).append('"');
        }
        return sb.append('}').toString();
    }

    private static String extract(String json, String key) {
        String needle = "\"" + key + "\":\"";
        int s = json.indexOf(needle);
        if (s < 0)
            return "";
        s += needle.length();
        int e = json.indexOf('"', s);
        return e < 0 ? "" : json.substring(s, e);
    }

    private static String or(String a, String b) {
        return (a == null || a.isBlank()) ? b : a;
    }

    private static byte[] salt() {
        byte[] b = new byte[16];
        new SecureRandom().nextBytes(b);
        return b;
    }

    private static String b64(byte[] b) {
        return Base64.getEncoder().encodeToString(b);
    }

    private static byte[] hash(byte[] salt, String pw) throws Exception {
        MessageDigest md = MessageDigest.getInstance("SHA-256");
        md.update(salt);
        md.update(pw.getBytes(StandardCharsets.UTF_8));
        return md.digest();
    }

    private static void async(Runnable r) {
        Thread t = new Thread(r);
        t.setDaemon(true);
        t.start();
    }
}