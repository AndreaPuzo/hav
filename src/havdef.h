#ifndef _HAVDEF_H
# define _HAVDEF_H

# include <stdint.h>

typedef char     hav_char_t  , * hav_char_p  ;
typedef uint8_t  hav_byte_t  , * hav_byte_p  ;
typedef uint16_t hav_word_t  , * hav_word_p  ;
typedef uint32_t hav_dword_t , * hav_dword_p ;
typedef uint64_t hav_qword_t , * hav_qword_p ;

typedef struct hav_s hav_t , * hav_p ;
typedef struct hav_native_s hav_native_t , * hav_native_p ;

struct hav_s {
    hav_dword_t state ;

    struct {
        hav_byte_p  buf ;
        hav_qword_t len ;
        hav_qword_t ip  ;
    } code ;
    
    struct {
        hav_byte_p  buf ;
        hav_qword_t len ;
    } data ;

    struct {
        hav_qword_p buf ;
        hav_qword_t len ;
        hav_qword_t bp  ;
        hav_qword_t sp  ;
    } stack ;

    struct {
        hav_native_p buf ;
        hav_qword_t  len ;
    } natives ;
} ;

struct hav_native_s {
    hav_char_p    name ;
    hav_byte_t    argc ;
    hav_dword_t (*func)(hav_p, hav_byte_t, hav_qword_p) ; 
} ;

#endif