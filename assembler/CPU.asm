; customasm ruledef for the cpu
#bits 8

#ruledef reg{
    r0 => 0b00000
    r1 => 0b00001
    r2 => 0b00010
    r3 => 0b00011
    r4 => 0b00100
    r5 => 0b00101
    r6 => 0b00110
    r7 => 0b00111
    r8 => 0b01000
    r9 => 0b01001
    ra => 0b01010
    rb => 0b01011
    rc => 0b01100
    rd => 0b01101
    re => 0b01110
    rf => 0b01111
    rr => 0b10000 ;return register
    r16 =>0b10000
    sp => 0b10001
    bp => 0b10010
    ;13 registers to spare - status/flags/paging?
}

#ruledef{

    nop             => 0x01

    jmp {addr:i16}  => 0x10 @ addr
    je  {addr:i16}  => 0x11 @ addr
    jz  {addr:i16}  => 0x11 @ addr
    jne {addr:i16}  => 0x12 @ addr
    jnz {addr:i16}  => 0x12 @ addr
    jg  {addr:i16}  => 0x13 @ addr
    jl  {addr:i16}  => 0x14 @ addr
    jge {addr:i16}  => 0x15 @ addr
    jle {addr:i16}  => 0x16 @ addr


    ; unsigned arithmetic
    add %{rd: reg}, %{rs1: reg}, %{rs2: reg}        => 0x40 @ rd @ rs1 @ rs2 @ 0b0
    sub %{rd: reg}, %{rs1: reg}, %{rs2: reg}        => 0x41 @ rd @ rs1 @ rs2 @ 0b0
    mul %{rd: reg}, %{rs1: reg}, %{rs2: reg}        => 0x42 @ rd @ rs1 @ rs2 @ 0b0
    div %{rd: reg}, %{rs1: reg}, %{rs2: reg}        => 0x43 @ rd @ rs1 @ rs2 @ 0b0
    shr %{rd: reg}, %{rs1: reg}, %{rs2: reg}        => 0x44 @ rd @ rs1 @ rs2 @ 0b0
    shl %{rd: reg}, %{rs1: reg}, %{rs2: reg}        => 0x45 @ rd @ rs1 @ rs2 @ 0b0
    inc %{rd: reg}                                  => 0x46 @ rd @ 0b000
    dec %{rd: reg}                                  => 0x47 @ rd @ 0b000
    and %{rd: reg}, %{rs1: reg}, %{rs2: reg}        => 0x48 @ rd @ rs1 @rs2 @ 0b0
    or  %{rd: reg}, %{rs1: reg}, %{rs2: reg}        => 0x49 @ rd @ rs1 @rs2 @ 0b0
    xor %{rd: reg}, %{rs1: reg}, %{rs2: reg}        => 0x4a @ rd @ rs1 @rs2 @ 0b0
    not %{rd: reg}, %{rs1: reg}                     => 0x4b @ rd @ rs1
    cmp %{rs1: reg}, %{rs2: reg}                    => 0x4c @ 0b00000 @ rs1 @ rs2 @ 0b0

        ; immediate
    add %{rd: reg}, %{rs1: reg}, ${imm: i16}       => 0x50 @ rd @ rs1 @ 0b000000 @ imm
    sub %{rd: reg}, %{rs1: reg}, ${imm: i16}       => 0x51 @ rd @ rs1 @ 0b000000 @ imm
    mul %{rd: reg}, %{rs1: reg}, ${imm: i16}       => 0x52 @ rd @ rs1 @ 0b000000 @ imm
    div %{rd: reg}, %{rs1: reg}, ${imm: i16}       => 0x53 @ rd @ rs1 @ 0b000000 @ imm
    shr %{rd: reg}, %{rs1: reg}, ${imm: i16}       => 0x54 @ rd @ rs1 @ 0b000000 @ imm
    shl %{rd: reg}, %{rs1: reg}, ${imm: i16}       => 0x55 @ rd @ rs1 @ 0b000000 @ imm
    and %{rd: reg}, %{rs1: reg}, ${imm: i16}       => 0x58 @ rd @ rs1 @ 0b000000 @ imm
    or  %{rd: reg}, %{rs1: reg}, ${imm: i16}       => 0x59 @ rd @ rs1 @ 0b000000 @ imm
    xor %{rd: reg}, %{rs1: reg}, ${imm: i16}       => 0x5a @ rd @ rs1 @ 0b000000 @ imm
    cmp %{rs1: reg}, ${imm: i16}                   => 0x5c @ 0b000 @ rs1 @ imm


    ; data movement (byte)
    movb %{rd: reg}, %{rs: reg}                                 => 0xa0 @ rs @ rd
    movb %{rd: reg}, (%{rbase: reg})                            => 0xa1 @ rd @ rbase
    movb (%{rbase: reg}), %{rs: reg}                            => 0xa2 @ rbase @ rs
    movb %{rd: reg}, {off:i16}(%{rbase: reg})                   => 0xa3 @ rd @ rbase @ off
    movb {off:i16}(%{rbase: reg}), %{rs: reg}                   => 0xa4 @ rs @ rbase @ off
    movb %{rd: reg}, %{roffset: reg}(%{rbase: reg},{scl: i8})   => 0xa5 @ rd @ rbase @ roffset @ scl
    movb %{roffset: reg}(%{rbase: reg},{scl:i8}), %{rs: reg}    => 0xa6 @ rs @ rbase @ roffset @ scl
    movb %{rd: reg}, ${imm: i8}                                 => 0xa7 @ 0b00000 @ rd @ imm

    ; data movement (word)
    mov %{rd: reg}, %{rs: reg}                               => 0xa8 @ rs @ rd @ 0b000000
    mov %{rd: reg}, (%{rs: reg})                             => 0xa9 @ rs @ rd @ 0b000000
    mov (%{rd: reg}), %{rs: reg}                             => 0xaa @ rs @ rd @ 0b000000
    mov %{rd: reg}, {off:i16}(%{rs: reg})                    => 0xab @ rs @ rd @ 0b000000 @ off
    mov {off:i16}(%{rd: reg}), %{rs: reg}                    => 0xac @ rs @ rd @ 0b000000 @ off
    mov %{rd: reg}, %{ro: reg}(%{rs: reg}, ${scl:i8})        => 0xad @ rs @ rd @ ro @ 0b0 @ scl
    mov %{ro: reg}(%{rd: reg}, ${scl:i8}), %{rs: reg}        => 0xae @ rs @ rd @ ro @ 0b0 @ scl
    mov %{rd: reg}, ${imm: i16}                              => 0xaf @ 0b000 @ rd @ imm
    mov {off:i16}(%{rd: reg}), ${imm: i16}                   => 0xb0 @ 0b000 @ rd @ off @ imm
    mov %{ro: reg}(%{rd: reg}, ${scl:i8}), ${imm: i16}       => 0xb1 @ 0b00000 @ rd @ ro @ 0b0 @ scl @ imm

    push %{rs: reg}                          => 0xc0 @ 0b000 @ rs
    ;push (%{rs: reg})                        => 0xc1 @ 0b0000 @ rs
    ;push {off:i16}(%{rs: reg})                => 0xc2 @ 0b0000 @ rs @ off
    ;push %{ro: reg}(%{rs: reg},{scl:i8})    => 0xc3 @ ro @ rs @ scl
    push ${imm: i16}                         => 0xc4 @ imm

    pop %{rd: reg}                          => 0xcf @ 0b000 @ rd
    call {address: i16}                     => 0xd0 @ address
    ret                                     => 0xd1
    ; wipe 'argw' number of bytes off the stack from arguments
    ret {argw: i16}                          => 0xd2 @ argw

    out %{rs: reg}                          => 0xd3 @ 0b000 @ rs

    hlt => 0xff

    entry {address: i16}    => address ; specify the entry point to the code

    data@ {address: i16}      => address
}
