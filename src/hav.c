#ifdef _HAV_IS_EXTERNAL
# include <hav/hav.h>
#else
# include "hav.h"
#endif

#include <math.h>

#ifdef _HAV_PRINT_ERRORS
# define _eprint hav_fmteprint
#else
# define _eprint(__fmt, ...)
#endif

# define _NATIVE(__hav) (__hav)->natives

_HAV_API hav_dword_t hav_ctor (
    hav_p       hav         ,
    hav_qword_t max_natives
)
{
    /* check if the machine is running */
    if (hav->state) {
        return HAV_FAILURE ;
    }

    /* allocate the native buffer */
    hav->natives.buf = (hav_native_p)hav_alloc(max_natives * sizeof(hav_native_t)) ;

    if (hav_is_nullptr(hav->natives.buf)) {
        return HAV_FAILURE ;
    }

    hav->natives.len = max_natives ;
    hav->state = 1 ; /* ready to start */

    return HAV_SUCCESS ;
}

_HAV_API hav_dword_t hav_dtor (
    hav_p hav
)
{
    if (hav_is_nullptr(hav)) {
        return HAV_FAILURE ;
    }

    hav->state = 0 ;

    hav->code.buf = HAV_NULL ;
    hav->code.len = hav->code.ip = 0 ;

    hav->data.buf = HAV_NULL ;
    hav->data.len = 0 ;

    hav->stack.buf = HAV_NULL ;
    hav->stack.len = hav->stack.bp = hav->stack.sp = 0 ;

    if (!hav_is_nullptr(hav->natives.buf)) {
        hav_dealloc(hav->natives.buf) ;
    }

    hav->natives.buf = HAV_NULL ;
    hav->natives.len = 0 ;

    return HAV_SUCCESS ;
}

_HAV_API hav_dword_t hav_add_native (
    hav_p        hav    ,
    hav_native_t native ,
    hav_qword_p  addr
)
{
    hav_qword_t i ;

    for (i = 0 ; i < _NATIVE(hav).len ; ++i) {
        if (hav_is_nullptr(_NATIVE(hav).buf[i].func)) {
            _NATIVE(hav).buf[i] = native ;
            break ;
        }
    }

    if (!hav_is_nullptr(addr)) {
        *addr = i ;
    }

    return (i != _NATIVE(hav).len) ? HAV_SUCCESS : HAV_FAILURE ;
}

# define _CODE(__hav)  (__hav)->code
# define _IP(__hav)    _CODE(__hav).ip
# define _DATA(__hav)  (__hav)->data
# define _STACK(__hav) (__hav)->stack
# define _BP(__hav)    _STACK(__hav).bp
# define _SP(__hav)    _STACK(__hav).sp

#define __uint(__bits) uint ## __bits ## _t
#define __int(__bits)  int ## __bits ## _t

hav_dword_t __fetch_inst (
    hav_p      hav       ,
    hav_byte_t print_asm
)
{
    static hav_char_p inst_names [] = {
        "Nop  " , "Stop " ,
        "Push " , "Push " , "Push " , "Push " ,
        "Pop  " ,
        "Dup  " , "Dup  " , "Dup  " , "Dup  " ,
        "Swap " , "Swap " , "Swap " , "Swap " ,
        "Load " , "Load " , "Load " , "Load " ,
        "Store" , "Store" , "Store" , "Store" ,
        "Not  " ,
        "And  " , "Or   " , "Xor  " , "Sal  " , "Shr  " , "Sar  " ,
        "Neg  " ,
        "FNeg " ,
        "Add  " , "Sub  " , "Mul  " , "Div  " , "Mod  " ,
        "IMul " , "IDiv " , "IMod " ,
        "FAdd " , "FSub " , "FMul " , "FDiv " , "FMod " ,
        "Nat  " ,
        "Jump " , "Jump " ,
        "Jump0" , "Jump0" ,
        "Jump1" , "Jump1" ,
        "Inv  " , "Inv  " , "Ret  " ,
        "IsEq " , "IsLs " , "IsGt " ,
        "IsNe " , "IsLe " , "IsGe " ,
        "IsFEq" , "IsFLs" , "IsFGt" ,
        "IsFNe" , "IsFLe" , "IsFGe" ,
        "Cvfu " , "Cvfi " ,
        "Cvuf " , "Cvif " ,
        "CrSt " ,
    } ;

    enum {
        _NO_ARGS           ,
        _STACK_1           ,
        _STACK_2           ,
        _STACK_3           ,
        _CODE_1_8          ,
        _CODE_1_16         ,
        _CODE_1_32         ,
        _CODE_1_64         ,
        _CODE_1_8_STACK_1  ,
        _CODE_1_16_STACK_1 ,
        _CODE_1_32_STACK_1 ,
        _CODE_1_64_STACK_1 ,
        _CODE_2_8          ,
        _CODE_1_8_STACK_N  ,
        _CODE_1_16_STACK_N ,
        _CODE_1_32_STACK_N ,
        _CODE_1_64_STACK_N ,
    } ;

    static hav_char_t inst_forms [] = {
        _NO_ARGS   , _NO_ARGS   ,
        _CODE_1_8  , _CODE_1_16 , _CODE_1_32 , _CODE_1_64 ,
        _STACK_1   ,
        _CODE_1_8_STACK_N  , _CODE_1_16_STACK_N , _CODE_1_32_STACK_N , _CODE_1_64_STACK_N ,
        _CODE_1_8_STACK_N  , _CODE_1_16_STACK_N , _CODE_1_32_STACK_N , _CODE_1_64_STACK_N ,
        _STACK_1   , _STACK_1   , _STACK_1   , _STACK_1   ,
        _STACK_2   , _STACK_2   , _STACK_2   , _STACK_2   ,
        _STACK_1   ,
        _STACK_2   , _STACK_2   , _STACK_2   , _STACK_2   , _STACK_2   ,  _STACK_2   ,
        _STACK_1   ,
        _STACK_1   ,
        _STACK_2   , _STACK_2   , _STACK_2   , _STACK_2   , _STACK_2   ,
        _STACK_2   , _STACK_2   , _STACK_2   ,
        _STACK_2   , _STACK_2   , _STACK_2   , _STACK_2   , _STACK_2   ,
        _CODE_2_8  ,
        _CODE_1_32 , _CODE_1_64 ,
        _CODE_1_32_STACK_1 , _CODE_1_64_STACK_1 ,
        _CODE_1_32_STACK_1 , _CODE_1_64_STACK_1 ,
        _CODE_1_32_STACK_1 , _CODE_1_64_STACK_1 , _STACK_2   ,
        _STACK_2   , _STACK_2   , _STACK_2   ,
        _STACK_2   , _STACK_2   , _STACK_2   ,
        _STACK_2   , _STACK_2   , _STACK_2   ,
        _STACK_2   , _STACK_2   , _STACK_2   ,
        _STACK_1   , _STACK_1   ,
        _STACK_1   , _STACK_1   ,
        _NO_ARGS   ,
    } ;

    if (HAV_N_INSTS <= _CODE(hav).buf[_IP(hav)]) {
        _eprint("0x%016lX | error: invalid instruction 0x%02X\n",
            _IP(hav), _CODE(hav).buf[_IP(hav)]) ;
        return HAV_FAILURE ;
    }

    hav_char_p inst_name = inst_names[_CODE(hav).buf[_IP(hav)]] ;
    hav_char_t inst_form = inst_forms[_CODE(hav).buf[_IP(hav)]] ;

    if (print_asm) {
        hav_fmtprint("0x%016lX | " , _IP(hav)) ;
    }

    ++_IP(hav) ;

    switch (inst_form) {
    case _NO_ARGS : {
        if (print_asm) {
            hav_fmtprint("%<8s\n" , inst_name) ;
        }
    } break ;

    case _STACK_1 : {
        if (_SP(hav) <= _BP(hav)) {
            _eprint("error: SP is out of frame stack\n") ;
            return HAV_FAILURE ;
        }

        if (print_asm) {
            hav_qword_t sp_bp = (_SP(hav) - 1) - _BP(hav) ;
            hav_fmtprint("%<8s $[0x%016lX + %u]\n",
                inst_name, _BP(hav), sp_bp) ;
        }
    } break ;

    case _STACK_2 : {
        if (_SP(hav) <= _BP(hav) + 1) {
            _eprint("error: SP is out of frame stack\n") ;
            return HAV_FAILURE ;
        }

        if (print_asm) {
            hav_qword_t sp_bp = (_SP(hav) - 1) - _BP(hav) ;
            hav_fmtprint("%<8s $[0x%016lX + %u] , $[0x%016lX + %u]\n",
                inst_name, _BP(hav), sp_bp - 1, _BP(hav), sp_bp) ;
        }
    } break ;

#define _CODE_1_X(__bits, __form)                                                 \
    case _CODE_1_ ## __bits : {                                                   \
        if (_CODE(hav).len < _IP(hav) + ((__bits) / 8)) {                         \
            _eprint("error: missing operands for \'%s\' (format: %u)\n",          \
                inst_name, inst_form) ;                                           \
            return HAV_FAILURE ;                                                  \
        }                                                                         \
                                                                                  \
        if (print_asm) {                                                          \
            __uint(__bits) arg = *(__uint(__bits) *)(_CODE(hav).buf + _IP(hav)) ; \
            hav_fmtprint("%<8s 0x%0" __form "lX\n" , inst_name, (uint64_t)arg) ;  \
        }                                                                         \
    } break ;

    _CODE_1_X(8, "2")
    _CODE_1_X(16, "4")
    _CODE_1_X(32, "8")
    _CODE_1_X(64, "16")

#define _CODE_1_X_STACK_1(__bits, __form)                                         \
    case _CODE_1_ ## __bits ## _STACK_1 : {                                       \
        if (_CODE(hav).len < _IP(hav) + ((__bits) / 8)) {                         \
            _eprint("error: missing operands for \'%s\' (format: %u)\n",          \
                inst_name, inst_form) ;                                           \
            return HAV_FAILURE ;                                                  \
        }                                                                         \
                                                                                  \
        if (_SP(hav) <= _BP(hav)) {                                               \
            _eprint("error: SP is out of frame stack\n") ;                        \
            return HAV_FAILURE ;                                                  \
        }                                                                         \
                                                                                  \
        if (print_asm) {                                                          \
            __uint(__bits) arg = *(__uint(__bits) *)(_CODE(hav).buf + _IP(hav)) ; \
            hav_qword_t sp_bp = (_SP(hav) - 1) - _BP(hav) ;                       \
            hav_fmtprint("%<8s 0x%0" __form "lX , $[0x%016lX + %u]\n",            \
                inst_name, (uint64_t)arg, _BP(hav), sp_bp) ;                      \
        }                                                                         \
    } break ;

    _CODE_1_X_STACK_1(8, "2")
    _CODE_1_X_STACK_1(16, "4")
    _CODE_1_X_STACK_1(32, "8")
    _CODE_1_X_STACK_1(64, "16")

    case _CODE_2_8 : {
        if (_CODE(hav).len < _IP(hav) + 2) {
            _eprint("error: missing operands for \'%s\' (format: %u)\n",
                inst_name, inst_form) ;
            return HAV_FAILURE ;
        }

        if (print_asm) {
            hav_byte_t arg0 = _CODE(hav).buf[_IP(hav) + 0] ;
            hav_byte_t arg1 = _CODE(hav).buf[_IP(hav) + 1] ;
            hav_fmtprint("%<8s 0x%02X , 0x%02X\n" , inst_name, arg0, arg1) ;
        }
    } break ;

#define _CODE_1_X_STACK_N(__bits, __form)                                        \
    case _CODE_1_ ## __bits ## _STACK_N : {                                      \
        if (_CODE(hav).len < _IP(hav) + sizeof(__uint(__bits))) {                \
            _eprint("error: missing operand for \'%s\' (format: %u)\n",          \
                inst_name, inst_form) ;                                          \
            return HAV_FAILURE ;                                                 \
        }                                                                        \
                                                                                 \
        __uint(__bits) arg = *(__uint(__bits) *)(_CODE(hav).buf + _IP(hav)) ;    \
                                                                                 \
        if (_SP(hav) < _BP(hav) + arg) {                                         \
            _eprint("error: SP is out of frame stack\n") ;                       \
            return HAV_FAILURE ;                                                 \
        }                                                                        \
                                                                                 \
        if (print_asm) {                                                         \
            hav_fmtprint("%<8s 0x%0" __form "lX\n" , inst_name, (uint64_t)arg) ; \
        }                                                                        \
    } break ;

    _CODE_1_X_STACK_N(8, "2")
    _CODE_1_X_STACK_N(16, "4")
    _CODE_1_X_STACK_N(32, "8")
    _CODE_1_X_STACK_N(64, "16")
    }

    return HAV_SUCCESS ;
}

hav_dword_t __stack_push (
    hav_p       hav ,
    hav_qword_t val
)
{
    if (_STACK(hav).len <= _SP(hav)) {
        _eprint("error: SP is out of frame stack\n") ;
        return HAV_FAILURE ;
    }

    _STACK(hav).buf[_SP(hav)] = val ;
    ++_SP(hav) ;

    return HAV_SUCCESS ;
}

hav_dword_t __stack_pop (
    hav_p       hav ,
    hav_qword_p val
)
{
    if (_SP(hav) <= _BP(hav)) {
        _eprint("error: SP is out of frame stack\n") ;
        return HAV_FAILURE ;
    }

    --_SP(hav) ;

    if (val) {
        *val = _STACK(hav).buf[_SP(hav)] ;
    }

    return HAV_SUCCESS ;
}

hav_dword_t __load_at (
    hav_p       hav  ,
    hav_qword_t addr ,
    hav_qword_t len  ,
    hav_byte_p  data
)
{
    hav_byte_p  src_buf ;
    hav_qword_t src_len ;

    if (0 == (addr >> 48)) {
        src_buf = _DATA(hav).buf ;
        src_len = _DATA(hav).len ;
    } else if (1 == (addr >> 48)) {
        src_buf = (hav_byte_p)_STACK(hav).buf ;
        src_len = _STACK(hav).len ;
    } else if (2 == (addr >> 48)) {
        src_buf = _CODE(hav).buf ;
        src_len = _CODE(hav).len ;
    }

    if (src_len < addr + len) {
        _eprint("0x%016lX is out of memory\n", addr) ;
        return HAV_FAILURE ;
    }

    memcpy(data, src_buf + addr, len) ;
    
    return HAV_SUCCESS ;
}

hav_dword_t __store_at (
    hav_p            hav  ,
    hav_qword_t      addr ,
    hav_qword_t      len  ,
    const hav_byte_p data
)
{
    hav_byte_p  dst_buf ;
    hav_qword_t dst_len ;

    if (0 == (addr >> 48)) {
        dst_buf = _DATA(hav).buf ;
        dst_len = _DATA(hav).len ;
    } else if (1 == (addr >> 48)) {
        dst_buf = (hav_byte_p)_STACK(hav).buf ;
        dst_len = _STACK(hav).len ;
    } else if (2 == (addr >> 48)) {
        dst_buf = _CODE(hav).buf ;
        dst_len = _CODE(hav).len ;
    }

    if (dst_len < addr + len) {
        _eprint("0x%016lX is out of memory\n", addr) ;
        return HAV_FAILURE ;
    }

    memcpy(dst_buf + addr, data, len) ;

    return HAV_SUCCESS ;
}

hav_dword_t __execute_inst (
    hav_p hav
)
{
    switch (_CODE(hav).buf[_IP(hav) - 1]) {
    case HAV_INST_NOP : {
        /* nothing */
    } break ;

    case HAV_INST_STOP : {
        hav->state = 0 ;
    } break ;

#define _INST_PUSH_X(__bits)                                                  \
    case HAV_INST_PUSH_ ## __bits : {                                         \
        __uint(__bits) arg = *(__uint(__bits) *)(_CODE(hav).buf + _IP(hav)) ; \
        _IP(hav) += sizeof(__uint(__bits)) ;                                  \
        return __stack_push(hav, arg) ;                                       \
    } break ;

    _INST_PUSH_X(8)
    _INST_PUSH_X(16)
    _INST_PUSH_X(32)
    _INST_PUSH_X(64)

    case HAV_INST_POP : {
        --_SP(hav) ;
    } break ;

#define _INST_DUP_X(__bits)                                                   \
    case HAV_INST_DUP_ ## __bits : {                                          \
        __uint(__bits) arg = *(__uint(__bits) *)(_CODE(hav).buf + _IP(hav)) ; \
        _IP(hav) += sizeof(__uint(__bits)) ;                                  \
        return __stack_push(hav, _STACK(hav).buf[(_SP(hav) - 1) - arg]) ;     \
    } break ;

    _INST_DUP_X(8)
    _INST_DUP_X(16)
    _INST_DUP_X(32)
    _INST_DUP_X(64)

#define _INST_SWAP_X(__bits)                                                      \
    case HAV_INST_SWAP_ ## __bits : {                                             \
        __uint(__bits) arg = *(__uint(__bits) *)(_CODE(hav).buf + _IP(hav)) ;     \
        _IP(hav) += sizeof(__uint(__bits)) ;                                      \
        hav_qword_t tmp = _STACK(hav).buf[(_SP(hav) - 1) - arg] ;                 \
        _STACK(hav).buf[(_SP(hav) - 1) - arg] = _STACK(hav).buf[(_SP(hav) - 1)] ; \
        _STACK(hav).buf[_SP(hav) - 1] = tmp ;                                     \
    } break ;

    _INST_SWAP_X(8)
    _INST_SWAP_X(16)
    _INST_SWAP_X(32)
    _INST_SWAP_X(64)

#define _INST_LOAD_X(__bits)                               \
    case HAV_INST_LOAD_ ## __bits : {                      \
        return __load_at(hav,                              \
            _STACK(hav).buf[_SP(hav) - 1],                 \
            sizeof(__uint(__bits)),                        \
            (hav_byte_p)(_STACK(hav).buf + (_SP(hav) - 1)) \
        ) ;                                                \
    } break ;

    _INST_LOAD_X(8)
    _INST_LOAD_X(16)
    _INST_LOAD_X(32)
    _INST_LOAD_X(64)

#define _INST_STORE_X(__bits)                        \
    case HAV_INST_STORE_ ## __bits : {               \
        --_SP(hav) ;                                 \
        return __store_at(hav,                       \
            _STACK(hav).buf[_SP(hav) - 1],           \
            sizeof(__uint(__bits)),                  \
            (hav_byte_p)(_STACK(hav).buf + _SP(hav)) \
        ) ;                                          \
    } break ;

    _INST_STORE_X(8)
    _INST_STORE_X(16)
    _INST_STORE_X(32)
    _INST_STORE_X(64)

#define __unynot(__arg) ~(__arg)
#define __unyneg(__arg) -(__arg)

#define _INST_UNARY(__ID, __fun, __typ)                            \
    case HAV_INST_ ## __ID : {                                     \
        __typ arg = *(__typ *)(_STACK(hav).buf + (_SP(hav) - 1)) ; \
        _STACK(hav).buf[_SP(hav) - 1] = __fun(arg) ;               \
    } break ;

    _INST_UNARY(NOT , __unynot , uint64_t) ;
    _INST_UNARY(NEG , __unyneg , uint64_t) ;
    _INST_UNARY(F_NEG , __unyneg , double) ;

#define __binand(__lhs, __rhs) ((__lhs) & (__rhs))
#define __binor(__lhs, __rhs)  ((__lhs) | (__rhs))
#define __binxor(__lhs, __rhs) ((__lhs) ^ (__rhs))
#define __binshl(__lhs, __rhs) ((__lhs) << (__rhs))
#define __binshr(__lhs, __rhs) ((__lhs) >> (__rhs))
#define __binadd(__lhs, __rhs) ((__lhs) + (__rhs))
#define __binsub(__lhs, __rhs) ((__lhs) - (__rhs))
#define __binmul(__lhs, __rhs) ((__lhs) * (__rhs))
#define __bindiv(__lhs, __rhs) ((__lhs) / (__rhs))
#define __binmod(__lhs, __rhs) ((__lhs) % (__rhs))

#define _INST_BINARY(__ID, __fun, __typ)                           \
    case HAV_INST_ ## __ID : {                                     \
        --_SP(hav) ;                                               \
        __typ lhs = *(__typ *)(_STACK(hav).buf + (_SP(hav) - 1)) ; \
        __typ rhs = *(__typ *)(_STACK(hav).buf + _SP(hav)) ;       \
        _STACK(hav).buf[_SP(hav) - 1] = __fun(lhs, rhs) ;          \
    } break ;

    _INST_BINARY(AND , __binand , uint64_t) ;
    _INST_BINARY(OR  , __binor  , uint64_t) ;
    _INST_BINARY(XOR , __binxor , uint64_t) ;
    _INST_BINARY(SHL , __binshl , uint64_t) ;
    _INST_BINARY(SHR , __binshr , uint64_t) ;
    _INST_BINARY(SAR , __binshr ,  int64_t) ;
    _INST_BINARY(ADD , __binadd , uint64_t) ;
    _INST_BINARY(SUB , __binsub , uint64_t) ;
    _INST_BINARY(MUL , __binmul , uint64_t) ;
    _INST_BINARY(DIV , __bindiv , uint64_t) ;
    _INST_BINARY(MOD , __binmod , uint64_t) ;
    _INST_BINARY(I_MUL , __binmul , int64_t) ;
    _INST_BINARY(I_DIV , __bindiv , int64_t) ;
    _INST_BINARY(I_MOD , __binmod , int64_t) ;
    _INST_BINARY(F_ADD , __binadd , double) ;
    _INST_BINARY(F_SUB , __binsub , double) ;
    _INST_BINARY(F_MUL , __binmul , double) ;
    _INST_BINARY(F_DIV , __bindiv , double) ;
    _INST_BINARY(F_MOD , fmod , double) ;

    case HAV_INST_NATIVE : {
        hav_byte_t natx = _CODE(hav).buf[_IP(hav) + 0] ;
        hav_byte_t argc = _CODE(hav).buf[_IP(hav) + 1] ;

        if (_NATIVE(hav).len <= natx) {
            _eprint("error: invalid native address\n") ;
            return HAV_FAILURE ;
        }

        if (argc < _NATIVE(hav).buf[natx].argc) {
            _eprint("error: \'%s\' requires at least %u stack-arguments\n",
                _NATIVE(hav).buf[natx].name, _NATIVE(hav).buf[natx].argc) ;
            return HAV_FAILURE ;
        }

        if (_SP(hav) < _BP(hav) + argc) {
            _eprint("error: not enough stack-arguments to invoke \'%s\'\n",
                _NATIVE(hav).buf[natx].name) ;
            return HAV_FAILURE ;
        }

        _IP(hav) += 2 * sizeof(hav_byte_t) ;

        __stack_push(hav, _BP(hav)) ;
        _BP(hav) = (_SP(hav) - 1) - argc ;
        
        _NATIVE(hav).buf[natx].func(hav, argc, _STACK(hav).buf + _BP(hav)) ;

        _SP(hav) = _BP(hav) ;
        _BP(hav) = _STACK(hav).buf[_SP(hav) + argc] ;
    } break ;

#define _INST_JUMP_X(__bits)                                                \
    case HAV_INST_JUMP_ ## __bits : {                                       \
        __int(__bits) arg = *(__int(__bits) *)(_CODE(hav).buf + _IP(hav)) ; \
        if (arg < 0) {                                                      \
            _IP(hav) += arg ;                                               \
        } else {                                                            \
            _IP(hav) += sizeof(__int(__bits)) + arg ;                       \
        }                                                                   \
    } break ;

    _INST_JUMP_X(32) ;
    _INST_JUMP_X(64) ;

#define _INST_JUMP_IF_X(__bits, __opr, __ID)                                \
    case HAV_INST_JUMP_IF_ ## __ID ## _ ## __bits : {                       \
        --_SP(hav) ;                                                        \
        __int(__bits) arg = *(__int(__bits) *)(_CODE(hav).buf + _IP(hav)) ; \
                                                                            \
        if (0 __opr _STACK(hav).buf[_SP(hav)]) {                            \
            if (arg < 0) {                                                  \
                _IP(hav) += arg ;                                           \
            } else {                                                        \
                _IP(hav) += sizeof(__int(__bits)) + arg ;                   \
            }                                                               \
        } else {                                                            \
            _IP(hav) += sizeof(__int(__bits)) ;                             \
        }                                                                   \
    } break ;

    _INST_JUMP_IF_X(32, ==, 0) ;
    _INST_JUMP_IF_X(64, ==, 0) ;
    _INST_JUMP_IF_X(32, !=, 1) ;
    _INST_JUMP_IF_X(64, !=, 1) ;

#define _INST_INVOKE_X(__bits)                                                        \
    case HAV_INST_INVOKE_ ## __bits : {                                               \
        hav_qword_t ip = _IP(hav) + sizeof(__int(__bits)) ;                           \
                                                                                      \
        __int(__bits) addr = *(__int(__bits) *)(_CODE(hav).buf + _IP(hav)) ;          \
        if (addr < 0) {                                                               \
            _IP(hav) += addr ;                                                        \
        } else {                                                                      \
            _IP(hav) += sizeof(__int(__bits)) + addr ;                                \
        }                                                                             \
                                                                                      \
        --_SP(hav) ;                                                                  \
        hav_qword_t argc = _STACK(hav).buf[_SP(hav)] ;                                \
                                                                                      \
        if (_SP(hav) < _BP(hav) + argc) {                                             \
            _eprint("error: not enough stack-arguments to invoke 0x%016lX\n",         \
                _IP(hav)) ;                                                           \
            return HAV_FAILURE ;                                                      \
        }                                                                             \
                                                                                      \
        hav_qword_t bp = _SP(hav) - argc ;                                            \
                                                                                      \
        _SP(hav) += 2 ;                                                               \
        if (_STACK(hav).len < _SP(hav)) {                                             \
            _eprint("error: not enough stack slots to invoke 0x%016lX\n",             \
                _IP(hav)) ;                                                           \
            return HAV_FAILURE ;                                                      \
        }                                                                             \
                                                                                      \
        for (hav_qword_t idx = 0 ; idx <= argc ; ++idx) {                             \
            _STACK(hav).buf[_SP(hav) - idx] = _STACK(hav).buf[bp + argc - idx] ;      \
        }                                                                             \
                                                                                      \
        _STACK(hav).buf[bp + 0] = ip       ;                                          \
        _STACK(hav).buf[bp + 1] = _BP(hav) ;                                          \
                                                                                      \
        ++_SP(hav) ;                                                                  \
        _BP(hav) = bp ;                                                               \
    } break ;

    _INST_INVOKE_X(32) ;
    _INST_INVOKE_X(64) ;

    case HAV_INST_RETURN : {
        _BP(hav) = _STACK(hav).buf[_SP(hav) - 2] ;
        _IP(hav) = _STACK(hav).buf[_SP(hav) - 3] ;
        _STACK(hav).buf[_SP(hav) - 3] = _STACK(hav).buf[_SP(hav) - 1] ;
        _SP(hav) -= 2 ;
    } break ;

#define __cmpfun_eq(__lhs, __rhs) ((__lhs) == (__rhs))
#define __cmpfun_ls(__lhs, __rhs) ((__lhs) <  (__rhs))
#define __cmpfun_gt(__lhs, __rhs) ((__lhs) >  (__rhs))
#define __cmpfun_ne(__lhs, __rhs) ((__lhs) != (__rhs))
#define __cmpfun_le(__lhs, __rhs) ((__lhs) <= (__rhs))
#define __cmpfun_ge(__lhs, __rhs) ((__lhs) >= (__rhs))

#define _INST_COMP(__ID, __fun, __typ)                              \
    case HAV_INST_IS_ ## __ID : {                                    \
        --_SP(hav) ;                                                 \
        __typ lhs = *(__typ *)(_STACK(hav).buf + (_SP(hav) - 1)) ;   \
        __typ rhs = *(__typ *)(_STACK(hav).buf + _SP(hav)) ;         \
        _STACK(hav).buf[_SP(hav) - 1] = __fun(lhs, rhs) ;            \
    } break ;

    _INST_COMP(EQUAL            , __cmpfun_eq , uint64_t)
    _INST_COMP(LESS             , __cmpfun_ls , uint64_t)
    _INST_COMP(GREATER          , __cmpfun_gt , uint64_t)
    _INST_COMP(NOT_EQUAL        , __cmpfun_ne , uint64_t)
    _INST_COMP(EQUAL_OR_LESS    , __cmpfun_le , uint64_t)
    _INST_COMP(EQUAL_OR_GREATER , __cmpfun_ge , uint64_t)
    _INST_COMP(F_EQUAL            , __cmpfun_eq , double)
    _INST_COMP(F_LESS             , __cmpfun_ls , double)
    _INST_COMP(F_GREATER          , __cmpfun_gt , double)
    _INST_COMP(F_NOT_EQUAL        , __cmpfun_ne , double)
    _INST_COMP(F_EQUAL_OR_LESS    , __cmpfun_le , double)
    _INST_COMP(F_EQUAL_OR_GREATER , __cmpfun_ge , double)

#define _INST_CONV(__ID, __typdst, __typsrc)                                \
    case HAV_INST_ ## __ID : {                                              \
        __typsrc arg = *(__typsrc *)(_STACK(hav).buf + (_SP(hav) - 1)) ;    \
        *(__typdst *)(_STACK(hav).buf + (_SP(hav) - 1)) = (__typdst)(arg) ; \
    } break ;

    _INST_CONV(FLOAT_TO_UINT , uint64_t , double)
    _INST_CONV(FLOAT_TO_INT  , int64_t , double)
    _INST_CONV(UINT_TO_FLOAT , double , uint64_t)
    _INST_CONV(INT_TO_FLOAT  , double , int64_t)

    case HAV_INST_CLEAR_STACK : {
        _SP(hav) = _BP(hav) ;
    } break ;
    }

    return HAV_SUCCESS ;
}

void __dump_stack (
    hav_p       hav   ,
    hav_qword_t depth
)
{
    if (_SP(hav) < depth) {
        depth = _SP(hav) ;
    }

    hav_fmtprint("Stack (%u):\n", depth) ;
    while (depth) {
        hav_fmtprint("0x%016lX | 0x%016lX\n", (_SP(hav) - (depth + 1)), _STACK(hav).buf[_SP(hav) - depth--]) ;
    }
    hav_fmtprint("0x%016lX | (SP)\n", _SP(hav)) ;
}

_HAV_API hav_dword_t hav_clock (
    hav_p      hav         ,
    hav_byte_t print_asm   ,
    hav_byte_t print_stack
)
{
    if (!hav->state || hav_is_nullptr(hav)) {
        return HAV_FAILURE ;
    }

    if (hav->code.len <= hav->code.ip) {
        _eprint("IP (0x%lX) is out of code\n", hav->code.ip) ;
        return HAV_FAILURE ;
    }

    if (print_stack) {
        __dump_stack(hav, -1) ;
    }

    if (HAV_SUCCESS != __fetch_inst(hav, print_asm)) {
        return HAV_FAILURE ;
    }

    return __execute_inst(hav) ;
}

_HAV_API hav_dword_t hav_clocks (
    hav_p       hav         ,
    hav_byte_t  print_asm   ,
    hav_byte_t  print_stack ,
    hav_qword_t clocks
)
{
    if (!hav->state || hav_is_nullptr(hav)) {
        return HAV_FAILURE ;
    }

    hav_qword_t clock_idx = 0 ;

    do {
        if (hav->code.len <= hav->code.ip) {
            _eprint("IP (0x%lX) is out of code\n", hav->code.ip) ;
            return HAV_FAILURE ;
        }

        if (print_stack) {
            __dump_stack(hav, -1) ;
        }

        if (HAV_SUCCESS != __fetch_inst(hav, print_asm)) {
            return HAV_FAILURE ;
        }

        if (HAV_SUCCESS != __execute_inst(hav)) {
            return HAV_FAILURE ;
        }

        if ((hav_qword_t)-1 == clocks) {
            ++clock_idx ;
        }

//        getchar() ;
    } while (hav->state && clock_idx < clocks) ;

    return HAV_SUCCESS ;
}