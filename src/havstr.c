#ifdef _HAV_IS_EXTERNAL
# include <hav/hav.h>
#else
# include "hav.h"
#endif

#include <locale.h>
#include <errno.h>
#include <math.h>

typedef struct __fmt_s __fmt_t , * __fmt_p ;
typedef struct __str_s __str_t , * __str_p ;
typedef struct __num_s __num_t , * __num_p ;

struct __fmt_s {
    hav_char_t  alignment    ;
    hav_char_t  prefix [2]   ;
    hav_char_t  suffix [2]   ;
    hav_char_t  padding_char ;
    hav_dword_t width        ;
    hav_dword_t precision    ;
} ;

struct __str_s {
    hav_char_p data      ;
    hav_char_t chars [2] ;
} ;

struct __num_s {
    hav_char_t sign     ;
    hav_char_t base     ;
    hav_char_t type     ;
    hav_char_t notation ;
    hav_char_t exp_type ;
    hav_char_t exp_base ;
    int64_t    exp_num  ;

    union {
        uint64_t as_uint  ;
        int64_t  as_int   ;
        double   as_float ;
    } ;
} ;

hav_qword_t __fmtstr (
    hav_char_p  dst ,
    hav_qword_t len ,
    __fmt_p     fmt ,
    __str_p     str
) ;

hav_qword_t __fmtnum (
    hav_char_p  dst ,
    hav_qword_t len ,
    __fmt_p     fmt ,
    __num_p     num
) ;

_HAV_API hav_qword_t hav_vfmtstr (
    hav_char_p       dst ,
    hav_qword_t      len ,
    const hav_char_p src ,
    va_list          ap
)
{
    hav_qword_t n , i ;

    for (n = i = 0 ; src[i] && n < len ;) {
        
        /* check for a format sequence */
        if ('%' == src[i] && '%' != src[++i]) {

            __fmt_t fmt ;
            __num_t num ;
            __str_t str ;

            /* initialize format variables */

            fmt.prefix[0] = fmt.prefix[1] = 0 ;
            fmt.suffix[0] = fmt.suffix[1] = 0 ;
            fmt.alignment    = '>' ;
            fmt.padding_char = ' ' ;
            fmt.width        = 0   ;
            fmt.precision    = 0   ;

            num.sign     = 0   ;
            num.base     = 10  ;
            num.type     = 'u' ;
            num.notation = ' ' ;

            str.data = HAV_NULL ;
            str.chars[0] = str.chars[1] = 0 ;

            /* scan the sequence */

            if ('[' == src[i]) {
                /* enable prefix */
                fmt.prefix[0] = ' ' ;
                ++i ;
            }

            if (']' == src[i]) {
                /* enable suffix */
                fmt.suffix[0] = ' ' ;
                ++i ;
            }

            if ('+' == src[i]) {
                /* force sign */
                num.sign = '+' ;
                ++i ;
            }

            if ('<' == src[i] || '-' == src[i] || /* default */ '>' == src[i]) {
                /* set the alignment */
                fmt.alignment = src[i] ;
                ++i ;
            }

            if ('0' == src[i] || ' ' == src[i]) {
                /* set the padding character */
                fmt.padding_char = src[i] ;
                ++i ;
            } else if (':' == src[i]) {
                ++i ;
                /* set the padding character */
                fmt.padding_char = src[i] ;
                ++i ;
            }

            if ('*' == src[i]) {
                /* set the width */
                fmt.width = va_arg(ap, hav_dword_t) ;
                ++i ;
            } else if ('1' <= src[i] && src[i] <= '9') {
                /* scan the width */
                do {
                    fmt.width *= 10 ;
                    fmt.width += src[i] - '0' ;
                    ++i ;
                } while ('0' <= src[i] && src[i] <= '9') ;
            }

            if ('.' == src[i]) {
                ++i ;

                if ('*' == src[i]) {
                    /* set the precision */
                    fmt.precision = va_arg(ap, hav_dword_t) ;
                    ++i ;
                } else if ('1' <= src[i] && src[i] <= '9') {
                    /* scan the precision */
                    do {
                        fmt.precision *= 10 ;
                        fmt.precision += src[i] - '0' ;
                        ++i ;
                    } while ('0' <= src[i] && src[i] <= '9') ;
                }
            }

            hav_byte_t is_long = 0 ;

            if ('L' == src[i] || 'l' == src[i]) {
                is_long = 1 ;
                ++i ;
            }

            switch (src[i]) {
            case 'B' : case 'b' : {
                if (fmt.prefix[0]) {
                    fmt.prefix[0] = '0'    ;
                    fmt.prefix[1] = src[i] ;
                } else if (fmt.suffix[0]) {
                    fmt.suffix[0] = src[i] ;
                }

                num.base = 2 ;
                goto _unsigned_integer ;
            } break ;

            case 'O' : case 'o' : {
                if (fmt.prefix[0]) {
                    fmt.prefix[0] = '0' ;
                } else if (fmt.suffix[0]) {
                    fmt.suffix[0] = src[i] ;
                }

                num.base = 8 ;
                goto _unsigned_integer ;
            } break ;

            case 'X' : case 'x' : {
                if (fmt.prefix[0]) {
                    fmt.prefix[0] = '0'    ;
                    fmt.prefix[1] = src[i] ;
                } else if (fmt.suffix[0]) {
                    if ('x' == src[i]) {
                        fmt.suffix[0] = 'h' ;
                    } else {
                        fmt.suffix[0] = 'H' ;
                    }
                }

                num.base = 16 ;
                goto _unsigned_integer ;
            } break ;

            case 'U' : case 'u' : {
                num.base = 10 ;

_unsigned_integer:
                if (is_long) {
                    num.as_uint = va_arg(ap, uint64_t) ;
                } else {
                    num.as_uint = va_arg(ap, uint32_t) ;
                }

                ++i ;
            } break ;

            case 'I' : case 'i' : {
                if (is_long) {
                    num.as_int = va_arg(ap, int64_t) ;
                } else {
                    num.as_int = va_arg(ap, int32_t) ;
                }

                num.base = 10  ;
                num.type = 'i' ;
                ++i ;
            } break ;

            case 'F' : case 'f' : {
                num.as_float = va_arg(ap, double) ;

                num.base = 10  ;
                num.type = 'f' ;
                ++i ;
            } break ;

            case 'E' : case 'e' : { /* a * 10^b */
                num.as_float = va_arg(ap, double) ;

                num.base = 10  ;
                num.exp_type = num.type = src[i] ;
                num.notation = 't' ;
                ++i ;
            } break ;

            case 'P' : case 'p' : { /* a * 2^b */
                num.as_float = va_arg(ap, double) ;

                num.base = 10 ;
                num.exp_type = num.type = src[i] ;
                num.notation = 't' ;
                ++i ;
            } break ;

            case 'C' : case 'c' : {
                if (fmt.prefix[0] || fmt.suffix[0]) {
                    fmt.prefix[0] = fmt.suffix[0] = '\'' ;
                }

                /* create the string */
                str.chars[0] = va_arg(ap, hav_dword_t) ;
                str.data = str.chars ;

                ++i ;
            } break ;
            
            case 'S' : case 's' : {
                /* check for prefix and suffix */
                if (fmt.prefix[0] || fmt.suffix[0]) {
                    fmt.prefix[0] = fmt.suffix[0] = '\"' ;
                }

                /* get the string */
                str.data = va_arg(ap, hav_char_p) ;

                ++i ;
            } break ;

            case 'R' : case 'r' : {
                /* check for prefix and suffix */
                if (fmt.prefix[0] || fmt.suffix[0]) {
                    fmt.prefix[0] = fmt.suffix[0] = '\"' ;
                }

                /* get the error as string */
                str.data = strerror(errno) ;

                ++i ;
            } break ;

            case 'N' : case 'n' : {
                hav_dword_p np = va_arg(ap, hav_dword_p) ;
                *np = n ;
                ++i ;
                goto _skip ;
            } break ;
            }

            if (hav_is_nullptr(dst)) {
                if (str.data) {
                    /* insert string */
                    n += __fmtstr(HAV_NULL, len - n, &fmt, &str) ;
                } else {
                    /* insert number */
                    n += __fmtnum(HAV_NULL, len - n, &fmt, &num) ;
                }
            } else {
                if (str.data) {
                    /* insert string */
                    n += __fmtstr(dst + n, len - n, &fmt, &str) ;
                } else {
                    /* insert number */
                    n += __fmtnum(dst + n, len - n, &fmt, &num) ;
                }
            }

_skip:
        } else {
            if (!hav_is_nullptr(dst)) {
                /* put the current character */
                dst[n] = src[i] ;
            }

            /* increment the indexes */

            ++n ;
            ++i ;
        }

    }

    if (!hav_is_nullptr(dst) && n < len) {
        /* set the end of string */
        dst[n] = 0 ;
    }

    return n ;
}

_HAV_API hav_qword_t hav_fmtstr (
    hav_char_p       dst ,
    hav_qword_t      len ,
    const hav_char_p src ,
    ...
)
{
    va_list ap ;

    va_start(ap, src) ;
    len = hav_vfmtstr(dst, len, src, ap) ;   
    va_end(ap) ;

    return len ;
}

hav_qword_t __fmtstr (
    hav_char_p  dst ,
    hav_qword_t len ,
    __fmt_p     fmt ,
    __str_p     str
)
{
    /* check for the nullptr */

    static const hav_char_t __str_NULL_buf [] = "(NULL)" ;
    static const hav_qword_t __str_NULL_len = sizeof(__str_NULL_buf) - 1 ;

    if (hav_is_nullptr(str->data)) {
        if (len < __str_NULL_len) {
            return 0 ;
        }

        if (hav_is_nullptr(dst)) {
            return __str_NULL_len ;
        }

        hav_cpynstr(dst, __str_NULL_buf, __str_NULL_len) ;
        return __str_NULL_len ;
    }

    /* compute the length */

    hav_qword_t str_len = hav_lenstr(str->data) ;
    hav_qword_t buf_len = 2 * !!fmt->prefix[0] + str_len ;

    if (fmt->precision && fmt->precision < buf_len) {
        /* truncate the string */
        buf_len = fmt->precision ;
        str_len = buf_len - 2 * !!fmt->prefix[0] ;
    }

    hav_qword_t pad_len [2] = { 0 , 0 } ;

    if (buf_len < fmt->width) {
        /* align the string */
        pad_len[0] = fmt->width - buf_len ;
        pad_len[1] = fmt->width - buf_len ;

        if ('>' == fmt->alignment) {
            /* right */
            pad_len[1] = 0 ;
        } else if ('<' == fmt->alignment) {
            /* left */
            pad_len[0] = 0 ;
        } else {
            /* center */
            pad_len[0] /= 2 ;
            pad_len[1] = fmt->width - (buf_len + pad_len[0]) ;
        }

        buf_len = fmt->width ;
    }

    if (hav_is_nullptr(dst)) {
        return buf_len ;
    }

    /* check the buffer length */

    if (len < buf_len) {
        return 0 ;
    }

    /* generate the string */

    hav_qword_t n = 0 ;

    while (pad_len[0]--) {
        dst[n++] = fmt->padding_char ;
    }

    if (fmt->prefix[0]) {
        dst[n++] = fmt->prefix[0] ;
    }

    hav_cpynstr(dst + n, str->data, str_len) ;
    n += str_len ;

    if (fmt->suffix[0]) {
        dst[n++] = fmt->suffix[0] ;
    }

    while (pad_len[1]--) {
        dst[n++] = fmt->padding_char ;
    }

    return n ;
}

hav_char_p __fmtuint (
    __fmt_p fmt ,
    __num_p num
) ;

hav_char_p __fmtfloat (
    __fmt_p fmt ,
    __num_p num
) ;

hav_qword_t __fmtnum (
    hav_char_p  dst ,
    hav_qword_t len ,
    __fmt_p     fmt ,
    __num_p     num
)
{
    if (!fmt->precision) {
        fmt->precision = 4 ;
    }

    if ('t' == num->notation) {
        /* convert a value into its scientific notation */

        double scien_num ;
        double scien_base = 10.0 ;

        if ('u' == num->type) {
            scien_num = (double)num->as_uint ;
        } else if ('i' == num->type) {
            scien_num = (double)num->as_int ;
        } else {
            scien_num = (double)num->as_float ;

            if ('p' == num->type) {
                scien_base = 2.0 ;
            }
        }

        int64_t exp_num = 0 ;

        while (!(int64_t)scien_num) {
            scien_num *= scien_base ;
            --exp_num ;
        }

        while ((int64_t)scien_num) {
            if (!(int64_t)(scien_num / scien_base))
                break ;
            
            scien_num /= scien_base ;
            ++exp_num ;
        }

        num->as_float = scien_num ;
        num->exp_base = (hav_char_t)scien_base ;
        num->exp_num = exp_num ;
        num->type = 'f' ;
    }

    if ('i' == num->type) {
        /* convert signed integer to unsigned integer */
        if (num->as_int < 0) {
            num->sign = '-' ;
            num->as_int = -num->as_int ;
        }
        num->type = 'u' ;
    }

    /* generate the string */

    __str_t str ;

    if ('u' == num->type) {
        str.data = __fmtuint(fmt, num) ;
    } else {
        str.data = __fmtfloat(fmt, num) ;
    }

    /* clear format information */

    fmt->precision = 0 ;
    fmt->prefix[0] = fmt->prefix[1] = 0 ;
    fmt->suffix[0] = fmt->suffix[1] = 0 ;

    return __fmtstr(dst, len, fmt, &str) ;
}

static const hav_char_t __fmt_digs_u [] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" ;
static const hav_char_t __fmt_digs_l [] = "0123456789abcdefghijklmnopqrstuvwxyz" ;

hav_char_p __fmtuint (
    __fmt_p fmt ,
    __num_p num
)
{
    static hav_char_t buf [
        /* sign   */  1 +
        /* prefix */  2 +
        /* value  */ 64 +
        /* suffix */  2 +
        /* END    */  1
    ] ;

    hav_char_p bp = buf ;
    hav_char_p tb = (hav_char_p)__fmt_digs_u ;

    if (num->sign) {
        /* set the sign */
        *bp++ = num->sign ;
    }

    if (fmt->prefix[0]) {
        *bp++ = fmt->prefix[0] ;

        if (fmt->prefix[1]) {
            *bp++ = fmt->prefix[1] ;
        }
    }

    if ('a' <= fmt->prefix[1] || 'a' <= fmt->suffix[0]) {
        /* upper or lower case digits */
        tb = (hav_char_p)__fmt_digs_l ;
    }

    /* compute the digits */

    uint64_t digs = 0 ;
    uint64_t val ;
    
    val = num->as_uint ;

    do {
        val /= num->base ;
        ++digs ;
    } while (val) ;

    /* convert the number to string */

    val = num->as_uint ;
    num->as_uint = digs ;

    do {
        bp[--digs] = tb[val % num->base] ;
        val /= num->base ;
    } while (val) ;

    bp += num->as_uint ;

    if (fmt->suffix[0]) {
        *bp++ = fmt->suffix[0] ;

        if (fmt->suffix[1]) { /* never set in this version */
            *bp++ = fmt->suffix[1] ;
        }
    }

    *bp = 0 ;
    return buf ;
}

hav_char_p __fmtfloat (
    __fmt_p fmt ,
    __num_p num
)
{
    static hav_char_t buf [
        /* sign     */  1 +
        /* integer  */ 64 +
        /* dot      */  1 +
        /* decimal  */ 64 +
        /* exp type */  1 +
        /* exp sign */  1 +
        /* exponent */ 64 +
        /* END      */  1
    ] ;

    hav_char_p bp = buf ;

    if (num->sign) {
        /* set the sign */
        *bp++ = num->sign ;
    } else {
      if ((uint64_t)num->as_float < 0) {
        num->as_float = -num->as_float ;
        *bp++ = '-' ;
      }
    }

    uint64_t digs , val ;

    /* compute the digits of integer part */

    uint64_t int_digs = 0 ;
    
    val = (uint64_t)num->as_float ;

    do {
        val /= 10 ;
        ++int_digs ;
    } while (val) ;

    /* convert the integer part to string */

    val = (uint64_t)num->as_float ;
    digs = int_digs ;

    do {
        bp[--int_digs] = (val % 10) + '0' ;
        val /= 10 ;
    } while (val) ;

    bp += digs ;

    /* convert the decimal part to string */

    uint64_t dec_digs = fmt->precision ;

    num->as_float -= (uint64_t)num->as_float ;

    while (dec_digs--) {
        num->as_float *= 10 ;
    }

    val = (uint64_t)round(num->as_float) ; /* decimal part */

    struct lconv *lc = localeconv() ;
    uint64_t dec_point_len = hav_lenstr(lc->decimal_point) ;

    if ((sizeof(buf) - 1) < (uint64_t)(bp - buf) + dec_point_len * fmt->precision) {
        return HAV_NULL ;
    }

    if (val) {
        hav_cpystr(bp, lc->decimal_point) ;
        bp += dec_point_len ;

        digs = fmt->precision ;

        do {
            bp[--fmt->precision] = (val % 10) + '0' ;
            val /= 10 ;
        } while (val) ;

        bp += digs ;
    } else if (fmt->prefix[0] || fmt->suffix[0]) {
        hav_cpystr(bp, lc->decimal_point) ;
        bp += dec_point_len ;

        while (fmt->precision--) {
            *bp++ = '0' ;
        }
    }

    /* check the exponent */

    if (num->exp_num) {
        *bp++ = num->exp_type ;

        /* exponent sign */

        if (num->exp_num < 0) {
            *bp++ = '-' ;
            num->exp_num = -num->exp_num ;
        } else {
            *bp++ = '+' ;
        }

        /* compute the digits of the exponent */

        int_digs = 0 ;
    
        val = (uint64_t)num->exp_num ;

        do {
            val /= 10 ;
            ++int_digs ;
        } while (val) ;

        /* convert the exponent to string */

        val = (uint64_t)num->exp_num ;
        digs = int_digs ;

        do {
            bp[--int_digs] = (val % 10) + '0' ;
            val /= 10 ;
        } while (val) ;

        bp += digs ;
    }

    *bp = 0 ;
    return buf ;
}