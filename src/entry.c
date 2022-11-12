#ifdef _HAV_IS_EXTERNAL
# include <hav/hav.h>
#else
# include "hav.h"
#endif

#include <stdio.h>

hav_dword_t native_fmtprint (hav_p hav, hav_byte_t argc, hav_qword_p argv)
{
    hav_char_p ap = (hav_char_p)(argv + 1) ;

    if (1 == argc) {
        ap = HAV_NULL ;
    }

    hav_afmtprint(
        hav->data.buf + argv[0] ,
        ap
    ) ;

    return HAV_SUCCESS ;
}

hav_dword_t native_fmtstr (hav_p hav, hav_byte_t argc, hav_qword_p argv)
{
    hav_char_p ap = (hav_char_p)(argv + 3) ;

    if (3 == argc) {
        ap = HAV_NULL ;
    }

    hav_afmtstr(
        hav->data.buf + argv[0] ,
        argv[1]                 ,
        hav->data.buf + argv[2] ,
        ap
    ) ;

    return HAV_SUCCESS ;
}

hav_dword_t native_putchar (hav_p hav, hav_byte_t argc, hav_qword_p argv)
{
    putchar((int)argv[0]) ;
    return HAV_SUCCESS ;
}

int main (int argc, char **argv)
{
    hav_byte_t prog [] = {
        HAV_INST_PUSH_8        , 0x00 ,
        HAV_INST_PUSH_8        , 0xFF ,
        HAV_INST_PUSH_16       , 0x00 , 0x01 ,
        HAV_INST_PUSH_8        , 0x08 ,
        HAV_INST_PUSH_16       , 0x00 , 0x02 ,
        HAV_INST_ADD           ,
        HAV_INST_PUSH_8        , 0x01 ,
        HAV_INST_SUB           ,
        HAV_INST_NATIVE        , 0x01 , 0x04 ,
        HAV_INST_PUSH_8        , 0x00 ,
        HAV_INST_NATIVE        , 0x00 , 0x01 ,
        HAV_INST_PUSH_8        , 0x00 ,
        HAV_INST_PUSH_8        , 0x04 ,
        HAV_INST_PUSH_8        , 0x03 ,
        HAV_INST_PUSH_8        , 0x02 ,
        HAV_INST_PUSH_8        , 0x01 ,
        HAV_INST_PUSH_8        , 0x00 ,
        HAV_INST_LOAD_8        ,
        HAV_INST_SWAP_8        , 0x01 ,
        HAV_INST_LOAD_8        ,
        HAV_INST_SWAP_8        , 0x02 ,
        HAV_INST_LOAD_8        ,
        HAV_INST_SWAP_8        , 0x03 ,
        HAV_INST_LOAD_8        ,
        HAV_INST_SWAP_8        , 0x04 ,
        HAV_INST_LOAD_8        ,
        HAV_INST_SWAP_8        , 0x05 ,
        HAV_INST_LOAD_8        ,
        HAV_INST_POP           ,
        HAV_INST_NATIVE        , 0x02 , 0x01 ,
        HAV_INST_NATIVE        , 0x02 , 0x01 ,
        HAV_INST_NATIVE        , 0x02 , 0x01 ,
        HAV_INST_NATIVE        , 0x02 , 0x01 ,
        HAV_INST_NATIVE        , 0x02 , 0x01 ,
        HAV_INST_NOP           ,
        HAV_INST_PUSH_8        , 0x01 ,
        HAV_INST_PUSH_8        , 0x01 ,
        HAV_INST_PUSH_8        , 0x02 ,
        HAV_INST_INVOKE_32     , 0x0A , 0x00 , 0x00 , 0x00 ,
        HAV_INST_PUSH_8        , 0x02 ,
        HAV_INST_INT_TO_FLOAT  ,
        HAV_INST_PUSH_8        , 0x01 ,
        HAV_INST_NEG           ,
        HAV_INST_INT_TO_FLOAT  ,
        HAV_INST_F_ADD         ,
        HAV_INST_FLOAT_TO_UINT ,
        HAV_INST_STOP          ,

        // myAdd
        HAV_INST_POP           ,
        HAV_INST_PUSH_16       , 0x00 , 0x01 ,
        HAV_INST_SWAP_8        , 0x02 ,
        HAV_INST_SWAP_8        , 0x01 ,
        HAV_INST_ADD           ,
        HAV_INST_NATIVE        , 0x00 , 0x02 ,
        HAV_INST_RETURN        ,
        HAV_INST_STOP          , // secure guard
    } ;

    hav_byte_t data [4 * 1024] ;
    hav_qword_t stack [1024] ;

    hav_t hav ;

    hav.code.buf = prog         ;
    hav.code.len = sizeof(prog) ;
    hav.code.ip  = 0            ;

    hav.data.buf = data         ;
    hav.data.len = sizeof(data) ;

    hav.stack.buf = stack             ;
    hav.stack.len = sizeof(stack) / 8 ;
    hav.stack.sp = hav.stack.sp = 0   ;

    hav.state = 0 ;

    hav_ctor(&hav, 64) ;

    hav_cpystr(hav.data.buf + 0x100, "%i\n") ;

    hav_add_native(&hav, (hav_native_t){ "fmtprint" , 1 , native_fmtprint }, HAV_NULL) ;
    hav_add_native(&hav, (hav_native_t){ "fmtstr"   , 3 , native_fmtstr   }, HAV_NULL) ;
    hav_add_native(&hav, (hav_native_t){ "putchar"  , 1 , native_putchar  }, HAV_NULL) ;

    hav_fmtprint("--------------------------------\n") ;
    hav.state = 1 ;
    hav.code.ip = 0 ;
    hav_clocks(&hav, 1, 1, -1) ;
    hav_fmtprint("--------------------------------\n") ;
    hav.state = 1 ;
    hav.code.ip = 0 ;
    hav_clocks(&hav, 1, 0, -1) ;
    hav_fmtprint("--------------------------------\n") ;
    hav.state = 1 ;
    hav.code.ip = 0 ;
    hav_clocks(&hav, 0, 1, -1) ;
    hav_fmtprint("--------------------------------\n") ;
    hav.state = 1 ;
    hav.code.ip = 0 ;
    hav_clocks(&hav, 0, 0, -1) ;

    hav_dtor(&hav) ;

    return EXIT_SUCCESS ;
}