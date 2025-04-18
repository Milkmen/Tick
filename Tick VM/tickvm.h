#ifndef TICK_VM_H
#define TICK_VM_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/*
 TICK OPCODES:
*/
#define OP_HLT 0x00
#define OP_MOV 0x04 /* Move */
#define OP_ADD 0x06 /* Addition */
#define OP_SUB 0x07 /* Subtraction */
#define OP_MUL 0x08 /* Multiplication */
#define OP_DIV 0x09 /* Division */
#define OP_JMP 0x0F /* Jump (Absolute) */
#define OP_JIZ 0x10 /* Jump if Zero */
#define OP_JNZ 0x11 /* Jump if not Zero */
#define OP_EQL 0x16 /* Is Equal (Outputs to R255) */
#define OP_LDM 0x21 /* Load Memory */
#define OP_STM 0x22 /* Store Memory */
#define OP_OUT 0x36 /* Prints Number */
#define OP_PUT 0x37 /* Print Char */

#define AR_DL (1 << 1) /* Destination is Literal (register index) */
#define AR_SL (1 << 2) /* Source is Literal */

#define REG_COUNT 256

typedef struct {
    uint8_t  opcode;
    uint8_t  arginfo;
    uint32_t destination;
    uint32_t source;
} tick_instr_t;

typedef struct {
    uint32_t counter;
    uint32_t regs[REG_COUNT];
    uint32_t* memory;
    size_t    memory_size; /* in uint32_t units */
} tickvm_state_t;

/* Initialize VM: zero registers, allocate 'memory_bytes' bytes */
void tick_initialize(tickvm_state_t* vm, unsigned int memory_bytes) {
    if (memory_bytes % sizeof(uint32_t) != 0) {
        fprintf(stderr, "Memory size must be a multiple of %zu bytes\n", sizeof(uint32_t));
        exit(EXIT_FAILURE);
    }
    vm->counter = 0;
    vm->memory_size = memory_bytes / sizeof(uint32_t);
    vm->memory = (uint32_t*)malloc(memory_bytes);
    if (!vm->memory) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memset(vm->regs, 0, sizeof(vm->regs));
}

/* Copy 'count' instructions (32-bit words) into VM memory (unused here) */
void tick_load_mem(tickvm_state_t* vm, uint32_t* source, size_t count) {
    if (count > vm->memory_size) {
        fprintf(stderr, "Error: insn count exceeds vm memory size\n");
        exit(EXIT_FAILURE);
    }
    memcpy(vm->memory, source, count * sizeof(uint32_t));
}

/* Execute 'size' instructions in 'bytecode' */
int tick_exec(tickvm_state_t* vm, tick_instr_t* bytecode, uint32_t size) {
    while (vm->counter < size) {
        tick_instr_t* instr = &bytecode[vm->counter];
        uint32_t src_val = (instr->arginfo & AR_SL)
            ? instr->source
            : vm->regs[instr->source];
        uint32_t dst_idx = instr->destination; /* always register index */

        switch (instr->opcode) {
        case OP_HLT:
            /* return via literal or register */
            return (instr->arginfo & AR_DL)
                ? (int)instr->destination
                : (int)vm->regs[instr->destination];

        case OP_MOV:
            vm->regs[dst_idx] = src_val;
            break;

        case OP_ADD:
            vm->regs[dst_idx] += src_val;
            break;

        case OP_SUB:
            vm->regs[dst_idx] -= src_val;
            break;

        case OP_MUL:
            vm->regs[dst_idx] *= src_val;
            break;

        case OP_DIV:
            vm->regs[dst_idx] /= src_val;
            break;

        case OP_JMP:
            vm->counter = dst_idx;
            continue;

        case OP_JIZ:
            if (src_val == 0) {
                vm->counter = dst_idx;
                continue;
            }
            break;

        case OP_JNZ:
            if (src_val != 0) {
                vm->counter = dst_idx;
                continue;
            }
            break;

        case OP_EQL:
            vm->regs[REG_COUNT - 1] =
                (vm->regs[dst_idx] == src_val) ? 1u : 0u;
            break;

        case OP_LDM:
            vm->regs[dst_idx] = vm->memory[src_val];
            break;

        case OP_STM:
            if (dst_idx >= vm->memory_size) {
                fprintf(stderr, "Error: Not enough memory!\n");
                return -1;
            }
            vm->memory[dst_idx] = vm->regs[instr->source];
            break;

        case OP_OUT:
            /* print register value */
            printf("%u", vm->regs[dst_idx]);
            break;

        case OP_PUT:
            /* print register value as char */
            printf("%c", (char)vm->regs[dst_idx]);
            break;

        default:
            fprintf(stderr, "Invalid instruction at %u\n", vm->counter);
            return -1;
        }

        vm->counter++;
    }
    return 0;
}

#endif /* TICK_VM_H */
