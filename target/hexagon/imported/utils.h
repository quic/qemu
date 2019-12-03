/*
 *  Copyright (c) 2019 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/* 
 * 
 */


#ifndef _UTILS_H_
#define _UTILS_H_ 1

#define CALLBACK(X,...)  if ((X) != NULL) X(__VA_ARGS__);
#define CALLBACKP(X,...)  if ((X) != NULL) X(__VA_ARGS__);
#define CALLBACK_DEFINED(X) ((X) != NULL)

#define STRINGIZE(X) #X

//#define ARRAY_SIZE(X) (sizeof(X)/sizeof((X)[0]))

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define info(...) err_info((const char*)__FUNCTION__,__FILE__,__LINE__,__VA_ARGS__)
#ifdef FIXME
#define warn(...) err_warn((const char*)__FUNCTION__,__FILE__,__LINE__,__VA_ARGS__)
#else
#define warn(...) /* Nothing */
#endif
#define fatal(...) err_fatal((const char*)__FUNCTION__,__FILE__,__LINE__,__VA_ARGS__)
#define panic(...) err_panic((const char*)__FUNCTION__,__FILE__,__LINE__,__VA_ARGS__)
#define debug(...) err_debug(__VA_ARGS__)

#define pwarn(proc,...) err_warn((char*)__FUNCTION__,__FILE__,__LINE__,__VA_ARGS__)
#define pfatal(proc,...) err_fatal((char*)__FUNCTION__,__FILE__,__LINE__,__VA_ARGS__)
#define ppanic(proc,...) err_panic((char*)__FUNCTION__,__FILE__,__LINE__,__VA_ARGS__)

void err_info(const char *, const char *, int, const char *, ...);
void err_warn(const char *, const char *, int, const char *, ...);
void err_fatal(const char *, const char *, int, const char *, ...);
void err_panic(const char *, const char *, int, const char *, ...);


#ifdef VERIFICATION
#ifndef IFVERIF
#define IFVERIF(X) X
#endif
#else
#ifndef IFVERIF
#define IFVERIF(X)				/* NOTHING */
#endif
#endif

#endif							/* _UTILS_H_ */
