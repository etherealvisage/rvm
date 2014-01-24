#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>

#include "common/inst.h"

uint32_t *program;
uint32_t program_size;

typedef struct rvm_cpu_state {
    uint32_t pc, sp;
    uint32_t regs[8];
    bool zflag;
    bool nflag;

    bool jumped;
    bool halted;
} rvm_cpu_state;

typedef struct rvm_mem {
    uint32_t *contents;
    uint32_t size;
} rvm_mem;

static void rvm_mem_add_page(rvm_mem *mem);

static void sim_loop(rvm_cpu_state *cpu, rvm_mem *stack, rvm_mem *heap);

int main(int argc, char *argv[]) {
    if(argc != 2) {
        printf("Usage: %s program\n", argv[0]);
        exit(1);
    }

    int fd = open(argv[1], O_RDONLY);
    if(fd < 0) {
        printf("Cannot open file \"%s\": %m\n", argv[1]);
        exit(1);
    }
    struct stat fds;
    fstat(fd, &fds);

    program_size = fds.st_size;
    if(program_size % 4 != 0) {
        printf("Program size must be multiple of 4!\n");
        exit(1);
    }

    program = mmap(NULL, (program_size+0xfff)&~0xfff, PROT_READ, MAP_PRIVATE,
        fd, 0);

    if(!program) {
        printf("Failed to map program memory: %m\n");
        exit(1);
    }

    rvm_mem stack, heap;
    stack.size = 0;
    heap.size = 0;

    rvm_cpu_state cpu;
    cpu.pc = 0;
    cpu.sp = 0;
    cpu.halted = false;
    cpu.jumped = false;
    
    sim_loop(&cpu, &stack, &heap);

    close(fd);

    return 0;
}

static void rvm_mem_add_page(rvm_mem *mem) {
    mem->contents = realloc(mem->contents, mem->size += 0x1000);
    if(!mem->contents) {
        printf("Couldn't resize memory.\n");
        exit(1);
    }
}

static void sim_loop(rvm_cpu_state *cpu, rvm_mem *stack, rvm_mem *heap) {
    rvm_inst inst;
    while(!cpu->halted) {
        uint32_t ni;
        uint32_t following[3];
        uint32_t opfollow[3];
        uint32_t following_count = 0;
        if(cpu->pc*4 >= program_size) {
            printf("Tried to execute past end of program.\n");
            exit(1);
        }

        ni = program[cpu->pc++];
        rvm_inst_to_struct(ni, &inst);
        for(int i = 0; i < 3; i ++) {
            if(inst.optype[i] == RVM_OP_LCONST) {
                opfollow[i] = following_count;
                if(cpu->pc*4 >= program_size) {
                    printf("Invalid instruction -- extends past end of program\n");
                    exit(1);
                }
                following[following_count++] = program[cpu->pc++];
            }
        }

        switch(inst.type) {
        case RVM_INST_HLT:
            cpu->halted = true;
            break;
        default:
            printf("Instruction NYI.\n");
            break;
        }
    }
}
