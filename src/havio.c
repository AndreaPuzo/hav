#ifdef _HAV_IS_EXTERNAL
# include <hav/hav.h>
#else
# include "hav.h"
#endif

_HAV_API hav_qword_t hav_afmtfprint (
    FILE *           out ,
    const hav_char_p fmt ,
    hav_char_p       ap
)
{
    hav_char_p  buf ;
    hav_qword_t len ;

    len = hav_afmtstr(HAV_NULL, (hav_qword_t)-1, fmt, ap) + /* END */ 1 ;
    buf = (hav_char_p)hav_alloc(len * sizeof(hav_char_t)) ;

    if (hav_is_nullptr(buf)) {
        return HAV_EOF ;
    }

    hav_afmtstr(buf, len, fmt, ap) ;

    len = fputs(buf, out) ;
    hav_dealloc(buf) ;

    return len ;
}

_HAV_API hav_qword_t hav_vfmtfprint (
    FILE *           out ,
    const hav_char_p fmt ,
    va_list          ap
)
{
    hav_char_p  buf ;
    hav_qword_t len ;
    va_list ap_copy ;

    va_copy(ap_copy, ap) ;
    len = hav_vfmtstr(HAV_NULL, (hav_qword_t)-1, fmt, ap) + /* END */ 1 ;
    buf = (hav_char_p)hav_alloc(len * sizeof(hav_char_t)) ;

    if (hav_is_nullptr(buf)) {
        return HAV_EOF ;
    }

    hav_vfmtstr(buf, len, fmt, ap_copy) ;
    va_end(ap_copy) ;

    len = fputs(buf, out) ;
    hav_dealloc(buf) ;

    return len ;
}

_HAV_API hav_qword_t hav_fmtfprint (
    FILE *           out ,
    const hav_char_p fmt ,
    ...
)
{
    va_list ap ;

    va_start(ap, fmt) ;
    hav_qword_t len = hav_vfmtfprint(out, fmt, ap) ;
    va_end(ap) ;

    return len ;
}

_HAV_API hav_qword_t hav_afmtprint (
    const hav_char_p fmt ,
    hav_char_p       ap
)
{
    return hav_afmtfprint(stdout, fmt, ap) ;
}

_HAV_API hav_qword_t hav_vfmtprint (
    const hav_char_p fmt ,
    va_list          ap
)
{
    return hav_vfmtfprint(stdout, fmt, ap) ;
}

_HAV_API hav_qword_t hav_fmtprint (
    const hav_char_p fmt ,
    ...
)
{
    va_list ap ;

    va_start(ap, fmt) ;
    hav_qword_t len = hav_vfmtfprint(stdout, fmt, ap) ;
    va_end(ap) ;

    return len ;
}

_HAV_API hav_qword_t hav_afmteprint (
    const hav_char_p fmt ,
    hav_char_p       ap
)
{
    return hav_afmtfprint(stderr, fmt, ap) ;
}

_HAV_API hav_qword_t hav_vfmteprint (
    const hav_char_p fmt ,
    va_list          ap
)
{
    return hav_vfmtfprint(stderr, fmt, ap) ;
}

_HAV_API hav_qword_t hav_fmteprint (
    const hav_char_p fmt ,
    ...
)
{
    va_list ap ;

    va_start(ap, fmt) ;
    hav_qword_t len = hav_vfmtfprint(stderr, fmt, ap) ;
    va_end(ap) ;

    return len ;
}