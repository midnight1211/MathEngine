package com.mathengine.server.model;

import java.time.Instant;

/**
 * Shared model classes used across the server.
 *
 * Kept in one file for simplicity — split into separate files
 * once the project grows.
 */
public final class Models {

    private Models() {}

    // ── User account ──────────────────────────────────────────

    public static class User {
        public long    id;
        public String  username;
        public String  email;
        public String  passwordHash;   // BCrypt, never sent to clients
        public String  theme;          // "light" | "dark" | "system" | "high-contrast"
        public String  fontSize;       // "normal" | "large" | "x-large"
        public Instant createdAt;

        public User() {}
        public User(long id, String username, String email,
                    String passwordHash, String theme, String fontSize,
                    Instant createdAt) {
            this.id           = id;
            this.username     = username;
            this.email        = email;
            this.passwordHash = passwordHash;
            this.theme        = theme;
            this.fontSize     = fontSize;
            this.createdAt    = createdAt;
        }
    }

    // ── Calculation history entry ─────────────────────────────

    public static class HistoryEntry {
        public long    id;
        public long    userId;         // 0 = guest (not persisted)
        public String  expression;     // raw LaTeX / plain text the user typed
        public String  result;         // result string from C++ engine
        public String  operation;      // "arithmetic" | "integral" | "derivative" | "limit" | "matrix"
        public String  precisionMode;  // "symbolic" | "numerical"
        public Instant createdAt;

        public HistoryEntry() {}
        public HistoryEntry(long id, long userId, String expression, String result,
                            String operation, String precisionMode, Instant createdAt) {
            this.id            = id;
            this.userId        = userId;
            this.expression    = expression;
            this.result        = result;
            this.operation     = operation;
            this.precisionMode = precisionMode;
            this.createdAt     = createdAt;
        }
    }

    // ── Preferences ───────────────────────────────────────────

    public static class Preferences {
        public String theme;
        public String fontSize;

        public Preferences() {}
        public Preferences(String theme, String fontSize) {
            this.theme    = theme;
            this.fontSize = fontSize;
        }
    }

    // ── REST request / response bodies ───────────────────────

    /** POST /api/auth/register */
    public static class RegisterRequest {
        public String username;
        public String email;
        public String password;
    }

    /** POST /api/auth/login */
    public static class LoginRequest {
        public String username;
        public String password;
    }

    /** Response body for both register and login */
    public static class AuthResponse {
        public String token;
        public String username;
        public String theme;
        public String fontSize;

        public AuthResponse(String token, String username,
                            String theme, String fontSize) {
            this.token    = token;
            this.username = username;
            this.theme    = theme;
            this.fontSize = fontSize;
        }
    }

    /** POST /api/compute */
    public static class ComputeRequest {
        public String expression;
        public int    precisionFlag;   // 0 = symbolic, 1 = numerical
        public String operation;       // for history tagging
    }

    /** Response body for /api/compute */
    public static class ComputeResponse {
        public String  result;
        public String  expression;
        public String  operation;
        public String  precisionMode;
        public boolean ok;
        public String  error;

        public static ComputeResponse success(String expression, String result,
                                              String operation, String mode) {
            ComputeResponse r = new ComputeResponse();
            r.ok            = true;
            r.expression    = expression;
            r.result        = result;
            r.operation     = operation;
            r.precisionMode = mode;
            return r;
        }

        public static ComputeResponse failure(String expression, String error) {
            ComputeResponse r = new ComputeResponse();
            r.ok         = false;
            r.expression = expression;
            r.error      = error;
            return r;
        }
    }


    public static class ChatRequest {
	    public String sessionId;
	    public String message;
	    public String lastResult;
	    public com.fasterxml.jackson.databind.JsonNode workspace;
    }


    public static class ChatResponse {
	    public String reply;
	    public String engineInput;
	    public String result;
	    public String intent;
	    public double confidence;
	    public boolean ok;
	    public String error;
	    public String actionType;
	    public String actionTarget;
	    public com.fasterxml.jackson.databind.JsonNode actionPayload;

	    public static ChatResponse of(String reply, String engineInput, String result, String intent, double confidence) {
		    ChatResponse r = new ChatResponse();
		    r.ok = true;
		    r.reply = reply;
		    r.engineInput = engineInput;
		    r.result = result;
		    r.intent = intent;
		    r.confidence = confidence;
		    return r;
	    }

	    public static ChatResponse action(String reply, String intent, double confidence, String actionType, String actionTarget, com.fasterxml.jackson.databind.JsonNode actionPayload) {
		    ChatResponse r = new ChatResponse();
		    r.ok = true;
		    r.reply = reply;
		    r.intent = intent;
		    r.confidence = confidence;
		    r.actionType = actionType;
		    r.actionTarget = actionTarget;
		    r.actionPayload = actionPayload;
		    return r;
	    }

	    public static ChatResponse failure(String reply, String error) {
		    ChatResponse r = new ChatResponse();
		    r.ok = false;
		    r.reply = reply;
		    r.error = error;
		    return r;
	    }
    }

    /** POST /api/export */
    public static class ExportRequest {
        public String expression;
        public String result;
        /** "latex" | "unicode" | "plaintext" | "mathml" */
        public String format;
    }

    /** Response body for /api/export */
    public static class ExportResponse {
        public String format;
        public String content;

        public ExportResponse(String format, String content) {
            this.format  = format;
            this.content = content;
        }
    }
}
