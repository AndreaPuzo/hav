#include "hav.h"
#include <stdio.h>
#include <string.h>

const u64_t __c_bits_by_type [] = {  8 , 16 , 32 , 64 } ;
const u64_t __c_size_by_type [] = {  1 ,  2 ,  4 ,  8 } ;

extern u32_t vaddr_to_paddr (hav_t * hav, u16_t segx, u64_t * addr, u64_t * size, u8_t perm) ;

u32_t exec_out  (hav_t * hav, u8_t port, u64_t size, const void * data) ;
u32_t exec_in   (hav_t * hav, u8_t port, u64_t size, void * data) ;
u32_t exec_push (hav_t * hav, u64_t size, const void * data) ;
u32_t exec_pop  (hav_t * hav, u64_t size, void * data) ;
u32_t exec_int  (hav_t * hav, u32_t irq) ;
u32_t exec_iret (hav_t * hav) ;

u32_t fetch (hav_t * hav)
{
  if (NULL == memset(&hav->inst, 0, sizeof(hav->inst)))
    return hav->raise(hav, HAV_IRQ_GENERAL_FAULT) ;

  hav->flags |= HAV_FLAG_VB ;

  if (HAV_N_IRQS != hav->read(hav, hav->segs[HAV_SEG_CS], hav->ip, sizeof(hav->inst.data), hav->inst.data))
    return hav->irq ;

  hav->flags &= ~HAV_FLAG_VB ;

  u64_t ip = 0 ;
  u32_t group = 0 ;

_scan_group :

  if (0 == (group & 1)) {
    if (0xD0 == hav->inst.data[ip] && 0xD1 == hav->inst.data[ip]) {
      hav->inst.has_REP = 1 ;
      hav->inst.REP_cond = hav->inst.data[ip] & 1 ;
      ++ip ;

      // scan the next group
      group |= 1 ; goto _scan_group ;
    }
  }

  if (0 == (group & 2)) {
    if (0x60 <= hav->inst.data[ip] && hav->inst.data[ip] <= 0x63) {
      hav->inst.has_SOV = 1 ;
      hav->inst.segx = hav->inst.data[ip] & 3 ;
      ++ip ;

      // scan the next group
      group |= 2 ; goto _scan_group ;
    }
  }

  if (0 == (group & 4)) {
    if (0x66 == hav->inst.data[ip]) {
      hav->inst.has_ZOV = 1 ;
      ++ip ;
      
      // scan the next group
      group |= 4 ; goto _scan_group ;
    }
  }

  if (0 == (group & 8)) {
    if (0x67 == hav->inst.data[ip]) {
      hav->inst.has_AOV = 1 ;
      ++ip ;
      
      // scan the next group
      group |= 8 ; goto _scan_group ;
    }
  }

  // scan the opcodes

  hav->inst.opcode[0] = hav->inst.data[ip] ;

  if (0x0F == hav->inst.data[ip]) {
    ++ip ;
    hav->inst.opcode[1] = hav->inst.data[ip] ;
  }

  ++ip ;

  if (0 == hav->inst.has_SOV)
    hav->inst.segx = hav->segs[HAV_SEG_DS] ;

  // update the instruction pointer

  hav->inst.ip = ip ;
  hav->ip += ip ;

  return HAV_N_IRQS ;
}

u32_t fetch_SIB (hav_t * hav, u64_t * ip)
{
  u8_t byte = hav->inst.code[*ip] ;
  ++*ip ;

  hav->inst.scale = (byte >> 6) & 3 ;
  hav->inst.idx   = (byte >> 3) & 7 ;
  hav->inst.base  = (byte >> 0) & 7 ;

  switch (hav->inst.mod) {
  case 0 :
    if (HAV_REG_BP != hav->inst.base) {
      if (0 != hav->inst.has_AOV)
        hav->inst.addr += *(u32_t *)(hav->regs + hav->inst.base) ;
      else
        hav->inst.addr += *(u64_t *)(hav->regs + hav->inst.base) ;
    }

    if (HAV_REG_SP != hav->inst.idx) {
      if (0 != hav->inst.has_AOV)
        hav->inst.addr += *(u32_t *)(hav->regs + hav->inst.idx) * (1 << hav->inst.scale) ;
      else
        hav->inst.addr += *(u64_t *)(hav->regs + hav->inst.idx) * (1 << hav->inst.scale) ;
    }
    break ;

  case 1 :
  case 2 :
    if (0 != hav->inst.has_AOV)
      hav->inst.addr += *(u32_t *)(hav->regs + hav->inst.base) ;
    else
      hav->inst.addr += *(u64_t *)(hav->regs + hav->inst.base) ;
    
    if (HAV_REG_SP != hav->inst.idx) {
      if (0 != hav->inst.has_AOV)
        hav->inst.addr += *(u32_t *)(hav->regs + hav->inst.idx) * (1 << hav->inst.scale) ;
      else
        hav->inst.addr += *(u64_t *)(hav->regs + hav->inst.idx) * (1 << hav->inst.scale) ;
    }
    break ;
  
  default :
    return hav->raise(hav, HAV_IRQ_NON_MASKABLE) ;
  }

  return HAV_N_IRQS ;
}

u32_t fetch_disp (hav_t * hav, u8_t size, u64_t * ip)
{
  switch (size) {
  case 0 :
    hav->inst.disp = *(i8_t *)(hav->inst.code + *ip) ;
    break ;

  case 2 :
    hav->inst.disp = *(i32_t *)(hav->inst.code + *ip) ;
    break ;
  
  default :
    hav->raise(hav, HAV_IRQ_NON_MASKABLE) ;
  }

  *ip += 1 << size ;

  return HAV_N_IRQS ;
}

u32_t fetch_ModRM (hav_t * hav, u64_t * ip)
{
  u8_t byte = hav->inst.code[*ip] ;
  ++*ip ;

  hav->inst.mod = (byte >> 6) & 3 ;
  hav->inst.reg = (byte >> 3) & 7 ;
  hav->inst.rm  = (byte >> 0) & 7 ;

  switch (hav->inst.mod) {
  case 0 :
    if (HAV_REG_BP == hav->inst.rm) {
      // set the segment
      if (0 == hav->inst.has_SOV)
        hav->inst.segx = hav->segs[HAV_SEG_CS] ;

      // set the offset
      if (0 != hav->inst.has_AOV)
        hav->inst.addr += *(u32_t *)&hav->ip ;
      else
        hav->inst.addr += *(u64_t *)&hav->ip ;

      // fetch the displacement
      if (HAV_N_IRQS != fetch_disp(hav, 2, ip))
        return hav->irq ;

      // set the displacement
      hav->inst.addr += hav->inst.disp ;
      break ;
    }
  case 1 :
  case 2 : {
    // set the segment
    if (0 == hav->inst.has_SOV) {
      switch (hav->inst.rm) {
      case HAV_REG_AX : case HAV_REG_CX : case HAV_REG_DX : case HAV_REG_BX :
        hav->inst.segx = hav->segs[HAV_SEG_DS] ;
        break ;

      case HAV_REG_SP :
        break ;

      case HAV_REG_BP :
        hav->inst.segx = hav->segs[HAV_SEG_SS] ;
        break ;

      case HAV_REG_SI :
        hav->inst.segx = hav->segs[HAV_SEG_DS] ;
        break ;

      case HAV_REG_DI :
        hav->inst.segx = hav->segs[HAV_SEG_ES] ;
        break ;
      
      default :
        return hav->raise(hav, HAV_IRQ_NON_MASKABLE) ;
      }
    }

    if (HAV_REG_SP == hav->inst.rm) {
      // fetch the SIB byte
      if (HAV_N_IRQS != fetch_SIB(hav, ip))
        return hav->irq ;
    } else {
      if (0 != hav->inst.has_AOV)
        hav->inst.addr += *(u32_t *)(hav->regs + hav->inst.rm) ;
      else
        hav->inst.addr += *(u64_t *)(hav->regs + hav->inst.rm) ;
    }

    u32_t irq = HAV_N_IRQS ;

    switch (hav->inst.mod) {
    case 0 :
      break ;

    case 1 :
      // fetch the displacement
      irq = fetch_disp(hav, 0, ip) ;
      break ;

    case 2 :
      // fetch the displacement
      irq = fetch_disp(hav, 2, ip) ;
      break ;

    default :
      return hav->raise(hav, HAV_IRQ_NON_MASKABLE) ;
    }

    if (HAV_N_IRQS != irq)
      return hav->irq ;

    // set the displacement
    hav->inst.addr += hav->inst.disp ;
  } break ;

  case 3 :
    break ;

  default :
    return hav->raise(hav, HAV_IRQ_NON_MASKABLE) ;
  }

  // fprintf(stderr, "MODRM (0x%02X) = %u|%u|%u ; ADDR = 0x%016llX\n", byte, hav->inst.mod, hav->inst.reg, hav->inst.rm, hav->inst.addr) ;

  return HAV_N_IRQS ;
}

u32_t fetch_imm (hav_t * hav, u8_t size, u64_t * ip)
{
  switch (size) {
  case 0 :
    hav->inst.imm = *(i8_t *)(hav->inst.code + *ip) ;
    break ;

  case 1 :
    hav->inst.imm = *(i16_t *)(hav->inst.code + *ip) ;
    break ;

  case 2 :
    hav->inst.imm = *(i32_t *)(hav->inst.code + *ip) ;
    break ;

  case 3 :
    hav->inst.imm = *(i64_t *)(hav->inst.code + *ip) ;
    break ;

  default :
    hav->raise(hav, HAV_IRQ_NON_MASKABLE) ;
  }

  *ip += 1 << size ;

  return HAV_N_IRQS ;
}

u32_t exec_out (hav_t * hav, u8_t port, u64_t size, const void * data)
{
  switch (port) {
  case 0x00 : // stdin
    return hav->raise(hav, HAV_IRQ_GENERAL_FAULT) ;

  case 0x01 : { // stdout
    for (u64_t i = 0 ; i < size ; ++i)
      fputc(*(u8_t *)(data + i), stdout) ;
  } break ;

  case 0x02 : { // stderr
    for (u64_t i = 0 ; i < size ; ++i)
      fputc(*(u8_t *)(data + i), stderr) ;
  } break ;

  default :
    return hav->raise(hav, HAV_IRQ_GENERAL_FAULT) ;
  }

  return HAV_N_IRQS ;
}

u32_t exec_in (hav_t * hav, u8_t port, u64_t size, void * data)
{
  switch (port) {
  case 0x00 : { // stdin
    for (u64_t i = 0 ; i < size ; ++i)
      *(u8_t *)(data + i) = (u8_t)fgetc(stdin) ;
  } break ;

  default :
    return hav->raise(hav, HAV_IRQ_GENERAL_FAULT) ;
  }

  return HAV_N_IRQS ;
}

u32_t exec_push (hav_t * hav, u64_t size, const void * data)
{
  hav->regs[HAV_REG_SP] -= size ;

  if (HAV_N_IRQS != hav->write(hav, hav->segs[HAV_SEG_SS], hav->regs[HAV_REG_SP], size, data))
    return hav->raise(hav, HAV_IRQ_STACK_OVERFLOW) ;

  return HAV_N_IRQS ;
}

u32_t exec_pop (hav_t * hav, u64_t size, void * data)
{
  if (HAV_N_IRQS != hav->write(hav, hav->segs[HAV_SEG_SS], hav->regs[HAV_REG_SP], size, data))
    return hav->raise(hav, HAV_IRQ_STACK_UNDERFLOW) ;

  hav->regs[HAV_REG_SP] += size ;

  return HAV_N_IRQS ;
}

u32_t exec_int (hav_t * hav, u32_t irq)
{
  irq &= 0xFF ;

  if (
    (0 != (hav->flags & HAV_FLAG_I)) ||
    (HAV_IRQ_NON_MASKABLE == irq)
  ) {
    if (HAV_N_IRQS != exec_push(hav, 4, &hav->flags))
      return hav->irq ;

    if (HAV_N_IRQS != exec_push(hav, 2, hav->segs + HAV_SEG_CS))
      return hav->irq ;

    if (HAV_N_IRQS != exec_push(hav, 8, &hav->ip))
      return hav->irq ;

    u64_t _size = sizeof(hav_ide_t) ;
    u64_t _addr = hav->idt + irq * _size ;

    if (HAV_N_IRQS != hav->mem.read(hav, _addr, _size, hav->segs + HAV_SEG_CS))
      return hav->irq ;

    if (HAV_N_IRQS != hav->mem.read(hav, _addr, _size + 2, &hav->ip))
      return hav->irq ;

    hav->flags &= ~HAV_FLAG_I ;
  }

  hav->irq = irq ;

  return HAV_N_IRQS ;
}

u32_t exec_iret (hav_t * hav)
{
  if (HAV_N_IRQS != exec_pop(hav, 8, &hav->ip))
    return hav->irq ;

  if (HAV_N_IRQS != exec_pop(hav, 2, hav->segs + HAV_SEG_CS))
    return hav->irq ;

  if (HAV_N_IRQS != exec_pop(hav, 4, &hav->flags))
    return hav->irq ;

  return HAV_N_IRQS ;
}

u32_t check_cond (hav_t * hav, u32_t cond)
{
#define _SO(__op) (((hav->flags >> __SF) & 1) __op ((hav->flags >> __OF) & 1))

  switch (cond) {
  case 0  : return 0 != (hav->flags & HAV_FLAG_C) ;
  case 1  : return 0 == (hav->flags & HAV_FLAG_C) ;
  case 2  : return 0 != (hav->flags & HAV_FLAG_P) ;
  case 3  : return 0 == (hav->flags & HAV_FLAG_P) ;
  case 4  : return 0 != (hav->flags & HAV_FLAG_Z) ;
  case 5  : return 0 == (hav->flags & HAV_FLAG_Z) ;
  case 6  : return 0 != (hav->flags & HAV_FLAG_S) ;
  case 7  : return 0 == (hav->flags & HAV_FLAG_S) ;
  case 8  : return 0 != (hav->flags & HAV_FLAG_O) ;
  case 9  : return 0 == (hav->flags & HAV_FLAG_O) ;
  case 10 : return 0 == (hav->flags & (HAV_FLAG_C | HAV_FLAG_Z)) ;
  case 11 : return 0 != (hav->flags & (HAV_FLAG_C | HAV_FLAG_Z)) ;
  case 12 : return 0 == (hav->flags & HAV_FLAG_Z) && _SO(==) ;
  case 13 : return 0 != (hav->flags & HAV_FLAG_Z) || _SO(!=) ;
  case 14 : return _SO(==) ;
  case 15 : return _SO(!=) ;
  default : return 0 ;
  }
}

u32_t get_reg (hav_t * hav, u8_t regx, u8_t size, u64_t * data)
{
  regx &= 7 ;

  switch (size) {
  case 0 : {
    if (regx < 5)
      *data = ((i8_t *)(hav->regs + regx))[0] ;
    else
      *data = ((i8_t *)(hav->regs + (regx - 5)))[1] ;
  } break ;

  case 1  : *data = *(u16_t *)(hav->regs + regx) ; break ;
  case 2  : *data = *(u32_t *)(hav->regs + regx) ; break ;
  case 3  : *data = *(u64_t *)(hav->regs + regx) ; break ;
  default : return hav->raise(hav, HAV_IRQ_GENERAL_FAULT) ;
  }

  // fprintf(stderr, "(GET) REG[%u, %u] = 0x%016llX\n", regx, 1 << size, *data) ;

  return HAV_N_IRQS ;
}

u32_t get_ireg (hav_t * hav, u8_t regx, u8_t size, i64_t * data)
{
  regx &= 7 ;

  switch (size) {
  case 0 : {
    if (regx < 5)
      *data = ((i8_t *)(hav->regs + regx))[0] ;
    else
      *data = ((i8_t *)(hav->regs + (regx - 5)))[1] ;
  } break ;

  case 1  : *data = *(i16_t *)(hav->regs + regx) ; break ;
  case 2  : *data = *(i32_t *)(hav->regs + regx) ; break ;
  case 3  : *data = *(i64_t *)(hav->regs + regx) ; break ;
  default : return hav->raise(hav, HAV_IRQ_GENERAL_FAULT) ;
  }

  return HAV_N_IRQS ;
}

u32_t set_reg (hav_t * hav, u8_t regx, u8_t size, u64_t data)
{
  regx &= 7 ;

  switch (size) {
  case 0 : {
    if (regx < 5)
      ((u8_t *)(hav->regs + regx))[0] = (u8_t)data ;
    else
      ((u8_t *)(hav->regs + (regx - 5)))[1] = (u8_t)data ;
  } break ;

  case 1  : *(u16_t *)(hav->regs + regx) = (u16_t)data ; break ;
  case 2  : *(u64_t *)(hav->regs + regx) = (u32_t)data ; break ;
  case 3  : *(u64_t *)(hav->regs + regx) = (u64_t)data ; break ;
  default : return hav->raise(hav, HAV_IRQ_GENERAL_FAULT) ;
  }

  // fprintf(stderr, "(SET) REG[%u, %u] = 0x%016llX\n", regx, 1 << size, data) ;

  return HAV_N_IRQS ;
}

u32_t set_ireg (hav_t * hav, u8_t regx, u8_t size, i64_t data)
{
  regx &= 7 ;

  switch (size) {
  case 0 : {
    if (regx < 5)
      ((i8_t *)(hav->regs + regx))[0] = (i8_t)data ;
    else
      ((i8_t *)(hav->regs + (regx - 5)))[1] = (i8_t)data ;
  } break ;

  case 1  : *(i16_t *)(hav->regs + regx) = (i16_t)data ; break ;
  case 2  : *(i64_t *)(hav->regs + regx) = (i32_t)data ; break ;
  case 3  : *(i64_t *)(hav->regs + regx) = (i64_t)data ; break ;
  default : return hav->raise(hav, HAV_IRQ_GENERAL_FAULT) ;
  }

  return HAV_N_IRQS ;
}

u32_t get_modrm_reg (hav_t * hav, u8_t size, u64_t * data)
{
  return get_reg(hav, hav->inst.reg, size, data) ;
}

u32_t get_modrm_ireg (hav_t * hav, u8_t size, i64_t * data)
{
  return get_ireg(hav, hav->inst.reg, size, data) ;
}

u32_t get_modrm_rm (hav_t * hav, u8_t size, u64_t * data)
{
  switch (hav->inst.mod) {
  case 0 :
  case 1 :
  case 2 :
    return hav->read(hav, hav->inst.segx, hav->inst.addr, 1 << size, data) ;

  case 3 :
    return get_reg(hav, hav->inst.rm, size, data) ;

  default :
    return hav->raise(hav, HAV_IRQ_NON_MASKABLE) ;
  }
}

u32_t get_modrm_irm (hav_t * hav, u8_t size, i64_t * data)
{
  switch (hav->inst.mod) {
  case 0 :
  case 1 :
  case 2 : {
    u64_t _data ;

    if (HAV_N_IRQS != hav->read(hav, hav->inst.segx, hav->inst.addr, 1 << size, &_data))
      return hav->irq ;
    
    switch (size) {
    case 0 : *data = (i8_t )_data ; break ;
    case 1 : *data = (i16_t)_data ; break ;
    case 2 : *data = (i32_t)_data ; break ;
    case 3 : *data = (i64_t)_data ; break ;

    default :
      return hav->raise(hav, HAV_IRQ_NON_MASKABLE) ;
    }
  } break ;

  case 3 :
    return get_ireg(hav, hav->inst.rm, size, data) ;

  default :
    return hav->raise(hav, HAV_IRQ_NON_MASKABLE) ;
  }
}

u32_t set_modrm_reg (hav_t * hav, u8_t size, u64_t data)
{
  return set_reg(hav, hav->inst.reg, size, data) ;
}

u32_t set_modrm_ireg (hav_t * hav, u8_t size, i64_t data)
{
  return set_ireg(hav, hav->inst.reg, size, data) ;
}

u32_t set_modrm_rm (hav_t * hav, u8_t size, u64_t data)
{
  switch (hav->inst.mod) {
  case 0 :
  case 1 :
  case 2 :
    return hav->write(hav, hav->inst.segx, hav->inst.addr, size, &data) ;

  case 3 :
    return set_reg(hav, hav->inst.rm, size, data) ;

  default :
    return hav->raise(hav, HAV_IRQ_NON_MASKABLE) ;
  }
}

u32_t set_modrm_irm (hav_t * hav, u8_t size, i64_t data)
{
  switch (hav->inst.mod) {
  case 0 :
  case 1 :
  case 2 :
    return hav->write(hav, hav->inst.segx, hav->inst.addr, size, &data) ;

  case 3 :
    return set_ireg(hav, hav->inst.rm, size, data) ;

  default :
    return hav->raise(hav, HAV_IRQ_NON_MASKABLE) ;
  }
}

u32_t modrm_rm_is_mem (hav_t * hav)
{
  return hav->inst.mod < 3 ;
}

typedef union __fp_s __fp_t ;

union __fp_s {
  f32_t s ;
  f64_t d ;
} ;

u64_t __c_size [] = { 1 , 2 , 4 , 8 } ;
u64_t __c_bits [] = { 8 , 16 , 32 , 64 } ;

u64_t __c_sign [] = {
  1ULL << (8  - 1) ,
  1ULL << (16 - 1) ,
  1ULL << (32 - 1) ,
  1ULL << (64 - 1)
} ;

u64_t __c_mask [] = {
  (1ULL << 8 ) - 1 ,
  (1ULL << 16) - 1 ,
  (1ULL << 32) - 1 ,
  -1ULL
} ;

void update_flags_mov (hav_t * hav, u8_t size, u64_t r0)
{
  hav->flags &= ~(HAV_FLAG_C | HAV_FLAG_P | HAV_FLAG_Z | HAV_FLAG_S | HAV_FLAG_O) ;

  u8_t s0 = !!(r0 & __c_sign[size]) ;

  hav->flags |= (0 == (r0 & 1)) * HAV_FLAG_P ;
  hav->flags |= (0 == r0)       * HAV_FLAG_Z ;
  hav->flags |= s0              * HAV_FLAG_S ;
}

void update_flags_add_adc (hav_t * hav, u8_t size, u64_t r0, u64_t r1, u64_t r2)
{
  hav->flags &= ~(HAV_FLAG_C | HAV_FLAG_P | HAV_FLAG_Z | HAV_FLAG_S | HAV_FLAG_O) ;

  u8_t s0 = !!(r0 & __c_sign[size]) ;
  u8_t s1 = !!(r1 & __c_sign[size]) ;
  u8_t s2 = !!(r2 & __c_sign[size]) ;

  hav->flags |= (0 != (r2 >> __c_bits[size])) * HAV_FLAG_C ;
  hav->flags |= (0 == (r2 & 1)) * HAV_FLAG_P ;
  hav->flags |= (0 == r2)       * HAV_FLAG_Z ;
  hav->flags |= s2              * HAV_FLAG_S ;
  hav->flags |= (!(s0 ^ s1) && s0 ^ s2) * HAV_FLAG_O ;
}

void update_flags_sub_sbb (hav_t * hav, u8_t size, u64_t r0, u64_t r1, u64_t r2)
{
  hav->flags &= ~(HAV_FLAG_C | HAV_FLAG_P | HAV_FLAG_Z | HAV_FLAG_S | HAV_FLAG_O) ;

  u8_t s0 = !!(r0 & __c_sign[size]) ;
  u8_t s1 = !!(r1 & __c_sign[size]) ;
  u8_t s2 = !!(r2 & __c_sign[size]) ;

  hav->flags |= (0 != (r2 >> __c_bits[size])) * HAV_FLAG_C ;
  hav->flags |= (0 == (r2 & 1)) * HAV_FLAG_P ;
  hav->flags |= (0 == r2)       * HAV_FLAG_Z ;
  hav->flags |= s2              * HAV_FLAG_S ;
  hav->flags |= (s0 ^ s1 && s0 ^ s2) * HAV_FLAG_O ;
}

void update_flags_mul_div (hav_t * hav, u8_t size, u64_t r0, u64_t r1, u64_t r2)
{
  hav->flags &= ~(HAV_FLAG_C | HAV_FLAG_P | HAV_FLAG_Z | HAV_FLAG_S | HAV_FLAG_O) ;

  u8_t s2 = !!(r2 & __c_sign[size]) ;

  hav->flags |= (0 != (r2 >> __c_bits[size])) * (HAV_FLAG_C | HAV_FLAG_O) ;
  hav->flags |= (0 == (r2 & 1)) * HAV_FLAG_P ;
  hav->flags |= (0 == r2)       * HAV_FLAG_Z ;
  hav->flags |= s2              * HAV_FLAG_S ;
}

void update_flags_imul_idiv (hav_t * hav, u8_t size, i64_t r0, i64_t r1, i64_t r2)
{
  hav->flags &= ~(HAV_FLAG_C | HAV_FLAG_P | HAV_FLAG_Z | HAV_FLAG_S | HAV_FLAG_O) ;

  u8_t s2 = !!(r2 & __c_sign[size]) ;

  hav->flags |= (-1 != (r2 >> __c_bits[size])) * (HAV_FLAG_C | HAV_FLAG_O) ;
  hav->flags |= (0 == (r2 & 1)) * HAV_FLAG_P ;
  hav->flags |= (0 == r2)       * HAV_FLAG_Z ;
  hav->flags |= s2              * HAV_FLAG_S ;
}

void update_flags_and_or_xor (hav_t * hav, u8_t size, u64_t r0, u64_t r1, u64_t r2)
{
  hav->flags &= ~(HAV_FLAG_C | HAV_FLAG_P | HAV_FLAG_Z | HAV_FLAG_S | HAV_FLAG_O) ;

  u8_t s2 = !!(r2 & __c_sign[size]) ;

  hav->flags |= (0 == (r2 & 1)) * HAV_FLAG_P ;
  hav->flags |= (0 == r2)       * HAV_FLAG_Z ;
  hav->flags |= s2              * HAV_FLAG_S ;
}

void update_flags_shl (hav_t * hav, u8_t size, u64_t r0, u64_t r1, u64_t r2)
{
  hav->flags &= ~(HAV_FLAG_C | HAV_FLAG_P | HAV_FLAG_Z | HAV_FLAG_S | HAV_FLAG_O) ;

  u8_t s2 = !!(r2 & __c_sign[size]) ;

  hav->flags |= ((r0 >> (__c_bits[size] - r1)) & 1) * HAV_FLAG_C ;
  hav->flags |= (0 == (r2 & 1)) * HAV_FLAG_P ;
  hav->flags |= (0 == r2)       * HAV_FLAG_Z ;
  hav->flags |= s2              * HAV_FLAG_S ;

  if (1 == r1)
    hav->flags |= (0 != ((r0 >> (__c_bits[size] - 1)) & 1) ^ ((r0 >> (__c_bits[size] - 2)) & 1)) * HAV_FLAG_O ;
}

void update_flags_shr (hav_t * hav, u8_t size, u64_t r0, u64_t r1, u64_t r2)
{
  hav->flags &= ~(HAV_FLAG_C | HAV_FLAG_P | HAV_FLAG_Z | HAV_FLAG_S | HAV_FLAG_O) ;

  u8_t s2 = !!(r2 & __c_sign[size]) ;

  hav->flags |= ((r0 >> (r1 - 1)) & 1) * HAV_FLAG_C ;
  hav->flags |= (0 == (r2 & 1)) * HAV_FLAG_P ;
  hav->flags |= (0 == r2)       * HAV_FLAG_Z ;
  hav->flags |= s2              * HAV_FLAG_S ;

  if (1 == r1)
    hav->flags |= ((r0 >> (__c_bits[size] - 1)) & 1) * HAV_FLAG_O ;
}

void update_flags_sar (hav_t * hav, u8_t size, i64_t r0, i64_t r1, i64_t r2)
{
  hav->flags &= ~(HAV_FLAG_C | HAV_FLAG_P | HAV_FLAG_Z | HAV_FLAG_S | HAV_FLAG_O) ;

  u8_t s2 = !!(r2 & __c_sign[size]) ;

  hav->flags |= ((r0 >> (r1 - 1)) & 1) * HAV_FLAG_C ;
  hav->flags |= (0 == (r2 & 1)) * HAV_FLAG_P ;
  hav->flags |= (0 == r2)       * HAV_FLAG_Z ;
  hav->flags |= s2              * HAV_FLAG_S ;

  if (1 == r1)
    hav->flags |= ((r0 >> (__c_bits[size] - 1)) & 1) * HAV_FLAG_O ;
}

void update_fflags (hav_t * hav, u8_t size, __fp_t r0, __fp_t r1, __fp_t r2)
{

}

u32_t exec (hav_t * hav)
{
  u8_t * code = hav->inst.data + hav->inst.ip ;
  hav->inst.code = code ;

  u64_t ip = 0 ;

  struct {
    u64_t  r0, r1, r2 ;
    i64_t  i0, i1, i2 ;
    __fp_t f0, f1, f2 ;
  } data ;

#define _UIP(__size) \
  {                  \
    ip += (__size) ; \
  }

#define _USI(__size)                      \
  {                                       \
    if (0 == (hav->flags & HAV_FLAG_D))   \
      hav->regs[HAV_REG_SI] += (__size) ; \
    else                                  \
      hav->regs[HAV_REG_SI] -= (__size) ; \
  }

#define _UDI(__size)                      \
  {                                       \
    if (0 == (hav->flags & HAV_FLAG_D))   \
      hav->regs[HAV_REG_DI] += (__size) ; \
    else                                  \
      hav->regs[HAV_REG_DI] -= (__size) ; \
  }

#define _ZOV(__0, __1)              \
  {                                 \
    if (0 != hav->inst.has_ZOV)     \
      hav->inst.oprd_size = (__1) ; \
    else                            \
      hav->inst.oprd_size = (__0) ; \
  }

#define _AOV(__0, __1)              \
  {                                 \
    if (0 != hav->inst.has_AOV)     \
      hav->inst.addr_size = (__1) ; \
    else                            \
      hav->inst.addr_size = (__0) ; \
  }

#define _RAISE_0      \
  {                   \
    hav->ip += ip ;   \
    return hav->irq ; \
  }

#define _RAISE(__irq)                 \
  {                                   \
    hav->ip += ip ;                   \
    return hav->raise(hav, (__irq)) ; \
  }

// fetch, save
#define _UNAOP_MODRM_MR_0(__fun, __r0, __r1)                               \
  {                                                                        \
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))                               \
      _RAISE_0 ;                                                           \
                                                                           \
    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.__r0))  \
      _RAISE_0 ;                                                           \
                                                                           \
    data.__r1 = __fun(data.__r0) ;                                         \
                                                                           \
    if (HAV_N_IRQS != set_modrm_rm(hav, hav->inst.oprd_size, data.__r1))   \
      _RAISE_0 ;                                                           \
  }

// no fetch, save
#define _UNAOP_MODRM_MR_1(__fun, __r0, __r1)                               \
  {                                                                        \
    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.__r0))  \
      _RAISE_0 ;                                                           \
                                                                           \
    data.__r1 = __fun(data.__r0) ;                                         \
                                                                           \
    if (HAV_N_IRQS != set_modrm_rm(hav, hav->inst.oprd_size, data.__r1))   \
      _RAISE_0 ;                                                           \
  }

// fetch, save
#define _BINOP_MODRM_RM_0(__fun, __r0, __r1, __r2)                         \
  {                                                                        \
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))                               \
      _RAISE_0 ;                                                           \
                                                                           \
    if (HAV_N_IRQS != get_modrm_reg(hav, hav->inst.oprd_size, &data.__r0)) \
      _RAISE_0 ;                                                           \
                                                                           \
    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.__r1))  \
      _RAISE_0 ;                                                           \
                                                                           \
    data.__r2 = __fun(data.__r0, data.__r1) ;                              \
                                                                           \
    if (HAV_N_IRQS != set_modrm_reg(hav, hav->inst.oprd_size, data.__r2))  \
      _RAISE_0 ;                                                           \
  }

// no fetch, save
#define _BINOP_MODRM_RM_1(__fun, __r0, __r1, __r2)                         \
  {                                                                        \
    if (HAV_N_IRQS != get_modrm_reg(hav, hav->inst.oprd_size, &data.__r0)) \
      _RAISE_0 ;                                                           \
                                                                           \
    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.__r1))  \
      _RAISE_0 ;                                                           \
                                                                           \
    data.__r2 = __fun(data.__r0, data.__r1) ;                              \
                                                                           \
    if (HAV_N_IRQS != set_modrm_reg(hav, hav->inst.oprd_size, data.__r2))  \
      _RAISE_0 ;                                                           \
  }

// fetch, no save
#define _BINOP_MODRM_RM_3(__fun, __r0, __r1, __r2)                         \
  {                                                                        \
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))                               \
      _RAISE_0 ;                                                           \
                                                                           \
    if (HAV_N_IRQS != get_modrm_reg(hav, hav->inst.oprd_size, &data.__r0)) \
      _RAISE_0 ;                                                           \
                                                                           \
    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.__r1))  \
      _RAISE_0 ;                                                           \
                                                                           \
    data.__r2 = __fun(data.__r0, data.__r1) ;                              \
  }

// no fetch, no save
#define _BINOP_MODRM_RM_4(__fun, __r0, __r1, __r2)                         \
  {                                                                        \
    if (HAV_N_IRQS != get_modrm_reg(hav, hav->inst.oprd_size, &data.__r0)) \
      _RAISE_0 ;                                                           \
                                                                           \
    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.__r1))  \
      _RAISE_0 ;                                                           \
                                                                           \
    data.__r2 = __fun(data.__r0, data.__r1) ;                              \
  }

// fetch, save
#define _BINOP_MODRM_MR_0(__fun, __r0, __r1, __r2)                         \
  {                                                                        \
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))                               \
      _RAISE_0 ;                                                           \
                                                                           \
    if (HAV_N_IRQS != get_modrm_reg(hav, hav->inst.oprd_size, &data.__r1)) \
      _RAISE_0 ;                                                           \
                                                                           \
    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.__r0))  \
      _RAISE_0 ;                                                           \
                                                                           \
    data.__r2 = __fun(data.__r0, data.__r1) ;                              \
                                                                           \
    if (HAV_N_IRQS != set_modrm_reg(hav, hav->inst.oprd_size, data.__r2))  \
      _RAISE_0 ;                                                           \
  }

// no fetch, save
#define _BINOP_MODRM_MR_1(__fun, __r0, __r1, __r2)                         \
  {                                                                        \
    if (HAV_N_IRQS != get_modrm_reg(hav, hav->inst.oprd_size, &data.__r1)) \
      _RAISE_0 ;                                                           \
                                                                           \
    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.__r0))  \
      _RAISE_0 ;                                                           \
                                                                           \
    data.__r2 = __fun(data.__r0, data.__r1) ;                              \
                                                                           \
    if (HAV_N_IRQS != set_modrm_reg(hav, hav->inst.oprd_size, data.__r2))  \
      _RAISE_0 ;                                                           \
  }

// no fetch, save, fetch imm
#define _BINOP_MODRM_MR_2(__fun, __r0, __r1, __r2)                         \
  {                                                                        \
    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.__r0))  \
      _RAISE_0 ;                                                           \
                                                                           \
    if (HAV_N_IRQS != fetch_imm(hav, hav->inst.oprd_size, &ip))            \
      _RAISE_0 ;                                                           \
                                                                           \
    data.__r1 = hav->inst.imm ;                                            \
    data.__r2 = __fun(data.__r0, data.__r1) ;                              \
                                                                           \
    if (HAV_N_IRQS != set_modrm_rm(hav, hav->inst.oprd_size, data.__r2))   \
      _RAISE_0 ;                                                           \
  }

// fetch, no save
#define _BINOP_MODRM_MR_3(__fun, __r0, __r1, __r2)                         \
  {                                                                        \
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))                               \
      _RAISE_0 ;                                                           \
                                                                           \
    if (HAV_N_IRQS != get_modrm_reg(hav, hav->inst.oprd_size, &data.__r1)) \
      _RAISE_0 ;                                                           \
                                                                           \
    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.__r0))  \
      _RAISE_0 ;                                                           \
                                                                           \
    data.__r2 = __fun(data.__r0, data.__r1) ;                              \
  }

// no fetch, no save
#define _BINOP_MODRM_MR_4(__fun, __r0, __r1, __r2)                         \
  {                                                                        \
    if (HAV_N_IRQS != get_modrm_reg(hav, hav->inst.oprd_size, &data.__r1)) \
      _RAISE_0 ;                                                           \
                                                                           \
    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.__r0))  \
      _RAISE_0 ;                                                           \
                                                                           \
    data.__r2 = __fun(data.__r0, data.__r1) ;                              \
  }

// no fetch, no save, fetch imm
#define _BINOP_MODRM_MR_5(__fun, __r0, __r1, __r2)                         \
  {                                                                        \
    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.__r0))  \
      _RAISE_0 ;                                                           \
                                                                           \
    if (HAV_N_IRQS != fetch_imm(hav, hav->inst.oprd_size, &ip))            \
      _RAISE_0 ;                                                           \
                                                                           \
    data.__r1 = hav->inst.imm ;                                            \
    data.__r2 = __fun(data.__r0, data.__r1) ;                              \
  }

// (sign-extended) fetch, save
#define _BINOP_MODRM_IRM_0(__fun, __r0, __r1, __r2)                         \
  {                                                                         \
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))                                \
      _RAISE_0 ;                                                            \
                                                                            \
    if (HAV_N_IRQS != get_modrm_ireg(hav, hav->inst.oprd_size, &data.__r0)) \
      _RAISE_0 ;                                                            \
                                                                            \
    if (HAV_N_IRQS != get_modrm_irm(hav, hav->inst.oprd_size, &data.__r1))  \
      _RAISE_0 ;                                                            \
                                                                            \
    data.__r2 = __fun(data.__r0, data.__r1) ;                               \
                                                                            \
    if (HAV_N_IRQS != set_modrm_ireg(hav, hav->inst.oprd_size, data.__r2))  \
      _RAISE_0 ;                                                            \
  }

// (sign-extended) no fetch, save
#define _BINOP_MODRM_IRM_1(__fun, __r0, __r1, __r2)                         \
  {                                                                         \
    if (HAV_N_IRQS != get_modrm_ireg(hav, hav->inst.oprd_size, &data.__r0)) \
      _RAISE_0 ;                                                            \
                                                                            \
    if (HAV_N_IRQS != get_modrm_irm(hav, hav->inst.oprd_size, &data.__r1))  \
      _RAISE_0 ;                                                            \
                                                                            \
    data.__r2 = __fun(data.__r0, data.__r1) ;                               \
                                                                            \
    if (HAV_N_IRQS != set_modrm_ireg(hav, hav->inst.oprd_size, data.__r2))  \
      _RAISE_0 ;                                                            \
  }

// (sign-extended) fetch, save
#define _BINOP_MODRM_IMR_0(__fun, __r0, __r1, __r2)                         \
  {                                                                         \
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))                                \
      _RAISE_0 ;                                                            \
                                                                            \
    if (HAV_N_IRQS != get_modrm_ireg(hav, hav->inst.oprd_size, &data.__r1)) \
      _RAISE_0 ;                                                            \
                                                                            \
    if (HAV_N_IRQS != get_modrm_irm(hav, hav->inst.oprd_size, &data.__r0))  \
      _RAISE_0 ;                                                            \
                                                                            \
    data.__r2 = __fun(data.__r0, data.__r1) ;                               \
                                                                            \
    if (HAV_N_IRQS != set_modrm_ireg(hav, hav->inst.oprd_size, data.__r2))  \
      _RAISE_0 ;                                                            \
  }

// (sign-extended) no fetch, save
#define _BINOP_MODRM_IMR_1(__fun, __r0, __r1, __r2)                         \
  {                                                                         \
    if (HAV_N_IRQS != get_modrm_ireg(hav, hav->inst.oprd_size, &data.__r1)) \
      _RAISE_0 ;                                                            \
                                                                            \
    if (HAV_N_IRQS != get_modrm_irm(hav, hav->inst.oprd_size, &data.__r0))  \
      _RAISE_0 ;                                                            \
                                                                            \
    data.__r2 = __fun(data.__r0, data.__r1) ;                               \
                                                                            \
    if (HAV_N_IRQS != set_modrm_ireg(hav, hav->inst.oprd_size, data.__r2))  \
      _RAISE_0 ;                                                            \
  }

// (sign-extended) no fetch, save, fetch imm
#define _BINOP_MODRM_IMR_2(__fun, __r0, __r1, __r2)                        \
  {                                                                        \
    if (HAV_N_IRQS != get_modrm_irm(hav, hav->inst.oprd_size, &data.__r0)) \
      _RAISE_0 ;                                                           \
                                                                           \
    if (HAV_N_IRQS != fetch_imm(hav, hav->inst.oprd_size, &ip))            \
      _RAISE_0 ;                                                           \
                                                                           \
    data.__r1 = hav->inst.imm ;                                            \
    data.__r2 = __fun(data.__r0, data.__r1) ;                              \
                                                                           \
    if (HAV_N_IRQS != set_modrm_irm(hav, hav->inst.oprd_size, data.__r2))  \
      _RAISE_0 ;                                                           \
  }

  switch (hav->inst.opcode[0]) {

#define __fun_add(__v0, __v1) (__v0) + (__v1)
#define __fun_sub(__v0, __v1) (__v0) - (__v1)

  case 0x00 : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_RM_0(__fun_add, r0, r1, r2)
    update_flags_add_adc(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x01 : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_RM_0(__fun_add, r0, r1, r2)
    update_flags_add_adc(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x02 : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_MR_0(__fun_add, r0, r1, r2)
    update_flags_add_adc(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x03 : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_MR_0(__fun_add, r0, r1, r2)
    update_flags_add_adc(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x04 : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_RM_0(__fun_sub, r0, r1, r2)
    update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x05 : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_RM_0(__fun_sub, r0, r1, r2)
    update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x06 : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_MR_0(__fun_sub, r0, r1, r2)
    update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x07 : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_MR_0(__fun_sub, r0, r1, r2)
    update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x08 : case 0x09 : case 0x0A : case 0x0B :
    return exec_push(hav, 2, hav->segs + (hav->inst.opcode[0] - 0x08)) ;

  case 0x0C : case 0x0D : case 0x0E :
    return exec_pop(hav, 2, hav->segs + (hav->inst.opcode[0] - 0x0B)) ;

  case 0x0F : {
    switch (hav->inst.opcode[1]) {
    case 0x00 : case 0x01 : case 0x02 : case 0x03 :
    case 0x04 : case 0x05 : case 0x06 : case 0x07 :
    case 0x08 : case 0x09 : case 0x0A : case 0x0B :
    case 0x0C : case 0x0D : case 0x0E : case 0x0F : { _ZOV(0, 0) _AOV(3, 2)
      if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
        _RAISE_0 ;

      u32_t cond = check_cond(hav, hav->inst.opcode[0] & 0x0F) ;

      // set the condition result
      if (HAV_N_IRQS != set_modrm_rm(hav, hav->inst.oprd_size, cond))
        _RAISE_0 ;
      
      update_flags_mov(hav, hav->inst.oprd_size, cond) ;
    } break ;

    case 0x10 : {
      u16_t ss = hav->segs[HAV_SEG_SS] ;
      u64_t sp = hav->regs[HAV_REG_SP] ;
      u64_t bp = hav->regs[HAV_REG_BP] ;

      hav->segs[HAV_SEG_SS] = hav->sysenv.segs[HAV_SEG_SS] ;
      hav->regs[HAV_REG_SP] = hav->sysenv.sp ;
      hav->regs[HAV_REG_BP] = hav->sysenv.bp ;

      // push the current context

      if (HAV_N_IRQS != exec_push(hav, 2, hav->segs + HAV_SEG_CS))
        _RAISE_0 ;
      
      if (HAV_N_IRQS != exec_push(hav, 2, hav->segs + HAV_SEG_DS))
        _RAISE_0 ;
      
      if (HAV_N_IRQS != exec_push(hav, 2, hav->segs + HAV_SEG_ES))
        _RAISE_0 ;
      
      if (HAV_N_IRQS != exec_push(hav, 2, &ss))
        _RAISE_0 ;

      if (HAV_N_IRQS != exec_push(hav, 4, &hav->flags))
        _RAISE_0 ;

      if (HAV_N_IRQS != exec_push(hav, 8, &hav->ip))
        _RAISE_0 ;

      if (HAV_N_IRQS != exec_push(hav, 8, &sp))
        _RAISE_0 ;

      if (HAV_N_IRQS != exec_push(hav, 8, &bp))
        _RAISE_0 ;

      // update segments and registers

      hav->segs[HAV_SEG_CS] = hav->sysenv.segs[HAV_SEG_CS] ;
      hav->segs[HAV_SEG_DS] = hav->sysenv.segs[HAV_SEG_DS] ;
      hav->segs[HAV_SEG_ES] = hav->sysenv.segs[HAV_SEG_ES] ;

      hav->flags = hav->sysenv.flags ;
      hav->ip = hav->sysenv.ip ;

      // do not update the instruction pointer
      return HAV_N_IRQS ;
    }

    case 0x11 : {
      // pop the current context

      if (HAV_N_IRQS != exec_pop(hav, 8, hav->regs + HAV_REG_BP))
        _RAISE_0 ;

      if (HAV_N_IRQS != exec_pop(hav, 8, hav->regs + HAV_REG_SP))
        _RAISE_0 ;

      if (HAV_N_IRQS != exec_pop(hav, 8, &hav->ip))
        _RAISE_0 ;

      if (HAV_N_IRQS != exec_pop(hav, 4, &hav->flags))
        _RAISE_0 ;

      if (HAV_N_IRQS != exec_pop(hav, 2, hav->segs + HAV_SEG_SS))
        _RAISE_0 ;

      if (HAV_N_IRQS != exec_pop(hav, 2, hav->segs + HAV_SEG_ES))
        _RAISE_0 ;

      if (HAV_N_IRQS != exec_pop(hav, 2, hav->segs + HAV_SEG_DS))
        _RAISE_0 ;

      if (HAV_N_IRQS != exec_pop(hav, 2, hav->segs + HAV_SEG_CS))
        _RAISE_0 ;

      // do not update the instruction pointer
      return HAV_N_IRQS ;
    }

    case 0x12 : {
      if (0 != (hav->flags & HAV_FLAG_IOPL))
        _RAISE(HAV_IRQ_GENERAL_PROTECT) ;
      
      // do not save the context

      hav->flags = hav->sysenv.flags ;
      hav->segs[HAV_SEG_CS] = hav->sysenv.segs[HAV_SEG_CS] ;
      hav->segs[HAV_SEG_DS] = hav->sysenv.segs[HAV_SEG_DS] ;
      hav->segs[HAV_SEG_ES] = hav->sysenv.segs[HAV_SEG_ES] ;
      hav->segs[HAV_SEG_SS] = hav->sysenv.segs[HAV_SEG_SS] ;
      hav->ip = hav->sysenv.ip ;
      hav->regs[HAV_REG_SP] = hav->sysenv.sp ;
      hav->regs[HAV_REG_BP] = hav->sysenv.bp ;

      // do not update the instruction pointer
      return HAV_N_IRQS ;
    }

    case 0x13 : {
      if (0 != (hav->flags & HAV_FLAG_IOPL))
        _RAISE(HAV_IRQ_GENERAL_PROTECT) ;
      
      hav->sysenv.flags = hav->flags ;
      hav->sysenv.segs[HAV_SEG_CS] = hav->segs[HAV_SEG_CS] ;
      hav->sysenv.segs[HAV_SEG_DS] = hav->segs[HAV_SEG_DS] ;
      hav->sysenv.segs[HAV_SEG_ES] = hav->segs[HAV_SEG_ES] ;
      hav->sysenv.segs[HAV_SEG_SS] = hav->segs[HAV_SEG_SS] ;
      hav->sysenv.ip = hav->ip ;
      hav->sysenv.sp = hav->regs[HAV_REG_SP] ;
      hav->sysenv.bp = hav->regs[HAV_REG_BP] ;

      // halt the machine
      hav->flags &= ~HAV_FLAG_A1 ;

      // do not update the instruction pointer
      return HAV_N_IRQS ;
    }

    case 0x14 : { _ZOV(2, 1) _AOV(3, 2)
      if (0 != (hav->flags & HAV_FLAG_IOPL))
        _RAISE(HAV_IRQ_GENERAL_PROTECT) ;

      if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
        _RAISE_0 ;

      if (0 == modrm_rm_is_mem(hav))
        _RAISE(HAV_IRQ_GENERAL_FAULT) ;

      u16_t segx ;
      u64_t addr ;
      u64_t size ;

      if (HAV_N_IRQS != get_modrm_rm(hav, 2, (u64_t *)&segx))
        _RAISE_0 ;

      hav->inst.addr += 2 ;

      if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &addr))
        _RAISE_0 ;

      size = 256 * sizeof(hav_ide_t) ; // 256 interrupts

      if (HAV_N_IRQS != vaddr_to_paddr(hav, segx, &addr, &size, HAV_SEG_PERM_SDT))
        _RAISE_0 ;
      
      // set the interrupt descriptor table
      hav->idt = addr ;
    } break ;

    case 0x15 : { _ZOV(2, 1) _AOV(3, 2)
      if (0 != (hav->flags & HAV_FLAG_IOPL))
        _RAISE(HAV_IRQ_GENERAL_PROTECT) ;

      if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
        _RAISE_0 ;

      if (0 == modrm_rm_is_mem(hav))
        _RAISE(HAV_IRQ_GENERAL_FAULT) ;

      if (HAV_N_IRQS != set_modrm_rm(hav, 8, hav->idt))
        _RAISE_0 ;
    } break ;

    case 0x16 : { _ZOV(2, 1) _AOV(3, 2)
      if (0 != (hav->flags & HAV_FLAG_IOPL))
        _RAISE(HAV_IRQ_GENERAL_PROTECT) ;

      if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
        _RAISE_0 ;

      if (0 == modrm_rm_is_mem(hav))
        _RAISE(HAV_IRQ_GENERAL_FAULT) ;

      u16_t segx ;
      u64_t addr ;
      u64_t size ;

      if (HAV_N_IRQS != get_modrm_rm(hav, 2, (u64_t *)&segx))
        _RAISE_0 ;

      hav->inst.addr += 2 ;

      if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &addr))
        _RAISE_0 ;

      size = 4 * sizeof(hav_sde_t) ; // at least 4 segments

      if (HAV_N_IRQS != vaddr_to_paddr(hav, segx, &addr, &size, HAV_SEG_PERM_SDT))
        _RAISE_0 ;

      // set the segment descriptor table
      hav->sdt = addr ;
    } break ;

    case 0x17 : { _ZOV(2, 1) _AOV(3, 2)
      if (0 != (hav->flags & HAV_FLAG_IOPL))
        _RAISE(HAV_IRQ_GENERAL_PROTECT) ;

      if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
        _RAISE_0 ;

      if (0 == modrm_rm_is_mem(hav))
        _RAISE(HAV_IRQ_GENERAL_FAULT) ;

      if (HAV_N_IRQS != set_modrm_rm(hav, 8, hav->sdt))
        _RAISE_0 ;
    } break ;

#define __fun_bswap_16(__v0) hav_byteswap_16(__v0)

    case 0x18 : { _ZOV(2, 1) _AOV(3, 2)
      _UNAOP_MODRM_MR_0(__fun_bswap_16, r0, r1)
    } break ;

#define __fun_bswap_32_64(__v0) hav->inst.has_SOV ? hav_byteswap_64(__v0) : hav_byteswap_32(__v0)

    case 0x19 : { _ZOV(2, 1) _AOV(3, 2)
      _UNAOP_MODRM_MR_0(__fun_bswap_32_64, r0, r1)
    } break ;
    
    case 0x1A : case 0x1B :
    case 0x1C : case 0x1D : case 0x1E :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;

    case 0x1F : { _AOV(3, 2)
      if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
        _RAISE_0 ;

      switch (hav->inst.reg) {
      case 0 :
        break ;

      case 1 : case 2 : case 3 :
      case 4 : case 5 : case 6 : case 7 :
        _UIP(1 << ((hav->inst.opcode[1] & 7) - 1))
        break ;
      
      default :
        _RAISE(HAV_IRQ_NON_MASKABLE) ;
      }
    } break ;

    case 0x20 : case 0x21 : case 0x22 : case 0x23 :
    case 0x24 : case 0x25 : case 0x26 : case 0x27 :
    case 0x28 : case 0x29 : case 0x2A : case 0x2B :
    case 0x2C : case 0x2D : case 0x2E : case 0x2F : { _ZOV(0, 1) _AOV(3, 2)
      if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
        _RAISE_0 ;

      if (0 != check_cond(hav, hav->inst.opcode[1] & 0x0F)) {
        if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.r0))
          _RAISE_0 ;

        if (HAV_N_IRQS != set_modrm_reg(hav, hav->inst.oprd_size, data.r0))
          _RAISE_0 ;

        update_flags_mov(hav, hav->inst.oprd_size, data.r0) ;
      }
    } break ;

    case 0x30 : case 0x31 : case 0x32 : case 0x33 :
    case 0x34 : case 0x35 : case 0x36 : case 0x37 :
    case 0x38 : case 0x39 : case 0x3A : case 0x3B :
    case 0x3C : case 0x3D : case 0x3E : case 0x3F : { _ZOV(2, 3) _AOV(3, 2)
      if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
        _RAISE_0 ;

      if (0 != check_cond(hav, hav->inst.opcode[1] & 0x0F)) {
        if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.r0))
          _RAISE_0 ;

        if (HAV_N_IRQS != set_modrm_reg(hav, hav->inst.oprd_size, data.r0))
          _RAISE_0 ;

        update_flags_mov(hav, hav->inst.oprd_size, data.r0) ;
      }
    } break ;

    case 0x40 : case 0x41 : case 0x42 : case 0x43 :
    case 0x44 : case 0x45 : case 0x46 : case 0x47 :
    case 0x48 : case 0x49 : case 0x4A : case 0x4B :
    case 0x4C : case 0x4D : case 0x4E : case 0x4F : { _ZOV(0, 1) _AOV(3, 2)
      if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
        _RAISE_0 ;

      if (0 != check_cond(hav, hav->inst.opcode[1] & 0x0F)) {
        if (HAV_N_IRQS != get_modrm_reg(hav, hav->inst.oprd_size, &data.r0))
          _RAISE_0 ;

        if (HAV_N_IRQS != set_modrm_rm(hav, hav->inst.oprd_size, data.r0))
          _RAISE_0 ;

        update_flags_mov(hav, hav->inst.oprd_size, data.r0) ;
      }
    } break ;

    case 0x50 : case 0x51 : case 0x52 : case 0x53 :
    case 0x54 : case 0x55 : case 0x56 : case 0x57 :
    case 0x58 : case 0x59 : case 0x5A : case 0x5B :
    case 0x5C : case 0x5D : case 0x5E : case 0x5F : { _ZOV(2, 3) _AOV(3, 2)
      if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
        _RAISE_0 ;

      if (0 != check_cond(hav, hav->inst.opcode[1] & 0x0F)) {
        if (HAV_N_IRQS != get_modrm_reg(hav, hav->inst.oprd_size, &data.r0))
          _RAISE_0 ;

        if (HAV_N_IRQS != set_modrm_rm(hav, hav->inst.oprd_size, data.r0))
          _RAISE_0 ;

        update_flags_mov(hav, hav->inst.oprd_size, data.r0) ;
      }
    } break ;

    case 0x60 : case 0x61 : case 0x62 : case 0x63 :
    case 0x64 : case 0x65 : case 0x66 : case 0x67 :
    case 0x68 : case 0x69 : case 0x6A : case 0x6B :
    case 0x6C : case 0x6D : case 0x6E : case 0x6F :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;

    case 0x70 : case 0x71 : case 0x72 : case 0x73 :
    case 0x74 : case 0x75 : case 0x76 : case 0x77 :
    case 0x78 : case 0x79 : case 0x7A : case 0x7B :
    case 0x7C : case 0x7D : case 0x7E : case 0x7F :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;

    case 0x80 : case 0x81 : case 0x82 : case 0x83 :
    case 0x84 : case 0x85 : case 0x86 : case 0x87 :
    case 0x88 : case 0x89 : case 0x8A : case 0x8B :
    case 0x8C : case 0x8D : case 0x8E : case 0x8F :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;

    case 0x90 : case 0x91 : case 0x92 : case 0x93 :
    case 0x94 : case 0x95 : case 0x96 : case 0x97 :
    case 0x98 : case 0x99 : case 0x9A : case 0x9B :
    case 0x9C : case 0x9D : case 0x9E : case 0x9F :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;

    case 0xA0 : case 0xA1 : case 0xA2 : case 0xA3 :
    case 0xA4 : case 0xA5 : case 0xA6 : case 0xA7 :
    case 0xA8 : case 0xA9 : case 0xAA : case 0xAB :
    case 0xAC : case 0xAD : case 0xAE : case 0xAF :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;

    case 0xB0 : case 0xB1 : case 0xB2 : case 0xB3 :
    case 0xB4 : case 0xB5 : case 0xB6 : case 0xB7 :
    case 0xB8 : case 0xB9 : case 0xBA : case 0xBB :
    case 0xBC : case 0xBD : case 0xBE : case 0xBF :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;
    
    case 0xC0 : case 0xC1 : case 0xC2 : case 0xC3 :
    case 0xC4 : case 0xC5 : case 0xC6 : case 0xC7 :
    case 0xC8 : case 0xC9 : case 0xCA : case 0xCB :
    case 0xCC : case 0xCD : case 0xCE : case 0xCF :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;
    
    case 0xD0 : case 0xD1 : case 0xD2 : case 0xD3 :
    case 0xD4 : case 0xD5 : case 0xD6 : case 0xD7 :
    case 0xD8 : case 0xD9 : case 0xDA : case 0xDB :
    case 0xDC : case 0xDD : case 0xDE : case 0xDF :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;

    case 0xE0 : case 0xE1 : case 0xE2 : case 0xE3 :
    case 0xE4 : case 0xE5 : case 0xE6 : case 0xE7 :
    case 0xE8 : case 0xE9 : case 0xEA : case 0xEB :
    case 0xEC : case 0xED : case 0xEE : case 0xEF :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;

    case 0xF0 : case 0xF1 : case 0xF2 : case 0xF3 :
    case 0xF4 : case 0xF5 : case 0xF6 : case 0xF7 :
    case 0xF8 : case 0xF9 : case 0xFA : case 0xFB :
    case 0xFC : case 0xFD : case 0xFE : case 0xFF :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;

    default :
      _RAISE(HAV_IRQ_NON_MASKABLE) ;
    } // 0F
  } break ;

#define __fun_and(__v0, __v1) (__v0) & (__v1)
#define __fun_or(__v0, __v1)  (__v0) | (__v1)
#define __fun_xor(__v0, __v1) (__v0) ^ (__v1)
#define __fun_shl(__v0, __v1) (__v0) << (__v1)
#define __fun_shr(__v0, __v1) (__v0) >> (__v1)

  case 0x10 : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_RM_0(__fun_and, r0, r1, r2)
    update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x11 : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_RM_0(__fun_and, r0, r1, r2)
    update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x12 : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_MR_0(__fun_and, r0, r1, r2)
    update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x13 : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_MR_0(__fun_and, r0, r1, r2)
    update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x14 : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_RM_0(__fun_or, r0, r1, r2)
    update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x15 : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_RM_0(__fun_or, r0, r1, r2)
    update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x16 : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_MR_0(__fun_or, r0, r1, r2)
    update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x17 : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_MR_0(__fun_or, r0, r1, r2)
    update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x18 : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_RM_0(__fun_xor, r0, r1, r2)
    update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x19 : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_RM_0(__fun_xor, r0, r1, r2)
    update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x1A : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_MR_0(__fun_xor, r0, r1, r2)
    update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x1B : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_MR_0(__fun_xor, r0, r1, r2)
    update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x1C : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_RM_0(__fun_shl, r0, r1, r2)
    update_flags_shl(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x1D : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_RM_0(__fun_shl, r0, r1, r2)
    update_flags_shl(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x1E : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_MR_0(__fun_shl, r0, r1, r2)
    update_flags_shl(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x1F : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_MR_0(__fun_shl, r0, r1, r2)
    update_flags_shl(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x20 : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_RM_0(__fun_shr, r0, r1, r2)
    update_flags_shr(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x21 : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_RM_0(__fun_shr, r0, r1, r2)
    update_flags_shr(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x22 : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_MR_0(__fun_shr, r0, r1, r2)
    update_flags_shr(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x23 : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_MR_0(__fun_shr, r0, r1, r2)
    update_flags_shr(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x24 : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_IRM_0(__fun_shr, i0, i1, i2)
    update_flags_sar(hav, hav->inst.oprd_size, data.i0, data.i1, data.i2) ;
  } break ;

  case 0x25 : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_IRM_0(__fun_shr, i0, i1, i2)
    update_flags_sar(hav, hav->inst.oprd_size, data.i0, data.i1, data.i2) ;
  } break ;

  case 0x26 : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_IMR_0(__fun_shr, i0, i1, i2)
    update_flags_sar(hav, hav->inst.oprd_size, data.i0, data.i1, data.i2) ;
  } break ;

  case 0x27 : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_IMR_0(__fun_shr, i0, i1, i2)
    update_flags_sar(hav, hav->inst.oprd_size, data.i0, data.i1, data.i2) ;
  } break ;

  case 0x28 : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_RM_3(__fun_sub, r0, r1, r2)
    update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x29 : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_RM_3(__fun_sub, r0, r1, r2)
    update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x2A : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_MR_3(__fun_sub, r0, r1, r2)
    update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x2B : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_MR_3(__fun_sub, r0, r1, r2)
    update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x2C : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_RM_3(__fun_and, r0, r1, r2)
    update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x2D : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_RM_3(__fun_and, r0, r1, r2)
    update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x2E : { _ZOV(0, 1) _AOV(3, 2)
    _BINOP_MODRM_MR_3(__fun_and, r0, r1, r2)
    update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x2F : { _ZOV(2, 3) _AOV(3, 2)
    _BINOP_MODRM_MR_3(__fun_and, r0, r1, r2)
    update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x30 : { _ZOV(0, 1) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_reg(hav, HAV_REG_AX, hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.r1))
      _RAISE_0 ;

    data.r2 = data.r0 * data.r1 ;

    if (HAV_N_IRQS != set_reg(hav, HAV_REG_AX, hav->inst.oprd_size, data.r2))
      _RAISE_0 ;

    update_flags_mul_div(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x31 : { _ZOV(2, 3) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_reg(hav, HAV_REG_AX, hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.r1))
      _RAISE_0 ;

    data.r2 = data.r0 * data.r1 ;

    if (HAV_N_IRQS != set_reg(hav, HAV_REG_AX, hav->inst.oprd_size, data.r2))
      _RAISE_0 ;

    update_flags_mul_div(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x32 : { _ZOV(0, 1) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_reg(hav, HAV_REG_AX, hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.r1))
      _RAISE_0 ;
    
    if (0 == data.r1)
      _RAISE(HAV_IRQ_DIV_BY_ZERO) ;

    data.r2 = data.r0 % data.r1 ;

    if (HAV_N_IRQS != set_reg(hav, HAV_REG_DX, hav->inst.oprd_size, data.r2))
      _RAISE_0 ;

    data.r2 = data.r0 / data.r1 ;

    if (HAV_N_IRQS != set_reg(hav, HAV_REG_AX, hav->inst.oprd_size, data.r2))
      _RAISE_0 ;

    update_flags_mul_div(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x33 : { _ZOV(2, 3) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_reg(hav, HAV_REG_AX, hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.r1))
      _RAISE_0 ;
    
    if (0 == data.r1)
      _RAISE(HAV_IRQ_DIV_BY_ZERO) ;

    data.r2 = data.r0 % data.r1 ;

    if (HAV_N_IRQS != set_reg(hav, HAV_REG_DX, hav->inst.oprd_size, data.r2))
      _RAISE_0 ;

    data.r2 = data.r0 / data.r1 ;

    if (HAV_N_IRQS != set_reg(hav, HAV_REG_AX, hav->inst.oprd_size, data.r2))
      _RAISE_0 ;

    update_flags_mul_div(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x34 : { _ZOV(0, 1) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_ireg(hav, HAV_REG_AX, hav->inst.oprd_size, &data.i0))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_modrm_irm(hav, hav->inst.oprd_size, &data.i1))
      _RAISE_0 ;

    data.i2 = data.i0 * data.i1 ;

    if (HAV_N_IRQS != set_ireg(hav, HAV_REG_AX, hav->inst.oprd_size, data.i2))
      _RAISE_0 ;

    update_flags_imul_idiv(hav, hav->inst.oprd_size, data.i0, data.i1, data.i2) ;
  } break ;

  case 0x35 : { _ZOV(2, 3) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_ireg(hav, HAV_REG_AX, hav->inst.oprd_size, &data.i0))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_modrm_irm(hav, hav->inst.oprd_size, &data.i1))
      _RAISE_0 ;

    data.i2 = data.i0 * data.i1 ;

    if (HAV_N_IRQS != set_ireg(hav, HAV_REG_AX, hav->inst.oprd_size, data.i2))
      _RAISE_0 ;

    update_flags_imul_idiv(hav, hav->inst.oprd_size, data.i0, data.i1, data.i2) ;
  } break ;

  case 0x36 : { _ZOV(0, 1) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_ireg(hav, HAV_REG_AX, hav->inst.oprd_size, &data.i0))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_modrm_irm(hav, hav->inst.oprd_size, &data.i1))
      _RAISE_0 ;
    
    if (0 == data.i1)
      _RAISE(HAV_IRQ_DIV_BY_ZERO) ;

    data.i2 = data.i0 % data.i1 ;

    if (HAV_N_IRQS != set_ireg(hav, HAV_REG_DX, hav->inst.oprd_size, data.i2))
      _RAISE_0 ;

    data.i2 = data.i0 / data.i1 ;

    if (HAV_N_IRQS != set_ireg(hav, HAV_REG_AX, hav->inst.oprd_size, data.i2))
      _RAISE_0 ;

    update_flags_imul_idiv(hav, hav->inst.oprd_size, data.i0, data.i1, data.i2) ;
  } break ;

  case 0x37 : { _ZOV(2, 3) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_ireg(hav, HAV_REG_AX, hav->inst.oprd_size, &data.i0))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_modrm_irm(hav, hav->inst.oprd_size, &data.i1))
      _RAISE_0 ;
    
    if (0 == data.i1)
      _RAISE(HAV_IRQ_DIV_BY_ZERO) ;

    data.i2 = data.i0 % data.i1 ;

    if (HAV_N_IRQS != set_ireg(hav, HAV_REG_DX, hav->inst.oprd_size, data.i2))
      _RAISE_0 ;

    data.i2 = data.i0 / data.i1 ;

    if (HAV_N_IRQS != set_ireg(hav, HAV_REG_AX, hav->inst.oprd_size, data.i2))
      _RAISE_0 ;

    update_flags_imul_idiv(hav, hav->inst.oprd_size, data.i0, data.i1, data.i2) ;
  } break ;

  case 0x38 : { _ZOV(1, 1) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    // set the segment into the base
    if (HAV_N_IRQS != set_reg(hav, HAV_REG_BX, 1, hav->inst.segx))
      _RAISE_0 ;

    // set the address into the destination register
    if (HAV_N_IRQS != set_modrm_reg(hav, hav->inst.oprd_size, hav->inst.addr))
      _RAISE_0 ;
  } break ;

  case 0x39 : { _ZOV(2, 3) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    // set the segment into the base
    if (HAV_N_IRQS != set_reg(hav, HAV_REG_BX, 1, hav->inst.segx))
      _RAISE_0 ;

    // set the address into the destination register
    if (HAV_N_IRQS != set_modrm_reg(hav, hav->inst.oprd_size, hav->inst.addr))
      _RAISE_0 ;
  } break ;

  case 0x3A : { _ZOV(1, 1) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    // set the segment into the base
    if (HAV_N_IRQS != set_reg(hav, HAV_REG_BX, 1, hav->segs[HAV_SEG_DS]))
      _RAISE_0 ;

    // set the address into the destination register
    if (HAV_N_IRQS != set_modrm_reg(hav, hav->inst.oprd_size, hav->inst.addr))
      _RAISE_0 ;
  } break ;

  case 0x3B : { _ZOV(2, 3) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    // set the segment into the base
    if (HAV_N_IRQS != set_reg(hav, HAV_REG_BX, 1, hav->segs[HAV_SEG_DS]))
      _RAISE_0 ;

    // set the address into the destination register
    if (HAV_N_IRQS != set_modrm_reg(hav, hav->inst.oprd_size, hav->inst.addr))
      _RAISE_0 ;
  } break ;

  case 0x3C : { _ZOV(1, 1) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    // set the segment into the base
    if (HAV_N_IRQS != set_reg(hav, HAV_REG_BX, 1, hav->segs[HAV_SEG_ES]))
      _RAISE_0 ;

    // set the address into the destination register
    if (HAV_N_IRQS != set_modrm_reg(hav, hav->inst.oprd_size, hav->inst.addr))
      _RAISE_0 ;
  } break ;

  case 0x3D : { _ZOV(2, 3) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    // set the segment into the base
    if (HAV_N_IRQS != set_reg(hav, HAV_REG_BX, 1, hav->segs[HAV_SEG_ES]))
      _RAISE_0 ;

    // set the address into the destination register
    if (HAV_N_IRQS != set_modrm_reg(hav, hav->inst.oprd_size, hav->inst.addr))
      _RAISE_0 ;
  } break ;

  case 0x3E : { _ZOV(1, 1) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    // set the segment into the base
    if (HAV_N_IRQS != set_reg(hav, HAV_REG_BX, 1, hav->segs[HAV_SEG_SS]))
      _RAISE_0 ;

    // set the address into the destination register
    if (HAV_N_IRQS != set_modrm_reg(hav, hav->inst.oprd_size, hav->inst.addr))
      _RAISE_0 ;
  } break ;

  case 0x3F : { _ZOV(2, 3) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    // set the segment into the base
    if (HAV_N_IRQS != set_reg(hav, HAV_REG_BX, 1, hav->segs[HAV_SEG_SS]))
      _RAISE_0 ;

    // set the address into the destination register
    if (HAV_N_IRQS != set_modrm_reg(hav, hav->inst.oprd_size, hav->inst.addr))
      _RAISE_0 ;
  } break ;

#define __fun_adc(__v0, __v1) (__v0) + (__v1) + !!(hav->flags & HAV_FLAG_C)
#define __fun_sbb(__v0, __v1) (__v0) - (__v1) - !!(hav->flags & HAV_FLAG_C)

  case 0x40 : {
    _BINOP_MODRM_RM_0(__fun_adc, r0, r1, r2)
    update_flags_add_adc(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x41 : {
    _BINOP_MODRM_RM_0(__fun_adc, r0, r1, r2)
    update_flags_add_adc(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x42 : {
    _BINOP_MODRM_MR_0(__fun_adc, r0, r1, r2)
    update_flags_add_adc(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x43 : {
    _BINOP_MODRM_MR_0(__fun_adc, r0, r1, r2)
    update_flags_add_adc(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x44 : {
    _BINOP_MODRM_RM_0(__fun_sbb, r0, r1, r2)
    update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x45 : {
    _BINOP_MODRM_RM_0(__fun_sbb, r0, r1, r2)
    update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x46 : {
    _BINOP_MODRM_MR_0(__fun_sbb, r0, r1, r2)
    update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x47 : {
    _BINOP_MODRM_MR_0(__fun_sbb, r0, r1, r2)
    update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
  } break ;

  case 0x48 : { _ZOV(0, 1)
    u8_t port ;

    // read the port
    port = *(u8_t *)(code + ip) ; _UIP(1)

    if (HAV_N_IRQS != exec_in(hav, port, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_AX))
      _RAISE_0 ;
  } break ;

  case 0x49 : { _ZOV(2, 3)
    u8_t port ;

    // read the port
    port = *(u8_t *)(code + ip) ; _UIP(1)

    if (HAV_N_IRQS != exec_in(hav, port, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_AX))
      _RAISE_0 ;
  } break ;

  case 0x4A : { _ZOV(0, 1)
    u8_t port ;

    // read the port
    port = *(u8_t *)(code + ip) ; _UIP(1)

    if (HAV_N_IRQS != exec_out(hav, port, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_AX))
      _RAISE_0 ;
  } break ;

  case 0x4B : { _ZOV(2, 3)
    u8_t port ;

    // read the port
    port = *(u8_t *)(code + ip) ; _UIP(1)

    if (HAV_N_IRQS != exec_out(hav, port, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_AX))
      _RAISE_0 ;
  } break ;

  case 0x4C : { _ZOV(0, 1)
    u8_t port ;

    // read the port
    if (HAV_N_IRQS != get_reg(hav, HAV_REG_DX, 0, (u64_t *)&port))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_in(hav, port, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_AX))
      _RAISE_0 ;
  } break ;

  case 0x4D : { _ZOV(2, 3)
    u8_t port ;

    // read the port
    if (HAV_N_IRQS != get_reg(hav, HAV_REG_DX, 0, (u64_t *)&port))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_in(hav, port, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_AX))
      _RAISE_0 ;
  } break ;

  case 0x4E : { _ZOV(0, 1)
    u8_t port ;

    // read the port
    if (HAV_N_IRQS != get_reg(hav, HAV_REG_DX, 0, (u64_t *)&port))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_out(hav, port, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_AX))
      _RAISE_0 ;
  } break ;

  case 0x4F : { _ZOV(2, 3)
    u8_t port ;

    // read the port
    if (HAV_N_IRQS != get_reg(hav, HAV_REG_DX, 0, (u64_t *)&port))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_out(hav, port, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_AX))
      _RAISE_0 ;
  } break ;

  case 0x50 : case 0x51 : case 0x52 : case 0x53 :
  case 0x54 : case 0x55 : case 0x56 : case 0x57 : { _ZOV(2, 3)
    if (HAV_N_IRQS != get_reg(hav, hav->inst.opcode[0] & 7, hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;
  } break ;

  case 0x58 : case 0x59 : case 0x5A : case 0x5B :
  case 0x5C : case 0x5D : case 0x5E : case 0x5F : { _ZOV(2, 3)
    if (HAV_N_IRQS != exec_pop(hav, 1 << hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;
    
    if (HAV_N_IRQS != set_reg(hav, hav->inst.opcode[0] & 7, hav->inst.oprd_size, data.r0))
      _RAISE_0 ;
  } break ;

  case 0x60 : case 0x61 : case 0x62 : case 0x63 :
    _RAISE(HAV_IRQ_GENERAL_FAULT) ;

#define __fun_neg(__v0) ~(__v0) + 1

  case 0x64 : { _ZOV(0, 1) _AOV(3, 2)
    _UNAOP_MODRM_MR_0(__fun_neg, r0, r1)
    update_flags_mov(hav, hav->inst.oprd_size, data.r1) ;
  } break ;

  case 0x65 : { _ZOV(2, 3) _AOV(3, 2)
    _UNAOP_MODRM_MR_0(__fun_neg, r0, r1)
    update_flags_mov(hav, hav->inst.oprd_size, data.r1) ;
  } break ;

  case 0x66 : case 0x67 :
    _RAISE(HAV_IRQ_GENERAL_FAULT) ;

#define __fun_not(__v0) ~(__v0)

  case 0x68 : { _ZOV(0, 1) _AOV(3, 2)
    _UNAOP_MODRM_MR_0(__fun_not, r0, r1)
    update_flags_mov(hav, hav->inst.oprd_size, data.r1) ;
  } break ;

  case 0x69 : { _ZOV(2, 3) _AOV(3, 2)
    _UNAOP_MODRM_MR_0(__fun_not, r0, r1)
    update_flags_mov(hav, hav->inst.oprd_size, data.r1) ;
  } break ;

  case 0x6A : { _AOV(3, 2)
    u16_t alloc_size ;
    u8_t nesting_level ;

    alloc_size = *(u16_t *)(code + ip) ; _UIP(2) ;
    nesting_level = *(u8_t *)(code + ip) ; _UIP(1) ;

    if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.addr_size, hav->regs + HAV_REG_BP))
      _RAISE_0 ;
    
    u64_t frame_tmp = hav->regs[HAV_REG_SP] ;

    if (0 != nesting_level) {
      --nesting_level ;

      for (u8_t i = 0 ; i < nesting_level ; ++i) {
        hav->regs[HAV_REG_BP] -= 1 << hav->inst.addr_size ;

        if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.addr_size, hav->regs + HAV_REG_BP))
          _RAISE_0 ;
      }

      if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.addr_size, &frame_tmp))
        _RAISE_0 ;
    }

    hav->regs[HAV_REG_BP] = frame_tmp ;
    hav->regs[HAV_REG_SP] -= alloc_size ;
  } break ;

  case 0x6B : { _AOV(3, 2)
    // set SP to BP
    hav->regs[HAV_REG_SP] = hav->regs[HAV_REG_BP] ;

    // pop BP from stack
    if (HAV_N_IRQS != exec_pop(hav, 1 << hav->inst.addr_size, hav->regs + HAV_REG_BP))
      _RAISE_0 ;
  } break ;

  case 0x6C : { _ZOV(0, 1) _AOV(3, 2)
    // push AX, CX, DX, BX, BP, SI, DI onto the stack

    if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_AX))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_CX))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_DX))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_BX))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.addr_size, hav->regs + HAV_REG_BP))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.addr_size, hav->regs + HAV_REG_SI))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.addr_size, hav->regs + HAV_REG_DI))
      _RAISE_0 ;
  } break ;

  case 0x6D : { _ZOV(2, 3) _AOV(3, 2)
    // push AX, CX, DX, BX, BP, SI, DI onto the stack

    if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_AX))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_CX))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_DX))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_BX))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.addr_size, hav->regs + HAV_REG_BP))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.addr_size, hav->regs + HAV_REG_SI))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.addr_size, hav->regs + HAV_REG_DI))
      _RAISE_0 ;
  } break ;

  case 0x6E : { _ZOV(0, 1) _AOV(3, 2)
    // pop DI, SI, BP, BX, DX, CX, AX from stack

    if (HAV_N_IRQS != exec_pop(hav, 1 << hav->inst.addr_size, hav->regs + HAV_REG_DI))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_pop(hav, 1 << hav->inst.addr_size, hav->regs + HAV_REG_SI))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_pop(hav, 1 << hav->inst.addr_size, hav->regs + HAV_REG_BP))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_pop(hav, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_BX))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_pop(hav, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_DX))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_pop(hav, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_CX))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_pop(hav, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_AX))
      _RAISE_0 ;
  } break ;

  case 0x6F : { _ZOV(2, 3) _AOV(3, 2)
    // pop DI, SI, BP, BX, DX, CX, AX from stack

    if (HAV_N_IRQS != exec_pop(hav, 1 << hav->inst.addr_size, hav->regs + HAV_REG_DI))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_pop(hav, 1 << hav->inst.addr_size, hav->regs + HAV_REG_SI))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_pop(hav, 1 << hav->inst.addr_size, hav->regs + HAV_REG_BP))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_pop(hav, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_BX))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_pop(hav, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_DX))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_pop(hav, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_CX))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_pop(hav, 1 << hav->inst.oprd_size, hav->regs + HAV_REG_AX))
      _RAISE_0 ;
  } break ;

  case 0x70 : case 0x71 : case 0x72 : case 0x73 :
  case 0x74 : case 0x75 : case 0x76 : case 0x77 :
  case 0x78 : case 0x79 : case 0x7A : case 0x7B :
  case 0x7C : case 0x7D : case 0x7E : case 0x7F : { _ZOV(0, 1)
    if (0 != check_cond(hav, hav->inst.opcode[0] & 0x0F))
      goto _jmp_near_i8 ;

    _UIP(1 << hav->inst.oprd_size) ;
  } break ;

  case 0x80 : { _ZOV(0, 1) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    if (HAV_N_IRQS != set_modrm_reg(hav, hav->inst.oprd_size, data.r0))
      _RAISE_0 ;

    update_flags_mov(hav, hav->inst.oprd_size, data.r0) ;
  } break ;

  case 0x81 : { _ZOV(2, 3) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    if (HAV_N_IRQS != set_modrm_reg(hav, hav->inst.oprd_size, data.r0))
      _RAISE_0 ;

    update_flags_mov(hav, hav->inst.oprd_size, data.r0) ;
  } break ;

  case 0x82 : { _ZOV(0, 1) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_modrm_reg(hav, hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    if (HAV_N_IRQS != set_modrm_rm(hav, hav->inst.oprd_size, data.r0))
      _RAISE_0 ;

    update_flags_mov(hav, hav->inst.oprd_size, data.r0) ;
  } break ;

  case 0x83 : { _ZOV(2, 3) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_modrm_reg(hav, hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    if (HAV_N_IRQS != set_modrm_rm(hav, hav->inst.oprd_size, data.r0))
      _RAISE_0 ;

    update_flags_mov(hav, hav->inst.oprd_size, data.r0) ;
  } break ;

  case 0x84 : { _ZOV(0, 1) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_modrm_reg(hav, hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.r1))
      _RAISE_0 ;

    if (HAV_N_IRQS != set_modrm_rm(hav, hav->inst.oprd_size, data.r0))
      _RAISE_0 ;

    if (HAV_N_IRQS != set_modrm_reg(hav, hav->inst.oprd_size, data.r1))
      _RAISE_0 ;
  } break ;

  case 0x85 : { _ZOV(2, 3) _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_modrm_reg(hav, hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.r1))
      _RAISE_0 ;

    if (HAV_N_IRQS != set_modrm_rm(hav, hav->inst.oprd_size, data.r0))
      _RAISE_0 ;

    if (HAV_N_IRQS != set_modrm_reg(hav, hav->inst.oprd_size, data.r1))
      _RAISE_0 ;
  } break ;

  case 0x86 : case 0x87 :
    _RAISE(HAV_IRQ_UNDEFINED_INST) ;

  case 0x88 : { _ZOV(0, 1)
    // read the immediate value
    if (0 != hav->inst.has_ZOV) {
      data.r0 = *(u16_t *)(code + ip) ; _UIP(2)
    } else {
      data.r0 = *(u8_t *)(code + ip) ; _UIP(1)
    }

    // push the immediate value onto the stack
    if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;
  } break ;

  case 0x89 : { _ZOV(2, 3)
    // read the immediate value
    if (0 != hav->inst.has_ZOV) {
      data.r0 = *(u64_t *)(code + ip) ; _UIP(8)
    } else {
      data.r0 = *(u32_t *)(code + ip) ; _UIP(4)
    }

    // push the immediate value onto the stack
    if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;
  } break ;

  case 0x8A : { _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    switch (hav->inst.reg) {
    case 0 : { _ZOV(0, 1)
      _BINOP_MODRM_MR_2(__fun_add, r0, r1, r2)
      update_flags_add_adc(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 1 : { _ZOV(2, 3)
      _BINOP_MODRM_MR_2(__fun_add, r0, r1, r2)
      update_flags_add_adc(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 2 : { _ZOV(0, 1)
      _BINOP_MODRM_MR_2(__fun_sub, r0, r1, r2)
      update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 3 : { _ZOV(2, 3)
      _BINOP_MODRM_MR_2(__fun_sub, r0, r1, r2)
      update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 4 : { _ZOV(0, 1)
      _BINOP_MODRM_MR_2(__fun_adc, r0, r1, r2)
      update_flags_add_adc(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 5 : { _ZOV(2, 3)
      _BINOP_MODRM_MR_2(__fun_adc, r0, r1, r2)
      update_flags_add_adc(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 6 : { _ZOV(0, 1)
      _BINOP_MODRM_MR_2(__fun_sbb, r0, r1, r2)
      update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 7 : { _ZOV(2, 3)
      _BINOP_MODRM_MR_2(__fun_sbb, r0, r1, r2)
      update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    default :
      _RAISE(HAV_IRQ_NON_MASKABLE) ;
    }
  } break ;

  case 0x8B : { _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    switch (hav->inst.reg) {
    case 0 : { _ZOV(0, 1)
      _BINOP_MODRM_MR_5(__fun_sub, r0, r1, r2)
      update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 1 : { _ZOV(2, 3)
      _BINOP_MODRM_MR_5(__fun_sub, r0, r1, r2)
      update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 2 : { _ZOV(0, 1)
      _BINOP_MODRM_MR_5(__fun_and, r0, r1, r2)
      update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 3 : { _ZOV(2, 3)
      _BINOP_MODRM_MR_5(__fun_and, r0, r1, r2)
      update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 4 : { _ZOV(0, 1)
      _BINOP_MODRM_MR_5(__fun_and, r0, r1, r2)
      update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 5 : { _ZOV(2, 3)
      _BINOP_MODRM_MR_5(__fun_and, r0, r1, r2)
      update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 6 : { _ZOV(0, 1)
      _BINOP_MODRM_MR_5(__fun_or, r0, r1, r2)
      update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 7 : { _ZOV(2, 3)
      _BINOP_MODRM_MR_5(__fun_or, r0, r1, r2)
      update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    default :
      _RAISE(HAV_IRQ_NON_MASKABLE) ;
    }
  } break ;

  case 0x8C : { _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    switch (hav->inst.reg) {
    case 0 : { _ZOV(0, 1)
      _BINOP_MODRM_MR_2(__fun_xor, r0, r1, r2)
      update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 1 : { _ZOV(2, 3)
      _BINOP_MODRM_MR_2(__fun_xor, r0, r1, r2)
      update_flags_and_or_xor(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 2 : { _ZOV(0, 1)
      _BINOP_MODRM_MR_2(__fun_shl, r0, r1, r2)
      update_flags_shl(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 3 : { _ZOV(2, 3)
      _BINOP_MODRM_MR_2(__fun_shl, r0, r1, r2)
      update_flags_shl(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 4 : { _ZOV(0, 1)
      _BINOP_MODRM_MR_2(__fun_shl, r0, r1, r2)
      update_flags_shr(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 5 : { _ZOV(2, 3)
      _BINOP_MODRM_MR_2(__fun_shl, r0, r1, r2)
      update_flags_shr(hav, hav->inst.oprd_size, data.r0, data.r1, data.r2) ;
    } break ;

    case 6 : { _ZOV(0, 1)
      _BINOP_MODRM_IMR_2(__fun_shr, i0, i1, i2)
      update_flags_sar(hav, hav->inst.oprd_size, data.i0, data.i1, data.i2) ;
    } break ;

    case 7 : { _ZOV(2, 3)
      _BINOP_MODRM_IMR_2(__fun_shr, i0, i1, i2)
      update_flags_sar(hav, hav->inst.oprd_size, data.i0, data.i1, data.i2) ;
    } break ;

    default :
      _RAISE(HAV_IRQ_NON_MASKABLE) ;
    }
  } break ;

#define __fun_inc(__v0) (__v0) + 1
#define __fun_dec(__v0) (__v0) - 1

  case 0x8D : { _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    switch (hav->inst.reg) {
    case 0x00 : { _ZOV(0, 1)
      _UNAOP_MODRM_MR_1(__fun_inc, r0, r1)
      update_flags_add_adc(hav, hav->inst.oprd_size, data.r0, 1, data.r1) ;
    } break ;

    case 0x01 : { _ZOV(2, 3)
      _UNAOP_MODRM_MR_1(__fun_inc, r0, r1)
      update_flags_add_adc(hav, hav->inst.oprd_size, data.r0, 1, data.r1) ;
    } break ;

    case 0x02 : { _ZOV(0, 1)
      _UNAOP_MODRM_MR_1(__fun_dec, r0, r1)
      update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, 1, data.r1) ;
    } break ;

    case 0x03 : { _ZOV(2, 3)
      _UNAOP_MODRM_MR_1(__fun_dec, r0, r1)
      update_flags_sub_sbb(hav, hav->inst.oprd_size, data.r0, 1, data.r1) ;
    } break ;

    case 0x04 : case 0x05 : case 0x06 : case 0x07 :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;

    default :
      _RAISE(HAV_IRQ_NON_MASKABLE) ;
    }
  } break ;

  case 0x8E : { _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;

    switch (hav->inst.reg) {
    case 0 : { _ZOV(0, 1)
      i64_t bytes ;

      // read the relative address
      if (HAV_N_IRQS != get_modrm_irm(hav, hav->inst.oprd_size, &bytes))
        _RAISE_0 ;

      // update the instruction pointer
      hav->ip += ip + bytes ;

      // do not update the IP
      return HAV_N_IRQS ;
    }

    case 1 : { _ZOV(2, 3)
      i64_t bytes ;

      // read the relative address
      if (HAV_N_IRQS != get_modrm_irm(hav, hav->inst.oprd_size, &bytes))
        _RAISE_0 ;

      // update the instruction pointer
      hav->ip += ip + bytes ;

      // do not update the IP
      return HAV_N_IRQS ;
    } break ;

    case 2 : { _ZOV(0, 1)
      if (0 == modrm_rm_is_mem(hav))
        _RAISE(HAV_IRQ_GENERAL_FAULT) ;

      u16_t segx ;
      u64_t addr ;

      // read the next code segment descriptor
      if (HAV_N_IRQS != get_modrm_rm(hav, 2, (u64_t *)&segx))
        _RAISE_0 ;

      hav->inst.addr += 2 ;

      // read the next instruction pointer
      if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &addr))
        _RAISE_0 ;

      // set the next instruction pointer

      hav->segs[HAV_SEG_CS] = segx ;
      hav->ip = addr ;

      // do not update the IP
      return HAV_N_IRQS ;
    }

    case 3 : { _ZOV(2, 3)
      if (0 == modrm_rm_is_mem(hav))
        _RAISE(HAV_IRQ_GENERAL_FAULT) ;

      u16_t segx ;
      u64_t addr ;

      // read the next code segment descriptor
      if (HAV_N_IRQS != get_modrm_rm(hav, 2, (u64_t *)&segx))
        _RAISE_0 ;

      hav->inst.addr += 2 ;

      // read the next instruction pointer
      if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &addr))
        _RAISE_0 ;

      // set the next instruction pointer

      hav->segs[HAV_SEG_CS] = segx ;
      hav->ip = addr ;

      // do not update the IP
      return HAV_N_IRQS ;
    } break ;

    case 4 : { _ZOV(0, 1)
      i64_t bytes ;

      // read the relative address
      if (HAV_N_IRQS != get_modrm_irm(hav, hav->inst.oprd_size, &bytes))
        _RAISE_0 ;

      // update the current instruction pointer
      hav->ip += ip ;

      // push the current instruction pointer onto the stack
      if (HAV_N_IRQS != exec_push(hav, 8, &hav->ip))
        _RAISE_0 ;

      // update the instruction pointer
      hav->ip += bytes ;

      // do not update the IP
      return HAV_N_IRQS ;
    }

    case 5 : { _ZOV(2, 3)
      i64_t bytes ;

      // read the relative address
      if (HAV_N_IRQS != get_modrm_irm(hav, hav->inst.oprd_size, &bytes))
        _RAISE_0 ;

      // update the current instruction pointer
      hav->ip += ip ;

      // push the current instruction pointer onto the stack
      if (HAV_N_IRQS != exec_push(hav, 8, &hav->ip))
        _RAISE_0 ;

      // update the instruction pointer
      hav->ip += bytes ;

      // do not update the IP
      return HAV_N_IRQS ;
    }

    case 6 : { _ZOV(0, 1)
      if (0 == modrm_rm_is_mem(hav))
        _RAISE(HAV_IRQ_GENERAL_FAULT) ;

      u16_t segx ;
      u64_t addr ;

      // read the next code segment descriptor
      if (HAV_N_IRQS != get_modrm_rm(hav, 2, (u64_t *)&segx))
        _RAISE_0 ;

      hav->inst.addr += 2 ;

      // read the next instruction pointer
      if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &addr))
        _RAISE_0 ;
      
      // update the current instruction pointer
      hav->ip += ip ;

      // push the current code segment descriptor onto the stack
      if (HAV_N_IRQS != exec_push(hav, 2, hav->segs + HAV_SEG_CS))
        _RAISE_0 ;

      // push the current instruction pointer onto the stack
      if (HAV_N_IRQS != exec_push(hav, 8, &hav->ip))
        _RAISE_0 ;

      // set the next instruction pointer

      hav->segs[HAV_SEG_CS] = segx ;
      hav->ip = addr ;

      // do not update the IP
      return HAV_N_IRQS ;
    }

    case 7 : { _ZOV(2, 3)
      if (0 == modrm_rm_is_mem(hav))
        _RAISE(HAV_IRQ_GENERAL_FAULT) ;

      u16_t segx ;
      u64_t addr ;

      // read the next code segment descriptor
      if (HAV_N_IRQS != get_modrm_rm(hav, 2, (u64_t *)&segx))
        _RAISE_0 ;

      hav->inst.addr += 2 ;

      // read the next instruction pointer
      if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &addr))
        _RAISE_0 ;
      
      // update the current instruction pointer
      hav->ip += ip ;

      // push the current code segment descriptor onto the stack
      if (HAV_N_IRQS != exec_push(hav, 2, hav->segs + HAV_SEG_CS))
        _RAISE_0 ;

      // push the current instruction pointer onto the stack
      if (HAV_N_IRQS != exec_push(hav, 8, &hav->ip))
        _RAISE_0 ;

      // set the next instruction pointer

      hav->segs[HAV_SEG_CS] = segx ;
      hav->ip = addr ;

      // do not update the IP
      return HAV_N_IRQS ;
    }

    default :
      _RAISE(HAV_IRQ_NON_MASKABLE) ;
    }
  } break ;

  case 0x8F : { _AOV(3, 2)
    if (HAV_N_IRQS != fetch_ModRM(hav, &ip))
      _RAISE_0 ;
    
    switch (hav->inst.reg) {
    case 0 : { _ZOV(0, 1)
      if (HAV_N_IRQS != fetch_imm(hav, hav->inst.oprd_size, &ip))
        _RAISE_0 ;

      if (HAV_N_IRQS != set_modrm_ireg(hav, hav->inst.oprd_size, hav->inst.imm))
        _RAISE_0 ;

      update_flags_mov(hav, hav->inst.oprd_size, (u64_t)hav->inst.imm) ;
    } break ;

    case 1 : { _ZOV(2, 3)
      if (HAV_N_IRQS != fetch_imm(hav, hav->inst.oprd_size, &ip))
        _RAISE_0 ;

      if (HAV_N_IRQS != set_modrm_ireg(hav, hav->inst.oprd_size, hav->inst.imm))
        _RAISE_0 ;

      update_flags_mov(hav, hav->inst.oprd_size, (u64_t)hav->inst.imm) ;
    } break ;

    case 2 : { _ZOV(0, 1)
      if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.r0))
        _RAISE_0 ;

      if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.oprd_size, &data.r0))
        _RAISE_0 ;
    } break ;

    case 3 : { _ZOV(2, 3)
      if (HAV_N_IRQS != get_modrm_rm(hav, hav->inst.oprd_size, &data.r0))
        _RAISE_0 ;

      if (HAV_N_IRQS != exec_push(hav, 1 << hav->inst.oprd_size, &data.r0))
        _RAISE_0 ;
    } break ;

    case 4 : { _ZOV(0, 1)
      if (HAV_N_IRQS != exec_pop(hav, 1 << hav->inst.oprd_size, &data.r0))
        _RAISE_0 ;

      if (HAV_N_IRQS != set_modrm_rm(hav, hav->inst.oprd_size, data.r0))
        _RAISE_0 ;
    } break ;

    case 5 : { _ZOV(2, 3)
      if (HAV_N_IRQS != exec_pop(hav, 1 << hav->inst.oprd_size, &data.r0))
        _RAISE_0 ;

      if (HAV_N_IRQS != set_modrm_rm(hav, hav->inst.oprd_size, data.r0))
        _RAISE_0 ;
    } break ;

    case 6 : case 7 :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;

    default :
      _RAISE(HAV_IRQ_NON_MASKABLE) ;
    }
  } break ;

  case 0x90 : case 0x91 : case 0x92 : case 0x93 :
  case 0x94 : case 0x95 : case 0x96 : case 0x97 : { _ZOV(0, 1)
    // get AX
    if (HAV_N_IRQS != get_reg(hav, HAV_REG_AX, hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    // get the register
    if (HAV_N_IRQS != get_reg(hav, hav->inst.opcode[0] & 7, hav->inst.oprd_size, &data.r1))
      _RAISE_0 ;

    // set the register into AX
    if (HAV_N_IRQS != set_reg(hav, HAV_REG_AX, hav->inst.oprd_size, data.r1))
      _RAISE_0 ;

    // set AX into the register
    if (HAV_N_IRQS != set_reg(hav, hav->inst.opcode[0] & 7, hav->inst.oprd_size, data.r0))
      _RAISE_0 ;
  } break ;

  case 0x98 : case 0x99 : case 0x9A : case 0x9B :
  case 0x9C : case 0x9D : case 0x9E : case 0x9F : { _ZOV(2, 3)
    // get AX
    if (HAV_N_IRQS != get_reg(hav, HAV_REG_AX, hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    // get the register
    if (HAV_N_IRQS != get_reg(hav, hav->inst.opcode[0] & 7, hav->inst.oprd_size, &data.r1))
      _RAISE_0 ;

    // set the register into AX
    if (HAV_N_IRQS != set_reg(hav, HAV_REG_AX, hav->inst.oprd_size, data.r1))
      _RAISE_0 ;

    // set AX into the register
    if (HAV_N_IRQS != set_reg(hav, hav->inst.opcode[0] & 7, hav->inst.oprd_size, data.r0))
      _RAISE_0 ;
  } break ;

  case 0xA0 : { _ZOV(0, 1) _AOV(3, 2)
    u8_t port ;
    u64_t addr ;

    // read the port
    if (HAV_N_IRQS != get_reg(hav, HAV_REG_DX, 0, (u64_t *)&port))
      _RAISE_0 ;
    
    // read the address
    if (HAV_N_IRQS != get_reg(hav, HAV_REG_SI, hav->inst.addr_size, &addr))
      _RAISE_0 ;
    
    if (0 == hav->inst.has_SOV)
      hav->inst.segx = HAV_SEG_DS ;

    if (HAV_N_IRQS != exec_in(hav, port, 1 << hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    // read the data
    if (HAV_N_IRQS != hav->write(hav, hav->inst.segx, addr, 1 << hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    // update the address
    _UDI(hav->inst.oprd_size) ;
  } break ;

  case 0xA1 : { _ZOV(2, 3) _AOV(3, 2)
    u8_t port ;
    u64_t addr ;

    // read the port
    if (HAV_N_IRQS != get_reg(hav, HAV_REG_DX, 0, (u64_t *)&port))
      _RAISE_0 ;
    
    // read the address
    if (HAV_N_IRQS != get_reg(hav, HAV_REG_SI, hav->inst.addr_size, &addr))
      _RAISE_0 ;
    
    if (0 == hav->inst.has_SOV)
      hav->inst.segx = HAV_SEG_DS ;

    if (HAV_N_IRQS != exec_in(hav, port, 1 << hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    // read the data
    if (HAV_N_IRQS != hav->write(hav, hav->inst.segx, addr, 1 << hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    // update the address
    _UDI(hav->inst.oprd_size) ;
  } break ;

  case 0xA2 : { _ZOV(0, 1) _AOV(3, 2)
    u8_t port ;
    u64_t addr ;

    // read the port
    if (HAV_N_IRQS != get_reg(hav, HAV_REG_DX, 0, (u64_t *)&port))
      _RAISE_0 ;
    
    // read the address
    if (HAV_N_IRQS != get_reg(hav, HAV_REG_SI, hav->inst.addr_size, &addr))
      _RAISE_0 ;
    
    if (0 == hav->inst.has_SOV)
      hav->inst.segx = HAV_SEG_DS ;

    // read the data
    if (HAV_N_IRQS != hav->read(hav, hav->inst.segx, addr, 1 << hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_out(hav, port, 1 << hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    // update the address
    _USI(hav->inst.oprd_size) ;
  } break ;

  case 0xA3 : { _ZOV(2, 3) _AOV(3, 2)
    u8_t port ;
    u64_t addr ;

    // read the port
    if (HAV_N_IRQS != get_reg(hav, HAV_REG_DX, 0, (u64_t *)&port))
      _RAISE_0 ;
    
    // read the address
    if (HAV_N_IRQS != get_reg(hav, HAV_REG_SI, hav->inst.addr_size, &addr))
      _RAISE_0 ;
    
    if (0 == hav->inst.has_SOV)
      hav->inst.segx = HAV_SEG_DS ;

    // read the data
    if (HAV_N_IRQS != hav->read(hav, hav->inst.segx, addr, 1 << hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    if (HAV_N_IRQS != exec_out(hav, port, 1 << hav->inst.oprd_size, &data.r0))
      _RAISE_0 ;

    // update the address
    _USI(hav->inst.oprd_size) ;
  } break ;

  case 0xA4 : { _ZOV(0, 1) _AOV(3, 2)
    // TODO
    _USI(hav->inst.oprd_size) ;
  } break ;

  case 0xA5 : { _ZOV(2, 3) _AOV(3, 2)
    // TODO
    _USI(hav->inst.oprd_size) ;
  } break ;

  case 0xA6 : { _ZOV(0, 1) _AOV(3, 2)
    // TODO
    _UDI(hav->inst.oprd_size) ;
    _USI(hav->inst.oprd_size) ;
  } break ;

  case 0xA7 : { _ZOV(2, 3) _AOV(3, 2)
    // TODO
    _UDI(hav->inst.oprd_size) ;
    _USI(hav->inst.oprd_size) ;
  } break ;

  case 0xA8 : { _ZOV(0, 1) _AOV(3, 2)
    // TODO
    _UDI(hav->inst.oprd_size) ;
    _USI(hav->inst.oprd_size) ;
  } break ;

  case 0xA9 : { _ZOV(2, 3) _AOV(3, 2)
    // TODO
    _UDI(hav->inst.oprd_size) ;
    _USI(hav->inst.oprd_size) ;
  } break ;

  case 0xAA : { _ZOV(0, 1) _AOV(3, 2)
    // TODO
    _UDI(hav->inst.oprd_size) ;
  } break ;

  case 0xAB : { _ZOV(2, 3) _AOV(3, 2)
    // TODO
    _UDI(hav->inst.oprd_size) ;
  } break ;

  case 0xAC : { _ZOV(0, 1) _AOV(3, 2)
    // TODO
    _USI(hav->inst.oprd_size) ;
  } break ;

  case 0xAD : { _ZOV(2, 3) _AOV(3, 2)
    // TODO
    _USI(hav->inst.oprd_size) ;
  } break ;

  case 0xAE : { _ZOV(0, 1) _AOV(3, 2)
    // TODO
    _USI(hav->inst.oprd_size) ;
  } break ;

  case 0xAF : { _ZOV(2, 3) _AOV(3, 2)
    // TODO
    _USI(hav->inst.oprd_size) ;
  } break ;

  case 0xB0 : case 0xB1 : case 0xB2 : case 0xB3 :
  case 0xB4 : case 0xB5 : case 0xB6 : case 0xB7 : { _ZOV(0, 1)
    // read the immediate value (signed-extended)
    if (0 != hav->inst.has_ZOV) {
      data.i0 = *(i16_t *)(code + ip) ; _UIP(2) ;
    } else {
      data.i0 = *(i8_t *)(code + ip) ; _UIP(1) ;
    }

    // copy the immediate value into the register
    if (HAV_N_IRQS != set_ireg(hav, hav->inst.opcode[0] & 7, hav->inst.oprd_size, data.i0))
      _RAISE_0 ;
  } break ;

  case 0xB8 : case 0xB9 : case 0xBA : case 0xBB :
  case 0xBC : case 0xBD : case 0xBE : case 0xBF : { _ZOV(2, 3)
      // read the immediate value (signed-extended)
      if (0 != hav->inst.has_ZOV) {
        data.i0 = *(i64_t *)(code + ip) ; _UIP(8) ;
      } else {
        data.i0 = *(i32_t *)(code + ip) ; _UIP(4) ;
      }

      // copy the immediate value into the register
      if (HAV_N_IRQS != set_ireg(hav, hav->inst.opcode[0] & 7, hav->inst.oprd_size, data.i0))
        _RAISE_0 ;
  } break ;

  case 0xC0 : case 0xC1 : case 0xC2 : case 0xC3 :
  case 0xC4 : case 0xC5 : case 0xC6 : case 0xC7 :
  case 0xC8 : case 0xC9 : case 0xCA : case 0xCB :
  case 0xCC : case 0xCD : case 0xCE : case 0xCF :
    _RAISE(HAV_IRQ_UNDEFINED_INST) ;

  case 0xD0 : case 0xD1 :
    _RAISE(HAV_IRQ_GENERAL_FAULT) ;

  case 0xD2 :
    _RAISE(HAV_IRQ_UNDEFINED_INST) ;

  case 0xD3 : _ZOV(2, 1)
    return exec_push(hav, 1 << hav->inst.oprd_size, &hav->flags) ;

  case 0xD4 : _ZOV(2, 1)
    return exec_pop(hav, 1 << hav->inst.oprd_size, &hav->flags) ;

  case 0xD5 : {
    u32_t cc = 0 != (hav->flags & HAV_FLAG_C) ;
    hav->flags &= ~HAV_FLAG_C ;
    hav->flags |= (~cc & 1) << __CF ;
  } break ;

  case 0xD6 : {
    hav->flags &= ~HAV_FLAG_C ;
  } break ;

  case 0xD7 : {
    hav->flags |= HAV_FLAG_C ;
  } break ;

  case 0xD8 : {
    if (0 != (hav->flags & HAV_FLAG_IOPL))
      _RAISE(HAV_IRQ_GENERAL_PROTECT) ;

    hav->flags &= ~HAV_FLAG_I ;
  } break ;

  case 0xD9 : {
    if (0 != (hav->flags & HAV_FLAG_IOPL))
      _RAISE(HAV_IRQ_GENERAL_PROTECT) ;

    hav->flags |= HAV_FLAG_I ;
  } break ;

  case 0xDA : {
    hav->flags &= ~HAV_FLAG_D ;
  } break ;

  case 0xDB : {
    hav->flags |= HAV_FLAG_D ;
  } break ;

  case 0xDC : {
    // raise an interrupt
    if (HAV_N_IRQS != exec_int(hav, *(u8_t *)(code + ip)))
      _RAISE_0 ;

    _UIP(1)
  } break ;

  case 0xDD :
    return exec_int(hav, HAV_IRQ_SINGLE_STEP) ;

  case 0xDE :
    return exec_int(hav, HAV_IRQ_BREAKPOINT) ;

  case 0xDF :
    return exec_iret(hav) ;

  case 0xE0 : {
    i16_t bytes ;

    // read the relative address
    if (0 != hav->inst.has_ZOV) {
      bytes = *(i16_t *)(code + ip) ; _UIP(2)
    } else {
      bytes = *(i8_t *)(code + ip) ; _UIP(1)
    }

    if (0 != hav->regs[HAV_REG_CX] && 0 == (hav->flags & HAV_FLAG_Z)) {
      // decrease the loop counter
      --hav->regs[HAV_REG_CX] ;

      // jump to the loop label
      hav->ip += ip + bytes ;
      return HAV_N_IRQS ;
    }
  } break ;

  case 0xE1 : {
    i16_t bytes ;

    // read the relative address
    if (0 != hav->inst.has_ZOV) {
      bytes = *(i16_t *)(code + ip) ; _UIP(2)
    } else {
      bytes = *(i8_t *)(code + ip) ; _UIP(1)
    }

    if (0 != hav->regs[HAV_REG_CX] && 0 != (hav->flags & HAV_FLAG_Z)) {
      // decrease the loop counter
      --hav->regs[HAV_REG_CX] ;

      // jump to the loop label
      hav->ip += ip + bytes ;
      return HAV_N_IRQS ;
    }
  } break ;

  case 0xE2 : {
    i16_t bytes ;

    // read the relative address
    if (0 != hav->inst.has_ZOV) {
      bytes = *(i16_t *)(code + ip) ; _UIP(2)
    } else {
      bytes = *(i8_t *)(code + ip) ; _UIP(1)
    }

    if (0 != hav->regs[HAV_REG_CX]) {
      // decrease the loop counter
      --hav->regs[HAV_REG_CX] ;

      // jump to the loop label
      hav->ip += ip + bytes ;
      return HAV_N_IRQS ;
    }
  } break ;

  case 0xE3 : {
    if (0 != (hav->flags & HAV_FLAG_IOPL))
      _RAISE(HAV_IRQ_GENERAL_PROTECT) ;

    hav->flags &= ~HAV_FLAG_A1 ;

    if (0 != hav->opts.verbose)
      fprintf(stderr, "Program HALTED\n") ;
  } break ;

  case 0xE4 : {
    u16_t bytes ;

    // read the # of bytes to pop from stack
    if (0 != hav->inst.has_ZOV)
      bytes = *(u16_t *)(code + ip) ;
    else
      bytes = *(u8_t *)(code + ip) ;

    // update the instruction pointer
    // _UIP(1) // useless because the pop of IP from stack

    // pop `bytes` from stack
    hav->regs[HAV_REG_SP] += bytes ;
  }

  case 0xE5 : {
    // pop the next instruction pointer from stack
    if (HAV_N_IRQS != exec_pop(hav, 8, &hav->ip))
      _RAISE_0 ;

    // do not update the IP
    return HAV_N_IRQS ;
  }

  case 0xE6 : {
    u16_t bytes ;

    // read the # of bytes to pop from stack
    if (0 != hav->inst.has_ZOV)
      bytes = *(u16_t *)(code + ip) ;
    else
      bytes = *(u8_t *)(code + ip) ;

    // update the instruction pointer
    // _UIP(1) // useless because the pop of IP from stack

    // pop `bytes` from stack
    hav->regs[HAV_REG_SP] += bytes ;
  }

  case 0xE7 : {
    // pop the next instruction pointer from stack
    if (HAV_N_IRQS != exec_pop(hav, 8, &hav->ip))
      _RAISE_0 ;

    // pop the next code segment descriptor from stack
    if (HAV_N_IRQS != exec_pop(hav, 2, hav->segs + HAV_SEG_CS))
      _RAISE_0 ;

    // do not update the IP
    return HAV_N_IRQS ;
  }

_jmp_near_i8 :
  case 0xE8 : {
    i16_t bytes ;

    // read the relative address
    if (0 != hav->inst.has_ZOV) {
      bytes = *(i16_t *)(code + ip) ; _UIP(2)
    } else {
      bytes = *(i8_t *)(code + ip) ; _UIP(1)
    }

    // update the instruction pointer
    hav->ip += ip + bytes ;

    // do not update the IP
    return HAV_N_IRQS ;
  }

_jmp_near_i32 :
  case 0xE9 : {
    i32_t bytes ;

    // read the relative address
    bytes = *(i32_t *)(code + ip) ; _UIP(4)

    // update the instruction pointer
    hav->ip += ip + bytes ;

    // do not update the IP
    return HAV_N_IRQS ;
  }

_jmp_far_i8 :
  case 0xEA : {
    u16_t segx ;
    u16_t addr ;

    // read the next code segment descriptor
    segx = *(u16_t *)(code + ip) ; _UIP(2)

    // read the next instruction pointer
    if (0 != hav->inst.has_ZOV) {
      addr = *(u16_t *)(code + ip) ;
      // _UIP(2) // useless
    } else {
      addr = *(u8_t *)(code + ip) ;
      // _UIP(1) // useless
    }

    // set the next instruction pointer

    hav->segs[HAV_SEG_CS] = segx ;
    hav->ip = addr ;

    // do not update the IP
    return HAV_N_IRQS ;
  }

_jmp_far_i32 :
  case 0xEB : {
    u16_t segx ;
    u64_t addr ;

    // read the next code segment descriptor
    segx = *(u16_t *)(code + ip) ; _UIP(2)

    // read the next instruction pointer
    if (0 != hav->inst.has_ZOV) {
      addr = *(u64_t *)(code + ip) ;
      // _UIP(8) // useless
    } else {
      addr = *(u32_t *)(code + ip) ;
      // _UIP(2) // useless
    }

    // set the next instruction pointer

    hav->segs[HAV_SEG_CS] = segx ;
    hav->ip = addr ;

    // do not update the IP
    return HAV_N_IRQS ;
  }

  case 0xEC : {
    i16_t bytes ;

    // read the relative address
    if (0 != hav->inst.has_ZOV) {
      bytes = *(i16_t *)(code + ip) ; _UIP(2)
    } else {
      bytes = *(i8_t *)(code + ip) ; _UIP(1)
    }

    // update the current instruction pointer
    hav->ip += ip ;

    // push the current instruction pointer onto the stack
    if (HAV_N_IRQS != exec_push(hav, 8, &hav->ip))
      _RAISE_0 ;

    // set the next instruction pointer
    hav->ip += bytes ;

    // do not update the IP
    return HAV_N_IRQS ;
  }

  case 0xED : {
    i32_t bytes ;

    // read the relative address
    bytes = *(i32_t *)(code + ip) ; _UIP(4)

    // update the current instruction pointer
    hav->ip += ip ;

    // push the current instruction pointer onto the stack
    if (HAV_N_IRQS != exec_push(hav, 8, &hav->ip))
      _RAISE_0 ;

    // set the next instruction pointer
    hav->ip += bytes ;

    // do not update the IP
    return HAV_N_IRQS ;
  }

  case 0xEE : {
    u16_t segx ;
    u16_t addr ;

    // read the next code segment descriptor
    segx = *(u16_t *)(code + ip) ; _UIP(2)

    // read the next instruction pointer
    if (0 != hav->inst.has_ZOV) {
      addr = *(u16_t *)(code + ip) ; _UIP(2)
    } else {
      addr = *(u8_t *)(code + ip) ; _UIP(1)
    }

    // update the current instruction pointer
    hav->ip += ip ;

    // push the current code segment descriptor onto the stack
    if (HAV_N_IRQS != exec_push(hav, 2, hav->segs + HAV_SEG_CS))
      _RAISE_0 ;

    // push the current instruction pointer onto the stack
    if (HAV_N_IRQS != exec_push(hav, 8, &hav->ip))
      _RAISE_0 ;

    // set the next instruction pointer

    hav->segs[HAV_SEG_CS] = segx ;
    hav->ip = addr ;

    // do not update the IP
    return HAV_N_IRQS ;
  }

  case 0xEF : {
    u16_t segx ;
    u64_t addr ;

    // read the next code segment descriptor
    segx = *(u16_t *)(code + ip) ; _UIP(2)

    // read the next instruction pointer
    if (0 != hav->inst.has_ZOV) {
      addr = *(u64_t *)(code + ip) ; _UIP(8)
    } else {
      addr = *(u32_t *)(code + ip) ; _UIP(4)
    }

    // update the current instruction pointer
    hav->ip += ip ;

    // push the current code segment descriptor onto the stack
    if (HAV_N_IRQS != exec_push(hav, 2, hav->segs + HAV_SEG_CS))
      _RAISE_0 ;

    // push the current instruction pointer onto the stack
    if (HAV_N_IRQS != exec_push(hav, 8, &hav->ip))
      _RAISE_0 ;

    // set the next instruction pointer

    hav->segs[HAV_SEG_CS] = segx ;
    hav->ip = addr ;

    // do not update the IP
    return HAV_N_IRQS ;
  }

  case 0xF0 : {

  } break ;

  case 0xF1 : {

  } break ;

  case 0xF2 : {

  } break ;

  case 0xF3 : {

  } break ;

  case 0xF4 : {

  } break ;

  case 0xF5 : {

  } break ;

  case 0xF6 : {

  } break ;

  case 0xF7 : {

  } break ;

  case 0xF8 : {

  } break ;

  case 0xF9 : {

  } break ;

  case 0xFA : {

  } break ;

  case 0xFB : {

  } break ;

  case 0xFC : {

  } break ;

  case 0xFD : {

  } break ;

  case 0xFE : {

  } break ;

  case 0xFF : {
    switch (hav->inst.opcode[1]) {
    case 0x00 : {

    } break ;

    case 0x01 : {

    } break ;

    case 0x02 : {

    } break ;

    case 0x03 : {

    } break ;

    case 0x04 : {

    } break ;

    case 0x05 : {

    } break ;

    case 0x06 : {

    } break ;

    case 0x07 : {

    } break ;

    case 0x08 : {

    } break ;

    case 0x09 : {

    } break ;

    case 0x0A : {

    } break ;

    case 0x0B : {

    } break ;

    case 0x0C : {

    } break ;

    case 0x0D : {

    } break ;

    case 0x0E : {

    } break ;

    case 0x0F : {

    } break ;

    case 0x10 : {

    } break ;

    case 0x11 : {

    } break ;

    case 0x12 : {

    } break ;

    case 0x13 : {

    } break ;

    case 0x14 : {

    } break ;

    case 0x15 : {

    } break ;

    case 0x16 : {

    } break ;

    case 0x17 : {

    } break ;

    case 0x18 : {

    } break ;

    case 0x19 : {

    } break ;

    case 0x1A : {

    } break ;

    case 0x1B : {

    } break ;

    case 0x1C : {

    } break ;

    case 0x1D : {

    } break ;

    case 0x1E : {

    } break ;

    case 0x1F : {

    } break ;

    case 0x20 : {

    } break ;

    case 0x21 : {

    } break ;

    case 0x22 : {

    } break ;

    case 0x23 : {

    } break ;

    case 0x24 : {

    } break ;

    case 0x25 : {

    } break ;

    case 0x26 : {

    } break ;

    case 0x27 : {

    } break ;

    case 0x28 : {

    } break ;

    case 0x29 : {

    } break ;

    case 0x2A : {

    } break ;

    case 0x2B : {

    } break ;

    case 0x2C : {

    } break ;

    case 0x2D : {

    } break ;

    case 0x2E : {

    } break ;

    case 0x2F : {

    } break ;

    case 0x30 : {

    } break ;

    case 0x31 : {

    } break ;

    case 0x32 : {

    } break ;

    case 0x33 : {

    } break ;

    case 0x34 : {

    } break ;

    case 0x35 : {

    } break ;

    case 0x36 : {

    } break ;

    case 0x37 : {

    } break ;

    case 0x38 : {

    } break ;

    case 0x39 : {

    } break ;

    case 0x3A : {

    } break ;

    case 0x3B : {

    } break ;

    case 0x3C : {

    } break ;

    case 0x3D : {

    } break ;

    case 0x3E : {

    } break ;

    case 0x3F : {

    } break ;

    case 0x40 : {

    } break ;

    case 0x41 : {

    } break ;

    case 0x42 : {

    } break ;

    case 0x43 : {

    } break ;

    case 0x44 : {

    } break ;

    case 0x45 : {

    } break ;

    case 0x46 : {

    } break ;

    case 0x47 : {

    } break ;

    case 0x48 : {

    } break ;

    case 0x49 : {

    } break ;

    case 0x4A : {

    } break ;

    case 0x4B : {

    } break ;

    case 0x4C : {

    } break ;

    case 0x4D : {

    } break ;

    case 0x4E : {

    } break ;

    case 0x4F : {

    } break ;

    case 0x50 : {

    } break ;

    case 0x51 : {

    } break ;

    case 0x52 : {

    } break ;

    case 0x53 : {

    } break ;

    case 0x54 : {

    } break ;

    case 0x55 : {

    } break ;

    case 0x56 : {

    } break ;

    case 0x57 : {

    } break ;

    case 0x58 : {

    } break ;

    case 0x59 : {

    } break ;

    case 0x5A : {

    } break ;

    case 0x5B : {

    } break ;

    case 0x5C : {

    } break ;

    case 0x5D : {

    } break ;

    case 0x5E : {

    } break ;

    case 0x5F : {

    } break ;

    case 0x60 : {

    } break ;

    case 0x61 : {

    } break ;

    case 0x62 : {

    } break ;

    case 0x63 : {

    } break ;


    case 0x64 : {

    } break ;

    case 0x65 : {

    } break ;

    case 0x66 : {

    } break ;

    case 0x67 : {

    } break ;

    case 0x68 : {

    } break ;

    case 0x69 : {

    } break ;

    case 0x6A : {

    } break ;

    case 0x6B : {

    } break ;

    case 0x6C : {

    } break ;

    case 0x6D : {

    } break ;

    case 0x6E : {

    } break ;

    case 0x6F : {

    } break ;

    case 0x70 : {

    } break ;

    case 0x71 : {

    } break ;

    case 0x72 : {

    } break ;

    case 0x73 : {

    } break ;

    case 0x74 : {

    } break ;

    case 0x75 : {

    } break ;

    case 0x76 : {

    } break ;

    case 0x77 : {

    } break ;

    case 0x78 : {

    } break ;

    case 0x79 : {

    } break ;

    case 0x7A : {

    } break ;

    case 0x7B : {

    } break ;

    case 0x7C : {

    } break ;

    case 0x7D : {

    } break ;

    case 0x7E : {

    } break ;

    case 0x7F : {

    } break ;

    case 0x80 : {

    } break ;

    case 0x81 : {

    } break ;

    case 0x82 : {

    } break ;

    case 0x83 : {

    } break ;

    case 0x84 : {

    } break ;

    case 0x85 : {

    } break ;

    case 0x86 : {

    } break ;

    case 0x87 : {

    } break ;

    case 0x88 : case 0x89 : case 0x8A : case 0x8B :
    case 0x8C : case 0x8D : case 0x8E : case 0x8F :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;

    case 0x90 : case 0x91 : case 0x92 : case 0x93 :
    case 0x94 : case 0x95 : case 0x96 : case 0x97 :
    case 0x98 : case 0x99 : case 0x9A : case 0x9B :
    case 0x9C : case 0x9D : case 0x9E : case 0x9F :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;

    case 0xA0 : case 0xA1 : case 0xA2 : case 0xA3 :
    case 0xA4 : case 0xA5 : case 0xA6 : case 0xA7 :
    case 0xA8 : case 0xA9 : case 0xAA : case 0xAB :
    case 0xAC : case 0xAD : case 0xAE : case 0xAF :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;

    case 0xB0 : case 0xB1 : case 0xB2 : case 0xB3 :
    case 0xB4 : case 0xB5 : case 0xB6 : case 0xB7 :
    case 0xB8 : case 0xB9 : case 0xBA : case 0xBB :
    case 0xBC : case 0xBD : case 0xBE : case 0xBF :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;
    
    case 0xC0 : case 0xC1 : case 0xC2 : case 0xC3 :
    case 0xC4 : case 0xC5 : case 0xC6 : case 0xC7 :
    case 0xC8 : case 0xC9 : case 0xCA : case 0xCB :
    case 0xCC : case 0xCD : case 0xCE : case 0xCF :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;
    
    case 0xD0 : case 0xD1 : case 0xD2 : case 0xD3 :
    case 0xD4 : case 0xD5 : case 0xD6 : case 0xD7 :
    case 0xD8 : case 0xD9 : case 0xDA : case 0xDB :
    case 0xDC : case 0xDD : case 0xDE : case 0xDF :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;

    case 0xE0 : case 0xE1 : case 0xE2 : case 0xE3 :
    case 0xE4 : case 0xE5 : case 0xE6 : case 0xE7 :
    case 0xE8 : case 0xE9 : case 0xEA : case 0xEB :
    case 0xEC : case 0xED : case 0xEE : case 0xEF :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;

    case 0xF0 : case 0xF1 : case 0xF2 : case 0xF3 :
    case 0xF4 : case 0xF5 : case 0xF6 : case 0xF7 :
    case 0xF8 : case 0xF9 : case 0xFA : case 0xFB :
    case 0xFC : case 0xFD : case 0xFE : case 0xFF :
      _RAISE(HAV_IRQ_UNDEFINED_INST) ;

    default :
      _RAISE(HAV_IRQ_NON_MASKABLE) ;
    } // FF
  } break ;

  default :
    _RAISE(HAV_IRQ_NON_MASKABLE) ;
  }

  // update the instruction pointer

  hav->ip += ip ;

  return HAV_N_IRQS ;
}