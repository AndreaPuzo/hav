#ifndef _HAVSTR_H
# define _HAVSTR_H

# ifdef _HAV_IS_EXTERNAL
#  include <hav/havcfg.h>
#  include <hav/havdef.h>
# else
#  include "havcfg.h"
#  include "havdef.h"
# endif

# include <stdarg.h>
# include <string.h>

# define hav_lenstr(__str)                strlen(__str)
# define hav_lennstr(__str, __len)        strnlen((__str), (__len))
# define hav_cpystr(__dst, __src)         strcpy((__dst), (__src))
# define hav_cpynstr(__dst, __src, __len) strncpy((__dst), (__src), (__len))
# define hav_dupstr(__str)                strdup(__str)
# define hav_dupnstr(__str, __len)        strndup((__str), (__len))
# define hav_cmpstr(__lhs, __rhs)         strcmp((__lhs), (__rhs))
# define hav_cmpnstr(__lhs, __rhs, __len) strcmp((__lhs), (__rhs), (__len))

_HAV_API hav_qword_t hav_afmtstr (
    hav_char_p       dst ,
    hav_qword_t      len ,
    const hav_char_p src ,
    hav_char_p       ap
) ;

_HAV_API hav_qword_t hav_vfmtstr (
    hav_char_p       dst ,
    hav_qword_t      len ,
    const hav_char_p src ,
    va_list          ap
) ;

_HAV_API hav_qword_t hav_fmtstr (
    hav_char_p       dst ,
    hav_qword_t      len ,
    const hav_char_p src ,
    ...
) ;

#endif