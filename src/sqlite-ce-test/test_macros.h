/*
** SQLite/CE Test Framework - Assertion Macros
**
** Lightweight assertion macros for unit testing.
** These macros work with the test framework in test_main.c.
**
** Usage:
**   static int test_example(void) {
**       int x = 5;
**       TEST_ASSERT(x == 5);               // Basic assertion
**       TEST_ASSERT_EQ(x, 5);              // Equality check with better error msg
**       TEST_ASSERT_NEQ(x, 0);             // Not equal
**       TEST_ASSERT_NULL(ptr);             // Null check
**       TEST_ASSERT_NOT_NULL(ptr);         // Not null check
**       TEST_ASSERT_STR_EQ(a, "hello");    // String comparison
**       return TEST_PASS;                  // All assertions passed
**   }
*/

#ifndef SQLITE_CE_TEST_MACROS_H
#define SQLITE_CE_TEST_MACROS_H

/* Test result values */
#define TEST_PASS 1
#define TEST_FAIL 0

/* External debug context setter (defined in test_main.c) */
extern void SetDebugContext(const char *fmt, int val);
extern void ClearDebugContext(void);

/*
** Basic assertion - fails if condition is false.
** Sets debug context to file:line on failure.
*/
#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            SetDebugContext("line %d", __LINE__); \
            return TEST_FAIL; \
        } \
    } while(0)

/*
** Equality assertion for integers.
** On failure, shows expected vs actual.
*/
#define TEST_ASSERT_EQ(actual, expected) \
    do { \
        int _a = (actual); \
        int _e = (expected); \
        if (_a != _e) { \
            SetDebugContext("line %d", __LINE__); \
            return TEST_FAIL; \
        } \
    } while(0)

/*
** Not-equal assertion for integers.
*/
#define TEST_ASSERT_NEQ(actual, notexpected) \
    do { \
        if ((actual) == (notexpected)) { \
            SetDebugContext("line %d", __LINE__); \
            return TEST_FAIL; \
        } \
    } while(0)

/*
** Null pointer assertion.
*/
#define TEST_ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            SetDebugContext("line %d", __LINE__); \
            return TEST_FAIL; \
        } \
    } while(0)

/*
** Not-null pointer assertion.
*/
#define TEST_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            SetDebugContext("line %d", __LINE__); \
            return TEST_FAIL; \
        } \
    } while(0)

/*
** String equality assertion.
** Handles NULL strings safely.
*/
#define TEST_ASSERT_STR_EQ(actual, expected) \
    do { \
        const char *_a = (actual); \
        const char *_e = (expected); \
        if (_a == NULL && _e == NULL) break; \
        if (_a == NULL || _e == NULL) { \
            SetDebugContext("line %d", __LINE__); \
            return TEST_FAIL; \
        } \
        { \
            const char *p = _a, *q = _e; \
            while (*p && *q && *p == *q) { p++; q++; } \
            if (*p != *q) { \
                SetDebugContext("line %d", __LINE__); \
                return TEST_FAIL; \
            } \
        } \
    } while(0)

/*
** SQLite success assertion.
** Use for checking sqlite_exec and similar calls.
*/
#define TEST_ASSERT_SQLITE_OK(rc) \
    do { \
        if ((rc) != SQLITE_OK) { \
            SetDebugContext("rc=%d at line %d", (rc)); \
            return TEST_FAIL; \
        } \
    } while(0)

/*
** Greater than assertion.
*/
#define TEST_ASSERT_GT(actual, threshold) \
    do { \
        if ((actual) <= (threshold)) { \
            SetDebugContext("line %d", __LINE__); \
            return TEST_FAIL; \
        } \
    } while(0)

/*
** Less than assertion.
*/
#define TEST_ASSERT_LT(actual, threshold) \
    do { \
        if ((actual) >= (threshold)) { \
            SetDebugContext("line %d", __LINE__); \
            return TEST_FAIL; \
        } \
    } while(0)

/*
** Range assertion (inclusive).
*/
#define TEST_ASSERT_RANGE(actual, low, high) \
    do { \
        int _v = (actual); \
        if (_v < (low) || _v > (high)) { \
            SetDebugContext("line %d", __LINE__); \
            return TEST_FAIL; \
        } \
    } while(0)

#endif /* SQLITE_CE_TEST_MACROS_H */
