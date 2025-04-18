#define TICK_VM_IMPLEMENTATION
#include "tickvm.h"

int main(int argc, char* argv[])
{
    FILE* f;
    long            file_size;
    size_t          instr_count;
    tick_instr_t* program;
    tickvm_state_t  vm;
    size_t          read;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s program.bin\n", argv[0]);
        return 1;
    }

    /* 1) Open the binary file */
    f = fopen(argv[1], "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    /* 2) Determine file size */
    if (fseek(f, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(f);
        return 1;
    }
    file_size = ftell(f);
    if (file_size < 0) {
        perror("ftell");
        fclose(f);
        return 1;
    }
    rewind(f);

    /* 3) Allocate buffer for instructions */
    instr_count = file_size / sizeof(tick_instr_t);
    program = (tick_instr_t*)malloc(instr_count * sizeof(tick_instr_t));
    if (!program) {
        fprintf(stderr, "Out of memory\n");
        fclose(f);
        return 1;
    }

    /* 4) Read the file into program[] */
    read = fread(program, sizeof(tick_instr_t), instr_count, f);
    if (read != instr_count) {
        fprintf(stderr, "Read error: expected %zu instructions, got %zu\n",
            instr_count, read);
        free(program);
        fclose(f);
        return 1;
    }
    fclose(f);

    /* 5) Initialize VM with, say, 1 KiB of data memory */
    tick_initialize(&vm, 1024);

    /* 6) Execute the loaded program */
    tick_exec(&vm, program, (uint32_t)instr_count);

    /* 7) Clean up */
    free(vm.memory);
    free(program);

    return 0;
}
