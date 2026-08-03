package com.mathengine.server.api;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * DocsController
 * ───────────────
 * Feature 4 (Localized Fallback Knowledge Base): GET /api/docs/search
 * serves the same static, hand-curated definitions from
 * docs_kb/knowledge_base.json that chatbot/knowledge.py reads directly
 * off disk for the desktop path. This endpoint exists for parity with
 * clients that aren't the Python subprocess (a web frontend, another
 * service, or a future non-desktop chatbot deployment) — every entry
 * still names the exact engine op and payload shape it corresponds to,
 * so explanations from either path stay tethered to what the engine
 * actually implements rather than to general math knowledge.
 *
 * GET /api/docs/search?query=taylor_series
 * GET /api/docs/search?query=derivative&limit=5
 */
@RestController
@RequestMapping("/api/docs")
public class DocsController {

    private final ObjectMapper mapper = new ObjectMapper();
    private List<Map<String, Object>> entries;

    @SuppressWarnings("unchecked")
    private synchronized List<Map<String, Object>> loadEntries() {
        if (entries != null) return entries;
        try {
            Path path = locateKb();
            JsonNode root = mapper.readTree(path.toFile());
            List<Map<String, Object>> out = new ArrayList<>();
            for (JsonNode entry : root.get("entries")) {
                out.add(mapper.convertValue(entry, Map.class));
            }
            entries = out;
        } catch (Exception e) {
            System.err.println("[DocsController] Failed to load knowledge base: " + e.getMessage());
            entries = List.of();
        }
        return entries;
    }

    /** Server's working directory is server/, so docs_kb/ is one level up
     * — the same locate-relative-to-root pattern ChatbotService uses for
     * chatbot/cli.py. */
    private static Path locateKb() throws IOException {
        Path dir = Paths.get("").toAbsolutePath();
        for (int i = 0; i < 6 && dir != null; i++, dir = dir.getParent()) {
            Path candidate = dir.resolve("docs_kb").resolve("knowledge_base.json");
            if (Files.isRegularFile(candidate)) return candidate;
        }
        throw new IOException("docs_kb/knowledge_base.json not found near " + Paths.get("").toAbsolutePath());
    }

    @GetMapping("/search")
    public ResponseEntity<?> search(@RequestParam String query,
                                     @RequestParam(defaultValue = "3") int limit) {
        if (query == null || query.isBlank()) {
            return ResponseEntity.badRequest().body(Map.of("error", "query is required"));
        }
        String q = query.toLowerCase().trim();
        List<Map<String, Object>> all = loadEntries();

        List<Map<String, Object>> exact = all.stream()
            .filter(e -> q.equals(e.get("id")) || q.equalsIgnoreCase(String.valueOf(e.get("title"))))
            .toList();
        if (!exact.isEmpty()) {
            return ResponseEntity.ok(Map.of("query", query, "results", exact.subList(0, Math.min(limit, exact.size()))));
        }

        List<Map<String, Object>> scored = new ArrayList<>();
        for (Map<String, Object> e : all) {
            String title = String.valueOf(e.get("title")).toLowerCase();
            String definition = String.valueOf(e.get("definition")).toLowerCase();
            int score = 0;
            if (title.contains(q)) score += 3;
            if (definition.contains(q)) score += 2;
            for (String word : q.split("\\s+")) {
                if (title.contains(word)) score += 1;
            }
            if (score > 0) scored.add(Map.of("score", score, "entry", e));
        }
        scored.sort((a, b) -> (int) b.get("score") - (int) a.get("score"));

        List<Map<String, Object>> results = scored.stream()
            .map(m -> (Map<String, Object>) m.get("entry"))
            .limit(limit)
            .toList();

        return ResponseEntity.ok(Map.of("query", query, "results", results));
    }
}
