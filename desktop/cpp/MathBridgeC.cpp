/**
 * MathBridgeC.cpp
 * ────────────────
 * Plain C-ABI wrapper around CoreEngine::compute(), so the same
 * libmathengine.so / .dylib / .dll can be called directly from Python
 * via ctypes — no JVM, no Spring Boot server required.
 *
 * This lets desktop/python/chatbot run standalone: it loads this library
 * with ctypes and calls mathengine_compute() the same way the JNI layer
 * and the REST server already do internally.
 */

#include "CoreEngine.hpp"
#include <cstring>
#include <cstdlib>
#include <string>

#if defined(_WIN32)
	#define MATHENGINE_API extern "C" __declspec(dllexport)
#else
	#define MATHENGINE_API extern "C" __attribute__((visibility("default")))
#endif

namespace
{
	// Allocates aa NUL-terminated copy of `s` with malloc so the caller
	// (Python via ctypes) can free it with mathengine_free().
	char *dupToMalloc(const std::string &s)
	{
		char *buf = static_cast<char *>(std::malloc(s.size() + 1));
		if (!buf)
			return nullptr;
		std::memcpy(buf, s.c_str(), s.size() + 1);
		return buf;
	}
}

/**
 * Evaluate a math expression through the same C++ engine used by the
 * JavaFX client and the Spring Boot server.
 *
 * @param expression	Math query string (same format the JNI/server accept)
 * @param precisionFlag 0 = symbolic, 1 = numerical
 * @return  malloc'd, NUL-terminated result string. Caller must free it
 * 	    with mathengine_free(). Returns nullptr only on allocation failure.
 */
MATHENGINE_API const char *mathengine_compute(const char *expression, int precisionFlag)
{
	try
	{
		CoreEngine::PrecisionMode mode = (precisionFlag == 0) ? CoreEngine::PrecisionMode::SYMBOLIC
								      : CoreEngine::PrecisionMode::NUMERICAL;

		std::string input = expression ? expression : "";
		if (input.size() > 65536)
			return dupToMalloc("ERROR: expression too long (max 65536 chars)");

		std::string result = CoreEngine::compute(input, mode);
		return dupToMalloc(result);
	}
	catch (const std::bad_alloc &)
	{
		return dupToMalloc("ERROR: C++ out-of-memory (expression may be too complex)");
	}
	catch (const std::exception &ex)
	{
		return dupToMalloc(std::string("ERROR: ") + ex.what());
	}
	catch (...)
	{
		return dupToMalloc("ERROR: unknown C++ exception");
	}
}

/** Frees a string previously returned by mathengine_compute() or mathengine_version(). */
MATHENGINE_API void mathengine_free(const char *ptr)
{
	std::free(const_cast<char *>(ptr));
}

/** Returns the engine version/build string (malloc'd - free with mathengine_free()). */
MATHENGINE_API const char *mathengine_version()
{
	try
	{
		return dupToMalloc(CoreEngine::getVersion());
	}
	catch (...)
	{
		return dupToMalloc("unknown");
	}
}

