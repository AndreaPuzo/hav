#ifndef _HAVCFGX_H
# define _HAVCFGX_H

# ifdef _MSC_VER
#  define _HAV_PACK(__typdef) __pragma( pack(push, 1) ) __typdef __pragma( pack(pop))
# else
#  define _HAV_PACK(__typdef) __typdef __attribute__((__packed__))
# endif

/* shared and static library */

# ifndef _HAV_API
#  if defined(_MSC_VER) && !defined(_HAV_IS_STATIC)
#   ifdef _HAV_EXPORT
#    define _HAV_API __declspec(dllexport)
#   else
#    define _HAV_API __declspec(dllimport)
#   endif
#  else
#   define _HAV_API
#  endif
# endif

/* heap */

# ifndef HAV_NULL
#  ifdef __cplusplus
#   define HAV_NULL 0
#  else
#   define HAV_NULL ((void *)0)
#  endif
# endif

# define hav_is_nullptr(__ptr) (HAV_NULL == (__ptr))

# ifndef _HAV_HAS_ALLOCATORS
#  define _HAV_HAS_ALLOCATORS

#  include <stdlib.h>

#  define hav_alloc(__len)          malloc(__len)
#  define hav_calloc(__elm, __len)  calloc((__elm), (__len))
#  define hav_realloc(__ptr, __len) realloc((__ptr), (__len))
#  define hav_dealloc(__ptr)        free(__ptr)
# endif

# ifndef HAV_SUCCESS
#  define HAV_SUCCESS 0
# endif

# ifndef HAV_FAILURE
#  define HAV_FAILURE 1
# endif

#endif