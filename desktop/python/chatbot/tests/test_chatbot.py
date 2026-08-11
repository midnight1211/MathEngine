"""
Run with:  python -m unittest discover -s chatbot/tests -t chatbot
(from the repo root), or  python tests/test_chatbot.py  from chatbot/.
No third-party dependencies.
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from nlp_engine import NLPChatbot  # noqa: E402


class TestSmallTalk(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_greeting(self):
        r = self.bot.handle("s1", "hello")
        self.assertEqual(r.intent, "smalltalk.greeting")
        self.assertIsNone(r.engine_input)

    def test_help(self):
        r = self.bot.handle("s1", "what can you do?")
        self.assertEqual(r.intent, "smalltalk.help")


class TestCalculus(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_derivative(self):
        r = self.bot.handle("s1", "derivative of x^2 + 3x")
        self.assertEqual(r.intent, "calculus.derivative")
        self.assertEqual(r.engine_input, "diff[x^2 + 3x,x]")
        self.assertEqual(r.precision_flag, 0)

    def test_definite_integral(self):
        r = self.bot.handle("s1", "integrate x^2 from 0 to 3")
        self.assertEqual(r.intent, "calculus.integral_definite")
        self.assertEqual(r.engine_input, "definite_int[x^2,0,3]")

    def test_indefinite_integral(self):
        r = self.bot.handle("s1", "integrate sin(x)")
        self.assertEqual(r.intent, "calculus.integral_indefinite")
        self.assertEqual(r.engine_input, "integrate[sin(x)]")

    def test_limit(self):
        r = self.bot.handle("s1", "limit of 1/x as x approaches infinity")
        self.assertEqual(r.intent, "calculus.limit_inf")
        self.assertIn('"expr":"1/x"', r.engine_input)

    def test_followup_pronoun(self):
        self.bot.handle("s1", "derivative of x^3")
        r = self.bot.handle("s1", "now integrate that")
        self.assertEqual(r.intent, "calculus.integral_indefinite")
        self.assertEqual(r.engine_input, "integrate[x^3]")


class TestLinearAlgebra(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_determinant(self):
        r = self.bot.handle("s1", "determinant of [[1,2],[3,4]]")
        self.assertEqual(r.intent, "la.determinant")
        self.assertEqual(r.engine_input, "la:determinant|[[1,2],[3,4]]")

    def test_inverse_followup(self):
        self.bot.handle("s1", "determinant of [[1,2],[3,4]]")
        r = self.bot.handle("s1", "now find its inverse")
        self.assertEqual(r.intent, "la.inverse")
        self.assertIn("la:inverse|", r.engine_input)

    def test_solve_system(self):
        r = self.bot.handle("s1", "solve the system [[2,1],[1,3]] with b [5,10]")
        self.assertEqual(r.intent, "la.solve")
        self.assertTrue(r.engine_input.startswith("la:solve|"))


class TestStatistics(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_mean(self):
        r = self.bot.handle("s1", "mean of [4,8,15,16,23,42]")
        self.assertEqual(r.intent, "stat.mean")
        self.assertEqual(r.engine_input, 'stat:mean|{"x":[4,8,15,16,23,42]}')

    def test_stddev(self):
        r = self.bot.handle("s1", "standard deviation of [1,2,3,4,5]")
        self.assertEqual(r.intent, "stat.stddev")


class TestNumberTheory(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_gcd(self):
        r = self.bot.handle("s1", "gcd of 48 and 18")
        self.assertEqual(r.intent, "nt.gcd")
        self.assertEqual(r.engine_input, 'nt:gcd|{"a":48,"b":18}')

    def test_is_prime(self):
        r = self.bot.handle("s1", "is 97 prime")
        self.assertEqual(r.intent, "nt.is_prime")
        self.assertEqual(r.engine_input, 'nt:is_prime|{"n":97}')


class TestDiscreteMath(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_combinations(self):
        r = self.bot.handle("s1", "10 choose 3")
        self.assertEqual(r.intent, "dm.combinations")
        self.assertEqual(r.engine_input, "combinations[10,3]")


class TestGeometry(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_distance(self):
        r = self.bot.handle("s1", "distance between points (0,0) and (3,4)")
        self.assertEqual(r.intent, "geo.distance")
        self.assertIn('"x1":0', r.engine_input)
        self.assertIn('"y2":4', r.engine_input)


class TestDiffEq(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_rk4(self):
        r = self.bot.handle("s1", "solve dy/dt = -2y with y(0) = 5 to t = 4")
        self.assertEqual(r.intent, "de.rk4")
        self.assertIn('"y0":5', r.engine_input)
        self.assertIn('"t1":4', r.engine_input)


class TestArithmeticPassthrough(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_plain_expression(self):
        r = self.bot.handle("s1", "2^8 + sqrt(16)")
        self.assertEqual(r.intent, "arithmetic.passthrough")
        self.assertEqual(r.engine_input, "2^8 + sqrt(16)")

    def test_unrecognized_falls_back(self):
        r = self.bot.handle("s1", "please do the thing with the stuff")
        self.assertEqual(r.intent, "fallback.passthrough")
        self.assertLess(r.confidence, 0.5)


class TestBatch2Calculus(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_hessian_strips_wrt_clause(self):
        r = self.bot.handle("s1", "hessian of x^2*y with respect to x,y")
        self.assertEqual(r.intent, "calculus.hessian")
        self.assertEqual(r.engine_input, 'calc:hessian|{"expr":"x^2*y","vars":["x","y"]}')

    def test_log_diff_not_shadowed_by_derivative(self):
        r = self.bot.handle("s1", "logarithmic derivative of x^2*sin(x)")
        self.assertEqual(r.intent, "calculus.log_diff")

    def test_implicit_diff_not_shadowed_by_differentiate(self):
        r = self.bot.handle("s1", "implicitly differentiate x^2+y^2=1")
        self.assertEqual(r.intent, "calculus.implicit_diff")

    def test_curl(self):
        r = self.bot.handle("s1", "curl of [y,-x,0] with respect to x,y,z")
        self.assertEqual(r.intent, "calculus.curl")
        self.assertIn('"exprs":["y","-x","0"]', r.engine_input)


class TestBatch2LinearAlgebra(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_matrix_add(self):
        r = self.bot.handle("s1", "add [[1,2],[3,4]] and [[5,6],[7,8]]")
        self.assertEqual(r.intent, "la.add")
        self.assertEqual(r.engine_input, "la:matrix_add|[[1,2],[3,4]]|[[5,6],[7,8]]")

    def test_dot_product(self):
        r = self.bot.handle("s1", "dot product of [1,2,3] and [4,5,6]")
        self.assertEqual(r.engine_input, "la:dot_product|[1,2,3]|[4,5,6]")

    def test_mod_inverse_not_shadowed_by_plain_inverse(self):
        r = self.bot.handle("s1", "modular inverse of 3 mod 11")
        self.assertEqual(r.intent, "nt.mod_inverse")

    def test_plain_inverse_still_works(self):
        r = self.bot.handle("s1", "inverse of [[1,2],[3,4]]")
        self.assertEqual(r.intent, "la.inverse")


class TestBatch2Statistics(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_percentile(self):
        r = self.bot.handle("s1", "percentile of [1,2,3,4,5,6,7,8,9,10] at the 90th")
        self.assertEqual(r.engine_input, 'stat:percentile|{"x":[1,2,3,4,5,6,7,8,9,10],"p":90}')

    def test_binomial_pmf(self):
        r = self.bot.handle("s1", "binomial probability of 3 successes in 10 trials with p=0.4")
        self.assertEqual(r.engine_input, 'stat:binomial_pmf|{"k":3,"n":10,"p":0.4}')


class TestBatch2NumberTheory(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_divisors(self):
        r = self.bot.handle("s1", "divisors of 28")
        self.assertEqual(r.engine_input, 'nt:divisors|{"n":28}')

    def test_next_prime(self):
        r = self.bot.handle("s1", "next prime after 97")
        self.assertEqual(r.engine_input, 'nt:next_prime|{"n":97}')


class TestBatch2DiscreteMath(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_derangements_uses_dm_prefix(self):
        r = self.bot.handle("s1", "derangements of 5")
        self.assertEqual(r.engine_input, 'dm:derangements|{"n":5}')

    def test_bfs(self):
        r = self.bot.handle("s1", "bfs from node [[0,1,0],[1,0,1],[0,1,0]] 0")
        self.assertEqual(r.intent, "dm.bfs")


class TestBatch2Geometry(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_3d_distance_not_confused_with_2d(self):
        r = self.bot.handle("s1", "distance between (0,0,0) and (1,1,1)")
        self.assertEqual(r.intent, "geo.distance_3d")

    def test_2d_distance_still_works(self):
        r = self.bot.handle("s1", "distance between points (0,0) and (3,4)")
        self.assertEqual(r.intent, "geo.distance")


class TestBatch2NumericalAnalysis(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_newton_captures_x0_not_prose(self):
        r = self.bot.handle("s1", "newtons method x^2-2 starting near 1")
        self.assertEqual(r.intent, "na.newton")
        self.assertIn('"f":"x^2-2"', r.engine_input)
        self.assertIn('"x0":1', r.engine_input)

    def test_trapezoidal_captures_expr_not_empty(self):
        r = self.bot.handle("s1", "trapezoidal rule x^2 from 0 to 2")
        self.assertEqual(r.intent, "na.trapezoidal")
        self.assertIn('"f":"x^2"', r.engine_input)


class TestBatch3(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_pigeonhole(self):
        r = self.bot.handle("s1", "pigeonhole with 10 items and 3 holes")
        self.assertEqual(r.engine_input, 'dm:pigeonhole|{"items":10,"holes":3}')

    def test_dihedral(self):
        r = self.bot.handle("s1", "dihedral group of order 6")
        self.assertEqual(r.engine_input, 'aa:dihedral|{"n":6}')

    def test_normal_qf_partial_args_use_correct_defaults(self):
        r = self.bot.handle("s1", "normal quantile for p=0.95")
        self.assertEqual(r.engine_input, 'stat:normal_qf|{"p":0.95,"mu":0,"sigma":1}')

    def test_prime_pi_not_shadowed_by_primes_upto(self):
        r = self.bot.handle("s1", "prime counting function of 50")
        self.assertEqual(r.intent, "nt.prime_pi")

    def test_primes_upto_still_works(self):
        r = self.bot.handle("s1", "how many primes up to 100")
        self.assertEqual(r.intent, "nt.primes_upto")


class TestBatch4(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_logistic_growth_variable_names_dont_corrupt_numbers(self):
        # "x0" and "T" must not have their digits confused with x0's value.
        r = self.bot.handle("s1", "logistic growth with r=0.5 K=1000 x0=20 T=10")
        self.assertEqual(r.engine_input, 'am:logistic_growth|{"r":0.5,"K":1000,"x0":20,"T":10}')

    def test_sir_model(self):
        r = self.bot.handle("s1", "sir model with beta=0.4 gamma=0.1")
        self.assertEqual(r.intent, "am.sir")
        self.assertIn('"beta":0.4,"gamma":0.1', r.engine_input)

    def test_gamblers_ruin(self):
        r = self.bot.handle("s1", "gamblers ruin p=0.5 start=5 target=10")
        self.assertEqual(r.engine_input, 'prob:gamblers_ruin|{"p":0.5,"start":5,"target":10}')

    def test_bernoulli_de(self):
        r = self.bot.handle("s1", "bernoulli equation P=1, Q=x, n=2")
        self.assertEqual(r.engine_input, 'de:bernoulli|{"P":"1","Q":"x","n":"2","x":"x"}')

    def test_number_extraction_ignores_variable_suffix_digits(self):
        from entities import find_numbers
        self.assertEqual(find_numbers("x0=20 y0=9 S0=990"), ["20", "9", "990"])


class TestBatch5LinearAlgebra(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_adjugate(self):
        r = self.bot.handle("s1", "adjugate of [[1,2],[3,4]]")
        self.assertEqual(r.engine_input, "la:adjugate|[[1,2],[3,4]]")

    def test_hadamard_two_matrices(self):
        r = self.bot.handle("s1", "hadamard product of [[1,2],[3,4]] and [[5,6],[7,8]]")
        self.assertEqual(r.engine_input, "la:matrix_hadamard|[[1,2],[3,4]]|[[5,6],[7,8]]")

    def test_scalar_multiply(self):
        r = self.bot.handle("s1", "multiply the matrix [[1,2],[3,4]] by 3")
        self.assertEqual(r.engine_input, "la:scalar_multiply|[[1,2],[3,4]]|3")

    def test_inverse_gaussjordan_not_shadowed_by_plain_inverse(self):
        r = self.bot.handle("s1", "inverse via gauss-jordan of [[1,2],[3,4]]")
        self.assertEqual(r.intent, "la.inverse_gaussjordan")

    def test_plain_inverse_still_works_after_batch5(self):
        r = self.bot.handle("s1", "inverse of [[1,2],[3,4]]")
        self.assertEqual(r.intent, "la.inverse")

    def test_change_of_basis_three_matrices(self):
        r = self.bot.handle("s1", "change of basis [[1,0],[0,1]] [[1,1],[0,1]] [[1,2],[3,4]]")
        self.assertEqual(r.engine_input, "la:change_of_basis|[[1,0],[0,1]]|[[1,1],[0,1]]|[[1,2],[3,4]]")

    def test_make_zero_two_scalars(self):
        r = self.bot.handle("s1", "zero matrix 3 by 2")
        self.assertEqual(r.engine_input, "la:make_zero|3|2")


class TestBatch6Statistics(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_t_pdf_is_pure_scalar_no_duplicate_x_key(self):
        r = self.bot.handle("s1", "t pdf at x=1 df=5")
        self.assertEqual(r.engine_input, 'stat:t_pdf|{"x":1,"df":5}')

    def test_bayes_theorem(self):
        r = self.bot.handle("s1", "bayes theorem prior=0.3 likelihood=0.8 marginal=0.5")
        self.assertEqual(r.engine_input, 'stat:bayes|{"prior":0.3,"likelihood":0.8,"marginal":0.5}')

    def test_ci_mean_t_uses_real_vector(self):
        r = self.bot.handle("s1", "confidence interval t mean [1,2,3,4,5]")
        self.assertEqual(r.engine_input, 'stat:ci_mean_t|{"x":[1,2,3,4,5],"alpha":0.05}')

    def test_multinomial_uses_counts_key(self):
        r = self.bot.handle("s1", "multinomial test [10,20,30]")
        self.assertEqual(r.engine_input, 'stat:multinomial|{"counts":[10,20,30]}')

    def test_fisher_exact_four_cells(self):
        r = self.bot.handle("s1", "fishers exact test a=5 b=3 c=2 d=10")
        self.assertEqual(r.engine_input, 'stat:fisher_exact|{"a":5,"b":3,"c":2,"d":10,"alpha":0.05}')

    def test_chi_sq_indep_table(self):
        r = self.bot.handle("s1", "chi-squared test of independence [[10,20],[30,40]]")
        self.assertEqual(r.engine_input, 'stat:chi_sq_indep|{"table":[[10,20],[30,40]]}')

    def test_anova_groups(self):
        r = self.bot.handle("s1", "one-way anova [[5,6,7],[8,9,10],[6,7,8]]")
        self.assertEqual(r.engine_input, 'stat:anova_one|{"groups":[[5,6,7],[8,9,10],[6,7,8]]}')


class TestBatch7Statistics(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_chi_sq_test_uses_expected_key(self):
        r = self.bot.handle("s1", "chi-squared goodness-of-fit [10,20,30] and [15,15,30]")
        self.assertEqual(r.engine_input, 'stat:chi_sq_test|{"x":[10,20,30],"expected":[15,15,30],"alpha":0.05}')

    def test_kaplan_meier_t_status_keys(self):
        r = self.bot.handle("s1", "kaplan-meier of [5,6,7,8] and [1,0,1,1]")
        self.assertEqual(r.engine_input, 'stat:kaplan_meier|{"t":[5,6,7,8],"status":[1,0,1,1]}')

    def test_p_chart_defectives_n_keys(self):
        r = self.bot.handle("s1", "p-chart with defectives [2,3,1] and n [50,50,50]")
        self.assertEqual(r.engine_input, 'stat:p_chart|{"defectives":[2,3,1],"n":[50,50,50]}')


class TestNewFeatures(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_workspace_sync_resolves_pronoun(self):
        r = self.bot.handle("s1", "what is the determinant of this matrix?",
                             workspace={"lastExpression": "[[1,2],[2,4]]"})
        self.assertEqual(r.intent, "la.determinant")
        self.assertEqual(r.engine_input, "la:determinant|[[1,2],[2,4]]")

    def test_workspace_sync_persists_across_turns(self):
        self.bot.handle("s1", "hello", workspace={"lastExpression": "[[1,2],[2,4]]"})
        r = self.bot.handle("s1", "find its inverse")
        self.assertEqual(r.intent, "la.inverse")
        self.assertEqual(r.engine_input, "la:inverse|[[1,2],[2,4]]")

    def test_plot_action_detected(self):
        r = self.bot.handle("s1", "plot sin(x) from -10 to 10")
        self.assertEqual(r.intent, "action.plot")
        self.assertIsNotNone(r.action)
        self.assertEqual(r.action["type"], "SWITCH_TAB")
        self.assertEqual(r.action["target"], "Graph")
        self.assertEqual(r.action["payload"]["equations"], ["sin(x)"])

    def test_plot_action_multiple_equations(self):
        r = self.bot.handle("s1", "plot sin(x) and cos(x)")
        self.assertEqual(r.intent, "action.plot")
        self.assertEqual(r.action["payload"]["equations"], ["sin(x)", "cos(x)"])

    def test_plot_action_pronoun_resolves_last_expression(self):
        self.bot.handle("s1", "derivative of x^3")
        r = self.bot.handle("s1", "plot that")
        self.assertEqual(r.intent, "action.plot")
        self.assertEqual(r.action["payload"]["equations"], ["x^3"])

    def test_clear_graph_action(self):
        r = self.bot.handle("s1", "clear the graph")
        self.assertEqual(r.intent, "action.clear_graph")
        self.assertEqual(r.action["type"], "CLEAR_GRAPH")
        self.assertEqual(r.action["target"], "Graph")

    def test_non_plot_message_has_no_action(self):
        r = self.bot.handle("s1", "derivative of x^2")
        self.assertIsNone(r.action)

    def test_preflight_catches_unbalanced_parens(self):
        r = self.bot.handle("s1", "sin(2x")
        self.assertEqual(r.intent, "arithmetic.syntax_error")
        self.assertIn("character", r.reply)
        self.assertIsNone(r.engine_input)

    def test_valid_expression_not_flagged(self):
        r = self.bot.handle("s1", "sin(2*x)")
        self.assertEqual(r.intent, "arithmetic.passthrough")

    def test_knowledge_lookup_general_question(self):
        r = self.bot.handle("s1", "what is a taylor series")
        self.assertEqual(r.intent, "knowledge.lookup")
        self.assertIn("Taylor Series", r.reply)

    def test_knowledge_lookup_yields_to_instance_computation(self):
        r = self.bot.handle("s1", "what is the determinant of [[1,2],[3,4]]")
        self.assertEqual(r.intent, "la.determinant")


class TestFollowupFeatures(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_precision_toggle_to_numeric(self):
        self.bot.handle("s1", "derivative of x^2")
        r = self.bot.handle("s1", "give me that as a decimal")
        self.assertEqual(r.intent, "followup.precision_toggle")
        self.assertEqual(r.engine_input, "diff[x^2,x]")
        self.assertEqual(r.precision_flag, 1)

    def test_precision_toggle_to_symbolic(self):
        self.bot.handle("s1", "derivative of x^2")
        self.bot.handle("s1", "give me that as a decimal")
        r = self.bot.handle("s1", "show the exact value instead")
        self.assertEqual(r.precision_flag, 0)
        self.assertEqual(r.engine_input, "diff[x^2,x]")

    def test_precision_toggle_without_prior_context_falls_through(self):
        r = self.bot.handle("s2", "give me that as a decimal")
        self.assertEqual(r.intent, "fallback.passthrough")

    def test_explain_that_after_computation(self):
        self.bot.handle("s1", "determinant of [[1,2],[3,4]]")
        r = self.bot.handle("s1", "why does that matter?")
        self.assertEqual(r.intent, "knowledge.explain_that")
        self.assertIn("Determinant", r.reply)

    def test_explain_that_without_context(self):
        r = self.bot.handle("s3", "what does that mean")
        self.assertEqual(r.intent, "knowledge.explain_that.unknown")

    def test_explain_that_does_not_shadow_direct_kb_query(self):
        r = self.bot.handle("s4", "what is a derivative")
        self.assertEqual(r.intent, "knowledge.lookup")


class TestSessionManagementFeatures(unittest.TestCase):
    def setUp(self):
        self.bot = NLPChatbot()

    def test_history_recall(self):
        self.bot.handle("s1", "derivative of x^2")
        self.bot.handle("s1", "determinant of [[1,2],[3,4]]")
        r = self.bot.handle("s1", "what have we talked about")
        self.assertEqual(r.intent, "session.history")
        self.assertIn("determinant of [[1,2],[3,4]]", r.reply)
        self.assertIn("derivative of x^2", r.reply)

    def test_history_recall_empty_session(self):
        r = self.bot.handle("s2", "what have we talked about")
        self.assertEqual(r.intent, "session.history.empty")

    def test_clear_chat_resets_session(self):
        self.bot.handle("s1", "derivative of x^3")
        r = self.bot.handle("s1", "clear the chat")
        self.assertEqual(r.intent, "session.reset")
        # A pronoun reference after clearing should no longer resolve to x^3.
        r2 = self.bot.handle("s1", "now integrate that")
        self.assertNotIn("x^3", r2.engine_input)

    def test_suggestions_on_typo(self):
        r = self.bot.handle("s1", "derivitive of x^2")
        self.assertIn("Did you mean", r.reply)


if __name__ == "__main__":
    unittest.main()
