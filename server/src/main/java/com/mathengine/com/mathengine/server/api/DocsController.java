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

    /**
     * Finds docs_kb/knowledge_base.json. Checked in order:
     * MATHENGINE_DOCS_KB_DIR env var override, then a bounded recursive
     * search — same rationale/pattern as ChatbotService.locateCli().
     */
    private static Path locateKb() throws IOException {
        String override = System.getenv("MATHENGINE_DOCS_KB_DIR");
        if (override != null && !override.isBlank()) {
            Path candidate = Paths.get(override, "knowledge_base.json");
            if (Files.isRegularFile(candidate)) return candidate;
            System.err.println("[DocsController] MATHENGINE_DOCS_KB_DIR is set to '" + override +
                "' but no knowledge_base.json was found there.");
        }

        Path start = Paths.get("").toAbsolutePath();
        Path dir = start;
        for (int i = 0; i < 6 && dir != null; i++, dir = dir.getParent()) {
            Path found = findKbUnder(dir, 4);
            if (found != null) return found;
        }
        throw new IOException(
            "docs_kb/knowledge_base.json not found near " + start + " (searched up 6 levels, 4 levels deep each). " +
            "If your repo doesn't put docs_kb/ near the project root, set the MATHENGINE_DOCS_KB_DIR " +
            "environment variable to the folder containing knowledge_base.json.");
    }

    private static Path findKbUnder(Path dir, int maxDepth) {
        if (maxDepth < 0 || dir == null || !Files.isDirectory(dir)) return null;
        String name = dir.getFileName() != null ? dir.getFileName().toString() : "";
        if (SKIP_DIRS.contains(name)) return null;

        Path direct = dir.resolve("docs_kb").resolve("knowledge_base.json");
        if (Files.isRegularFile(direct)) return direct;

        if (maxDepth == 0) return null;
        try (var stream = Files.list(dir)) {
            for (Path child : (Iterable<Path>) stream.filter(Files::isDirectory)::iterator) {
                Path found = findKbUnder(child, maxDepth - 1);
                if (found != null) return found;
            }
        } catch (IOException ignored) {
        }
        return null;
    }

    private static final java.util.Set<String> SKIP_DIRS = java.util.Set.of(
        ".git", "target", "build", "node_modules", "__pycache__", ".idea", ".vscode");

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
