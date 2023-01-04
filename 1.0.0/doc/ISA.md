```
.------------------------------------------------------------------------------.
| Mod RM byte                                                                  |
'------------------------------------------------------------------------------'

.---------------------------------------------------------------------.
| Mod \ r/m | 0 | 1 | 2 | 3  | 4              | 5             | 6 | 7 |
|-----------+----------------+----------------+---------------+-------|
| 0         | [r/m]          | [SIB]          | [IP + disp32] | [r/m] |
|-----------+----------------+----------------+---------------'-------|
| 1         | [r/m + disp8]  | [SIB + disp8]  | [r/m + disp8]         |
|-----------+----------------+----------------+-----------------------|
| 2         | [r/m + disp32] | [SIB + disp32] | [r/m + disp32]        |
|-----------+----------------'----------------'-----------------------|
| 3         | r/m                                                     |
'---------------------------------------------------------------------'

.------------------------------------------------------------------------------.
| SIB byte                                                                     |
'------------------------------------------------------------------------------'

Mod 00
.----------------------------------------------------------------------------.
| idx \ base | 0 | 1 | 2 | 3 | 4  | 5                   | 6 | 7              |
|------------+--------------------+---------------------+--------------------|
| 0          |                    |                     |                    |
|------------|                    |                     |                    |
| 1          | [base + (idx * s)] | [(idx * s) + dsp32] | [base + (idx * s)] |
|------------|                    |                     |                    |
| 2          |                    |                     |                    |
|------------|                    |                     |                    |
| 3          |                    |                     |                    |
|------------+--------------------+---------------------+--------------------|
| 4          | [base]             | [dsp32]             | [base]             |
|------------+--------------------+---------------------+--------------------|
| 5          |                    |                     |                    |
|------------|                    |                     |                    |
| 6          | [base + (idx * s)] | [(idx * s) + dsp32] | [base + (idx * s)] |
|------------|                    |                     |                    |
| 7          |                    |                     |                    |
'----------------------------------------------------------------------------'

Mod 01
.--------------------------------------------.
| idx \ base | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|------------+-------------------------------|
| 0          |                               |
|------------|                               |
| 1          | [base + (idx * s) + disp8]    |
|------------|                               |
| 2          |                               |
|------------|                               |
| 3          |                               |
|------------+-------------------------------|
| 4          | [base + disp8]                |
|------------+-------------------------------|
| 5          |                               |
|------------|                               |
| 6          | [base + (idx * s) + disp8]    |
|------------|                               |
| 7          |                               |
'--------------------------------------------'

Mod 10
.--------------------------------------------.
| idx \ base | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|------------+-------------------------------|
| 0          |                               |
|------------|                               |
| 1          | [base + (idx * s) + disp32]   |
|------------|                               |
| 2          |                               |
|------------|                               |
| 3          |                               |
|------------+-------------------------------|
| 4          | [base + disp32]               |
|------------+-------------------------------|
| 5          |                               |
|------------|                               |
| 6          | [base + (idx * s) + disp32]   |
|------------|                               |
| 7          |                               |
'--------------------------------------------'

.------------------------------------------------------------------------------.
| Flags and Conditions                                                         |
'------------------------------------------------------------------------------'

.-----------------------------------------.
| F |                    IOPL       AC    |
| L |                     ||ODIT SZ | P C |
| A | 00000000 00000000 0??????? ??0?0?1? |
| G |                    |             |  |
| S |                   VB             A1 |
'-----------------------------------------'

0  --> AE/NB/NC --> CF == 1
1  --> B/NAE/C  --> CF == 0
2  --> P        --> PF == 1
3  --> NP       --> PF == 0
4  --> Z/E      --> ZF == 1
5  --> NZ/NE    --> ZF == 0
6  --> S        --> SF == 1
7  --> NS       --> SF == 0
8  --> O        --> OF == 1
9  --> NO       --> OF == 0
10 --> A/NBE    --> CF == 0 and ZF == 0
11 --> BE/NA    --> CF == 1 or  ZF == 1
12 --> G/NLE    --> ZF == 0 and SF == OF
13 --> LE/NG    --> ZF == 1 or  SF != OF
14 --> GE/NL    --> SF == OF
15 --> L/NGE    --> SF != OF

.------------------------------------------------------------------------------.
| Operations                                                                   |
'------------------------------------------------------------------------------'

.--------------------------------------------------------------------------.
| Opcode        | Syntax                | Description                      |
|---------------+-----------------------+----------------------------------|
| 00 /r         | add     r8    r/m8    |                                  |
| 01 /r         | add     r32   r/m32   |                                  |
| 02 /r         | add     r/m8  r8      |                                  |
| 03 /r         | add     r/m32 r32     |                                  |
| 04 /r         | sub     r8    r/m8    |                                  |
| 05 /r         | sub     r32   r/m32   |                                  |
| 06 /r         | sub     r/m8  r8      |                                  |
| 07 /r         | sub     r/m32 r32     |                                  |
| 08            | push    CS            |                                  |
| 09            | push    DS            |                                  |
| 0A            | push    ES            |                                  |
| 0B            | push    SS            |                                  |
| 0C            | pop     DS            |                                  |
| 0D            | pop     ES            |                                  |
| 0E            | pop     SS            |                                  |
| 0F 00 /r      | setcc   r/m8          |                                  |
| 0F 01 /r      | setcc   r/m8          |                                  |
| 0F 02 /r      | setcc   r/m8          |                                  |
| 0F 03 /r      | setcc   r/m8          |                                  |
| 0F 04 /r      | setcc   r/m8          |                                  |
| 0F 05 /r      | setcc   r/m8          |                                  |
| 0F 06 /r      | setcc   r/m8          |                                  |
| 0F 07 /r      | setcc   r/m8          |                                  |
| 0F 08 /r      | setcc   r/m8          |                                  |
| 0F 09 /r      | setcc   r/m8          |                                  |
| 0F 0A /r      | setcc   r/m8          |                                  |
| 0F 0B /r      | setcc   r/m8          |                                  |
| 0F 0C /r      | setcc   r/m8          |                                  |
| 0F 0D /r      | setcc   r/m8          |                                  |
| 0F 0E /r      | setcc   r/m8          |                                  |
| 0F 0F /r      | setcc   r/m8          |                                  |
| 0F 10         | syscall               |                                  |
| 0F 11         | sysret                |                                  |
| 0F 12         | sysenter              |                                  |
| 0F 13         | sysexit               |                                  |
| 0F 14 /r      | lidt    m16:32        |                                  |
| 0F 15 /r      | sidt    m             |                                  |
| 0F 16 /r      | lsdt    m16:32        |                                  |
| 0F 17 /r      | ssdt    m             |                                  |
| 0F 18 /r      | bswap   r/m16         |                                  |
| 0F 19 /r      | bswap   r/m32         |                                  |
| 0F 1A         | -                     | #UD                              |
| 0F 1B         | -                     | #UD                              |
| 0F 1C         | -                     | #UD                              |
| 0F 1D         | -                     | #UD                              |
| 0F 1E         | -                     | #UD                              |
| 0F 1F /0      | nop     r/m32         |                                  |
| 0F 1F /1      | nop     r/m32         |                                  |
| 0F 1F /2      | nop     r/m32         |                                  |
| 0F 1F /3      | nop     r/m32         |                                  |
| 0F 1F /4      | nop     r/m32         |                                  |
| 0F 1F /5      | nop     r/m32         |                                  |
| 0F 1F /6      | nop     r/m32         |                                  |
| 0F 1F /7      | nop     r/m32         |                                  |
| 0F 20 /r      | cmovcc  r8    r/m8    |                                  |
| 0F 21 /r      | cmovcc  r8    r/m8    |                                  |
| 0F 22 /r      | cmovcc  r8    r/m8    |                                  |
| 0F 23 /r      | cmovcc  r8    r/m8    |                                  |
| 0F 24 /r      | cmovcc  r8    r/m8    |                                  |
| 0F 25 /r      | cmovcc  r8    r/m8    |                                  |
| 0F 26 /r      | cmovcc  r8    r/m8    |                                  |
| 0F 27 /r      | cmovcc  r8    r/m8    |                                  |
| 0F 28 /r      | cmovcc  r8    r/m8    |                                  |
| 0F 29 /r      | cmovcc  r8    r/m8    |                                  |
| 0F 2A /r      | cmovcc  r8    r/m8    |                                  |
| 0F 2B /r      | cmovcc  r8    r/m8    |                                  |
| 0F 2C /r      | cmovcc  r8    r/m8    |                                  |
| 0F 2D /r      | cmovcc  r8    r/m8    |                                  |
| 0F 2E /r      | cmovcc  r8    r/m8    |                                  |
| 0F 2F /r      | cmovcc  r8    r/m8    |                                  |
| 0F 30 /r      | cmovcc  r32   r/m32   |                                  |
| 0F 31 /r      | cmovcc  r32   r/m32   |                                  |
| 0F 32 /r      | cmovcc  r32   r/m32   |                                  |
| 0F 33 /r      | cmovcc  r32   r/m32   |                                  |
| 0F 34 /r      | cmovcc  r32   r/m32   |                                  |
| 0F 35 /r      | cmovcc  r32   r/m32   |                                  |
| 0F 36 /r      | cmovcc  r32   r/m32   |                                  |
| 0F 37 /r      | cmovcc  r32   r/m32   |                                  |
| 0F 38 /r      | cmovcc  r32   r/m32   |                                  |
| 0F 39 /r      | cmovcc  r32   r/m32   |                                  |
| 0F 3A /r      | cmovcc  r32   r/m32   |                                  |
| 0F 3B /r      | cmovcc  r32   r/m32   |                                  |
| 0F 3C /r      | cmovcc  r32   r/m32   |                                  |
| 0F 3D /r      | cmovcc  r32   r/m32   |                                  |
| 0F 3E /r      | cmovcc  r32   r/m32   |                                  |
| 0F 3F /r      | cmovcc  r32   r/m32   |                                  |
| 0F 40 /r      | cmovcc  r/m8  r8      |                                  |
| 0F 41 /r      | cmovcc  r/m8  r8      |                                  |
| 0F 42 /r      | cmovcc  r/m8  r8      |                                  |
| 0F 43 /r      | cmovcc  r/m8  r8      |                                  |
| 0F 44 /r      | cmovcc  r/m8  r8      |                                  |
| 0F 45 /r      | cmovcc  r/m8  r8      |                                  |
| 0F 46 /r      | cmovcc  r/m8  r8      |                                  |
| 0F 47 /r      | cmovcc  r/m8  r8      |                                  |
| 0F 48 /r      | cmovcc  r/m8  r8      |                                  |
| 0F 49 /r      | cmovcc  r/m8  r8      |                                  |
| 0F 4A /r      | cmovcc  r/m8  r8      |                                  |
| 0F 4B /r      | cmovcc  r/m8  r8      |                                  |
| 0F 4C /r      | cmovcc  r/m8  r8      |                                  |
| 0F 4D /r      | cmovcc  r/m8  r8      |                                  |
| 0F 4E /r      | cmovcc  r/m8  r8      |                                  |
| 0F 4F /r      | cmovcc  r/m8  r8      |                                  |
| 0F 50 /r      | cmovcc  r/m32 r32     |                                  |
| 0F 51 /r      | cmovcc  r/m32 r32     |                                  |
| 0F 52 /r      | cmovcc  r/m32 r32     |                                  |
| 0F 53 /r      | cmovcc  r/m32 r32     |                                  |
| 0F 54 /r      | cmovcc  r/m32 r32     |                                  |
| 0F 55 /r      | cmovcc  r/m32 r32     |                                  |
| 0F 56 /r      | cmovcc  r/m32 r32     |                                  |
| 0F 57 /r      | cmovcc  r/m32 r32     |                                  |
| 0F 58 /r      | cmovcc  r/m32 r32     |                                  |
| 0F 59 /r      | cmovcc  r/m32 r32     |                                  |
| 0F 5A /r      | cmovcc  r/m32 r32     |                                  |
| 0F 5B /r      | cmovcc  r/m32 r32     |                                  |
| 0F 5C /r      | cmovcc  r/m32 r32     |                                  |
| 0F 5D /r      | cmovcc  r/m32 r32     |                                  |
| 0F 5E /r      | cmovcc  r/m32 r32     |                                  |
| 0F 5F /r      | cmovcc  r/m32 r32     |                                  |
| 0F 60         | -                     | #UD                              |
| 0F 61         | -                     | #UD                              |
| 0F 62         | -                     | #UD                              |
| 0F 63         | -                     | #UD                              |
| 0F 64         | -                     | #UD                              |
| 0F 65         | -                     | #UD                              |
| 0F 66         | -                     | #UD                              |
| 0F 67         | -                     | #UD                              |
| 0F 68         | -                     | #UD                              |
| 0F 69         | -                     | #UD                              |
| 0F 6A         | -                     | #UD                              |
| 0F 6B         | -                     | #UD                              |
| 0F 6C         | -                     | #UD                              |
| 0F 6D         | -                     | #UD                              |
| 0F 6E         | -                     | #UD                              |
| 0F 6F         | -                     | #UD                              |
| 0F 70         | -                     | #UD                              |
| 0F 71         | -                     | #UD                              |
| 0F 72         | -                     | #UD                              |
| 0F 73         | -                     | #UD                              |
| 0F 74         | -                     | #UD                              |
| 0F 75         | -                     | #UD                              |
| 0F 76         | -                     | #UD                              |
| 0F 77         | -                     | #UD                              |
| 0F 78         | -                     | #UD                              |
| 0F 79         | -                     | #UD                              |
| 0F 7A         | -                     | #UD                              |
| 0F 7B         | -                     | #UD                              |
| 0F 7C         | -                     | #UD                              |
| 0F 7D         | -                     | #UD                              |
| 0F 7E         | -                     | #UD                              |
| 0F 7F         | -                     | #UD                              |
| 0F 80         | -                     | #UD                              |
| 0F 81         | -                     | #UD                              |
| 0F 82         | -                     | #UD                              |
| 0F 83         | -                     | #UD                              |
| 0F 84         | -                     | #UD                              |
| 0F 85         | -                     | #UD                              |
| 0F 86         | -                     | #UD                              |
| 0F 87         | -                     | #UD                              |
| 0F 88         | -                     | #UD                              |
| 0F 89         | -                     | #UD                              |
| 0F 8A         | -                     | #UD                              |
| 0F 8B         | -                     | #UD                              |
| 0F 8C         | -                     | #UD                              |
| 0F 8D         | -                     | #UD                              |
| 0F 8E         | -                     | #UD                              |
| 0F 8F         | -                     | #UD                              |
| 0F 90         | -                     | #UD                              |
| 0F 91         | -                     | #UD                              |
| 0F 92         | -                     | #UD                              |
| 0F 93         | -                     | #UD                              |
| 0F 94         | -                     | #UD                              |
| 0F 95         | -                     | #UD                              |
| 0F 96         | -                     | #UD                              |
| 0F 97         | -                     | #UD                              |
| 0F 98         | -                     | #UD                              |
| 0F 99         | -                     | #UD                              |
| 0F 9A         | -                     | #UD                              |
| 0F 9B         | -                     | #UD                              |
| 0F 9C         | -                     | #UD                              |
| 0F 9D         | -                     | #UD                              |
| 0F 9E         | -                     | #UD                              |
| 0F 9F         | -                     | #UD                              |
| 0F A0         | -                     | #UD                              |
| 0F A1         | -                     | #UD                              |
| 0F A2         | -                     | #UD                              |
| 0F A3         | -                     | #UD                              |
| 0F A4         | -                     | #UD                              |
| 0F A5         | -                     | #UD                              |
| 0F A6         | -                     | #UD                              |
| 0F A7         | -                     | #UD                              |
| 0F A8         | -                     | #UD                              |
| 0F A9         | -                     | #UD                              |
| 0F AA         | -                     | #UD                              |
| 0F AB         | -                     | #UD                              |
| 0F AC         | -                     | #UD                              |
| 0F AD         | -                     | #UD                              |
| 0F AE         | -                     | #UD                              |
| 0F AF         | -                     | #UD                              |
| 0F B0         | -                     | #UD                              |
| 0F B1         | -                     | #UD                              |
| 0F B2         | -                     | #UD                              |
| 0F B3         | -                     | #UD                              |
| 0F B4         | -                     | #UD                              |
| 0F B5         | -                     | #UD                              |
| 0F B6         | -                     | #UD                              |
| 0F B7         | -                     | #UD                              |
| 0F B8         | -                     | #UD                              |
| 0F B9         | -                     | #UD                              |
| 0F BA         | -                     | #UD                              |
| 0F BB         | -                     | #UD                              |
| 0F BC         | -                     | #UD                              |
| 0F BD         | -                     | #UD                              |
| 0F BE         | -                     | #UD                              |
| 0F BF         | -                     | #UD                              |
| 0F C0         | -                     | #UD                              |
| 0F C1         | -                     | #UD                              |
| 0F C2         | -                     | #UD                              |
| 0F C3         | -                     | #UD                              |
| 0F C4         | -                     | #UD                              |
| 0F C5         | -                     | #UD                              |
| 0F C6         | -                     | #UD                              |
| 0F C7         | -                     | #UD                              |
| 0F C8         | -                     | #UD                              |
| 0F C9         | -                     | #UD                              |
| 0F CA         | -                     | #UD                              |
| 0F CB         | -                     | #UD                              |
| 0F CC         | -                     | #UD                              |
| 0F CD         | -                     | #UD                              |
| 0F CE         | -                     | #UD                              |
| 0F CF         | -                     | #UD                              |
| 0F D0         | -                     | #UD                              |
| 0F D1         | -                     | #UD                              |
| 0F D2         | -                     | #UD                              |
| 0F D3         | -                     | #UD                              |
| 0F D4         | -                     | #UD                              |
| 0F D5         | -                     | #UD                              |
| 0F D6         | -                     | #UD                              |
| 0F D7         | -                     | #UD                              |
| 0F D8         | -                     | #UD                              |
| 0F D9         | -                     | #UD                              |
| 0F DA         | -                     | #UD                              |
| 0F DB         | -                     | #UD                              |
| 0F DC         | -                     | #UD                              |
| 0F DD         | -                     | #UD                              |
| 0F DE         | -                     | #UD                              |
| 0F DF         | -                     | #UD                              |
| 0F E0         | -                     | #UD                              |
| 0F E1         | -                     | #UD                              |
| 0F E2         | -                     | #UD                              |
| 0F E3         | -                     | #UD                              |
| 0F E4         | -                     | #UD                              |
| 0F E5         | -                     | #UD                              |
| 0F E6         | -                     | #UD                              |
| 0F E7         | -                     | #UD                              |
| 0F E8         | -                     | #UD                              |
| 0F E9         | -                     | #UD                              |
| 0F EA         | -                     | #UD                              |
| 0F EB         | -                     | #UD                              |
| 0F EC         | -                     | #UD                              |
| 0F ED         | -                     | #UD                              |
| 0F EE         | -                     | #UD                              |
| 0F EF         | -                     | #UD                              |
| 0F F0         | -                     | #UD                              |
| 0F F1         | -                     | #UD                              |
| 0F F2         | -                     | #UD                              |
| 0F F3         | -                     | #UD                              |
| 0F F4         | -                     | #UD                              |
| 0F F5         | -                     | #UD                              |
| 0F F6         | -                     | #UD                              |
| 0F F7         | -                     | #UD                              |
| 0F F8         | -                     | #UD                              |
| 0F F9         | -                     | #UD                              |
| 0F FA         | -                     | #UD                              |
| 0F FB         | -                     | #UD                              |
| 0F FC         | -                     | #UD                              |
| 0F FD         | -                     | #UD                              |
| 0F FE         | -                     | #UD                              |
| 0F FF         | -                     | #UD                              |
| 10 /r         | and     r8    r/m8    |                                  |
| 11 /r         | and     r32   r/m32   |                                  |
| 12 /r         | and     r/m8  r8      |                                  |
| 13 /r         | and     r/m32 r32     |                                  |
| 14 /r         | or      r8    r/m8    |                                  |
| 15 /r         | or      r32   r/m32   |                                  |
| 16 /r         | or      r/m8  r8      |                                  |
| 17 /r         | or      r/m32 r32     |                                  |
| 18 /r         | xor     r8    r/m8    |                                  |
| 19 /r         | xor     r32   r/m32   |                                  |
| 1A /r         | xor     r/m8  r8      |                                  |
| 1B /r         | xor     r/m32 r32     |                                  |
| 1C /r         | shl     r8    r/m8    |                                  |
| 1D /r         | shl     r32   r/m32   |                                  |
| 1E /r         | shl     r/m8  r8      |                                  |
| 1F /r         | shl     r/m32 r32     |                                  |
| 20 /r         | shr     r8    r/m8    |                                  |
| 21 /r         | shr     r32   r/m32   |                                  |
| 22 /r         | shr     r/m8  r8      |                                  |
| 23 /r         | shr     r/m32 r32     |                                  |
| 24 /r         | sar     r8    r/m8    |                                  |
| 25 /r         | sar     r32   r/m32   |                                  |
| 26 /r         | sar     r/m8  r8      |                                  |
| 27 /r         | sar     r/m32 r32     |                                  |
| 28 /r         | cmp     r8    r/m8    |                                  |
| 29 /r         | cmp     r32   r/m32   |                                  |
| 2A /r         | cmp     r/m8  r8      |                                  |
| 2B /r         | cmp     r/m32 r32     |                                  |
| 2C /r         | test    r8    r/m8    |                                  |
| 2D /r         | test    r32   r/m32   |                                  |
| 2E /r         | test    r/m8  r8      |                                  |
| 2F /r         | test    r/m32 r32     |                                  |
| 30 /r         | mul     AX    r/m8    |                                  |
| 31 /r         | mul     AX    r/m32   |                                  |
| 32 /r         | div     AX    r/m8    |                                  |
| 33 /r         | div     AX    r/m32   |                                  |
| 34 /r         | imul    AX    r/m8    |                                  |
| 35 /r         | imul    AX    r/m32   |                                  |
| 36 /r         | idiv    AX    r/m8    |                                  |
| 37 /r         | idiv    AX    r/m32   |                                  |
| 38 /r         | lea     r16   m       |                                  |
| 39 /r         | lea     r32   m       |                                  |
| 3A /r         | lds     r8    m16:8   |                                  |
| 3B /r         | lds     r32   m16:32  |                                  |
| 3C /r         | les     r8    m16:8   |                                  |
| 3D /r         | les     r32   m16:32  |                                  |
| 3E /r         | lss     r8    m16:8   |                                  |
| 3F /r         | lss     r32   m16:32  |                                  |
| 40 /r         | adc     r8    r/m8    |                                  |
| 41 /r         | adc     r32   r/m32   |                                  |
| 42 /r         | adc     r/m8  r8      |                                  |
| 43 /r         | adc     r/m32 r32     |                                  |
| 44 /r         | sbb     r8    r/m8    |                                  |
| 45 /r         | sbb     r32   r/m32   |                                  |
| 46 /r         | sbb     r/m8  r8      |                                  |
| 47 /r         | sbb     r/m32 r32     |                                  |
| 48        i8  | in      AX    i8      |                                  |
| 49        i8  | in      AX    i8      |                                  |
| 4A        i8  | out     AX    i8      |                                  |
| 4B        i8  | out     AX    i8      |                                  |
| 4C            | in      AX    DX      |                                  |
| 4D            | in      AX    DX      |                                  |
| 4E            | out     AX    DX      |                                  |
| 4F            | out     AX    DX      |                                  |
| 50 + r        | push    r32           |                                  |
| 58 + r        | pop     r32           |                                  |
| 60 (SOV)      | CS                    | #GF                              |
| 61 (SOV)      | DS                    | #GF                              |
| 62 (SOV)      | ES                    | #GF                              |
| 63 (SOV)      | SS                    | #GF                              |
| 64 /r         | neg     r/m8          |                                  |
| 65 /r         | neg     r/m32         |                                  |
| 66 (ZOV)      | -                     | #GF                              |
| 67 (AOV)      | -                     | #GF                              |
| 68 /r         | not     r/m8          |                                  |
| 69 /r         | not     r/m32         |                                  |
| 6A    i16 i8  | enter   i16   i8      |                                  |
| 6B            | leave                 |                                  |
| 6C            | pusha                 |                                  |
| 6D            | pusha                 |                                  |
| 6E            | popa                  |                                  |
| 6F            | popa                  |                                  |
| 70            | jcc     i8            |                                  |
| 71            | jcc     i8            |                                  |
| 72            | jcc     i8            |                                  |
| 73            | jcc     i8            |                                  |
| 74            | jcc     i8            |                                  |
| 75            | jcc     i8            |                                  |
| 76            | jcc     i8            |                                  |
| 77            | jcc     i8            |                                  |
| 78            | jcc     i8            |                                  |
| 79            | jcc     i8            |                                  |
| 7A            | jcc     i8            |                                  |
| 7B            | jcc     i8            |                                  |
| 7C            | jcc     i8            |                                  |
| 7D            | jcc     i8            |                                  |
| 7E            | jcc     i8            |                                  |
| 7F            | jcc     i8            |                                  |
| 80 /r         | mov     r8    r/m8    |                                  |
| 81 /r         | mov     r32   r/m32   |                                  |
| 82 /r         | mov     r/m8  r8      |                                  |
| 83 /r         | mov     r/m32 r32     |                                  |
| 84 /r         | xhg     r8    r/m8    |                                  |
| 85 /r         | xhg     r32   r/m32   |                                  |
| 86            | -                     | #UD                              |
| 87            | -                     | #UD                              |
| 88        i8  | push    i8            |                                  |
| 89        i32 | push    i32           |                                  |
| 8A /0     i8  | add     r/m8  i8      |                                  |
| 8A /1     i32 | add     r/m32 i32     |                                  |
| 8A /2     i8  | sub     r/m8  i8      |                                  |
| 8A /3     i32 | sub     r/m32 i32     |                                  |
| 8A /4     i8  | adc     r/m8  i8      |                                  |
| 8A /5     i32 | adc     r/m32 i32     |                                  |
| 8A /6     i8  | sbb     r/m8  i8      |                                  |
| 8A /7     i32 | sbb     r/m32 i32     |                                  |
| 8B /0     i8  | cmp     r/m8  i8      |                                  |
| 8B /1     i32 | cmp     r/m32 i32     |                                  |
| 8B /2     i8  | test    r/m8  i8      |                                  |
| 8B /3     i32 | test    r/m32 i32     |                                  |
| 8B /4     i8  | and     r/m8  i8      |                                  |
| 8B /5     i32 | and     r/m32 i32     |                                  |
| 8B /6     i8  | or      r/m8  i8      |                                  |
| 8B /7     i32 | or      r/m32 i32     |                                  |
| 8C /0     i8  | xor     r/m8  i8      |                                  |
| 8C /1     i32 | xor     r/m32 i32     |                                  |
| 8C /2     i8  | shl     r/m8  i8      |                                  |
| 8C /3     i32 | shl     r/m32 i8      |                                  |
| 8C /4     i8  | shr     r/m8  i8      |                                  |
| 8C /5     i32 | shr     r/m32 i8      |                                  |
| 8C /6     i8  | sar     r/m8  i8      |                                  |
| 8C /7     i32 | sar     r/m32 i8      |                                  |
| 8D /0         | inc     r/m8          |                                  |
| 8D /1         | inc     r/m32         |                                  |
| 8D /2         | dec     r/m8          |                                  |
| 8D /3         | dec     r/m32         |                                  |
| 8D /4         | -                     | #UD                              |
| 8D /5         | -                     | #UD                              |
| 8D /6         | -                     | #UD                              |
| 8D /7         | -                     | #UD                              |
| 8E /0         | jmp     r/m8      (n) |                                  |
| 8E /1         | jmp     r/m32     (n) |                                  |
| 8E /2         | jmp     m16:8     (f) |                                  |
| 8E /3         | jmp     m16:32    (f) |                                  |
| 8E /4         | call    r/m8      (n) |                                  |
| 8E /5         | call    r/m32     (n) |                                  |
| 8E /6         | call    m16:8     (f) |                                  |
| 8E /7         | call    m16:32    (f) |                                  |
| 8F /0     i8  | mov     r/m8  i8      |                                  |
| 8F /1     i32 | mov     r/m32 i32     |                                  |
| 8F /2         | push    r/m8          |                                  |
| 8F /3         | push    r/m32         |                                  |
| 8F /4         | pop     r/m8          |                                  |
| 8F /5         | pop     r/m32         |                                  |
| 8F /6         | -                     | #UD                              |
| 8F /7         | -                     | #UD                              |
| 90 + r        | xhg     AX    r8      |                                  |
| 98 + r        | xhg     AX    r32     |                                  |
| A0            | ins     DI    DX      |                                  |
| A1            | ins     DI    DX      |                                  |
| A2            | outs    SI    DX      |                                  |
| A3            | outs    SI    DX      |                                  |
| A4            | bsps    SI            |                                  |
| A5            | bsps    SI            |                                  |
| A6            | movs    DI    SI      |                                  |
| A7            | movs    DI    SI      |                                  |
| A8            | cmps    DI    SI      |                                  |
| A9            | cmps    DI    SI      |                                  |
| AA            | stos    DI    AX      |                                  |
| AB            | stos    DI    AX      |                                  |
| AC            | lods    AX    SI      |                                  |
| AD            | lods    AX    SI      |                                  |
| AE            | scas    SI    AX      |                                  |
| AF            | scas    SI    AX      |                                  |
| B0 + r    i8  | mov     r     i8      |                                  |
| B8 + r    i32 | mov     r     i32     |                                  |
| C0            | -                     | #UD                              |
| C1            | -                     | #UD                              |
| C2            | -                     | #UD                              |
| C3            | -                     | #UD                              |
| C4            | -                     | #UD                              |
| C5            | -                     | #UD                              |
| C6            | -                     | #UD                              |
| C7            | -                     | #UD                              |
| C8            | -                     | #UD                              |
| C9            | -                     | #UD                              |
| CA            | -                     | #UD                              |
| CB            | -                     | #UD                              |
| CC            | -                     | #UD                              |
| CD            | -                     | #UD                              |
| CE            | -                     | #UD                              |
| CF            | -                     | #UD                              |
| D0 (REP)      | -                     | #GF                              |
| D1 (REP)      | -                     | #GF                              |
| D2            | -                     | #UD                              |
| D3            | pushf                 |                                  |
| D4            | popf                  |                                  |
| D5            | cmc                   |                                  |
| D6            | clc                   |                                  |
| D7            | stc                   |                                  |
| D8            | cli                   |                                  |
| D9            | sti                   |                                  |
| DA            | cld                   |                                  |
| DB            | std                   |                                  |
| DC        i8  | int     i8            |                                  |
| DD            | int     1             |                                  |
| DE            | int     3             |                                  |
| DF            | iret                  |                                  |
| E0        i8  | loop    i8            |                                  |
| E1        i8  | loop    i8            |                                  |
| E2        i8  | loop    i8            |                                  |
| E3            | hlt                   |                                  |
| E4        i8  | ret     i8        (n) |                                  |
| E5            | ret               (n) |                                  |
| E6        i8  | ret     i8        (f) |                                  |
| E7            | ret               (f) |                                  |
| E8        i8  | jmp     i8        (n) |                                  |
| E9        i32 | jmp     i32       (n) |                                  |
| EA    i16:i8  | jmp     i16:i8    (f) |                                  |
| EB    i16:i32 | jmp     i16:i32   (f) |                                  |
| EC        i8  | call    i8        (n) |                                  |
| ED        i32 | call    i32       (n) |                                  |
| EE    i16:i8  | call    i16:i8    (f) |                                  |
| EF    i16:i32 | call    i16:i32   (f) |                                  |
| F0 /r         | cvtb2sf r8    r/m8    |                                  |
| F1 /r         | cvtd2sf r32   r/m32   |                                  |
| F2 /r         | cvtb2df r8    r/m8    |                                  |
| F3 /r         | cvtd2df r32   r/m32   |                                  |
| F4 /r         | cvtfs2d r64   r/m32   |                                  |
| F5 /r         | cvtfd2s r32   r/m64   |                                  |
| F6 /r         | cvtb2sf r/m8  r8      |                                  |
| F7 /r         | cvtd2sf r/m32 r32     |                                  |
| F8 /r         | cvtb2df r/m8  r8      |                                  |
| F9 /r         | cvtd2df r/m32 r32     |                                  |
| FA /r         | cvtfs2d r/m64 r32     |                                  |
| FB /r         | cvtfd2s r/m32 r64     |                                  |
| FC /r         | fmov    fp32  r/m32   |                                  |
| FD /r         | fmov    r/m32 fp32    |                                  |
| FE            | -                     | #UD                              |
| FF 00 + r     | fadd    fp(0) fp(r)   | fp(0) := fp(0) + fp(r)           |
| FF 08 + r     | fsub    fp(0) fp(r)   | fp(0) := fp(0) - fp(r)           |
| FF 10     i8  | faddi   fp(0) i8      | fp(0) := fp(0) + i8              |
| FF 11     i32 | faddi   fp(0) i32     | fp(0) := fp(0) + i32             |
| FF 12     i8  | fsubi   fp(0) i8      | fp(0) := fp(0) - i8              |
| FF 13     i32 | fsubi   fp(0) i32     | fp(0) := fp(0) - i32             |
| FF 14         | fabs    fp(0)         | fp(0) := |fp(0)|                 |
| FF 15         | fchs    fp(0)         | fp(0) := -fp(0)                  |
| FF 16     i8  | fmuli   fp(0) i8      | fp(0) := fp(0) * i8              |
| FF 17     i32 | fmuli   fp(0) i32     | fp(0) := fp(0) * i32             |
| FF 18     i8  | fdivi   fp(0) i8      | fp(0) := fp(0) / i8              |
| FF 19     i32 | fdivi   fp(0) i32     | fp(0) := fp(0) / i32             |
| FF 1A     i8  | flodi   fp(0) i8      | fp(0) := (f32)i8                 |
| FF 1B     i32 | flodi   fp(0) i32     | fp(0) := (f32)i32                |
| FF 1C     i8  | flodi   fp(0) i8      | fp(0) := (f64)i8                 |
| FF 1D     i32 | flodi   fp(0) i32     | fp(0) := (f64)i32                |
| FF 1E         | f2xm1   fp(0)         | fp(0) := 2^fp(0) - 1             |
| FF 1F /0      | fadd    fp(0) m32fp   | fp(0) := fp(0) + [m32fp]         |
| FF 1F /1      | fadd    fp(0) m64fp   | fp(0) := fp(0) + [m64fp]         |
| FF 1F /2      | fsub    fp(0) m32fp   | fp(0) := fp(0) - [m32fp]         |
| FF 1F /3      | fsub    fp(0) m64fp   | fp(0) := fp(0) - [m64fp]         |
| FF 1F /4      | fmul    fp(0) m32fp   | fp(0) := fp(0) * [m32fp]         |
| FF 1F /5      | fmul    fp(0) m64fp   | fp(0) := fp(0) * [m64fp]         |
| FF 1F /6      | fdiv    fp(0) m32fp   | fp(0) := fp(0) / [m32fp]         |
| FF 1F /7      | fdiv    fp(0) m64fp   | fp(0) := fp(0) / [m64fp]         |
| FF 20 + r     | fmul    fp(0) fp(r)   | fp(0) := fp(0) / fp(r)           |
| FF 28 + r     | fdiv    fp(0) fp(r)   | fp(0) := fp(0) / fp(r)           |
| FF 30 + r     | fcmp    fp(0) fp(r)   | FLAGS := fp(0) ?= fp(r)          |
| FF 38 + r     | fxhg    fp(0) fp(r)   | fp(0) :=: fp(r)                  |
| FF 40 + r     | fmov    fp(0) fp(r)   | fp(0) :=  fp(r)                  |
| FF 48 + r     | fmov    fp(r) fp(0)   | fp(r) :=  fp(0)                  |
| FF 50         | fsin    fp(0)         | fp(0) := sin(fp(0))              |
| FF 51         | fcos    fp(0)         | fp(0) := cos(fp(0))              |
| FF 52         | ftan    fp(0)         | fp(0) := tan(fp(0))              |
| FF 53         | fasin   fp(0)         | fp(0) := asin(fp(0))             |
| FF 54         | facos   fp(0)         | fp(0) := acos(fp(0))             |
| FF 55         | fatan   fp(0)         | fp(0) := atan(fp(0))             |
| FF 56         | fsinh   fp(0)         | fp(0) := sinh(fp(0))             |
| FF 57         | fcosh   fp(0)         | fp(0) := cosh(fp(0))             |
| FF 58         | ftanh   fp(0)         | fp(0) := tanh(fp(0))             |
| FF 59         | fasinh  fp(0)         | fp(0) := asinh(fp(0))            |
| FF 5A         | facosh  fp(0)         | fp(0) := acosh(fp(0))            |
| FF 5B         | fatanh  fp(0)         | fp(0) := atanh(fp(0))            |
| FF 5C         | fsqrt   fp(0)         | fp(0) := sqrt(fp(0))             |
| FF 5D         | fcbrt   fp(0)         | fp(0) := cbrt(fp(0))             |
| FF 5E         | fcmp    fp(0)         | FLAGS := fp(0) ?= 0.0            |
| FF 5F /0      | fcmp    fp(0) m32fp   | FLAGS := fp(0) ?= [m32fp]        |
| FF 5F /1      | fcmp    fp(0) m64fp   | FLAGS := fp(0) ?= [m64fp]        |
| FF 5F /2      | flod    fp(0) m32fp   | fp(0) := [m32fp]                 |
| FF 5F /3      | flod    fp(0) m32fp   | fp(0) := [m64fp]                 |
| FF 5F /4      | fsto    fp(0) m32fp   | [m32fp] := fp(0)                 |
| FF 5F /5      | fsto    fp(0) m32fp   | [m64fp] := fp(0)                 |
| FF 5F /6      | flod    fp(1) m32fp   | fp(1) := [m32fp]                 |
| FF 5F /7      | flod    fp(1) m32fp   | fp(1) := [m64fp]                 |
| FF 60         | flod1   fp(0)         | fp(0) := +1.0                    |
| FF 61         | flodl2t fp(0)         | fp(0) := log2(10)                |
| FF 62         | flodl2e fp(0)         | fp(0) := log2(E)                 |
| FF 63         | flodlg2 fp(0)         | fp(0) := log10(2)                |
| FF 64         | flodln2 fp(0)         | fp(0) := log(2)                  |
| FF 65         | flodpi  fp(0)         | fp(0) := PI                      |
| FF 66         | flodz   fp(0)         | fp(0) := +0.0                    |
| FF 67         | flode   fp(0)         | fp(0) := E                       |
| FF 68         | flod2pi fp(0)         | fp(0) := 2 * PI                  |
| FF 69         | flod4pi fp(0)         | fp(0) := 4 * PI                  |
| FF 6A         | fexp    fp(0)         | fp(0) := exp(fp(0))              |
| FF 6B         | flog    fp(0)         | fp(0) := log(fp(0))              |
| FF 6C         | flog2   fp(0)         | fp(0) := log2(fp(0))             |
| FF 6D         | flog10  fp(0)         | fp(0) := log10(fp(0))            |
| FF 6C         | fmax    fp(0) fp(1)   | fp(0) := max(fp(0), fp(1))       |
| FF 6D         | fmin    fp(0) fp(1)   | fp(0) := min(fp(0), fp(1))       |
| FF 6E         | fyl2x   fp(1) fp(0)   | fp(1) := fp(1) * log2(fp(0))     |
| FF 6F         | fyl2x1  fp(1) fp(0)   | fp(1) := fp(1) * log2(fp(0) + 1) |
| FF 70 + r     | fxor    fp(0) fp(r)   | fp(0) ^= fp(r)                   |
| FF 78 + r     | fpow    fp(0) fp(r)   | fp(0) := pow(fp(0), fp(r))       |
| FF 80         | frnd    fp(0)         | fp(0) := round(fp(0))            |
| FF 81         | frndi   AX    fp(0)   | AX := round(fp(0))               |
| FF 82         | fsgn    AX    fp(0)   | AX := fp(0) < +0.0               |
| FF 83         | finf    AX    fp(0)   | AX := is_inf(fp(0))              |
| FF 84         | fnan    AX    fp(0)   | AX := is_nan(fp(0))              |
| FF 85         | fldpinf fp(0)         | fp(0) := +INFINITE               |
| FF 86         | fldninf fp(0)         | fp(0) := -INFINITE               |
| FF 87         | fldnan  fp(0)         | fp(0) := NAN                     |
| FF 88         | -                     | #UD                              |
| FF 89         | -                     | #UD                              |
| FF 8A         | -                     | #UD                              |
| FF 8B         | -                     | #UD                              |
| FF 8C         | -                     | #UD                              |
| FF 8D         | -                     | #UD                              |
| FF 8E         | -                     | #UD                              |
| FF 8F         | -                     | #UD                              |
| FF 90         | -                     | #UD                              |
| FF 91         | -                     | #UD                              |
| FF 92         | -                     | #UD                              |
| FF 93         | -                     | #UD                              |
| FF 94         | -                     | #UD                              |
| FF 95         | -                     | #UD                              |
| FF 96         | -                     | #UD                              |
| FF 97         | -                     | #UD                              |
| FF 98         | -                     | #UD                              |
| FF 99         | -                     | #UD                              |
| FF 9A         | -                     | #UD                              |
| FF 9B         | -                     | #UD                              |
| FF 9C         | -                     | #UD                              |
| FF 9D         | -                     | #UD                              |
| FF 9E         | -                     | #UD                              |
| FF 9F         | -                     | #UD                              |
| FF A0         | -                     | #UD                              |
| FF A1         | -                     | #UD                              |
| FF A2         | -                     | #UD                              |
| FF A3         | -                     | #UD                              |
| FF A4         | -                     | #UD                              |
| FF A5         | -                     | #UD                              |
| FF A6         | -                     | #UD                              |
| FF A7         | -                     | #UD                              |
| FF A8         | -                     | #UD                              |
| FF A9         | -                     | #UD                              |
| FF AA         | -                     | #UD                              |
| FF AB         | -                     | #UD                              |
| FF AC         | -                     | #UD                              |
| FF AD         | -                     | #UD                              |
| FF AE         | -                     | #UD                              |
| FF AF         | -                     | #UD                              |
| FF B0         | -                     | #UD                              |
| FF B1         | -                     | #UD                              |
| FF B2         | -                     | #UD                              |
| FF B3         | -                     | #UD                              |
| FF B4         | -                     | #UD                              |
| FF B5         | -                     | #UD                              |
| FF B6         | -                     | #UD                              |
| FF B7         | -                     | #UD                              |
| FF B8         | -                     | #UD                              |
| FF B9         | -                     | #UD                              |
| FF BA         | -                     | #UD                              |
| FF BB         | -                     | #UD                              |
| FF BC         | -                     | #UD                              |
| FF BD         | -                     | #UD                              |
| FF BE         | -                     | #UD                              |
| FF BF         | -                     | #UD                              |
| FF C0         | -                     | #UD                              |
| FF C1         | -                     | #UD                              |
| FF C2         | -                     | #UD                              |
| FF C3         | -                     | #UD                              |
| FF C4         | -                     | #UD                              |
| FF C5         | -                     | #UD                              |
| FF C6         | -                     | #UD                              |
| FF C7         | -                     | #UD                              |
| FF C8         | -                     | #UD                              |
| FF C9         | -                     | #UD                              |
| FF CA         | -                     | #UD                              |
| FF CB         | -                     | #UD                              |
| FF CC         | -                     | #UD                              |
| FF CD         | -                     | #UD                              |
| FF CE         | -                     | #UD                              |
| FF CF         | -                     | #UD                              |
| FF D0         | -                     | #UD                              |
| FF D1         | -                     | #UD                              |
| FF D2         | -                     | #UD                              |
| FF D3         | -                     | #UD                              |
| FF D4         | -                     | #UD                              |
| FF D5         | -                     | #UD                              |
| FF D6         | -                     | #UD                              |
| FF D7         | -                     | #UD                              |
| FF D8         | -                     | #UD                              |
| FF D9         | -                     | #UD                              |
| FF DA         | -                     | #UD                              |
| FF DB         | -                     | #UD                              |
| FF DC         | -                     | #UD                              |
| FF DD         | -                     | #UD                              |
| FF DE         | -                     | #UD                              |
| FF DF         | -                     | #UD                              |
| FF E0         | -                     | #UD                              |
| FF E1         | -                     | #UD                              |
| FF E2         | -                     | #UD                              |
| FF E3         | -                     | #UD                              |
| FF E4         | -                     | #UD                              |
| FF E5         | -                     | #UD                              |
| FF E6         | -                     | #UD                              |
| FF E7         | -                     | #UD                              |
| FF E8         | -                     | #UD                              |
| FF E9         | -                     | #UD                              |
| FF EA         | -                     | #UD                              |
| FF EB         | -                     | #UD                              |
| FF EC         | -                     | #UD                              |
| FF ED         | -                     | #UD                              |
| FF EE         | -                     | #UD                              |
| FF EF         | -                     | #UD                              |
| FF F0         | -                     | #UD                              |
| FF F1         | -                     | #UD                              |
| FF F2         | -                     | #UD                              |
| FF F3         | -                     | #UD                              |
| FF F4         | -                     | #UD                              |
| FF F5         | -                     | #UD                              |
| FF F6         | -                     | #UD                              |
| FF F7         | -                     | #UD                              |
| FF F8         | -                     | #UD                              |
| FF F9         | -                     | #UD                              |
| FF FA         | -                     | #UD                              |
| FF FB         | -                     | #UD                              |
| FF FC         | -                     | #UD                              |
| FF FD         | -                     | #UD                              |
| FF FE         | -                     | #UD                              |
| FF FF         | -                     | #UD                              |
'--------------------------------------------------------------------------'
```
