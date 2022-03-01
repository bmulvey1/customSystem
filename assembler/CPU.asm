; customasm ruledef for the cpu
#bits 8

#ruledef reg{
    r0 => 0x0
    r1 => 0x1
    r2 => 0x2
    r3 => 0x3
    r4 => 0x4
    r5 => 0x5
    r6 => 0x6
    r7 => 0x7
    r8 => 0x8
    r9 => 0x9
    r10 => 0xa
    r11 => 0xb
    si => 0xc
    di => 0xd
    sp => 0xe
    bp => 0xf
}

#ruledef{

    nop             => 0x00

    jmp {addr:i16}  => 0x10 @ addr
    je  {addr:i16}  => 0x11 @ addr
    jz  {addr:i16}  => 0x11 @ addr
    jne {addr:i16}  => 0x12 @ addr
    jnz {addr:i16}  => 0x12 @ addr
    jg  {addr:i16}  => 0x13 @ addr
    jl  {addr:i16}  => 0x14 @ addr
    jge {addr:i16}  => 0x15 @ addr
    jle {addr:i16}  => 0x16 @ addr


    ; simple arithmetic
    add %{rd: reg}, %{rs: reg}                       => 0x40 @ rs @ rd
    sub %{rd: reg}, %{rs: reg}                       => 0x41 @ rs @ rd
    mul %{rd: reg}, %{rs: reg}                       => 0x42 @ rs @ rd
    div %{rd: reg}, %{rs: reg}                       => 0x43 @ rs @ rd
    shr %{rd: reg}, %{rs: reg}                       => 0x44 @ rs @ rd
    shl %{rd: reg}, %{rs: reg}                       => 0x45 @ rs @ rd
    inc %{rd: reg}                                   => 0x46 @ 0b0000 @ rd
    dec %{rd: reg}                                   => 0x47 @ 0b0000 @ rd
    and %{rd: reg}, %{rs: reg}                       => 0x48 @ rs @ rd
    or  %{rd: reg}, %{rs: reg}                       => 0x49 @ rs @ rd
    xor %{rd: reg}, %{rs: reg}                       => 0x4a @ rs @ rd
    not %{rd: reg}                                   => 0x4b @ 0b0000 @ rd
    cmp %{rd: reg}, %{rs: reg}                       => 0x4c @ rs @ rd

    ; register indirect arithmetic
    add (%{rd: reg}), %{rs:reg}                      => 0x50 @ rs @ rd
    sub (%{rd: reg}), %{rs:reg}                      => 0x51 @ rs @ rd
    mul (%{rd: reg}), %{rs:reg}                      => 0x52 @ rs @ rd
    div (%{rd: reg}), %{rs:reg}                      => 0x53 @ rs @ rd
    shr (%{rd: reg}), %{rs:reg}                      => 0x54 @ rs @ rd
    shl (%{rd: reg}), %{rs:reg}                      => 0x55 @ rs @ rd
    and (%{rd: reg}), %{rs:reg}                      => 0x58 @ rs @ rd
    or  (%{rd: reg}), %{rs:reg}                      => 0x59 @ rs @ rd
    xor (%{rd: reg}), %{rs:reg}                      => 0x5a @ rs @ rd
    cmp (%{rd: reg}), %{rs:reg}                      => 0x5c @ rs @ rd

    ; register indirect with offset rs + imm
    add %{rd:reg}, {imm: i8}(%{rs: reg})             => 0x60 @ rs @ rd @ imm
    sub %{rd:reg}, {imm: i8}(%{rs: reg})             => 0x61 @ rs @ rd @ imm
    mul %{rd:reg}, {imm: i8}(%{rs: reg})             => 0x62 @ rs @ rd @ imm
    div %{rd:reg}, {imm: i8}(%{rs: reg})             => 0x63 @ rs @ rd @ imm
    shr %{rd:reg}, {imm: i8}(%{rs: reg})             => 0x64 @ rs @ rd @ imm
    shl %{rd:reg}, {imm: i8}(%{rs: reg})             => 0x65 @ rs @ rd @ imm
    and %{rd:reg}, {imm: i8}(%{rs: reg})             => 0x68 @ rs @ rd @ imm
    or  %{rd:reg}, {imm: i8}(%{rs: reg})             => 0x69 @ rs @ rd @ imm
    xor %{rd:reg}, {imm: i8}(%{rs: reg})             => 0x6a @ rs @ rd @ imm
    cmp %{rd:reg}, {imm: i8}(%{rs: reg})             => 0x6c @ rs @ rd @ imm
    
    ; register indirect with scale (ro * scale) + rs
    add %{rd:reg}, %{ro:reg}(%{rs: reg},{scl: i8})   => 0x70 @ rs @ rd @ 0b0000 @ ro @ scl
    sub %{rd:reg}, %{ro:reg}(%{rs: reg},{scl: i8})   => 0x71 @ rs @ rd @ 0b0000 @ ro @ scl
    mul %{rd:reg}, %{ro:reg}(%{rs: reg},{scl: i8})   => 0x72 @ rs @ rd @ 0b0000 @ ro @ scl
    div %{rd:reg}, %{ro:reg}(%{rs: reg},{scl: i8})   => 0x73 @ rs @ rd @ 0b0000 @ ro @ scl
    shr %{rd:reg}, %{ro:reg}(%{rs: reg},{scl: i8})   => 0x74 @ rs @ rd @ 0b0000 @ ro @ scl
    shl %{rd:reg}, %{ro:reg}(%{rs: reg},{scl: i8})   => 0x75 @ rs @ rd @ 0b0000 @ ro @ scl
    and %{rd:reg}, %{ro:reg}(%{rs: reg},{scl: i8})   => 0x78 @ rs @ rd @ 0b0000 @ ro @ scl
    or  %{rd:reg}, %{ro:reg}(%{rs: reg},{scl: i8})   => 0x79 @ rs @ rd @ 0b0000 @ ro @ scl
    xor %{rd:reg}, %{ro:reg}(%{rs: reg},{scl: i8})   => 0x7a @ rs @ rd @ 0b0000 @ ro @ scl
    cmp %{rd:reg}, %{ro:reg}(%{rs: reg},{scl: i8})   => 0x7c @ rs @ rd @ 0b0000 @ ro @ scl
    
    ; immediate arithmetic
    add %{rd: reg}, ${imm: i16}                      => 0x80 @ 0b0000 @ rd @ imm
    sub %{rd: reg}, ${imm: i16}                      => 0x81 @ 0b0000 @ rd @ imm
    mul %{rd: reg}, ${imm: i16}                      => 0x82 @ 0b0000 @ rd @ imm
    div %{rd: reg}, ${imm: i16}                      => 0x83 @ 0b0000 @ rd @ imm
    shr %{rd: reg}, ${imm: i16}                      => 0x84 @ 0b0000 @ rd @ imm
    shl %{rd: reg}, ${imm: i16}                      => 0x85 @ 0b0000 @ rd @ imm
    and %{rd: reg}, ${imm: i16}                      => 0x88 @ 0b0000 @ rd @ imm
    or  %{rd: reg}, ${imm: i16}                      => 0x89 @ 0b0000 @ rd @ imm
    xor %{rd: reg}, ${imm: i16}                      => 0x8a @ 0b0000 @ rd @ imm
    cmp %{rd: reg}, ${imm: i16}                      => 0x8c @ 0b0000 @ rd @ imm

    ; data movement (byte)
    movb %{rd: reg}, %{rs:reg}                      => 0xa0 @ rs @ rd
    movb %{rd:reg}, (%{rs:reg})                     => 0xa1 @ rs @ rd
    movb (%{rd:reg}), %{rs:reg}                      => 0xa2 @ rs @ rd
    movb %{rd:reg}, {off:i8}(%{rs:reg})             => 0xa3 @ rs @ rd @ off
    movb {off:i8}(%{rd:reg}), %{rs:reg}             => 0xa4 @ rs @ rd @ off
    movb %{rd: reg}, %{ro: reg}(%{rs: reg},{scl:i8})=> 0xa5 @ rs @ rd @ 0b0000 @ ro @ scl
    movb %{ro: reg}(%{rd: reg},{scl:i8}), %{rs: reg}=> 0xa6 @ rs @ rd @ 0b0000 @ ro @ scl
    movb %{rd: reg}, ${imm: i8}                      => 0xa7 @ 0b0000 @ rd @ imm

    ; data movement (word)
    mov %{rd: reg}, %{rs:reg}                       => 0xa8 @ rs @ rd
    mov %{rd:reg}, (%{rs:reg})                      => 0xa9 @ rs @ rd
    mov %{rd:reg}, (%{rs:reg})                      => 0xaa @ rs @ rd
    mov %{rd:reg}, {off:i8}(%{rs:reg})              => 0xab @ rs @ rd @ off
    mov {off:i8}(%{rd:reg}), %{rs:reg}              => 0xac @ rs @ rd @ off
    mov %{rd: reg}, %{ro: reg}(%{rs: reg},{scl:i8}) => 0xad @ rs @ rd @ 0b0000 @ ro @ scl
    mov %{ro: reg}(%{rd: reg},{scl:i8}), %{rs: reg} => 0xae @ rs @ rd @ 0b0000 @ ro @ scl
    mov %{rd: reg}, ${imm: i16}                      => 0xaf @ 0b0000 @ rd @ imm

    push %{rs:reg}                          => 0xc0 @ 0b0000 @ rs
    push (%{rs:reg})                        => 0xc1 @ 0b0000 @ rs
    push {off:i8}(%{rs:reg})                => 0xc2 @ 0b0000 @ rs @ off
    push %{ro: reg}(%{rs: reg},{scl:i8})    => 0xc3 @ ro @ rs @ scl
    push ${imm: i16}                         => 0xc4 @ imm

    pop %{rd: reg}                          => 0xcf @ 0b0000 @ rd
    call {address: i16}                     => 0xd0 @ address
    ret                                     => 0xd1
    ; wipe 'argw' number of bytes off the stack from arguments
    ret {argw: i8}                          => 0xd2 @ argw

    out %{rs: reg}                          => 0xd3 @ 0b0000 @ rs

    hlt => 0xff

    entry {address: i16}    => address ; specify the entry point to the code
}