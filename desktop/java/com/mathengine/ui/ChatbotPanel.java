package com.mathengine.ui;

import com.mathengine.chatbot.ChatbotBridge;
import com.mathengine.chatbot.ChatbotBridge.ChatResult;
import com.mathengine.jni.MathBridge;
import com.mathengine.model.PrecisionMode;
import javafx.application.Platform;
import javafx.concurrent.Task;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.control.Button;
import javafx.scene.control.ScrollPane;
import javafx.scene.control.TextField;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.VBox;
import javafx.scene.text.Text;
import javafx.scene.text.TextFlow;

import java.util.UUID;
import java.util.function.BiConsumer;
import java.util.function.Supplier;

/**
 * ChatbotPanel
 * ─────────────
 * Free-text "ask the math engine" tab. Wraps ChatbotBridge (the Python NLP
 * subprocess) + MathBridge (the C++ engine) so the whole round trip —
 * classify → compute → display — happens without the user ever touching
 * InputPanel's structured Symbolic/Numerical + operation-badge UI.
 *
 * Flow for one message:
 *   1. User types a sentence and hits Enter / Send.
 *   2. ChatbotBridge.classify() turns it into an engine command string
 *      (e.g. "derivative of x^2" → "diff[x^2,x]") on a background thread —
 *      along with a snapshot of what the Compute tab currently shows
 *      (Feature 1: Workspace Sync, via {@link #setWorkspaceSupplier}), so
 *      "why is this matrix singular?" can work as the very first message.
 *   3. If the classification carries a computation, MathBridge.compute()
 *      runs it exactly as InputPanel would — same JNI call, same engine.
 *      If it instead carries a UI action (Feature 2: Chat-Driven Actions,
 *      e.g. "plot sin(x) from -10 to 10"), that's dispatched to whatever
 *      listener was registered via {@link #setOnAction} instead of being
 *      computed — MainLayout wires this to switch tabs and populate
 *      GraphPanel.
 *   4. The reply + result are appended to the transcript, and (if a
 *      listener is attached via setOnComputed) reported upward so
 *      MainLayout can log it to OperationPanel's history alongside
 *      manually-typed computations.
 */
public class ChatbotPanel extends VBox {

	private final VBox transcript = new VBox(10);
	private final ScrollPane scrollPane = new ScrollPane(transcript);
	private final TextField inputField = new TextField();
	private final Button sendButton = new Button("Send ");

	private final ChatbotBridge chatbot = ChatbotBridge.getInstance();
	private final MathBridge engine = MathBridge.getInstance();
	private final String sessionId = UUID.randomUUID().toString();
	private String lastResult = null;

	/** Fired after a successful computation: (expression sent to engine, result). */
	private BiConsumer<String, String> onComputed;
	/** Feature 1: supplies a JSON object string snapshotting the Compute tab. */
	private Supplier<String> workspaceSupplier = () -> null;
	/** Feature 2: fired when the chatbot returns a UI action instead of a computation. */
	private ChatActionListener onAction;

	/** (actionType, actionTarget, actionPayloadJson) - see ChatbotBridge.ChatResult. */
	public interface ChatActionListener {
		void onAction(String actionType, String actionTarget, String actionPayloadJson);
	}

	public ChatbotPanel() {
		setSpacing(10);
		getStyleClass().add("chatbot-panel");
		setPadding(new Insets(12));

		transcript.setPadding(new Insets(8));
		transcript.setFillWidth(true);

		scrollPane.setFitToWidth(true);
		scrollPane.setPannable(true);
		VBox.setVgrow(scrollPane, Priority.ALWAYS);

		inputField.setPromptText(
				"Ask in plain English - \"derivative of x^2 + 3x\", \"determinant of [[1,2],[3,4]]\", \"gcd of 48 and 18\" ... ");
		HBox.setHgrow(inputField, Priority.ALWAYS);
		inputField.setOnAction(e -> send());
		sendButton.setOnAction(e -> send());
		sendButton.setDefaultButton(true);

		HBox inputRow = new HBox(8, inputField, sendButton);
		inputRow.setAlignment(Pos.CENTER);

		getChildren().addAll(scrollPane, inputRow);

		appendAssistantBubble(
				"Hi! Ask me a math question in plain English, or type a plain "
				+ "expression directly. I remember what we just talked about, "
				+ "so follow-ups like \"now integrate that\" work too.");
	}

	public void setOnComputed(BiConsumer<String, String> listener) {
		this.onComputed = listener;
	}

	/** Feature 1 (Workspace Sync): register a supplier MainLayout uses to
	 * report what the compute tab currently shows, e.g.:
	 * {@code panel.setWorkspaceSupplier(() -> "{\"lastExpression\':\"" + ... + "\"}");} */
	public void setWorkspaceSupplier(Supplier<String> supplier) {
		this.workspaceSupplier = supplier != null ? supplier : (() -> null);
	}

	/** Feature 2 (Chat-Driven Actions): register a listener for UI actions
	 * the chatbot requests instead of (or alongside) a computation. */
	public void setOnAction(ChatActionListener listener) {
		this.onAction = listener;
	}

	public void focusInput() {
		inputField.requestFocus();
	}

	// --- Send flow ---------------------------------------------------------------------
	
	private void send() {
		String text = inputField.getText();
		if (text == null || text.isBlank()) return;
		inputField.clear();
		appendUserBubble(text);

		Task<ChatResult> classifyTask = new Task<>() {
			@Override protected ChatResult call() {
				return chatbot.classify(sessionId, text, lastResult, workspaceSupplier.get());
			}
		};
		classifyTask.setOnSucceeded(e -> handleClassified(text, classifyTask.getValue()));
		classifyTask.setOnFailed(e -> appendAssistantBubble(
					"Something went wrong understanding that: " + classifyTask.getException().getMessage()));
		Thread t = new Thread(classifyTask, "chatbot-classify");
		t.setDaemon(true);
		t.start();
	}

	private void handleClassified(String originalText, ChatResult classification) {
		appendAssistantBubble(classification.reply);

		if (classification.hasAction()) {
			// Feature 2: a UI action takes priority over any computation -
			// there's nothing to send to the engine for e.g. "plot sin(x)".
			if (onAction != null) {
				Platform.runLater(() -> onAction.onAction(
							classification.actionType, classification.actionTarget, classification.actionPayloadJson));
			}
			return;
		}

		if (!classification.hasComputation()) {
			return;
		}

		PrecisionMode mode = classification.precisionFlag == 0
			? PrecisionMode.SYMBOLIC : PrecisionMode.NUMERICAL;
		String engineInput = classification.engineInput;

		Task<String> computeTask = new Task<>() {
			@Override protected String call() {
				return engine.compute(engineInput, mode);
			}
		};
		computeTask.setOnSucceeded(e -> {
			String result = computeTask.getValue();
			lastResult = result;
			appendResultBubble(result);
			if (onComputed != null) onComputed.accept(originalText, result);
		});
		computeTask.setOnFailed(e -> appendErrorBubble(
					computeTask.getException() != null
					? computeTask.getException().getMessage()
					: "Unknown engine error"));
		Thread t = new Thread(computeTask, "chatbot-compute");
		t.setDaemon(true);
		t.start();
	}

	// --- Transcript rendering ----------------------------------------------------------
	
	private void appendUserBubble(String text) {
		appendBubble(text, Pos.CENTER_RIGHT, "chat-bubble-user");
	}

	private void appendAssistantBubble(String text) {
		appendBubble(text, Pos.CENTER_LEFT, "chat-bubble-assistant");
	}

	private void appendResultBubble(String text) {
		appendBubble(text, Pos.CENTER_LEFT, "chat-bubble-result");
	}

	private void appendErrorBubble(String text) {
		appendBubble("Error: " + text, Pos.CENTER_LEFT, "chat-bubble-error");
	}

	private void appendBubble(String text, Pos alignment, String styleClass) {
		Text t = new Text(text);
		TextFlow flow = new TextFlow(t);
		flow.setMaxWidth(480);
		flow.setPadding(new Insets(8, 12, 8, 12));
		flow.getStyleClass().add(styleClass);

		HBox row = new HBox(flow);
		row.setAlignment(alignment);
		row.setMaxWidth(Double.MAX_VALUE);

		Platform.runLater(() -> {
			transcript.getChildren().add(row);
			scrollPane.setVvalue(1.0);
		});
	}
}
