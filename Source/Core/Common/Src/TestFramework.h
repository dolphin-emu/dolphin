// Stupidly simple automated testing framework
// by ector

// licence: Public Domain

// If TESTING_ENABLE is true, all tests across the project will run before main().
// If it's false, all tests will be destroyed by the linker, hopefully.

// Unfortunately, MSVC:s library linker seems to kill off unreferenced objects, even if the
// initialization has side effects. This makes this framework not work properly :(
// TODO(ector): Find solution.

// TODO(ector): make sure tests are destroyed and that things compile without TESTING_ENABLE :P

#define TESTING_ENABLE

#ifndef _TEST_FRAMEWORK_H
#define _TEST_FRAMEWORK_H

#include "Common.h"
#include <stdio.h>

#ifdef TESTING_ENABLE

namespace __test
{
extern int numTests;
extern int numTestsFailed;
}

struct TestRunnah
{
	const char* filename;
	const char* function;
	TestRunnah(const char* _filename, const char* _function)
		: filename(_filename), function(_function) {}


	bool AssertTrue(bool value, int line)
	{
		if (!value)
		{
			char string[256];
			sprintf(string, "%s:%s:%i: %s", filename, function, line, "failed");
			PanicAlert("Test Results: %s", string);
			TestFailed();
			return(false);
		}

		return(true);
	}


	template<class T>
	bool AssertEqual(T a, T b, int line)
	{
		if (!(a == b))
		{
			// TODO(ector) : better output
			char string[256];
			sprintf(string, "%s:%s:%i: %s", filename, function, line, "failed");
			PanicAlert("Test Results: %s", string);
			TestFailed();
			return(false);
		}
	}


	void TestFailed()
	{
		__test::numTestsFailed++;
	}
};


#define TEST(a)	\
	void TEST_ ## a(TestRunnah * __tr); \
	struct DUMMY_ ## a \
		: public TestRunnah { \
		DUMMY_ ## a() \
			: TestRunnah(__FILE__, # a) {\
			TEST_ ## a(this); __test::numTests++;} }; \
	DUMMY_ ## a ddummy_ ## a; \
	void TEST_ ## a(TestRunnah * __tr)

#else // TESTING_ENABLE

#define TEST(a)	\
	void TEST_ ## a(TestRunnah * __tr) \

#endif

#define CHECK(a) if (!__tr->AssertTrue(a, __LINE__)){return;}
#define CHECK_EQ(a, b) if (!__tr->AssertEqual(a, b, __LINE__)){return;}

int GetNumTests();
int GetNumTestsFailed();


#endif
