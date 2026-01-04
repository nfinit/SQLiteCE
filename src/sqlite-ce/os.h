/*
** os.h wrapper for Windows CE
**
** This file ensures config.h is included before the real os.h,
** setting up OS_WIN=0 and defining OsFile before os.h is processed.
*/

#ifndef _OS_H_CE_WRAPPER_
#define _OS_H_CE_WRAPPER_

/* Include our CE config first - this defines OS_WIN=0, OsFile, off_t */
#include "config.h"

/* Now include the real os.h from sqlite-2.8.17/src */
#include "../../sqlite-2.8.17/src/os.h"

#endif /* _OS_H_CE_WRAPPER_ */
