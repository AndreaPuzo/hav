#ifndef _HAVCFG_H
# define _HAVCFG_H

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

# ifndef HAV_NULL
#  define HAV_NULL ((void *)0)
# endif

# define hav_is_nullptr(__ptr) (HAV_NULL == (__ptr))

#endif