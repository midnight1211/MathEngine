package com.mathengine.server.chatbot;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import jakarta.annotation.PreDestroy;
import org.springframework.stereotype.Service;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

/**
 * ChatbotService
 * ───────────────
 * Server-side counterpart to the desktop's ChatbotBridge
 * (java/com/mathengine/chatbot/ChatbotBridge.java). Same subprocess
 * protocol (chatbot/cli.py, line-delimited JSON), but implemented
 * independently here because the server is its own Maven module with
 * no compile-time dependency on the desktop module's classes — it does,
 * however, already depend on Jackson (via spring-boot-starter-web), so
 * this version uses ObjectMapper instead of a hand-rolled parser.
 *
 * Kept deliberately parallel to ChatbotBridge.classify() in shape and
 * naming so the two stay easy to compare/maintain together; see
 * chatbot/README.md for the shared protocol both implementations speak.
 */
@Service
public class ChatbotService {

    private final ObjectMapper mapper = new ObjectMapper();

    private Process process;
    private BufferedReader stdout;
    private Writer stdin;
    private boolean started = false;
    private String startupError = null;

    public static final class ChatResult {
        public final String reply;
        public final String engineInput;
        public final int precisionFlag;
        public final String intent;
        public final double confidence;
        public final String actionType;
        public final String actionTarget;
        public final JsonNode actionPayload;

        ChatResult(String reply, String engineInput, int precisionFlag,
                   String intent, double confidence,
                   String actionType, String actionTarget, JsonNode actionPayload) {
            this.reply = reply;
            this.engineInput = engineInput;
            this.precisionFlag = precisionFlag;
            this.intent = intent;
            this.confidence = confidence;
            this.actionType = actionType;
            this.actionTarget = actionTarget;
            this.actionPayload = actionPayload;
        }

        public boolean hasComputation() { return engineInput != null && !engineInput.isBlank(); }
        public boolean hasAction() { return actionType != null && !actionType.isBlank(); }
    }

    private synchronized void ensureStarted() {
        if (started) return;
        started = true;
        try {
            Path cliScript = locateCli();
            String python = locatePython();
            ProcessBuilder pb = new ProcessBuilder(python, cliScript.toString());
            pb.directory(cliScript.getParent().toFile());
            process = pb.start();
            stdin = new OutputStreamWriter(process.getOutputStream(), StandardCharsets.UTF_8);
            stdout = new BufferedReader(new InputStreamReader(process.getInputStream(), StandardCharsets.UTF_8));
            startStderrPump(process);
            System.out.println("[ChatbotService] Started NLP subprocess: " + python + " " + cliScript);
        } catch (Exception e) {
            startupError = e.getMessage();
            System.err.println("[ChatbotService] WARNING: could not start NLP subprocess: " + startupError);
        }
    }

    /** See ChatbotBridge.startStderrPump() (desktop counterpart) — without
     * this, a Python startup crash would be invisible with no diagnostic
     * trail, since Java never reads process.getErrorStream() otherwise. */
    private static void startStderrPump(Process proc) {
        Thread t = new Thread(() -> {
            try (BufferedReader err = new BufferedReader(
                    new InputStreamReader(proc.getErrorStream(), StandardCharsets.UTF_8))) {
                String line;
                while ((line = err.readLine()) != null) {
                    System.err.println("[ChatbotService subprocess] " + line);
                }
            } catch (IOException ignored) {
            }
        }, "chatbot-stderr-pump");
        t.setDaemon(true);
        t.start();
    }

    /**
     * Finds chatbot/cli.py. Checked in order: MATHENGINE_CHATBOT_DIR env
     * var override, then a bounded recursive search from the working
     * directory and its ancestors — see ChatbotBridge.locateCli() (desktop
     * counterpart) for the full rationale; kept in sync manually since the
     * server is a separate Maven module with no compile-time dependency
     * on the desktop module's classes.
     */
    private static Path locateCli() throws IOException {
        String override = System.getenv("MATHENGINE_CHATBOT_DIR");
        if (override != null && !override.isBlank()) {
            Path candidate = Paths.get(override, "cli.py");
            if (Files.isRegularFile(candidate)) return candidate;
            System.err.println("[ChatbotService] MATHENGINE_CHATBOT_DIR is set to '" + override +
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
        return "python3";
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

    public synchronized ChatResult classify(String sessionId, String message, String lastResult) {
        return classify(sessionId, message, lastResult, null);
    }

    /**
     * @param workspace Optional (Feature 1: Workspace Sync) — a JSON node
     *                  snapshotting what a client's other views currently
     *                  show, forwarded to the Python subprocess as-is so it
     *                  can resolve references like "this matrix" even on a
     *                  session's first message. Pass null if not tracking.
     */
    public synchronized ChatResult classify(String sessionId, String message, String lastResult, JsonNode workspace) {
        ensureStarted();
        if (process == null || !process.isAlive()) {
            return fallbackResult(message, "unavailable");
        }
        try {
            ObjectNode req = mapper.createObjectNode();
            req.put("session_id", sessionId);
            req.put("message", message);
            if (lastResult != null) req.put("result", lastResult);
            if (workspace != null && !workspace.isNull()) req.set("workspace", workspace);

            stdin.write(mapper.writeValueAsString(req));
            stdin.write("\n");
            stdin.flush();

            String line = stdout.readLine();
            if (line == null) {
                started = false;
                return fallbackResult(message, "died");
            }
            JsonNode node = mapper.readTree(line);
            if (node.has("error")) {
                return new ChatResult("NLP error: " + node.get("error").asText(), message, 1,
                        "fallback.parse_error", 0.0, null, null, null);
            }
            String reply = node.path("reply").asText("");
            String engineInput = node.hasNonNull("engine_input") ? node.get("engine_input").asText() : null;
            int precisionFlag = node.path("precision_flag").asInt(0);
            String intent = node.path("intent").asText("unknown");
            double confidence = node.path("confidence").asDouble(0.0);

            String actionType = null, actionTarget = null;
            JsonNode actionPayload = null;
            JsonNode actionNode = node.get("action");
            if (actionNode != null && !actionNode.isNull()) {
                actionType = actionNode.path("type").asText(null);
                actionTarget = actionNode.path("target").asText(null);
                actionPayload = actionNode.get("payload");
            }
            return new ChatResult(reply, engineInput, precisionFlag, intent, confidence,
                    actionType, actionTarget, actionPayload);
        } catch (IOException e) {
            started = false;
            return fallbackResult(message, "io_error: " + e.getMessage());
        }
    }

    /** Mirrors ChatbotBridge.fallbackResult() (desktop counterpart) — only
     * forward the raw message to the engine when it already looks like a
     * bare expression, rather than guaranteeing a parser error for any
     * natural-language sentence whenever the subprocess is down. */
    private static ChatResult fallbackResult(String message, String reason) {
        if (looksLikePlainExpression(message)) {
            return new ChatResult(
                "NLP subprocess " + reason + " — sending your text straight to the engine.",
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

    @PreDestroy
    public synchronized void shutdown() {
        if (process == null) return;
        try {
            if (stdin != null) {
                stdin.write("__exit__\n");
                stdin.flush();
            }
        } catch (IOException ignored) {
        }
        process.destroy();
        started = false;
    }
}
