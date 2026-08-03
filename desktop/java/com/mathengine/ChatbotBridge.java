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
 * --------------
 * Java-side counterpart to MathBridge: instead of wrapping the C++ engine
 * over JNI, this wraps the Python NLP chatbot (chatbot/cli.py) over a
 * persistent subprocess speaking line-delimited JSON on stdin/stdout.
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
		public final String engineInput;
		public final int precisionFlag;
		public final String intent;
		public final double confidence;
		public final String actionType;
		public final String actionTarget;
		public final String actionPayloadJson;

		ChatResult(String reply, String engineInput, int precisionFlag, String intent, double confidence, String actionType, String actionTarget,
				String actionPayloadJson) {
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

	// -- Startup ------------------------------------------------------------------------
	
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
			System.out.println("[Chatbot] Started NLP subprocess: " + python + " " + cliScript);
		} catch (Exception e) {
			startupError = e.getMessage();
			System.err.println("[Chatbot] WARNING: could not start NLP subprocess = " + 
					"falling back to raw pass-through mode. Reason: " + startupError);
		}
	}

	/** Walks up from the working directory looking for chatbot/cli.py */
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

	public Optional<String> getStartupError() {
		return Optional.ofNullable(startupError);
	}

	// ── Public API ────────────────────────────────────────────────────────────

	public synchronized ChatResult classify(String sessionId, String message, String lastResult, String workspaceJson) {
		ensureStarted();
		if (process == null || !process.isAlive()) {
			return new ChatResult(
					"[NLP subprocess unavailable - sending your text straight to the engine]",
					message, 1, "fallback.no_subprocess", 0.0, null, null, null);
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
				req.append(",\"workspace\":").append(workspaceJson);
			}
			req.append('}');

			stdin.write(req.toString());
			stdin.write("\n");
			stdin.flush();

			String line = stdout.readLine();
			if (line == null) {
				started = false;
				return new ChatResult("[NLP subprocess exited unexpectedly]", message, 1,
                        "fallback.subprocess_died", 0.0, null, null, null);
            }
            return parseResponse(line, message);
        } catch (IOException e) {
            started = false;
            return new ChatResult("[NLP subprocess I/O error: " + e.getMessage() + "]",
                    message, 1, "fallback.io_error", 0.0, null, null, null);
        }
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
 
    public synchronized void shutdown() {
        if (process == null) return;
        try {
            if (stdin != null) {
                stdin.write("__exit__\n");
                stdin.flush();
            }
        } catch (IOException ignored) {}
        process.destroy();
        started = false;
    }
 
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
            if (c == '{') {
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
