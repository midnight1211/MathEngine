# =============================================================================
# src/main/cpp/calculus/CMakeLists.txt
#
# Builds the calculus module as a static library: libcalculus.a / calculus.lib
#
# The root CMakeLists.txt adds this via:
#   add_subdirectory(src/main/cpp/calculus)
# and then links it into the main shared library:
#   target_link_libraries(mathengine PRIVATE calculus)
#
# Files built here so far (Week 2, one subject):
#   core/Expression.cpp     — ExprPtr tree, evaluate, toString, toLatex,
#                             substitute, clone, contains, equal, complexity
#   core/Week4Bridge.cpp    — parse(): Lexer → Parser → ASTNode* → ExprPtr
#
# Files that will be added in subsequent weeks (already stubbed below so you
# only need to uncomment each line as you create the file):
#   differentiation/Derivative.cpp
#   differentiation/Partial.cpp
#   differentiation/Implicit.cpp
#   limits/Limits.cpp
#   integration/Symbolic.cpp
#   integration/Numerical.cpp
#   integration/Multivariable.cpp
#   series/Series.cpp
#   series/Convergence.cpp
#   vectorcalc/VectorOps.cpp
#   vectorcalc/LineIntegral.cpp
#   vectorcalc/SurfaceIntegral.cpp
#   vectorcalc/Theorems.cpp
#   optimization/Optimize.cpp
# =============================================================================

cmake_minimum_required(VERSION 3.16)

# ── Sources ───────────────────────────────────────────────────────────────────
# Add each .cpp here as you create it. The header files (.h) are not listed —
# CMake finds them automatically through the include directories below.

set(CALCULUS_SOURCES
        Calculus.cpp
        core/Expression.cpp
        core/Week4Bridge.cpp
        core/Simplify.cpp
        differentiation/Derivative.cpp
        differentiation/Partial.cpp
        differentiation/Implicit.cpp
        limits/Limits.cpp
        integration/Numerical.cpp
        integration/Symbolic.cpp
        integration/Multivariable.cpp
        series/Series.cpp
        series/Convergence.cpp
        vectorcalc/VectorOps.cpp
        vectorcalc/LineIntegral.cpp
        vectorcalc/SurfaceIntegral.cpp
        vectorcalc/Theorems.cpp
        optimization/Optimize.cpp
)

# ── Static library ────────────────────────────────────────────────────────────
# Static (not shared) so it links directly into mathengine.dll/libmathengine.so
# without needing a separate .so file deployed alongside it.

add_library(calculus STATIC ${CALCULUS_SOURCES})

# ── Include directories ───────────────────────────────────────────────────────
# PUBLIC  → also visible to any target that links calculus (e.g. the root lib)
# PRIVATE → only needed while compiling calculus itself

target_include_directories(calculus
        PUBLIC
        # The calculus module's own headers — other modules include these
        ${CMAKE_CURRENT_SOURCE_DIR}             # calculus/   (for #include "core/Expression.h")
        ${CMAKE_CURRENT_SOURCE_DIR}/core        # calculus/core/ (for #include "Expression.h")

        PRIVATE
        # week_4 headers needed by Week4Bridge.cpp
        ${CMAKE_SOURCE_DIR}/src/main/cpp/parser/include_week_four

        # JNI headers (needed if any calculus code ever calls JNI directly —
        # not the case right now, but harmless to include)
        ${JNI_INCLUDE_DIRS}
)

# ── C++ standard ─────────────────────────────────────────────────────────────
# Inherits from the root CMakeLists but we set it explicitly here so the
# calculus subdirectory can also be built standalone for unit testing.

target_compile_features(calculus PUBLIC cxx_std_17)

# ── Compiler warnings ─────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(calculus PRIVATE /W4 /WX-)
    # /WX- means warnings are not treated as errors during development.
    # Change to /WX to enforce zero warnings before submission.
else()
    target_compile_options(calculus PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── Optional: standalone unit test executable ─────────────────────────────────
# Build with:  cmake .. -DCALCULUS_BUILD_TESTS=ON  &&  cmake --build .
# Run  with:   ./calculus_tests   (Linux/macOS)  or  calculus_tests.exe  (Windows)

option(CALCULUS_BUILD_TESTS "Build calculus unit tests" OFF)

if(CALCULUS_BUILD_TESTS)
    add_executable(calculus_tests
            tests/test_expression.cpp   # create this file to add tests
    )

    target_link_libraries(calculus_tests PRIVATE calculus)

    target_include_directories(calculus_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/core
            ${CMAKE_SOURCE_DIR}/src/main/cpp/parser/include_week_four
    )

    target_compile_features(calculus_tests PRIVATE cxx_std_17)

    # Register with CTest so  cmake --build . --target test  runs them
    enable_testing()
    add_test(NAME CalculusTests COMMAND calculus_tests)
endif()