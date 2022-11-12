#ifndef _HAV_H
# define _HAV_H

# define _HAV_VERSION_MAJOR 0
# define _HAV_VERSION_MINOR 0
# define _HAV_VERSION_PATCH 0
# define _HAV_VERSION       ((_HAV_VERSION_MAJOR << 16) | (_HAV_VERSION_MINOR))

# define __HAV__ _HAV_VERSION

# ifdef _HAV_IS_EXTERNAL
#  include <hav/havcfg.h>
#  include <hav/havdef.h>
#  include <hav/havstr.h>
#  include <hav/havio.h>
#  include <hav/havinst.h>
# else
#  include "havcfg.h"
#  include "havdef.h"
#  include "havstr.h"
#  include "havio.h"
#  include "havinst.h"
# endif

_HAV_API hav_dword_t hav_ctor (
    hav_p       hav         ,
    hav_qword_t max_natives
) ;

_HAV_API hav_dword_t hav_dtor (
    hav_p hav
) ;

_HAV_API hav_dword_t hav_add_native (
    hav_p        hav    ,
    hav_native_t native ,
    hav_qword_p  addr
) ;

_HAV_API hav_dword_t hav_clock (
    hav_p      hav         ,
    hav_byte_t print_asm   ,
    hav_byte_t print_stack
) ;

_HAV_API hav_dword_t hav_clocks (
    hav_p       hav         ,
    hav_byte_t  print_asm   ,
    hav_byte_t  print_stack ,
    hav_qword_t clocks
) ;

#endif