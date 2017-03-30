#ifndef DMG_PORTING_H
#define DMG_PORTING_H

// Import C11: the good parts
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <assert.h>

#ifdef __cplusplus
#define DMG_EXTERN_BEGIN extern "C" {
#define DMG_EXTERN_END }
#else
#define DMG_EXTERN_BEGIN
#define DMG_EXTERN_END
#endif

#ifndef __GNUC__
#define __has_attribute(...) 0
#endif

#define DMG_INLINE

#ifndef DMG_INLINE
#if __has_attribute(always_inline)
#define DMG_INLINE inline __attribute__((always_inline))
#else
#define DMG_INLINE inline
#endif
#endif

#endif // DMG_PORTING_H