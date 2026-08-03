#pragma once
// calculus/Calculus.h
// Top-level include for the calculus module. Include this from CoreEngine.cpp.

#ifndef CALCULUS_H
#define CALCULUS_H

#include "core/Expression.hpp"
#include "core/Simplify.hpp"
#include "core/Week4Bridge.hpp"
#include "differentiation/Derivative.hpp"
#include "differentiation/Partial.hpp"
#include "differentiation/Implicit.hpp"
#include "limits/Limits.hpp"
#include "integration/Numerical.hpp"
#include "integration/Symbolic.hpp"
#include "integration/Multivariable.hpp"
#include "series/Series.hpp"
#include "series/Convergence.hpp"
#include "vectorcalc/VectorOps.hpp"
#include "vectorcalc/LineIntegral.hpp"
#include "vectorcalc/SurfaceIntegral.hpp"
#include "vectorcalc/Theorems.hpp"
#include "optimization/Optimize.hpp"

#include <string>

namespace Calculus {

// =============================================================================
// CalculusResult — universal result type returned to CoreEngine / JNI
// =============================================================================

struct CalculusResult {
    bool        ok      = true;
    std::string symbolic;       // simplified infix expression
    std::string latex;          // LaTeX form
    std::string numeric;        // numeric value where applicable
    std::string method;         // which strategy was used
    std::string error;
};

// =============================================================================
// dispatch()
// Called by CoreEngine with the raw operation string and JSON-like input.
//
// operation strings:
//   "diff"             input: {"expr":"x^2","var":"x","order":1}
//   "partial"          input: {"expr":"x^2*y","var":"x","order":1}
//   "partial_mixed"    input: {"expr":"x^2*y","var1":"x","var2":"y"}
//   "gradient"         input: {"expr":"x^2+y^2","vars":["x","y"]}
//   "jacobian"         input: {"exprs":["x^2","y*x"],"vars":["x","y"]}
//   "hessian"          input: {"expr":"x^2+y^2","vars":["x","y"]}
//   "laplacian"        input: {"expr":"x^2+y^2+z^2","vars":["x","y","z"]}
//   "implicit_diff"    input: {"expr":"x^2+y^2-1","indep":"x","dep":"y"}
//   "log_diff"         input: {"expr":"x^x","var":"x"}
//   "limit"            input: {"expr":"sin(x)/x","var":"x","point":"0"}
//   "limit_left"       input: same + "direction":"left"
//   "limit_right"      input: same + "direction":"right"
//   "limit_inf"        input: {"expr":"1/x","var":"x","point":"inf"}
//   "integrate"        input: {"expr":"x^2","var":"x"}
//   "definite_int"     input: {"expr":"x^2","var":"x","a":"0","b":"1"}
//   "numerical_int"    input: {"expr":"x^2","var":"x","a":0,"b":1,"method":"romberg"}
//   "double_int"       input: {"expr":"x+y","varX":"x","varY":"y","ax":0,"bx":1,"ay":0,"by":1}
//   "triple_int"       input: {"expr":"1","varX":"x","varY":"y","varZ":"z",...bounds...}
//   "polar_int"        input: {"expr":"r","varR":"r","varT":"t","r1":0,"r2":1,"t1":0,"t2":"2*pi"}
//   "taylor"           input: {"expr":"sin(x)","var":"x","center":0,"order":5}
//   "maclaurin"        input: {"expr":"exp(x)","var":"x","order":6}
//   "convergence"      input: {"expr":"1/n^2","var":"n"}
//   "grad"             input: {"expr":"x^2+y^2","vars":["x","y"]}
//   "div"              input: {"exprs":["x","y"],"vars":["x","y"]}
//   "curl"             input: {"exprs":["y","-x","0"],"vars":["x","y","z"]}
//   "optimize_1d"      input: {"expr":"x^3-3x","var":"x","a":-3,"b":3}
//   "optimize_nd"      input: {"expr":"x^2+y^2","vars":["x","y"],"min":[-2,-2],"max":[2,2]}
//   "lagrange"         input: {"f":"x+y","g":"x^2+y^2-1","vars":["x","y"],...bounds...}
// =============================================================================

CalculusResult dispatch(const std::string& operation,
                        const std::string& inputJson);

} // namespace Calculus

#endif // CALCULUS_H