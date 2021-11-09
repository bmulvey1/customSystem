# The Language
## overview

b-like (no types, everything is a word)


## grammar
### **Program**
 - **Stmt List | EOF**
### **Stmt List**
 - **Stmt | Stmt List**
 - Ɛ
### **Stmt**
 - **ID** = **Expression**
 - if(**Condition**){**Stmt List**}
 - someday this'll need while/for loops
### **Condition**
 - **Expression | Expression Tail**
### **Expression**
 - **Term | Term Tail**
### **Term**
 - **Factor | Factor Tail**
### **Factor**
 - **ID**
 - **Literal**
 - (**Condition**)
### **Expression Tail**
 - **Condition Operator | Expression**
 - Ɛ
### **Term Tail**
 - **Addition Operator | Term | Term Tail**
 - Ɛ
### **Factor Tail**
 - **Multiplicaion Operator | Factor | Factor Tail**
 - Ɛ
### COMMENT
 - \#