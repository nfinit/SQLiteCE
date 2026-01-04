/*
** ctype.h shim for Windows CE
**
** CE SDK may not have ctype.h. Provide minimal implementations.
*/

#ifndef _CTYPE_H_CE
#define _CTYPE_H_CE

#define isspace(c) ((c)==' ' || (c)=='\t' || (c)=='\n' || (c)=='\r' || (c)=='\f' || (c)=='\v')
#define isdigit(c) ((c)>='0' && (c)<='9')
#define isalpha(c) (((c)>='A' && (c)<='Z') || ((c)>='a' && (c)<='z'))
#define isalnum(c) (isalpha(c) || isdigit(c))
#define isupper(c) ((c)>='A' && (c)<='Z')
#define islower(c) ((c)>='a' && (c)<='z')
#define isprint(c) ((c)>=0x20 && (c)<=0x7E)
#define toupper(c) (islower(c) ? ((c)-'a'+'A') : (c))
#define tolower(c) (isupper(c) ? ((c)-'A'+'a') : (c))

#endif /* _CTYPE_H_CE */
