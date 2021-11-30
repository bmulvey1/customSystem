; customasm ruledef for the cpu
#bits 8

#ruledef reg{
    ra => 0x0
    rb => 0x1
    rc => 0x2
    rd => 0x3
    r0 => 0x4
    r1 => 0x5
    r2 => 0x6
    r3 => 0x7
    r4 => 0x8
    r5 => 0x9
    r6 => 0xa
    r7 => 0xb
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


    ; simple arithmetic
    add %{rs: reg}, %{rd: reg}                       => 0x40 @ rs @ rd
    sub %{rs: reg}, %{rd: reg}                       => 0x41 @ rs @ rd
    mul %{rs: reg}, %{rd: reg}                       => 0x42 @ rs @ rd
    div %{rs: reg}, %{rd: reg}                       => 0x43 @ rs @ rd
    shr %{rs: reg}, %{rd: reg}                       => 0x44 @ rs @ rd
    shl %{rs: reg}, %{rd: reg}                       => 0x45 @ rs @ rd
    inc %{rd: reg}                                  => 0x46 @ 0b0000 @ rd
    dec %{rd: reg}                                  => 0x47 @ 0b0000 @ rd
    and %{rs: reg}, %{rd: reg}                       => 0x48 @ rs @ rd
    or  %{rs: reg}, %{rd: reg}                       => 0x49 @ rs @ rd
    xor %{rs: reg}, %{rd: reg}                       => 0x4a @ rs @ rd
    not %{rd: reg}                                  => 0x4b @ 0b0000 @ rd
    cmp %{rs: reg}, %{rd: reg}                       => 0x4c @ rs @ rd

    ; register indirect arithmetic
    add (%{rs: reg}), %{rd:reg}                      => 0x50 @ rs @ rd
    sub (%{rs: reg}), %{rd:reg}                      => 0x51 @ rs @ rd
    mul (%{rs: reg}), %{rd:reg}                      => 0x52 @ rs @ rd
    div (%{rs: reg}), %{rd:reg}                      => 0x53 @ rs @ rd
    shr (%{rs: reg}), %{rd:reg}                      => 0x54 @ rs @ rd
    shl (%{rs: reg}), %{rd:reg}                      => 0x55 @ rs @ rd
    and (%{rs: reg}), %{rd:reg}                      => 0x58 @ rs @ rd
    or  (%{rs: reg}), %{rd:reg}                      => 0x59 @ rs @ rd
    xor (%{rs: reg}), %{rd:reg}                      => 0x5a @ rs @ rd
    cmp (%{rs: reg}), %{rd:reg}                      => 0x5c @ rs @ rd

    ; register indirect with offset rs + imm
    add {imm: i8}(%{rs: reg}), %{rd:reg}             => 0x60 @ rs @ rd @ imm
    sub {imm: i8}(%{rs: reg}), %{rd:reg}             => 0x61 @ rs @ rd @ imm
    mul {imm: i8}(%{rs: reg}), %{rd:reg}             => 0x62 @ rs @ rd @ imm
    div {imm: i8}(%{rs: reg}), %{rd:reg}             => 0x63 @ rs @ rd @ imm
    shr {imm: i8}(%{rs: reg}), %{rd:reg}             => 0x64 @ rs @ rd @ imm
    shl {imm: i8}(%{rs: reg}), %{rd:reg}             => 0x65 @ rs @ rd @ imm
    and {imm: i8}(%{rs: reg}), %{rd:reg}             => 0x68 @ rs @ rd @ imm
    or  {imm: i8}(%{rs: reg}), %{rd:reg}             => 0x69 @ rs @ rd @ imm
    xor {imm: i8}(%{rs: reg}), %{rd:reg}             => 0x6a @ rs @ rd @ imm
    cmp {imm: i8}(%{rs: reg}), %{rd:reg}             => 0x6c @ rs @ rd @ imm
    
    ; register indirect with scale (ro * scale) + rs
    add %{ro:reg}(%{rs: reg},{scl: i8}), %{rd:reg}   => 0x70 @ rs @ rd @ 0b0000 @ ro @ scl
    sub %{ro:reg}(%{rs: reg},{scl: i8}), %{rd:reg}   => 0x71 @ rs @ rd @ 0b0000 @ ro @ scl
    mul %{ro:reg}(%{rs: reg},{scl: i8}), %{rd:reg}   => 0x72 @ rs @ rd @ 0b0000 @ ro @ scl
    div %{ro:reg}(%{rs: reg},{scl: i8}), %{rd:reg}   => 0x73 @ rs @ rd @ 0b0000 @ ro @ scl
    shr %{ro:reg}(%{rs: reg},{scl: i8}), %{rd:reg}   => 0x74 @ rs @ rd @ 0b0000 @ ro @ scl
    shl %{ro:reg}(%{rs: reg},{scl: i8}), %{rd:reg}   => 0x75 @ rs @ rd @ 0b0000 @ ro @ scl
    and %{ro:reg}(%{rs: reg},{scl: i8}), %{rd:reg}   => 0x78 @ rs @ rd @ 0b0000 @ ro @ scl
    or  %{ro:reg}(%{rs: reg},{scl: i8}), %{rd:reg}   => 0x79 @ rs @ rd @ 0b0000 @ ro @ scl
    xor %{ro:reg}(%{rs: reg},{scl: i8}), %{rd:reg}   => 0x7a @ rs @ rd @ 0b0000 @ ro @ scl
    cmp %{ro:reg}(%{rs: reg},{scl: i8}), %{rd:reg}   => 0x7c @ rs @ rd @ 0b0000 @ ro @ scl
    
    ; immediate arithmetic
    add ${imm: i16}, %{rd: reg}                      => 0x80 @ 0b0000 @ rd @ imm
    sub ${imm: i16}, %{rd: reg}                      => 0x81 @ 0b0000 @ rd @ imm
    mul ${imm: i16}, %{rd: reg}                      => 0x82 @ 0b0000 @ rd @ imm
    div ${imm: i16}, %{rd: reg}                      => 0x83 @ 0b0000 @ rd @ imm
    shr ${imm: i16}, %{rd: reg}                      => 0x84 @ 0b0000 @ rd @ imm
    shl ${imm: i16}, %{rd: reg}                      => 0x85 @ 0b0000 @ rd @ imm
    and ${imm: i16}, %{rd: reg}                      => 0x88 @ 0b0000 @ rd @ imm
    or  ${imm: i16}, %{rd: reg}                      => 0x89 @ 0b0000 @ rd @ imm
    xor ${imm: i16}, %{rd: reg}                      => 0x8a @ 0b0000 @ rd @ imm
    cmp ${imm: i16}, %{rd: reg}                      => 0x8c @ 0b0000 @ rd @ imm

    ; data movement (byte)
    movb %{rs:reg}, %{rd: reg}                      => 0xa0 @ rs @ rd
    movb (%{rs:reg}), %{rd:reg}                     => 0xa1 @ rs @ rd
    movb %{rs:reg}, (%{rd:reg})                     => 0xa2 @ rs @ rd
    movb {off:i8}(%{rs:reg}), %{rd:reg}             => 0xa3 @ rs @ rd @ off
    movb %{rs:reg}, {off:i8}(%{rd:reg})             => 0xa4 @ rs @ rd @ off
    movb %{ro: reg}(%{rs: reg},{scl:i8}), %{rd: reg}=> 0xa5 @ rs @ rd @ 0b0000 @ ro @ scl
    movb %{rs: reg}, %{ro: reg}(%{rd: reg},{scl:i8})=> 0xa6 @ rs @ rd @ 0b0000 @ ro @ scl
    movb {imm: i8}, %{rd: reg}                      => 0xa7 @ 0b0000 @ rd @ imm

    ; data movement (word)
    movw %{rs:reg}, %{rd: reg}                      => 0xa8 @ rs @ rd
    movw (%{rs:reg}), %{rd:reg}                     => 0xa9 @ rs @ rd
    movw %{rs:reg}, (%{rd:reg})                     => 0xaa @ rs @ rd
    movw {off:i8}(%{rs:reg}), %{rd:reg}             => 0xab @ rs @ rd @ off
    movw %{rs:reg}, {off:i8}(%{rd:reg})             => 0xac @ rs @ rd @ off
    movw %{ro: reg}(%{rs: reg},{scl:i8}), %{rd: reg}=> 0xad @ rs @ rd @ 0b0000 @ ro @ scl
    movw %{rs: reg}, %{ro: reg}(%{rd: reg},{scl:i8})=> 0xae @ rs @ rd @ 0b0000 @ ro @ scl
    movw {imm: i16}, %{rd: reg}                     => 0xaf @ 0b0000 @ rd @ imm

    push %{rs:reg}                          => 0xc0 @ 0b0000 @ rs
    push (%{rs:reg})                        => 0xc1 @ 0b0000 @ rs
    push {off:i8}(%{rs:reg})                => 0xc2 @ 0b0000 @ rs @ off
    push %{ro: reg}(%{rs: reg},{scl:i8})    => 0xc3 @ ro @ rs @ scl
    push {imm: i16}                         => 0xc4 @ imm

    pop %{rd: reg}                          => 0xcf @ 0b0000 @ rd
    call {address: i16}                     => 0xd0 @ address
    ret                                     => 0xd1
    ; dynamically pop x number of args from the stack?
    ;ret {argc: i8}                          => 0xd2 @ argc

    hlt => 0xff

    entry {address: i16}    => address ; specify the entry point to the code
}