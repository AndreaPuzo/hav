; magic number
EF BE 70 45

; required memory size
00 01 00 00 00 00 00 00

; code size
10 00 00 00 00 00 00 00

; data size
0E 00 00 00 00 00 00 00

; stack size
00 00 01 00 00 00 00 00

19 F6    ; xor SI , SI    (dword)
80 06    ; mov AX , [SI]  (byte)
8B C0 00 ; cmp AX , 0     (byte)
74 06    ; jz  +6
4A 01    ; out AX , 1
8D CE    ; inc SI         (dword)
E8 F3    ; jmp -13
E3       ; hlt

; 11110011 +
; 00001101 =
; --------
; 00000000

'HELLO WORLD!' 0A 00

