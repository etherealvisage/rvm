#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
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
    memset(cpu.regs, 0, sizeof(cpu.regs));

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
        printf("CPU state:\n");
        printf("\tPC: %x\n", cpu->pc);
        printf("\tSP: %x\n", cpu->sp);
        printf("\tFlags: %s %s\n", cpu->zflag?"ZF":"", cpu->nflag?"NF":"");
        printf("\tRegisters:\n");
        printf("\t\t%08x %08x %08x %08x\n", cpu->regs[0], cpu->regs[1],
            cpu->regs[2], cpu->regs[3]);
        printf("\t\t%08x %08x %08x %08x\n", cpu->regs[4], cpu->regs[5],
            cpu->regs[6], cpu->regs[7]);
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

        if(cpu->jumped && inst.type != RVM_INST_ENTRY) {
            printf("Jumped to non-entry instruction!\n");
            exit(1);
        }

        if(rvm_inst_check_valid(&inst)) {
            printf("Invalid %s instruction!\n", rvm_inst_type_strings[inst.type]);
            exit(1);
        }

        // constants, if a target for *op is needed
        uint32_t opc[3];
        uint32_t *op[3];
        for(int i = 0; i < 3; i ++) {
            switch(inst.optype[i]) {
            case RVM_OP_ABSENT:
                op[i] = NULL;
                break;
            case RVM_OP_SCONST:
            case RVM_OP_STACK:
                opc[i] = inst.opval[i];
                op[i] = opc + i;
                break;
            case RVM_OP_REG:
                op[i] = cpu->regs + inst.opval[i];
                break;
            case RVM_OP_LCONST:
                op[i] = following + opfollow[i];
                break;
            }
        }

        switch(inst.type) {
        case RVM_INST_HLT:
            cpu->halted = true;
            break;
        case RVM_INST_ADD:
            if(op[2]) *op[2] = *op[0] + *op[1];
            else *op[0] += *op[1];
            break;
        case RVM_INST_SUB:
            if(op[2]) *op[2] = *op[0] - *op[1];
            else *op[0] -= *op[1];
            break;
        case RVM_INST_MUL:
            if(op[2]) *op[2] = *op[0] * *op[1];
            else *op[0] *= *op[1];
            break;
        case RVM_INST_DIV:
            if(op[2]) *op[2] = *op[0] / *op[1];
            else *op[0] /= *op[1];
            break;
        case RVM_INST_OR:
            if(op[2]) *op[2] = *op[0] | *op[1];
            else *op[0] |= *op[1];
            break;
        case RVM_INST_AND:
            if(op[2]) *op[2] = *op[0] & *op[1];
            else *op[0] &= *op[1];
            break;
        case RVM_INST_NOT:
            if(op[1]) *op[1] = ~ *op[0];
            else *op[0] = ~*op[0];
            break;
        case RVM_INST_XOR:
            if(op[2]) *op[2] = *op[0] ^ *op[1];
            else *op[0] ^= *op[1];
            break;
        case RVM_INST_SHL:
            if(op[2]) *op[2] = *op[0] << *op[1];
            else *op[0] <<= *op[1];
            break;
        case RVM_INST_SHR:
            if(op[2]) *op[2] = *op[0] >> *op[1];
            else *op[0] >>= *op[1];
            break;
        case RVM_INST_CMP:
            // TODO
            break;
        case RVM_INST_ENTRY:
            break;
        case RVM_INST_JMP:
            cpu->pc += *op[0];
            break;
        case RVM_INST_JE:
            if(cpu->zflag) cpu->pc += *op[0];
            break;
        case RVM_INST_JL:
            if(cpu->nflag) cpu->pc += *op[0];
            break;
        case RVM_INST_JLE:
            if(cpu->nflag && cpu->zflag) cpu->pc += *op[0];
            break;
        case RVM_INST_JNE:
            if(!cpu->zflag) cpu->pc += *op[0];
            break;
        case RVM_INST_JNL:
            if(!cpu->nflag) cpu->pc += *op[0];
            break;
        case RVM_INST_JNLE:
            if(!cpu->nflag && !cpu->zflag) cpu->pc += *op[0];
            break;
        case RVM_INST_CALL:
            // TODO
            break;
        case RVM_INST_RET:
            // TODO
            break;
        case RVM_INST_PUSH:
            // TODO
            break;
        case RVM_INST_POP:
            // TODO
            break;
        case RVM_INST_SWAP: {
            uint32_t t = *op[0];
            *op[0] = *op[1];
            *op[1] = t;
            break;
        }
        case RVM_INST_ALLOC:
            // TODO
            break;
        case RVM_INST_FREE:
            // TODO
            break;
        default:
            printf("Instruction NYI.\n");
            break;
        }
    }
}
