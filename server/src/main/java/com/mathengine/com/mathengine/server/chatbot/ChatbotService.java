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
            System.out.println("[ChatbotService] Started NLP subprocess: " + python + " " + cliScript);
        } catch (Exception e) {
            startupError = e.getMessage();
            System.err.println("[ChatbotService] WARNING: could not start NLP subprocess: " + startupError);
        }
    }
 
    /** Server's working directory is server/, so chatbot/ is one level up. */
    private static Path locateCli() throws IOException {
        Path dir = Paths.get("").toAbsolutePath();
        for (int i = 0; i < 6 && dir != null; i++, dir = dir.getParent()) {
            Path candidate = dir.resolve("chatbot").resolve("cli.py");
            if (Files.isRegularFile(candidate)) return candidate;
        }
        throw new IOException("chatbot/cli.py not found near " + Paths.get("").toAbsolutePath());
    }
 
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
            return new ChatResult(
                "NLP subprocess unavailable — sending your text straight to the engine.",
                message, 1, "fallback.no_subprocess", 0.0, null, null, null);
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
                return new ChatResult("NLP subprocess exited unexpectedly.", message, 1,
                        "fallback.subprocess_died", 0.0, null, null, null);
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
            return new ChatResult("NLP subprocess I/O error: " + e.getMessage(), message, 1,
                    "fallback.io_error", 0.0, null, null, null);
        }
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
