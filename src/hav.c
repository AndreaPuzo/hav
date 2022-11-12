#ifdef _HAV_IS_EXTERNAL
# include <hav/hav.h>
#else
# include "hav.h"
#endif

#include <stdio.h>
#include <locale.h>

int main (int argc, char **argv)
{ 
    hav_char_t str1 [] = "Welcome to Hav" ;
    hav_char_t str2 [] = "by Andpuv" ;
    hav_char_t str3 [] = "Copyright (c) 2022" ;
    
    hav_char_t fmt [] = "%-:.64s\n%n%-:.64s\n%:.64s\n%:.32x\n%]-:.32.2F%n" ;
    hav_char_t dst [1024] ;
    hav_dword_t a = 0 , b = 0 ;

    int32_t num1 = 65732 ;
    double num2 = 153.12 ;

    printf("length: %u\n", hav_fmtstr(dst, 1024, fmt, str1, &a, str2, str3, num1, num2, &b)) ;
    printf("n: (%u , %u)\n", a, b) ;
    printf("string:\n%s\n", dst) ;
}