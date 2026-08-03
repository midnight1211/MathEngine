// =============================================================================
// CommonUtils.cpp
//
// All implementations in CommonUtils are inline and live in CommonUtils.h.
// This translation unit exists so CMake can include CommonUtils in the build
// graph without a separate explicit rule, and to provide a place for any
// future non-inline implementations.
//
// Build note: add CommonUtils.cpp to the CMakeLists.txt source list alongside
// the other module .cpp files:
//
//   CoreEngine.cpp
//   CommonUtils.cpp          ← add this
//   NumberTheory.cpp
//   DiscreteMath.cpp
//   ...
// =============================================================================

#include "CommonUtils.hpp"

// No non-inline definitions at this time.
// All shared utilities are defined inline in CommonUtils.h to avoid ODR
// violations when multiple translation units include the header.