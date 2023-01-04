#include "hav.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern u32_t fetch     (hav_t * hav) ;
extern u32_t exec_push (hav_t * hav, u64_t size, const void * data) ;
extern u32_t exec_pop  (hav_t * hav, u64_t size, void * data) ;
extern u32_t exec_int  (hav_t * hav, u32_t irq) ;
extern u32_t exec_iret (hav_t * hav) ;
extern u32_t exec      (hav_t * hav) ;

u32_t vm_mem_read (hav_t * hav, u64_t addr, u64_t size, void * data)
{
  // fprintf(stderr, "MEMORY READ  : 0x%016llX (%zu)\n", addr, size) ;
  // getchar() ;

  if (hav->mem.size < addr + size)
    return hav->raise(hav, HAV_IRQ_OUT_OF_MEMORY) ;
  
  u8_t * _data = (u8_t *)data ;

  for (u64_t i = 0 ; i < size ; ++i)
    _data[i] = hav->mem.data[addr + i] ;
  
  return HAV_N_IRQS ;
}

u32_t vm_mem_write (hav_t * hav, u64_t addr, u64_t size, const void * data)
{
  // fprintf(stderr, "MEMORY WRITE : 0x%016llX (%zu)\n", addr, size) ;
  // getchar() ;

  if (hav->mem.size < addr + size)
    return hav->raise(hav, HAV_IRQ_OUT_OF_MEMORY) ;
  
  u8_t * _data = (u8_t *)data ;

  for (u64_t i = 0 ; i < size ; ++i)
    hav->mem.data[addr + i] = _data[i] ;
  
  return HAV_N_IRQS ;
}

u32_t vm_raise (hav_t * hav, u32_t irq)
{
  exec_int(hav, irq) ;
  return hav->irq ;
}

u32_t vaddr_to_paddr (hav_t * hav, u16_t segx, u64_t * addr, u64_t * size, u8_t perm)
{
  hav_sde_t sde ;

  if (HAV_N_IRQS != hav->mem.read(hav, hav->sdt + segx * sizeof(sde), sizeof(sde), &sde))
    return hav->irq ;
  
  if ((sde.perm & HAV_FLAG_IOPL) < (hav->flags & HAV_FLAG_IOPL) || 0 == (sde.perm & perm))
    return hav->raise(hav, HAV_IRQ_GENERAL_PROTECT) ;
  
  if (0 != (hav->flags & HAV_FLAG_VB)) {
    if (sde.size < *addr)
      return hav->raise(hav, HAV_IRQ_SEGMENT_FAULT) ;
    
    if (sde.size < *addr + *size)
      *size = sde.size - *addr ;
  } else {
    if (sde.size < *addr + *size)
      return hav->raise(hav, HAV_IRQ_SEGMENT_FAULT) ;
  }

  *addr += sde.addr ;

  return HAV_N_IRQS ;
}

u32_t vm_read (hav_t * hav, u16_t segx, u64_t addr, u64_t size, void * data)
{
  u64_t _addr = addr ;
  u64_t _size = size ;

  // fprintf(stderr, "READ: 0x%04X:%016llX\n", segx, addr) ;

  if (HAV_N_IRQS != vaddr_to_paddr(hav, segx, &_addr, &_size, HAV_SEG_PERM_R))
    return hav->irq ;

  return hav->mem.read(hav, _addr, _size, data) ;
}

u32_t vm_write (hav_t * hav, u16_t segx, u64_t addr, u64_t size, const void * data)
{
  u64_t _addr = addr ;
  u64_t _size = size ;

  // fprintf(stderr, "WRITE: 0x%04X:%016llX\n", segx, addr) ;

  if (HAV_N_IRQS != vaddr_to_paddr(hav, segx, &_addr, &_size, HAV_SEG_PERM_W))
    return hav->irq ;

  return hav->mem.write(hav, _addr, _size, data) ;
}

u32_t vm_clock (hav_t * hav)
{
  if (hav->opts.max_cks <= hav->cks) {
    hav->flags &= ~HAV_FLAG_A1 ;
    return HAV_N_IRQS ;
  }

  if (0 != hav->inst.has_REP && 0 != hav->regs[HAV_REG_CX]) {
    if (hav->inst.REP_cond == !!(hav->flags & HAV_FLAG_Z))
      goto _exec ;
  }

  if (HAV_N_IRQS != fetch(hav))
    return hav->irq ;

_exec :
  if (HAV_N_IRQS != exec(hav))
    return hav->irq ;

  ++hav->cks ;

  if (0 != hav->inst.has_REP && 0 != hav->regs[HAV_REG_CX])
    --hav->regs[HAV_REG_CX] ;

  return HAV_N_IRQS ;
}

u32_t load_img_state (hav_t * hav, const char * fn, int mag)
{
  // check the magic number
  if (HAV_MAG_2 != mag && HAV_MAG_3 != mag)
    return 1 ;
  
  // clear the machine before load image
  if (0 != (hav->flags & HAV_FLAG_A1)) {
    if (NULL != hav->mem.data) {
      free(hav->mem.data) ;
      hav->mem.data = NULL ;
    }

    if (NULL == memset(hav, 0, sizeof(*hav))) {
      fprintf(stderr, "panic: malloc() failed: not enough memory\n") ;
      exit(EXIT_FAILURE) ;
    }
  }

  FILE * fp ;

#ifdef _WIN32
  fp = fopen(fn, "rb") ;
#else
  fp = fopen(fn, "r") ;
#endif

  if (NULL == fp) {
    fprintf(stderr, "error: cannot load the state: cannot open `%s`\n", fn) ;
    return 1 ;
  }

  u32_t _mag ;

  if (1 != fread(&_mag, sizeof(int), 1, fp)) {
    fprintf(stderr, "error: cannot load the state: cannot load the magic number 0x%08X\n", mag) ;
    goto _error ;
  }

  if (mag != _mag) {
    fprintf(stderr, "error: cannot load the state: the magic number is not equal to 0x%08X\n", mag) ;
    goto _error ;
  }

  if (HAV_MAG_2 != mag) {
    if (1 != fread(&hav->opts, sizeof(hav->opts), 1, fp)) {
      fprintf(stderr, "error: cannot load the state: cannot load the options\n") ;
      goto _error ;
    }

    if (1 != fread(&hav->inst, sizeof(hav->inst), 1, fp)) {
      fprintf(stderr, "error: cannot load the state: cannot load the instruction\n") ;
      goto _error ;
    }

    if (1 != fread(&hav->cks, sizeof(hav->cks), 1, fp)) {
      fprintf(stderr, "error: cannot load the state: cannot load the clocks\n") ;
      goto _error ;
    }
  }

  if (1 != fread(&hav->flags, sizeof(hav->flags), 1, fp)) {
    fprintf(stderr, "error: cannot load the state: cannot load the flags\n") ;
    goto _error ;
  }

  if (1 != fread(&hav->ip, sizeof(hav->ip), 1, fp)) {
    fprintf(stderr, "error: cannot load the state: cannot load the instruction pointer\n") ;
    goto _error ;
  }

  if (1 != fread(hav->regs, sizeof(hav->regs), 1, fp)) {
    fprintf(stderr, "error: cannot load the state: cannot load the registers\n") ;
    goto _error ;
  }

  if (1 != fread(hav->fp, sizeof(hav->fp), 1, fp)) {
    fprintf(stderr, "error: cannot load the current state: cannot load the floating-point registers\n") ;
    goto _error ;
  }

  if (1 != fread(hav->segs, sizeof(hav->segs), 1, fp)) {
    fprintf(stderr, "error: cannot load the state: cannot load the segment registers\n") ;
    goto _error ;
  }

  if (HAV_MAG_2 != mag) {
    if (1 != fread(&hav->irq, sizeof(hav->irq), 1, fp)) {
      fprintf(stderr, "error: cannot load the state: cannot load the last interrupt request\n") ;
      goto _error ;
    }
  }

  if (1 != fread(&hav->idt, sizeof(hav->idt), 1, fp)) {
    fprintf(stderr, "error: cannot load the state: cannot load the interrupt descriptor table address\n") ;
    goto _error ;
  }

  if (1 != fread(&hav->sdt, sizeof(hav->sdt), 1, fp)) {
    fprintf(stderr, "error: cannot load the state: cannot load the segment descriptor table address\n") ;
    goto _error ;
  }

  if (1 != fread(&hav->mem.size, sizeof(hav->mem.size), 1, fp)) {
    fprintf(stderr, "error: cannot load the state: cannot load the memory size\n") ;
    goto _error ;
  }

  hav->mem.data = (u8_t *)malloc(hav->mem.size) ;

  if (NULL == hav->mem.data) {
    fclose(fp) ;
    fprintf(stderr, "panic: malloc() failed: not enough memory\n") ;
    exit(EXIT_FAILURE) ;
  }

  if (HAV_MAG_2 != mag) {
    if (1 != fread(hav->mem.data, hav->mem.size, 1, fp)) {
      fprintf(stderr, "error: cannot load the state: cannot load the memory data\n") ;
      goto _error ;
    }
  }

  fclose(fp) ;

  return 0 ;

_error :
  fclose(fp) ;

  return 1 ;
}

u32_t load_img_sys (hav_t * hav, const char * fn, int mag)
{
  // check the magic number
  if (HAV_MAG_0 != mag && HAV_MAG_1 != mag)
    return 1 ;

  if (0 != hav->opts.verbose)
    fprintf(stderr, "Try to load system image `%s` (0x%08X)\n", fn, mag) ;

  FILE * fp ;

#ifdef _WIN32
  fp = fopen(fn, "rb") ;
#else
  fp = fopen(fn, "r") ;
#endif

  if (NULL == fp) {
    fprintf(stderr, "error: cannot load the system: cannot open `%s`\n", fn) ;
    return 1 ;
  }

  u32_t _mag ;

  if (1 != fread(&_mag, sizeof(u32_t), 1, fp)) {
    fprintf(stderr, "error: cannot load the system: cannot load the magic number 0x%08X\n", mag) ;
    goto _error ;
  }

  if (mag != _mag) {
    fprintf(stderr, "error: cannot load the system: the magic number is not equal to 0x%08X\n", mag) ;
    goto _error ;
  }

  // load the memory size

  u64_t mem_size ;

  if (1 != fread(&mem_size, sizeof(mem_size), 1, fp)) {
    fprintf(stderr, "error: cannot load the system: cannot load the memory size\n") ;
    goto _error ;
  }

  mem_size *= KB ;

  if (hav->mem.size < mem_size) {
    if (NULL != hav->mem.data) {
      free(hav->mem.data) ;
      hav->mem.data = NULL ;
    }

    hav->mem.size = mem_size ;
    hav->mem.data = (u8_t *)malloc(hav->mem.size) ;

    if (NULL == hav->mem.data) {
      fclose(fp) ;
      fprintf(stderr, "panic: malloc() failed: not enough memory\n") ;
      exit(EXIT_FAILURE) ;
    }
  }

  // segment descriptor table

  static hav_sde_t sdt [] = {
    (hav_sde_t){ 0 , 0 , HAV_SEG_PERM_E | HAV_SEG_PERM_X | HAV_SEG_PERM_R | HAV_SEG_PERM_W   } , // total memory
    (hav_sde_t){ 0 , 0 , HAV_SEG_PERM_E | HAV_SEG_PERM_R | HAV_SEG_PERM_SDT                  } , // segment descriptor table
    (hav_sde_t){ 0 , 0 , HAV_SEG_PERM_E | HAV_SEG_PERM_R | HAV_SEG_PERM_IDT                  } , // interrupt descriptor table
    (hav_sde_t){ 0 , 0 , HAV_SEG_PERM_E | HAV_SEG_PERM_X | HAV_SEG_PERM_R                    } , // interrupt service functions
    (hav_sde_t){ 0 , 0 , HAV_SEG_PERM_E | HAV_SEG_PERM_X | HAV_SEG_PERM_R                    } , // kernel code segment
    (hav_sde_t){ 0 , 0 , HAV_SEG_PERM_E | HAV_SEG_PERM_R | HAV_SEG_PERM_W                    } , // kernel data segment
    (hav_sde_t){ 0 , 0 , HAV_SEG_PERM_E | HAV_SEG_PERM_R | HAV_SEG_PERM_W                    }   // kernel stack segment
  } ;

  // interrupt descriptor table

  static hav_ide_t idt [256] ;

  static u8_t isr [] = {
    0xDF , // iret
    0xE3   // hlt
  } ;

  // load system

  u64_t ker_code_size ;
  u64_t ker_data_size ;
  u64_t ker_stack_size ;

  if (1 != fread(&ker_code_size, sizeof(ker_code_size), 1, fp)) {
    fprintf(stderr, "error: cannot load the system: cannot load the kernel code size\n") ;
    goto _error ;
  }

  if (1 != fread(&ker_data_size, sizeof(ker_data_size), 1, fp)) {
    fprintf(stderr, "error: cannot load the system: cannot load the kernel data size\n") ;
    goto _error ;
  }

  if (1 != fread(&ker_stack_size, sizeof(ker_stack_size), 1, fp)) {
    fprintf(stderr, "error: cannot load the system: cannot load the kernel stack size\n") ;
    goto _error ;
  }

  sdt[0].addr = 0 ;
  sdt[0].size = hav->mem.size ;
  sdt[1].addr = 0 ;
  sdt[1].size = sizeof(sdt) ;
  sdt[2].addr = sdt[1].addr + sdt[1].size ;
  sdt[2].size = sizeof(idt) ;
  sdt[3].addr = sdt[2].addr + sdt[2].size ;
  sdt[3].size = sizeof(isr) ;
  sdt[4].addr = sdt[3].addr + sdt[3].size ;
  sdt[4].size = ker_code_size ;
  sdt[5].addr = sdt[4].addr + sdt[4].size ;
  sdt[5].size = ker_data_size ;
  sdt[6].addr = sdt[5].addr + sdt[5].size ;
  sdt[6].size = ker_stack_size ;

  for (int i = 0 ; i < 256 ; ++i) {
    idt[i].segx = 6 ;
    idt[i].addr = 0 ;
  }

  if (1 != fread(hav->mem.data + sdt[4].addr, ker_code_size, 1, fp)) {
    fprintf(stderr, "error: cannot load the system: cannot load the kernel code\n") ;
    goto _error ;
  }

  if (1 != fread(hav->mem.data + sdt[5].addr, ker_data_size, 1, fp)) {
    fprintf(stderr, "error: cannot load the system: cannot load the kernel data\n") ;
    goto _error ;
  }

  fclose(fp) ;

  if (NULL == memcpy(hav->mem.data + sdt[1].addr, sdt, sizeof(sdt))) {
    fprintf(stderr, "panic: memcpy() failed\n") ;
    exit(EXIT_FAILURE) ;
  }

  if (NULL == memcpy(hav->mem.data + sdt[2].addr, idt, sizeof(idt))) {
    fprintf(stderr, "panic: memcpy() failed\n") ;
    exit(EXIT_FAILURE) ;
  }

  if (NULL == memcpy(hav->mem.data + sdt[3].addr, isr, sizeof(isr))) {
    fprintf(stderr, "panic: memcpy() failed\n") ;
    exit(EXIT_FAILURE) ;
  }

  hav->sysenv.flags = HAV_FLAG_I ;
  hav->sysenv.segs[HAV_SEG_CS] = 4 ;
  hav->sysenv.segs[HAV_SEG_DS] = 5 ;
  hav->sysenv.segs[HAV_SEG_ES] = 5 ;
  hav->sysenv.segs[HAV_SEG_SS] = 6 ;
  hav->sysenv.ip = 0 ;
  hav->sysenv.sp = hav->sysenv.bp = 0 ;

  if (0 != hav->opts.verbose) {
    fprintf(
      stderr ,
      "Machine:\n"
      "  Memory : 0x%016llX\n"
      "  FLAGS  : 0x%08X\n"
      "  CS     : 0x%04X\n"
      "  DS     : 0x%04X\n"
      "  ES     : 0x%04X\n"
      "  SS     : 0x%04X\n"
      "  IP     : 0x%016llX\n"
      "  SP     : 0x%016llX\n"
      "  BP     : 0x%016llX\n"     ,
      hav->mem.size                ,
      hav->sysenv.flags            ,
      hav->sysenv.segs[HAV_SEG_CS] ,
      hav->sysenv.segs[HAV_SEG_DS] ,
      hav->sysenv.segs[HAV_SEG_ES] ,
      hav->sysenv.segs[HAV_SEG_SS] ,
      hav->sysenv.ip               ,
      hav->sysenv.sp               ,
      hav->sysenv.bp
    ) ;

    fprintf(stderr, "\nSegment Descriptor Table\n") ;

    for (u16_t i = 0 ; i < 7 ; ++i) {
      fprintf(
        stderr ,
        "\nSegment (0x%04X):\n"
        "  Address     : 0x%016llX\n"
        "  Size        : 0x%016llX\n"
        "  Permissions : 0x%04X\n"   ,
        i                         ,
        sdt[i].addr                  ,
        sdt[i].size                  ,
        sdt[i].perm
      ) ;
    }

    fprintf(stderr, "\nInterrupt Descriptor Table\n") ;

    for (u16_t i = 0 ; i < 256 ; ++i) {
      fprintf(
        stderr ,
        "0x%02X: 0x%04X:%016llX\n" ,
        i                          ,
        idt[i].segx                ,
        idt[i].addr
      ) ;
    }
  }

  hav->flags = hav->sysenv.flags ;
  hav->segs[HAV_SEG_CS] = hav->sysenv.segs[HAV_SEG_CS] ;
  hav->segs[HAV_SEG_DS] = hav->sysenv.segs[HAV_SEG_DS] ;
  hav->segs[HAV_SEG_ES] = hav->sysenv.segs[HAV_SEG_ES] ;
  hav->segs[HAV_SEG_SS] = hav->sysenv.segs[HAV_SEG_SS] ;
  hav->ip = hav->sysenv.ip ;
  hav->regs[HAV_REG_SP] = hav->sysenv.sp ;
  hav->regs[HAV_REG_BP] = hav->sysenv.bp ;
  hav->sdt = sdt[1].addr ;
  hav->idt = sdt[2].addr ;

  return 0 ;

_error :
  fclose(fp) ;

  return 1 ;
}

u32_t vm_load_img (hav_t * hav, const char * fn, int mag)
{
  if (0 == mag) {
      FILE * fp ;

#ifdef _WIN32
    fp = fopen(fn, "rb") ;
#else
    fp = fopen(fn, "r") ;
#endif

    if (NULL == fp) {
      fprintf(stderr, "error: cannot load the image: cannot open `%s`\n", fn) ;
      return 1 ;
    }

    if (1 != fread(&mag, sizeof(u32_t), 1, fp)) {
      fprintf(stderr, "error: cannot load the image: cannot load the magic number\n") ;
      fclose(fp) ;
      return 1 ;
    }

    fclose(fp) ;
  }

  if (0 != hav->opts.verbose)
    fprintf(stderr, "Try to load `%s` (0x%08X)\n", fn, mag) ;

  // load the machine state
  if (HAV_MAG_2 == mag || HAV_MAG_3 == mag)
    return load_img_state(hav, fn, mag) ;
  
  // load the operating system image
  if (HAV_MAG_0 == mag || HAV_MAG_1 == mag)
    return load_img_sys(hav, fn, mag) ;

  return 1 ;
}

u32_t vm_save_img (hav_t * hav, const char * fn, int mag)
{
  // check the magic number
  if (HAV_MAG_2 != mag && HAV_MAG_3 != mag)
    return 1 ;

  FILE * fp ;

#ifdef _WIN32
  fp = fopen(fn, "wb") ;
#else
  fp = fopen(fn, "w") ;
#endif

  if (NULL == fp) {
    fprintf(stderr, "error: cannot save the current state: cannot open/create `%s`\n", fn) ;
    return 1 ;
  }

  if (1 != fwrite(&mag, sizeof(int), 1, fp)) {
    fprintf(stderr, "error: cannot save the current state: cannot save the magic number 0x%08X\n", mag) ;
    goto _error ;
  }

  if (HAV_MAG_2 != mag) {
    if (1 != fwrite(&hav->opts, sizeof(hav->opts), 1, fp)) {
      fprintf(stderr, "error: cannot save the current state: cannot save the options\n") ;
      goto _error ;
    }

    if (1 != fwrite(&hav->inst, sizeof(hav->inst), 1, fp)) {
      fprintf(stderr, "error: cannot save the current state: cannot save the instruction\n") ;
      goto _error ;
    }

    if (1 != fwrite(&hav->cks, sizeof(hav->cks), 1, fp)) {
      fprintf(stderr, "error: cannot save the current state: cannot save the clocks\n") ;
      goto _error ;
    }
  }

  if (1 != fwrite(&hav->flags, sizeof(hav->flags), 1, fp)) {
    fprintf(stderr, "error: cannot save the current state: cannot save the flags\n") ;
    goto _error ;
  }

  if (1 != fwrite(&hav->ip, sizeof(hav->ip), 1, fp)) {
    fprintf(stderr, "error: cannot save the current state: cannot save the instruction pointer\n") ;
    goto _error ;
  }

  if (1 != fwrite(hav->regs, sizeof(hav->regs), 1, fp)) {
    fprintf(stderr, "error: cannot save the current state: cannot save the registers\n") ;
    goto _error ;
  }

  if (1 != fwrite(hav->fp, sizeof(hav->fp), 1, fp)) {
    fprintf(stderr, "error: cannot save the current state: cannot save the floating-point registers\n") ;
    goto _error ;
  }

  if (1 != fwrite(hav->segs, sizeof(hav->segs), 1, fp)) {
    fprintf(stderr, "error: cannot save the current state: cannot save the segment registers\n") ;
    goto _error ;
  }

  if (HAV_MAG_2 != mag) {
    if (1 != fwrite(&hav->irq, sizeof(hav->irq), 1, fp)) {
      fprintf(stderr, "error: cannot save the current state: cannot save the interrupt request\n") ;
      goto _error ;
    }
  }

  if (1 != fwrite(&hav->idt, sizeof(hav->idt), 1, fp)) {
    fprintf(stderr, "error: cannot save the current state: cannot save the interrupt descriptor table address\n") ;
    goto _error ;
  }

  if (1 != fwrite(&hav->sdt, sizeof(hav->sdt), 1, fp)) {
    fprintf(stderr, "error: cannot save the current state: cannot save the segment descriptor table address\n") ;
    goto _error ;
  }

  if (1 != fwrite(&hav->mem.size, sizeof(hav->mem.size), 1, fp)) {
    fprintf(stderr, "error: cannot save the current state: cannot save the memory size\n") ;
    goto _error ;
  }

  if (HAV_MAG_2 != mag) {
    if (1 != fwrite(hav->mem.data, hav->mem.size, 1, fp)) {
      fprintf(stderr, "error: cannot save the current state: cannot save the memory data\n") ;
      goto _error ;
    }
  }

  fclose(fp) ;

  return 0 ;

_error :
  fclose(fp) ;

  return 1 ;
}