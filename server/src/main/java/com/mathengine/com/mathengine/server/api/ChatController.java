package com.mathengine.server.api;

import com.mathengine.server.chatbot.ChatbotService;
import com.mathengine.server.engine.ServerEngineService;
import com.mathengine.server.model.Models;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

/**
 * ChatController
 * ───────────────
 * POST /api/chat
 *
 * ChatbotService and Models.ChatRequest/ChatResponse were already fully
 * built — a server-side counterpart to the desktop's ChatbotBridge, with
 * response DTOs shaped exactly for this — but nothing ever exposed them
 * over HTTP, so a non-desktop client (the mobile web UI, a future
 * integration) had no way to reach the NLP subprocess at all. This
 * controller is that missing wiring.
 *
 * Unlike the desktop's two-step async flow (ChatbotPanel classifies via
 * ChatbotBridge, then separately calls MathBridge.compute() once the
 * classification is back — see ChatbotPanel.java), this does both steps
 * in one request: a web/mobile client gets the classification *and* the
 * computed result in a single POST /api/chat round trip, since it has no
 * local engine binding of its own to call a second time. An action-only
 * reply (e.g. "plot sin(x)", "clear the graph") has no engine_input to
 * compute — those come back exactly as ChatbotService produced them, and
 * it's the client's job to actually apply the action.
 *
 * Public, like /api/compute — guests can talk to the chatbot without
 * signing in (see SecurityConfig). History isn't saved for chat turns
 * yet (unlike /api/compute); DatabaseManager has no chat-history schema
 * to write to, so this intentionally doesn't guess one.
 */
@RestController
@RequestMapping("/api")
public class ChatController {

    private final ChatbotService chatbot;
    private final ServerEngineService engine;

    public ChatController(ChatbotService chatbot, ServerEngineService engine) {
        this.chatbot = chatbot;
        this.engine  = engine;
    }

    @PostMapping("/chat")
    public ResponseEntity<?> chat(@RequestBody Models.ChatRequest req) {
        if (req.message == null || req.message.isBlank()) {
            return ResponseEntity.badRequest().body(
                Models.ChatResponse.failure("", "message is required"));
        }

        String sessionId = (req.sessionId != null && !req.sessionId.isBlank()) ? req.sessionId : "default";

        ChatbotService.ChatResult result =
            chatbot.classify(sessionId, req.message, req.lastResult, req.workspace);

        if (result.hasAction()) {
            return ResponseEntity.ok(Models.ChatResponse.action(
                result.reply, result.intent, result.confidence,
                result.actionType, result.actionTarget, result.actionPayload));
        }

        if (!result.hasComputation()) {
            // Smalltalk, a knowledge-base answer, a low-confidence fallback
            // hint, ... — nothing for the engine to evaluate.
            return ResponseEntity.ok(Models.ChatResponse.of(
                result.reply, result.engineInput, null, result.intent, result.confidence));
        }

        try {
            String computed = engine.compute(result.engineInput, result.precisionFlag);
            return ResponseEntity.ok(Models.ChatResponse.of(
                result.reply, result.engineInput, computed, result.intent, result.confidence));
        } catch (Exception e) {
            // The chatbot understood the request fine; the engine just
            // couldn't evaluate the resulting command (e.g. a syntax
            // edge case the local pre-flight check in validate.py
            // missed). Still return the classification — the client can
            // show the NLP-level reply — with the engine error attached
            // separately rather than masking a real classification
            // behind a generic 500.
            Models.ChatResponse r = Models.ChatResponse.of(
                result.reply, result.engineInput, null, result.intent, result.confidence);
            r.error = e.getMessage();
            return ResponseEntity.ok(r);
        }
    }

    /** GET /api/chat/status — health check, mirrors GET /api/engine/status
     * (ComputeController) — lets a client show "chatbot unavailable"
     * instead of silently getting fallback.no_subprocess replies. */
    @GetMapping("/chat/status")
    public ResponseEntity<?> status() {
        return ResponseEntity.ok(java.util.Map.of("available", chatbot.isAvailable()));
    }
}
