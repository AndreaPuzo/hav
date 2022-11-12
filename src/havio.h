#ifndef _HAVIO_H
# define _HAVIO_H

# ifdef _HAV_IS_EXTERNAL
#  include <hav/havcfg.h>
#  include <hav/havdef.h>
# else
#  include "havcfg.h"
#  include "havdef.h"
# endif

# include <stdarg.h>
# include <stdio.h>

# define HAV_EOF ((hav_qword_t)EOF)

_HAV_API hav_qword_t hav_afmtfprint (
    FILE *           out ,
    const hav_char_p fmt ,
    hav_char_p       ap
) ;

_HAV_API hav_qword_t hav_vfmtfprint (
    FILE *           out ,
    const hav_char_p fmt ,
    va_list          ap
) ;

_HAV_API hav_qword_t hav_fmtfprint (
    FILE *           out ,
    const hav_char_p fmt ,
    ...
) ;

_HAV_API hav_qword_t hav_afmtprint (
    const hav_char_p fmt ,
    hav_char_p       ap
) ;

_HAV_API hav_qword_t hav_vfmtprint (
    const hav_char_p fmt ,
    va_list          ap
) ;

_HAV_API hav_qword_t hav_fmtprint (
    const hav_char_p fmt ,
    ...
) ;

_HAV_API hav_qword_t hav_afmteprint (
    const hav_char_p fmt ,
    hav_char_p       ap
) ;

_HAV_API hav_qword_t hav_vfmteprint (
    const hav_char_p fmt ,
    va_list          ap
) ;

_HAV_API hav_qword_t hav_fmteprint (
    const hav_char_p fmt ,
    ...
) ;

#endif