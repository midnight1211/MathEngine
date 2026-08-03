cmake_minimum_required(VERSION 3.20)
project(mathengine CXX)

# ── C++ standard ──────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Platform-specific compiler flags ─────────────────────────────────────────
if(MSVC)
    # Windows / MSVC
    add_compile_options(/W4 /permissive-)
    add_compile_definitions(_USE_MATH_DEFINES)      # gives M_PI etc on MSVC
else()
    # Linux / macOS (GCC, Clang)
    add_compile_options(-Wall -Wextra -Wpedantic -fPIC)
    # _USE_MATH_DEFINES not needed; POSIX math constants are in <cmath>
    add_compile_definitions(_POSIX_C_SOURCE=200809L)
endif()

# ── Find dependencies ─────────────────────────────────────────────────────────
find_package(JNI REQUIRED)
find_package(Threads REQUIRED)

# ── Platform JNI include dirs ─────────────────────────────────────────────────
# JNI needs the platform-specific subdirectory (linux/, darwin/, win32/)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(JNI_PLATFORM_INCLUDE "${JAVA_INCLUDE_PATH}/linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(JNI_PLATFORM_INCLUDE "${JAVA_INCLUDE_PATH}/darwin")
elseif(WIN32)
    set(JNI_PLATFORM_INCLUDE "${JAVA_INCLUDE_PATH}/win32")
endif()

# ── Calculus static library ───────────────────────────────────────────────────
add_subdirectory(desktop/cpp/Calculus)

# ── Source files ──────────────────────────────────────────────────────────────
set(SOURCES
    desktop/cpp/CoreEngine.cpp
    desktop/cpp/MathBridgeJNI.cpp
    desktop/cpp/MathBridgeC.cpp
    desktop/cpp/CommonUtils.cpp
    desktop/cpp/Preprocessor.cpp
    desktop/cpp/AbstractAlgebra/AA.cpp
    desktop/cpp/AppliedMath/AM.cpp
    desktop/cpp/Calculus/Calculus.cpp
    desktop/cpp/Calculus/core/Expression.cpp
    desktop/cpp/Calculus/core/Simplify.cpp
    desktop/cpp/Calculus/core/Week4Bridge.cpp
    desktop/cpp/Calculus/differentiation/Derivative.cpp
    desktop/cpp/Calculus/differentiation/Implicit.cpp
    desktop/cpp/Calculus/differentiation/Partial.cpp
    desktop/cpp/Calculus/integration/Multivariable.cpp
    desktop/cpp/Calculus/integration/Numerical.cpp
    desktop/cpp/Calculus/integration/Symbolic.cpp
    desktop/cpp/Calculus/limits/Limits.cpp
    desktop/cpp/Calculus/optimization/Optimize.cpp
    desktop/cpp/Calculus/series/Convergence.cpp
    desktop/cpp/Calculus/series/Series.cpp
    desktop/cpp/Calculus/vectorcalc/LineIntegral.cpp
    desktop/cpp/Calculus/vectorcalc/SurfaceIntegral.cpp
    desktop/cpp/Calculus/vectorcalc/Theorems.cpp
    desktop/cpp/Calculus/vectorcalc/VectorOps.cpp
    desktop/cpp/ComplexAnalysis/CA.cpp
    desktop/cpp/DiscreteMath/DM.cpp
    desktop/cpp/DiffEq/DE.cpp
    desktop/cpp/DiffEq/DEAdv.cpp
    desktop/cpp/Geometry/Geom.cpp
    desktop/cpp/Linear_Algebra/LA.cpp
    desktop/cpp/NumberTheory/NumberTheory.cpp
    desktop/cpp/NumericalAnalysis/NA.cpp
    desktop/cpp/parser/src_week_four/evaluator.cpp
    desktop/cpp/parser/src_week_four/lexer.cpp
    desktop/cpp/parser/src_week_four/parser.cpp
    desktop/cpp/ProbabilityTheory/PT.cpp
    desktop/cpp/Statistics/Advanced_Statistics.cpp
    desktop/cpp/Statistics/Statistics.cpp
)

# ── Shared library ────────────────────────────────────────────────────────────
add_library(mathengine SHARED ${SOURCES})

# On Linux the output is  libmathengine.so
# On macOS                libmathengine.dylib
# On Windows              mathengine.dll
# Maven's -Djava.library.path points here; System.loadLibrary("mathengine") finds it.

target_include_directories(mathengine PRIVATE
    ${JNI_INCLUDE_DIRS}
    ${JNI_PLATFORM_INCLUDE}
    desktop/cpp/include
    desktop/cpp/parser/include_week_four
    desktop/cpp/utils
)

target_link_libraries(mathengine PRIVATE
    calculus
    Threads::Threads
    $<$<NOT:$<PLATFORM_ID:Windows>>:m>   # -lm on Linux/macOS, not needed on Windows
)

# ── Output directory ──────────────────────────────────────────────────────────
set(LIB_OUT "${CMAKE_SOURCE_DIR}/build/lib")
set_target_properties(mathengine PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${LIB_OUT}"   # .so / .dylib
    RUNTIME_OUTPUT_DIRECTORY "${LIB_OUT}"   # .dll on Windows
    ARCHIVE_OUTPUT_DIRECTORY "${LIB_OUT}"   # .lib on Windows
)

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS mathengine
    LIBRARY  DESTINATION lib
    RUNTIME  DESTINATION lib
)
