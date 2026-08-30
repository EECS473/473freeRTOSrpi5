.section .text.memset, "ax", @progbits
.global memset
.type memset, %function
.align 4

memset:
    cbz     x2, 9f
    mov     x3, x0
    and     w1, w1, #0xff

1:  tst     x3, #7
    b.eq    2f
    strb    w1, [x3], #1
    sub     x2, x2, #1
    cbz     x2, 9f
    b       1b

2:  cmp     x2, #8
    b.lo    4f
    mov     x5, x1
    orr     x5, x5, x5, lsl #8
    orr     x5, x5, x5, lsl #16
    orr     x5, x5, x5, lsl #32

3:  str     x5, [x3], #8
    sub    x2, x2, #8
    cmp    x2, #8
    b.hs   3b

4:  cbz    x2, 9f
5:  strb   w1, [x3], #1
    sub    x2, x2, #1
    cbnz   x2, 5b

9:  ret
    .size  memset, . - memset
    
