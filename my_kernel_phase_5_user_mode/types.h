/*
 * types.h - Common type definitions
 */

#ifndef TYPES_H
#define TYPES_H

/* Basic integer types */
typedef unsigned char   u8;
typedef unsigned short  u16;
typedef unsigned int    u32;
typedef unsigned long   u64;

typedef signed char     i8;
typedef signed short    i16;
typedef signed int      i32;
typedef signed long     i64;

/* NULL pointer */
#ifndef NULL
#define NULL ((void *)0)
#endif

#endif /* TYPES_H */
