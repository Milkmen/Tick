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