package com.mathengine.jni;

import com.mathengine.model.PrecisionMode;

/**
 * MathBridge — JNI interface between JavaFX and the C++ Core Engine.
 *
 * ┌─────────────────────────────────────────────────────────┐
 * │  JavaFX UI  ──→  MathBridge.compute()                  │
 * │                        │                                │
 * │                 JNI bridge (libmathengine.so/.dll)      │
 * │                        │                                │
 * │                  C++ Core Engine                        │
 * │                  mathengine_compute()                   │
 * └─────────────────────────────────────────────────────────┘
 *
 * LIBRARY LOADING
 * ─────────────────
 * System.loadLibrary("mathengine") searches java.library.path for:
 *   Linux/macOS : libmathengine.so
 *   Windows     : mathengine.dll
 *
 * Set at launch: java -Djava.library.path=src/main/cpp/build ...
 * (configured automatically in pom.xml javafx-maven-plugin options)
 *
 * HEADER GENERATION
 * ──────────────────
 * The Maven compiler plugin runs:
 *   javac -h src/main/cpp/include  <this file>
 * which generates:
 *   com_mathengine_jni_MathBridge.h
 * That header must be implemented in MathBridgeJNI.cpp (see C++ sources).
 */
public class MathBridge {

    // ── Singleton ─────────────────────────────────────────────────────────────
    private static MathBridge INSTANCE;

    private MathBridge() {}

    public static synchronized MathBridge getInstance() {
        if (INSTANCE == null) INSTANCE = new MathBridge();
        return INSTANCE;
    }

    // ── Native Library Loading ────────────────────────────────────────────────

    /** Indicates whether the native library loaded successfully. */
    private static boolean nativeLoaded = false;
    private static String  nativeError  = null;

    static {
        try {
            // Loads  libmathengine.so  (Linux/macOS)  or  mathengine.dll  (Windows)
            // from the directory specified by -Djava.library.path
            System.loadLibrary("mathengine");
            nativeLoaded = true;
        } catch (UnsatisfiedLinkError e) {
            nativeError = e.getMessage();
            System.err.println("[MathBridge] Native library not found — falling back to stub: " + nativeError);
        }
    }

    // ── Native Method Declarations ────────────────────────────────────────────
    // These signatures drive javac -h to generate the JNI C++ header.
    // The implementing C++ function names follow JNI mangling rules:
    //   Java_com_mathengine_jni_MathBridge_nativeCompute

    /**
     * Core computation call.
     *
     * @param expression  The math query string (e.g. "integrate(x^2, x)")
     *                    or matrix notation (e.g. "matrix:[[1,2],[3,4]]")
     * @param precisionFlag  0 = SYMBOLIC, 1 = NUMERICAL  (PrecisionMode.toNativeFlag())
     * @return  Result string from the C++ engine
     */
    private native String nativeCompute(String expression, int precisionFlag);

    /**
     * Handshake test: passes a string to C++ which modifies it and returns it.
     * Used to verify bidirectional JNI flow.
     *
     * @param input  Any test string
     * @return  The string echoed back with a C++ modification (e.g. suffix appended)
     */
    public native String handshakeTest(String input);

    /**
     * Version string from the C++ engine (for diagnostics).
     */
    public native String nativeEngineVersion();

    // ── Public API ────────────────────────────────────────────────────────────

    /**
     * Dispatches a computation through JNI.
     * Falls back to a Java stub if the native library is not loaded.
     *
     * The G5 precision mode is forwarded as an int on every call.
     */
    public String compute(String expression, PrecisionMode mode) {
        if (nativeLoaded) {
            return nativeCompute(expression, mode.toNativeFlag());
        } else {
            return stubCompute(expression, mode);
        }
    }

    /**
     * Verifies the JNI "handshake": a round-trip through native code.
     * A simple string is sent to C++, modified, and returned.
     */
    public String verifyHandshake() {
        if (!nativeLoaded) {
            return "[STUB] Native library not loaded. Error: " + nativeError;
        }
        String result = handshakeTest("PING");
        // Expect C++ to return "PING::PONG" or similar
        boolean ok = result != null && result.contains("PONG");
        return ok
            ? "✓ JNI handshake successful: " + result
            : "✗ JNI handshake unexpected response: " + result;
    }

    public boolean isNativeLoaded()  { return nativeLoaded;  }
    public String  getNativeError()  { return nativeError;   }

    // ── Java Stub (used when native lib is absent) ─────────────────────────────

    /**
     * Pure-Java fallback so the UI remains usable without the compiled .so/.dll.
     * Handles a small subset of expressions for development/testing.
     */
    private String stubCompute(String expression, PrecisionMode mode) {
        // calc: and matrix: prefixes require the native library.
        // Return a clear message instead of silently producing wrong output.
        if (expression.startsWith("calc:") || expression.startsWith("matrix:")) {
            return "[Native library not loaded — recompile the DLL to use calculus/matrix operations]";
        }

        String expr = expression.trim().toLowerCase();
        return switch (mode) {
            case SYMBOLIC   -> symbolicStub(expr);
            case NUMERICAL  -> numericalStub(expr);
        };
    }

    private String symbolicStub(String expr) {
        if (expr.contains("sin") && expr.contains("pi"))   return "0";
        if (expr.contains("integrate") && expr.contains("x^2")) return "x³/3 + C";
        if (expr.contains("sqrt(2)"))  return "√2";
        if (expr.contains("pi"))       return "π";
        if (expr.contains("e"))        return "e";
        return "[Symbolic stub] " + expr;
    }

    private String numericalStub(String expr) {
        if (expr.contains("pi"))       return "3.14159265358979";
        if (expr.contains("sqrt(2)"))  return "1.41421356237310";
        if (expr.contains("e"))        return "2.71828182845905";
        return "[Numerical stub] " + expr;
    }
}