package com.mathengine.chatbot;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.Map;
import java.util.Optional;

/**
 * ChatbotBridge
 * ──────────────
 * Java-side counterpart to MathBridge: instead of wrapping the C++ engine
 * over JNI, this wraps the Python NLP chatbot (chatbot/cli.py) over a
 * persistent subprocess speaking line-delimited JSON on stdin/stdout.
 *
 * ┌─────────────────────────────────────────────────────────────┐
 * │  ChatbotPanel  ──→  ChatbotBridge.classify(sessionId, text)  │
 * │                            │                                 │
 * │                     stdin/stdout JSONL                       │
 * │                            │                                 │
 * │                  python chatbot/cli.py                       │
 * └─────────────────────────────────────────────────────────────┘
 *
 * classify() only returns the *engine command* the message maps to
 * (chatbot/nlp_engine.py never touches the C++ engine itself). The
 * caller — ChatbotPanel, or ChatController on the server side — is
 * responsible for feeding ChatResult.engineInput into MathBridge.compute()
 * / ServerEngineService.compute() exactly like a typed-in expression
 * would be, so the chatbot rides the same execution path (and the same
 * history/auth plumbing) as manual input.
 *
 * The subprocess is started lazily on first use and kept alive for the
 * app's lifetime — one process per app instance, not per message —
 * because process startup dominates latency for a scripting-language
 * subprocess like this.
 */
public final class ChatbotBridge {

    private static ChatbotBridge INSTANCE;

    public static synchronized ChatbotBridge getInstance() {
        if (INSTANCE == null) INSTANCE = new ChatbotBridge();
        return INSTANCE;
    }

    /** Result of classifying one message. */
    public static final class ChatResult {
        public final String reply;
        public final String engineInput;   // null → nothing to compute (small talk)
        public final int precisionFlag;    // 0 = symbolic, 1 = numerical
        public final String intent;
        public final double confidence;
        public final String actionType;    // e.g. "SWITCH_TAB" — null if no action
        public final String actionTarget;  // e.g. "Graph"
        public final String actionPayloadJson; // raw JSON object string, caller parses as needed

        ChatResult(String reply, String engineInput, int precisionFlag,
                   String intent, double confidence,
                   String actionType, String actionTarget, String actionPayloadJson) {
            this.reply = reply;
            this.engineInput = engineInput;
            this.precisionFlag = precisionFlag;
            this.intent = intent;
            this.confidence = confidence;
            this.actionType = actionType;
            this.actionTarget = actionTarget;
            this.actionPayloadJson = actionPayloadJson;
        }

        public boolean hasComputation() { return engineInput != null && !engineInput.isBlank(); }
        public boolean hasAction() { return actionType != null && !actionType.isBlank(); }
    }

    private Process process;
    private BufferedReader stdout;
    private Writer stdin;
    private boolean started = false;
    private String startupError = null;

    private ChatbotBridge() {}

    // ── Startup ──────────────────────────────────────────────────────────────

    private synchronized void ensureStarted() {
        if (started) return;
        started = true;
        try {
            Path cliScript = locateCli();
            String python = locatePython();
            ProcessBuilder pb = new ProcessBuilder(python, cliScript.toString());
            pb.directory(cliScript.getParent().toFile());
            pb.redirectErrorStream(false);
            process = pb.start();
            stdin = new OutputStreamWriter(process.getOutputStream(), StandardCharsets.UTF_8);
            stdout = new BufferedReader(new InputStreamReader(process.getInputStream(), StandardCharsets.UTF_8));
            startStderrPump(process);
            System.out.println("[Chatbot] Started NLP subprocess: " + python + " " + cliScript);
        } catch (Exception e) {
            startupError = e.getMessage();
            System.err.println("[Chatbot] WARNING: could not start NLP subprocess — " +
                    "falling back to raw pass-through mode. Reason: " + startupError);
        }
    }

    /** Without this, anything the Python subprocess writes to stderr
     * (including exception tracebacks) is silently discarded — Java never
     * reads process.getErrorStream() otherwise, so a startup crash would
     * be invisible with no diagnostic trail at all. Runs on a daemon
     * thread so it never blocks shutdown. */
    private static void startStderrPump(Process proc) {
        Thread t = new Thread(() -> {
            try (BufferedReader err = new BufferedReader(
                    new InputStreamReader(proc.getErrorStream(), StandardCharsets.UTF_8))) {
                String line;
                while ((line = err.readLine()) != null) {
                    System.err.println("[Chatbot subprocess] " + line);
                }
            } catch (IOException ignored) {
                // Process died / stream closed — nothing more to read.
            }
        }, "chatbot-stderr-pump");
        t.setDaemon(true);
        t.start();
    }

    /**
     * Finds chatbot/cli.py. Checked in order:
     *   1. MATHENGINE_CHATBOT_DIR env var, if set — the most reliable option
     *      when the repo doesn't match the layout this was originally built
     *      against (Capstone-main/chatbot/cli.py at the repo root).
     *   2. A bounded recursive search from the working directory and each
     *      of its ancestors (up to 6 levels up, 4 levels down each) for a
     *      "cli.py" whose parent directory is named "chatbot" — handles
     *      nested layouts like "desktop/python/chatbot/cli.py" without
     *      hardcoding the exact path.
     */
    private static Path locateCli() throws IOException {
        String override = System.getenv("MATHENGINE_CHATBOT_DIR");
        if (override != null && !override.isBlank()) {
            Path candidate = Paths.get(override, "cli.py");
            if (Files.isRegularFile(candidate)) return candidate;
            System.err.println("[Chatbot] MATHENGINE_CHATBOT_DIR is set to '" + override +
                "' but no cli.py was found there.");
        }

        Path start = Paths.get("").toAbsolutePath();
        Path dir = start;
        for (int i = 0; i < 6 && dir != null; i++, dir = dir.getParent()) {
            Path found = findCliUnder(dir, 4);
            if (found != null) return found;
        }
        throw new IOException(
            "chatbot/cli.py not found near " + start + " (searched up 6 levels, 4 levels deep each). " +
            "If your repo doesn't put chatbot/ near the project root, set the MATHENGINE_CHATBOT_DIR " +
            "environment variable to the folder containing cli.py.");
    }

    /** Bounded depth-first search for <dir>/.../chatbot/cli.py, skipping
     * directories that are never worth descending into. */
    private static Path findCliUnder(Path dir, int maxDepth) {
        if (maxDepth < 0 || dir == null || !Files.isDirectory(dir)) return null;
        String name = dir.getFileName() != null ? dir.getFileName().toString() : "";
        if (SKIP_DIRS.contains(name)) return null;

        Path direct = dir.resolve("chatbot").resolve("cli.py");
        if (Files.isRegularFile(direct)) return direct;

        if (maxDepth == 0) return null;
        try (var stream = Files.list(dir)) {
            for (Path child : (Iterable<Path>) stream.filter(Files::isDirectory)::iterator) {
                Path found = findCliUnder(child, maxDepth - 1);
                if (found != null) return found;
            }
        } catch (IOException ignored) {
        }
        return null;
    }

    private static final java.util.Set<String> SKIP_DIRS = java.util.Set.of(
        ".git", "target", "build", "node_modules", "__pycache__", ".idea", ".vscode");

    private static String locatePython() {
        String override = System.getenv("MATHENGINE_PYTHON");
        if (override != null && !override.isBlank()) return override;
        for (String candidate : new String[]{"python3", "python"}) {
            if (isOnPath(candidate)) return candidate;
        }
        return "python3"; // best-effort default; ensureStarted() will report the failure
    }

    private static boolean isOnPath(String exe) {
        try {
            Process p = new ProcessBuilder(exe, "--version").redirectErrorStream(true).start();
            return p.waitFor() == 0;
        } catch (Exception e) {
            return false;
        }
    }

    public boolean isAvailable() {
        ensureStarted();
        return process != null && process.isAlive();
    }

    public Optional<String> getStartupError() {
        return Optional.ofNullable(startupError);
    }

    // ── Public API ────────────────────────────────────────────────────────────

    /**
     * Sends one message to the chatbot and blocks for its classification.
     * Thread-safe (single subprocess, single request in flight at a time) —
     * call from a background thread, same as MathBridge.compute().
     *
     * @param sessionId    Distinguishes conversations (e.g. per logged-in
     *                     user, or a fixed "desktop" id for the single-window
     *                     app).
     * @param message      Raw text the user typed into the chat panel.
     * @param lastResult   Optional — the engine's result from the previous
     *                     turn, so the chatbot can (in future) reference it.
     *                     Pass null if not tracking this.
     * @param workspaceJson Optional — a raw JSON object string snapshotting
     *                     what the desktop UI's other tabs currently show
     *                     (Feature 1: Workspace Sync), e.g.
     *                     {"activeTab":"Compute","lastExpression":"[[1,2],[2,4]]"}.
     *                     Pass null if there's nothing to report.
     */
    public synchronized ChatResult classify(String sessionId, String message, String lastResult,
                                             String workspaceJson) {
        ensureStarted();
        if (process == null || !process.isAlive()) {
            return fallbackResult(message, "unavailable");
        }
        try {
            StringBuilder req = new StringBuilder();
            req.append('{');
            req.append("\"session_id\":").append(Json.quote(sessionId)).append(',');
            req.append("\"message\":").append(Json.quote(message));
            if (lastResult != null) {
                req.append(",\"result\":").append(Json.quote(lastResult));
            }
            if (workspaceJson != null && !workspaceJson.isBlank()) {
                // Raw JSON object, embedded as-is (not string-quoted).
                req.append(",\"workspace\":").append(workspaceJson);
            }
            req.append('}');

            stdin.write(req.toString());
            stdin.write("\n");
            stdin.flush();

            String line = stdout.readLine();
            if (line == null) {
                started = false; // process died — allow a restart on next call
                return fallbackResult(message, "died");
            }
            return parseResponse(line, message);
        } catch (IOException e) {
            started = false;
            return fallbackResult(message, "io_error: " + e.getMessage());
        }
    }

    /**
     * Used only when the Python subprocess is down (never started, crashed,
     * or an I/O error). Rather than always forwarding the raw message to
     * MathBridge.compute() — which reliably fails loudly for any natural-
     * language sentence, not just malformed math — only do so when the text
     * already looks like a bare expression the engine could plausibly parse
     * on its own (mirrors chatbot/intents.py's looks_like_plain_expression,
     * kept in sync manually since Java has no dependency on the Python
     * source). Otherwise just report the outage.
     */
    private static ChatResult fallbackResult(String message, String reason) {
        if (looksLikePlainExpression(message)) {
            return new ChatResult(
                "[NLP subprocess " + reason + " — sending your text straight to the engine]",
                message, 1, "fallback.no_subprocess", 0.0, null, null, null);
        }
        return new ChatResult(
            "The NLP subprocess is " + reason + ", so I can't understand plain-English requests "
            + "right now — try typing a plain expression instead (e.g. \"2^8 + sqrt(16)\").",
            null, 1, "fallback.no_subprocess", 0.0, null, null, null);
    }

    private static final java.util.regex.Pattern PLAIN_EXPR =
        java.util.regex.Pattern.compile("^[0-9a-zA-Z_+\\-*/^().,\\s!%]+$");
    private static final java.util.regex.Pattern HAS_OP_OR_DIGIT =
        java.util.regex.Pattern.compile("[\\d+\\-*/^()!%]");

    private static boolean looksLikePlainExpression(String text) {
        if (text == null) return false;
        String t = text.trim();
        if (t.isEmpty() || !PLAIN_EXPR.matcher(t).matches()) return false;
        if (HAS_OP_OR_DIGIT.matcher(t).find()) return true;
        return t.split("\\s+").length <= 2;
    }

    /** Overload for callers with no workspace state to report. */
    public ChatResult classify(String sessionId, String message, String lastResult) {
        return classify(sessionId, message, lastResult, null);
    }

    private static ChatResult parseResponse(String line, String fallbackMessage) {
        Map<String, Object> obj = Json.parseObject(line);
        if (obj.containsKey("error")) {
            return new ChatResult("[NLP error: " + obj.get("error") + "]",
                    fallbackMessage, 1, "fallback.parse_error", 0.0, null, null, null);
        }
        String reply = String.valueOf(obj.getOrDefault("reply", ""));
        Object engineInputObj = obj.get("engine_input");
        String engineInput = engineInputObj == null ? null : String.valueOf(engineInputObj);
        int precisionFlag = (int) Json.asNumber(obj.getOrDefault("precision_flag", 0));
        String intent = String.valueOf(obj.getOrDefault("intent", "unknown"));
        double confidence = Json.asNumber(obj.getOrDefault("confidence", 0.0));

        // Feature 2: action is a nested object; Json.parseValue returns
        // nested objects as a raw substring (see parseValue's "{" branch),
        // so we re-parse that substring for its type/target/payload fields.
        String actionType = null, actionTarget = null, actionPayloadJson = null;
        Object actionObj = obj.get("action");
        if (actionObj instanceof String && !((String) actionObj).isBlank()) {
            Map<String, Object> action = Json.parseObject((String) actionObj);
            Object t = action.get("type");
            Object tgt = action.get("target");
            Object payload = action.get("payload");
            actionType = t == null ? null : String.valueOf(t);
            actionTarget = tgt == null ? null : String.valueOf(tgt);
            actionPayloadJson = payload == null ? null : String.valueOf(payload);
        }
        return new ChatResult(reply, engineInput, precisionFlag, intent, confidence,
                actionType, actionTarget, actionPayloadJson);
    }

    /** Cleanly stops the subprocess (call on app shutdown, e.g. from Launcher). */
    public synchronized void shutdown() {
        if (process == null) return;
        try {
            if (stdin != null) {
                stdin.write("__exit__\n");
                stdin.flush();
            }
        } catch (IOException ignored) {
            // Process may already be gone — fall through to destroy().
        }
        process.destroy();
        started = false;
    }

    // ── Minimal hand-rolled JSON (no dependency added for the desktop module) ──

    /**
     * A tiny JSON reader/writer sufficient for the chatbot's flat
     * request/response objects (string/number/bool values, no nesting).
     * The desktop module has no JSON library on its classpath (only the
     * Spring server does, via Jackson), so this avoids adding one just
     * for a handful of fields.
     */
    static final class Json {

        static String quote(String s) {
            if (s == null) return "null";
            StringBuilder out = new StringBuilder("\"");
            for (int i = 0; i < s.length(); i++) {
                char c = s.charAt(i);
                switch (c) {
                    case '"': out.append("\\\""); break;
                    case '\\': out.append("\\\\"); break;
                    case '\n': out.append("\\n"); break;
                    case '\r': out.append("\\r"); break;
                    case '\t': out.append("\\t"); break;
                    default:
                        if (c < 0x20) out.append(String.format("\\u%04x", (int) c));
                        else out.append(c);
                }
            }
            out.append('"');
            return out.toString();
        }

        static double asNumber(Object o) {
            if (o instanceof Number) return ((Number) o).doubleValue();
            try { return Double.parseDouble(String.valueOf(o)); } catch (Exception e) { return 0.0; }
        }

        /** Parses a single flat JSON object into a Map<String,Object>
         * (String, Double, Boolean, or null values). Good enough for the
         * chatbot's response shape; not a general-purpose parser. */
        static Map<String, Object> parseObject(String s) {
            Map<String, Object> map = new HashMap<>();
            int i = skipWs(s, 0);
            if (i >= s.length() || s.charAt(i) != '{') return map;
            i++;
            i = skipWs(s, i);
            while (i < s.length() && s.charAt(i) != '}') {
                int[] keyEnd = new int[1];
                String key = parseString(s, i, keyEnd);
                i = skipWs(s, keyEnd[0]);
                if (i < s.length() && s.charAt(i) == ':') i++;
                i = skipWs(s, i);
                int[] valEnd = new int[1];
                Object val = parseValue(s, i, valEnd);
                map.put(key, val);
                i = skipWs(s, valEnd[0]);
                if (i < s.length() && s.charAt(i) == ',') { i++; i = skipWs(s, i); }
            }
            return map;
        }

        private static int skipWs(String s, int i) {
            while (i < s.length() && Character.isWhitespace(s.charAt(i))) i++;
            return i;
        }

        private static Object parseValue(String s, int i, int[] endOut) {
            if (i >= s.length()) { endOut[0] = i; return null; }
            char c = s.charAt(i);
            if (c == '"') return parseString(s, i, endOut);
            if (c == '{') { // nested object — return raw substring, unused by ChatbotBridge today
                int depth = 0, start = i;
                for (; i < s.length(); i++) {
                    if (s.charAt(i) == '{') depth++;
                    else if (s.charAt(i) == '}') { depth--; if (depth == 0) { i++; break; } }
                }
                endOut[0] = i;
                return s.substring(start, i);
            }
            if (s.startsWith("null", i)) { endOut[0] = i + 4; return null; }
            if (s.startsWith("true", i)) { endOut[0] = i + 4; return Boolean.TRUE; }
            if (s.startsWith("false", i)) { endOut[0] = i + 5; return Boolean.FALSE; }
            int start = i;
            while (i < s.length() && "+-0123456789.eE".indexOf(s.charAt(i)) >= 0) i++;
            endOut[0] = i;
            try { return Double.parseDouble(s.substring(start, i)); } catch (Exception e) { return 0.0; }
        }

        private static String parseString(String s, int i, int[] endOut) {
            if (i >= s.length() || s.charAt(i) != '"') { endOut[0] = i; return ""; }
            i++;
            StringBuilder sb = new StringBuilder();
            while (i < s.length() && s.charAt(i) != '"') {
                char c = s.charAt(i);
                if (c == '\\' && i + 1 < s.length()) {
                    char n = s.charAt(i + 1);
                    switch (n) {
                        case 'n': sb.append('\n'); break;
                        case 'r': sb.append('\r'); break;
                        case 't': sb.append('\t'); break;
                        case '"': sb.append('"'); break;
                        case '\\': sb.append('\\'); break;
                        case '/': sb.append('/'); break;
                        case 'u':
                            if (i + 5 < s.length()) {
                                sb.append((char) Integer.parseInt(s.substring(i + 2, i + 6), 16));
                                i += 4;
                            }
                            break;
                        default: sb.append(n);
                    }
                    i += 2;
                } else {
                    sb.append(c);
                    i++;
                }
            }
            endOut[0] = i + 1;
            return sb.toString();
        }
    }
}
