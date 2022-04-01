# ALU

## Input signals

A - 1st input to ALU

B - 2nd input to ALU

ALUControl - select ALU function

| Value | Function |
| ----- | -------- |
| 0000  | Add      |
| 0001  | Subtract |
| 0010  | Multiply |
| 0011  | Divide   |
| 0100  | Shift right |
| 0101  | Shift left |
| 0110  | AND |
| 0111  | OR |
| 1000  | XOR |
| 1001  | NOT |
| 1010  | None |
| 1011  | None |
| 1100  | None |
| 1101  | None |
| 1110  | None |
| 1111  | None |


## Output signals

ALUFlags - negative, zero, carry, overflow

Result - result of selected function