import sys, os
ROOT = "/home/david_school/Capstone"
sys.path.insert(0, os.path.join(ROOT, "desktop", "python"))
os.chdir(ROOT)
from chatbot.intent import intent_to_expr
form chatbot.engine_native import ENGINE

M = "[[4,1],[2,3]]"
M2 = "[[1,0],[0,1]]"
cases = [
    {"op":"integral","expr":"x^2","var":"x","a":0,"b":5},
    {"op":"indef_integral","expr":"x^2","var":"x"},
    {"op":"derivative","expr":"sin(x)*cos(x)","var":"x"},
    {"op":"partial","expr":"x^2+y^2","var":"x"},
    {"op":"nth_derivative","expr":"x^4","var":"x","n":2},
    {"op":"limit","expr":"sin(x)/x","var":"x","point":0},
    {"op":"taylor","expr":"e^x","point":0,"order":6},
    {"op":"maclaurin","expr":"e^x","order":6},
    {"op":"fourier","expr":"x","a":-3.14159,"b":3.14159,"order":4},
    {"op":"romberg","expr":"exp(-x^2)","var":"x","a":-5,"b":5},
    {"op":"numerical_int","expr":"x^2","var":"x","a":0,"b":5},
    {"op":"newton_raphson","expr":"x^2-2","var":"x","a":1},
    {"op":"gradient","expr":"x^2+y^2"},
    {"op":"curl","expr":"y,-x,0"},
    {"op":"divergence","expr":"x,y,z"},
    {"op":"det","matrix":M},
    {"op":"inverse","matrix":M},
    {"op":"eigenvalues","matrix":M},
    {"op":"null_space","matrix":M},
    {"op":"transpose","matrix":M},
    {"op":"rref","matrix":M},
    {"op":"rank","matrix":M},
    {"op":"trace","matrix":M},
    {"op":"lu","matrix":M},
    {"op":"qr","matrix":M},
    {"op":"svd","matrix":M},
    {"op":"matmul","matrix":M,"matrix2":M2},
    {"op":"scale","matrix":M,"n":2},
    {"op":"solve_system","matrix":M,"matrix2":"[[1],[2]]"},
    {"op":"factorial","n":5},
    {"op":"gcd","expr":"48,18"},
    {"op":"lcm","expr":"4,6"},
    {"op":"prime_test","n":97},
    {"op":"factorize","n":360},
    {"op":"fibonacci","n":10},
    {"op":"mean","expr":"1,2,3,4"},
    {"op":"std","expr":"1,2,3,4"},
    {"op":"variance","expr":"1,2,3,4"},
    {"op":"median","expr":"1,2,3,4"},
    {"op":"normal_pdf","point":1.5,"a":0,"b":1},
    {"op":"normal_cdf","point":1.5,"a":0,"b":1},
    {"op":"binomial_pmf","n":10,"p":0.5,"point":3},
    {"op":"binomial_cdf","n":10,"p":0.5,"point":3},
    {"op":"poisson_pmf","a":2,"point":3},
    {"op":"poisson_cdf","a":2,"point":3},
    {"op":"t_cdf","point":1.5,"n":10},
    {"op":"chisq_cdf","point":1.5,"n":10},
    {"op":"linear_reg","a":"[1,2,3,4]","b":"[2,4,5,8]"},
    {"op":"poly_reg","a":"[1,2,3,4]","b":"[2,4,5,8]","n":2},
    {"op":"markov","matrix":"0.7,0.3;0.4,0.6","expr":"0.5,0.5","steps":10},
    {"op":"bisection","expr":"x^2-2","var":"x","a":0,"b":2},
    {"op":"secant","expr":"x^2-2","var":"x","a":0,"b":2},
    {"op":"solve_equation","expr":"x^2-4","var":"x"},
    {"op":"solve_linsys","matrix":"[[1,1,5],[1,-1,1]]"},
    {"op":"laplace","expr":"t","var":"t"},
    {"op":"inv_laplace","expr":"1/(s^2)"},
    {"op":"fft","expr":"1,1,0,-1,-1,-1,0,1"},
    {"op":"ifft","expr":"1,1,0,-1,-1,-1,0,1"},
    {"op":"dot_product","matrix":"1,2,3","matrix2":"4,5,6"},
    {"op":"cross_product","matrix":"1,2,3","matrix2":"4,5,6"},
    {"op":"convex_hull","expr":"0,0;1,0;1,1;0,1"},
    {"op":"compute","expr":"2+3*sin(pi/4)"},
]
bad = []
for c in cases:
    e= intent_to_expr(c)
    try:
        r = ENGINE.compute(e, 0)
    except Exception as ex:
        r = f"EXC {ex}"
    flat = r.replace("\n", " | ")[:110]
    status = "BAD " if ("ERROR" in r or "Unknown" in r or "Unexpected" in r or "rror" in r) else "ok  "
    if status.strip() == "BAD":
        bad.append(c["op"])
    print(f"{status}{c['op']:16s} {e[:70]:70s} -> {flat}")
print("\nBROKEN:", bad)
