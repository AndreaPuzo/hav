#include "hav.h"
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int main (int argc, char ** argv)
{
  if (0 == argc) {
    fprintf(stderr, "panic: command line: program name not found\n") ;
    exit(EXIT_FAILURE) ;
  }

  if (argc < 2) {
    fprintf(stderr, "fatal: no images have been provided\n") ;
    fprintf(stderr, "usage: %s [options...] [images...] -a [arguments...]\n", argv[0]) ;
    exit(EXIT_FAILURE) ;
  }

  if (0 == strcmp(argv[1], "--help") || 0 == strcmp(argv[1], "-h")) {
    fprintf(
      stderr                                                   ,
      "usage: %s [options...] [images...] -a [arguments...]\n" 
      "options:\n"
      "  -h, --help    | print this help page\n"
      "  -v, --version | print the version\n"
      "  -c, --clocks  | set the max number of clocks to perform\n"
      "      --verbose | print extra information\n"
                                                               ,
      argv[0]
    ) ;

    exit(EXIT_SUCCESS) ;
  }

  // scan the options

  struct {
    u64_t  clocks  ;
    u64_t  size    ;
    int    verbose ;
    char * image   ;
    char * state   ;
  } opts ;

  opts.clocks  = -1     ;
  opts.size    = 8 * MB ;
  opts.verbose = 0      ;
  opts.image   = NULL   ;
  opts.state   = NULL   ;

  for (int i = 1 ; i < argc ; ++i) {
    if (0 == strcmp(argv[i], "-c") || 0 == strcmp(argv[i], "--clocks")) {
      if (argc == ++i) {
        fprintf(stderr, "fatal: missing argument for `%s`: clocks\n", argv[i - 1]) ;
        exit(EXIT_FAILURE) ;
      }

      opts.clocks = strtoull(argv[i], NULL, 0) ;
    } else if (0 == strcmp(argv[i], "-m") || 0 == strcmp(argv[i], "--mem-size")) {
      if (argc == ++i) {
        fprintf(stderr, "fatal: missing argument for `%s`: memory size\n", argv[i - 1]) ;
        exit(EXIT_FAILURE) ;
      }

      opts.size = strtoull(argv[i], NULL, 0) * KB ;
    } else if (0 == strcmp(argv[i], "-s") || 0 == strcmp(argv[i], "--state")) {
      if (NULL != opts.state) {
        fprintf(stderr, "error: too many state images\n") ;
        exit(EXIT_FAILURE) ;
      }

      if (argc == ++i) {
        fprintf(stderr, "fatal: missing argument for `%s`: state image\n", argv[i - 1]) ;
        exit(EXIT_FAILURE) ;
      }

      opts.size = strtoull(argv[i], NULL, 0) * KB ;
    } else if (0 == strcmp(argv[i], "--verbose")) {
      opts.verbose = 1 ;
    } else {
      if (NULL != opts.image) {
        fprintf(stderr, "error: too many images\n") ;
        exit(EXIT_FAILURE) ;
      }

      opts.image = argv[i] ;
    }
  }

  static hav_t hav ;

  if (NULL == memset(&hav, 0, sizeof(hav))) {
    fprintf(stderr, "panic: memset() failed\n") ;
    exit(EXIT_FAILURE) ;
  }

  hav.mem.read  = vm_mem_read  ;
  hav.mem.write = vm_mem_write ;
  hav.raise     = vm_raise     ;
  hav.read      = vm_read      ;
  hav.write     = vm_write     ;
  hav.clock     = vm_clock     ;
  hav.load      = vm_load_img  ;
  hav.save      = vm_save_img  ;

  hav.opts.max_cks = opts.clocks ;
  hav.opts.verbose = opts.verbose ;

  if (NULL != opts.state) {
    // load the state image
    if (0 != hav.load(&hav, opts.state, 0)) {
      fprintf(stderr, "fatal: something has gone wrong loading the state image `%s`\n", opts.state) ;
      exit(EXIT_FAILURE) ;
    }

    goto _start ;
  }

  hav.mem.size = opts.size ;
  hav.mem.data = (u8_t *)malloc(hav.mem.size) ;

  if (NULL == hav.mem.data) {
    fprintf(stderr, "panic: malloc() failed: not enough memory\n") ;
    exit(EXIT_FAILURE) ;
  }

  // load the image
  if (0 != hav.load(&hav, opts.image, 0)) {
    fprintf(stderr, "fatal: something has gone wrong loading the image `%s`\n", opts.image) ;
    exit(EXIT_FAILURE) ;
  }

_start :

  // run the machine

  hav.flags |= HAV_FLAG_A1 ;

hav.regs[HAV_REG_SI] = 50 ;

  if (0 != opts.verbose) {
    while (0 != (hav.flags & HAV_FLAG_A1)) {
      fprintf(stderr, "FLAGS : 0x%08X\n", hav.flags) ;
      fprintf(stderr, "IP    : 0x%04X:%016llX\n", hav.segs[HAV_SEG_CS], hav.ip) ;
      fprintf(stderr, "AX    : 0x%016llX\n", hav.regs[HAV_REG_AX]) ;
      fprintf(stderr, "CX    : 0x%016llX\n", hav.regs[HAV_REG_CX]) ;
      fprintf(stderr, "DX    : 0x%016llX\n", hav.regs[HAV_REG_DX]) ;
      fprintf(stderr, "BX    : 0x%016llX\n", hav.regs[HAV_REG_BX]) ;
      fprintf(stderr, "SP    : 0x%04X:%016llX\n", hav.segs[HAV_SEG_SS], hav.regs[HAV_REG_SP]) ;
      fprintf(stderr, "BP    : 0x%04X:%016llX\n", hav.segs[HAV_SEG_SS], hav.regs[HAV_REG_BP]) ;
      fprintf(stderr, "SI    : 0x%04X:%016llX\n", hav.segs[HAV_SEG_DS], hav.regs[HAV_REG_SI]) ;
      fprintf(stderr, "DI    : 0x%04X:%016llX\n", hav.segs[HAV_SEG_ES], hav.regs[HAV_REG_DI]) ;
      
      u32_t irq = hav.clock(&hav) ;

      fprintf(stderr, "DATA:") ;
      for (int i = 0 ; i < 16 ; ++i)
        fprintf(stderr, " %02X", hav.inst.data[i]) ;
      fprintf(stderr, "\n") ;

      if (HAV_N_IRQS != irq)
        fprintf(stderr, "interrupt: %u\n", irq) ;
    }
  } else {
    while (0 != (hav.flags & HAV_FLAG_A1))
      hav.clock(&hav) ;
  }

  // get the exit code

  int exit_code = *(int *)(hav.regs + HAV_REG_AX) ;

  if (0 != opts.verbose)
    fprintf(stderr, "Program terminated with code %d\n", exit_code) ;

  exit(exit_code) ;
}