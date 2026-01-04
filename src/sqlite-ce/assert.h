/*
** assert.h for Windows CE
**
** Provides useful debug assertions that break into debugger on failure.
*/

#ifndef _ASSERT_H_CE
#define _ASSERT_H_CE

#ifdef NDEBUG
#define assert(exp) ((void)0)
#else

#ifdef __cplusplus
extern "C" {
#endif
void ce_assert_fail(const char *exp, const char *file, int line);
#ifdef __cplusplus
}
#endif

#define assert(exp) ((exp) ? (void)0 : ce_assert_fail(#exp, __FILE__, __LINE__))

#endif /* NDEBUG */

#endif /* _ASSERT_H_CE */
