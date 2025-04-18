![Tick Logo](tick.png)

Tick is a general purpose programming language consisting of these 3 parts:
- Compiler - A small compiler that produces ASM (Coming Soon...)
- Assembler - An assembler that produces a custom binary
- Virtual Machine - A VM that can execute the binary

Tick ASM Example:
This should output "ABCDE"
```
LOOP:
	MOV R1, 65
	ADD R1, R0
	PUT R1
	ADD R0, 1
	EQL R0, 5
	JNZ END, R255
	JMP LOOP
END:
	HLT
```

