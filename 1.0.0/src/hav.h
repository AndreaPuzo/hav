#ifndef _HAV_H
# define _HAV_H

# include <stdint.h>

typedef uint8_t  u8_t  ;
typedef uint16_t u16_t ;
typedef uint32_t u32_t ;
typedef uint64_t u64_t ;
typedef int8_t   i8_t  ;
typedef int16_t  i16_t ;
typedef int32_t  i32_t ;
typedef int64_t  i64_t ;
typedef float    f32_t ;
typedef double   f64_t ;

typedef struct hav_ide_s hav_ide_t ;
typedef struct hav_sde_s hav_sde_t ;
typedef struct hav_mem_s hav_mem_t ;
typedef struct hav_s     hav_t     ;

struct hav_ide_s {
  u16_t segx ;
  u64_t addr ;
} ;

struct hav_sde_s {
  u64_t addr ;
  u64_t size ;
  u16_t perm ;
} ;

struct hav_mem_s {
  u64_t  size ;
  u8_t * data ;

  u32_t (* read  ) (hav_t *, u64_t, u64_t, void *) ;
  u32_t (* write ) (hav_t *, u64_t, u64_t, const void *) ;
} ;

enum {
  HAV_REG_AX ,
  HAV_REG_CX ,
  HAV_REG_DX ,
  HAV_REG_BX ,
  HAV_REG_SP ,
  HAV_REG_BP ,
  HAV_REG_SI ,
  HAV_REG_DI ,

  HAV_N_REGS
} ;

enum {
  HAV_SEG_CS ,
  HAV_SEG_DS ,
  HAV_SEG_ES ,
  HAV_SEG_SS ,

  HAV_N_SEGS
} ;

enum {
  __CF   =  0 ,
  __A1   =  1 ,
  __PF   =  2 ,
  __A00  =  3 ,
  __ACF  =  4 ,
  __A01  =  5 ,
  __ZF   =  6 ,
  __SF   =  7 ,
  __TF   =  8 ,
  __IF   =  9 ,
  __DF   = 10 ,
  __OF   = 11 ,
  __IOPL = 12 ,
  __VB   = 14
} ;

enum {
  __E   = 0 ,
  __X   = 1 ,
  __R   = 2 ,
  __W   = 3 ,
  __SDT = 4 ,
  __IDT = 5 ,
} ;

enum {
  HAV_SEG_PERM_E    = 1 << __E    ,
  HAV_SEG_PERM_X    = 1 << __X    ,
  HAV_SEG_PERM_R    = 1 << __R    ,
  HAV_SEG_PERM_W    = 1 << __W    ,
  HAV_SEG_PERM_SDT  = 1 << __SDT  ,
  HAV_SEG_PERM_IDT  = 1 << __IDT  ,
  HAV_SEG_PERM_IOPL = 3 << __IOPL
  
} ;

enum {
  HAV_FLAG_C    = 1 << __CF   ,
  HAV_FLAG_A1   = 1 << __A1   ,
  HAV_FLAG_P    = 1 << __PF   ,
  HAV_FLAG_A00  = 1 << __A00  ,
  HAV_FLAG_AC   = 1 << __ACF  ,
  HAV_FLAG_A01  = 1 << __A01  ,
  HAV_FLAG_Z    = 1 << __ZF   ,
  HAV_FLAG_S    = 1 << __SF   ,
  HAV_FLAG_T    = 1 << __TF   ,
  HAV_FLAG_I    = 1 << __IF   ,
  HAV_FLAG_D    = 1 << __DF   ,
  HAV_FLAG_O    = 1 << __OF   ,
  HAV_FLAG_IOPL = 3 << __IOPL ,
  HAV_FLAG_VB   = 1 << __VB
} ;

enum {
  HAV_IRQ_DIV_BY_ZERO     ,
  HAV_IRQ_SINGLE_STEP     ,
  HAV_IRQ_NON_MASKABLE    ,
  HAV_IRQ_BREAKPOINT      ,
  HAV_IRQ_UNDEFINED_INST  ,
  HAV_IRQ_GENERAL_PROTECT ,
  HAV_IRQ_GENERAL_FAULT   ,
  HAV_IRQ_SEGMENT_FAULT   ,
  HAV_IRQ_STACK_OVERFLOW  ,
  HAV_IRQ_STACK_UNDERFLOW ,
  HAV_IRQ_OUT_OF_MEMORY   ,

  HAV_N_IRQS = 0x100
} ;

struct hav_s {
  u64_t     cks               ; // clocks
  u32_t     flags             ; // status flags
  u64_t     ip                ; // next instruction pointer
  u64_t     regs [HAV_N_REGS] ; // registers
  u64_t     fp   [HAV_N_REGS] ; // floating-point registers
  u16_t     segs [HAV_N_SEGS] ; // segments
  u32_t     irq               ; // interrupt request
  u64_t     idt               ; // interrupt descriptor table
  u64_t     sdt               ; // segment descriptor table
  hav_mem_t mem               ; // random access memory

  u32_t (* raise ) (hav_t *, u32_t) ;
  u32_t (* read  ) (hav_t *, u16_t, u64_t, u64_t, void *) ;
  u32_t (* write ) (hav_t *, u16_t, u64_t, u64_t, const void *) ;
  u32_t (* clock ) (hav_t *) ;
  u32_t (* load  ) (hav_t *, const char *, int) ;
  u32_t (* save  ) (hav_t *, const char *, int) ;

  struct {
    u64_t  ip         ;
    u8_t   data [16]  ;
    u8_t * code       ;
    u8_t   opcode [2] ;

    struct {
      u8_t  has_REP   : 1 ;
      u8_t  REP_cond  : 1 ;
      u8_t  has_ZOV   : 1 ;
      u8_t  oprd_size : 2 ;
      u8_t  has_AOV   : 1 ;
      u8_t  addr_size : 2 ;
      u8_t  has_SOV   : 1 ;
      u16_t segx          ;
    } ;

    struct {
      u8_t mod : 2 ;
      u8_t reg : 3 ;
      u8_t rm  : 3 ;
    } ;

    struct {
      u8_t scale : 2 ;
      u8_t idx   : 3 ;
      u8_t base  : 3 ;
    } ;

    i64_t disp ;
    u64_t addr ;
    i64_t imm  ;
  } inst ;

  struct {
    u32_t flags             ;
    u64_t ip                ;
    u64_t sp                ;
    u64_t bp                ;
    u16_t segs [HAV_N_SEGS] ;
  } sysenv ;

  struct {
    u8_t  verbose : 1 ;
    u64_t max_cks     ;
  } opts ;
} ;

enum {
  HAV_MAG_0 = 0x4570BEEF , // operating system image
  HAV_MAG_1 = 0x4570FADE , // operating system image
  HAV_MAG_2 = 0x4570DECA , // machine main data
  HAV_MAG_3 = 0x4570CAFE   // machine data
} ;

# define KB 1024
# define MB (1024 * KB)
# define GB (1024 * MB)

#ifdef _MSC_VER
# include <stdlib.h>
# define hav_byteswap_16(__x) _byteswap_ushort(__x)
# define hav_byteswap_32(__x) _byteswap_ulong(__x)
# define hav_byteswap_64(__x) _byteswap_uint64(__x)
#else
# include <byteswap.h>
# define hav_byteswap_16(__x) bswap_16(__x)
# define hav_byteswap_32(__x) bswap_32(__x)
# define hav_byteswap_64(__x) bswap_64(__x)
#endif

u32_t vm_mem_read (hav_t * hav, u64_t addr, u64_t size, void * data) ;
u32_t vm_mem_write (hav_t * hav, u64_t addr, u64_t size, const void * data) ;
u32_t vm_raise (hav_t * hav, u32_t irq) ;
u32_t vm_read (hav_t * hav, u16_t segx, u64_t addr, u64_t size, void * data) ;
u32_t vm_write (hav_t * hav, u16_t segx, u64_t addr, u64_t size, const void * data) ;
u32_t vm_clock (hav_t * hav) ;
u32_t vm_load_img (hav_t * hav, const char * fn, int mag) ;
u32_t vm_save_img (hav_t * hav, const char * fn, int mag) ;

#endif
