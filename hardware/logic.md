# ALU

## Input signals

A - 1st input to ALU

B - 2nd input to ALU

ALUControl - select ALU function

| Value | Function |
| ----- | -------- |
| 000  | Add      |
| 001  | Subtract |
| 010  | Multiply |
| 011  | Divide   |
| 100  | AND |
| 101  | OR |
| 110  | XOR |
| 111  | NOT |



## Output signals

ALUFlags - negative, zero, carry, overflow

Result - result of selected function