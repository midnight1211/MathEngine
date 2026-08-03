package com.mathengine.model;

/**
 * PrecisionMode - maps to the G5 Precision Toggle state.
 * 
 * SYMBOLIC: Return exact results (e.g. "π", "e", "√2", "1/3")
 * NUMERICAL: Return floating-point approximations (e.g. "3.14159265")
 * 
 * The ordinal is passed as an int through the JNI bridge so C++ can branch
 * on it without string comparison:
 *     0 = SYMBOLIC
 *     1 = NUMERICAL
 */
public enum PrecisionMode {
    SYMBOLIC("Symbolic"),
    NUMERICAL("Numerical");

    private final String label;

    PrecisionMode(String label) { this.label = label; }

    public String displayName() { return label; }

    /** Converts to the int flag expected by the C++ core engine. */
    public int toNativeFlag() { return this.ordinal(); }
}