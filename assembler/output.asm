.entry code
code:

mov %r1, 3;j
mov %r2, 2;k
mov %r3, 1;l
mov %r1, %r2
sub %r1, %r3;.t1
mov %r2, -2(%rbp)
sub %r2, %r1;m
mov %r0, %r1;j
mov %r1, %r1;k
mov %r3, %r3;l
mov %r2, %r2;m
mov %r0, %r0;i
out %r2