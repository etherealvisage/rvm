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

static void dump_cpu_state(rvm_cpu_state *cpu);

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
    stack.contents = NULL;
    heap.size = 0;
    heap.contents = NULL;

    rvm_cpu_state cpu;
    cpu.pc = 0;
    cpu.sp = 0;
    cpu.zflag = false; cpu.nflag = false;
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
    uint32_t heap_top = 0;
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

        uint32_t old_pc = cpu->pc;
        ni = program[cpu->pc++];
        rvm_inst_to_struct(ni, &inst);
        for(int i = 0; i < 3; i ++) {
            if(inst.optype[i] % 3 == 1) { // lconst
                opfollow[i] = following_count;
                if(cpu->pc*4 >= program_size) {
                    printf("Instruction extends past end of program\n");
                    exit(1);
                }
                following[following_count++] = program[cpu->pc++];
            }
        }

        if(cpu->jumped && inst.type != RVM_INST_ENTRY) {
            printf("Jumped to non-entry instruction!\n");
            exit(1);
        }
        else if(cpu->jumped) cpu->jumped = false;

        if(rvm_inst_check_valid(&inst)) {
            printf("Invalid %s instruction!\n",
                rvm_inst_type_strings[inst.type]);
            exit(1);
        }
        // as an optimization, skip the operand parsing for entry.
        if(inst.type == RVM_INST_ENTRY) continue;

        // constants, if a target for *op is needed
        uint32_t opc[3];
        uint32_t *op[3];
        for(int i = 0; i < 3; i ++) {
            if(inst.optype[i] == RVM_OP_ABSENT) {
                op[i] = NULL;
                continue;
            }
            switch(inst.optype[i] % 3) {
            case 0: // sconst
                opc[i] = inst.opval[i];
                op[i] = opc + i;
                break;
            case 1: // lconst
                op[i] = following + opfollow[i];
                break;
            case 2: // reg
                op[i] = cpu->regs + inst.opval[i];
                break;
            }

            switch(inst.optype[i] / 3) {
            case 0: // value
                break;
            case 1: // stack
                // todo: check for over/underflow
                // todo: check if within stack frame
                op[i] = heap->contents + *op[i] + cpu->sp;
                break;
            case 2: // heap
                op[i] = heap->contents + *op[i];
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
        case RVM_INST_CMP: {
            uint32_t result = *op[0] - *op[1];
            if(result == 0) cpu->zflag = true;
            else cpu->zflag = false;
            if(result & (1<<31)) cpu->nflag = true;
            else cpu->nflag = false;
            break;
        }
        case RVM_INST_ENTRY: // naught to do.
            break;
        case RVM_INST_JMP:
            cpu->pc += *op[0] - 1;
            cpu->jumped = true;
            break;
        case RVM_INST_JE:
            if(cpu->zflag) {
                cpu->pc = old_pc + *op[0];
                cpu->jumped = true;
            }
            break;
        case RVM_INST_JL:
            if(cpu->nflag) {
                cpu->pc = old_pc + *op[0];
                cpu->jumped = true;
            }
            break;
        case RVM_INST_JLE:
            if(cpu->nflag && cpu->zflag) {
                cpu->pc = old_pc + *op[0];
                cpu->jumped = true;
            }
            break;
        case RVM_INST_JNE:
            if(!cpu->zflag) {
                cpu->pc = old_pc + *op[0];
                cpu->jumped = true;
            }
            break;
        case RVM_INST_JNL:
            if(!cpu->nflag) {
                cpu->pc = old_pc + *op[0];
                cpu->jumped = true;
            }
            break;
        case RVM_INST_JNLE:
            if(!cpu->nflag && !cpu->zflag) {
                cpu->pc = old_pc + *op[0];
                cpu->jumped = true;
            }
            break;
        case RVM_INST_CALL:
            // TODO: establish new frame
            if(cpu->sp * 4 + 4 >= stack->size) rvm_mem_add_page(stack);
            stack->contents[cpu->sp ++] = cpu->pc;
            cpu->pc = old_pc + *op[0];
            cpu->jumped = true;

            break;
        case RVM_INST_RET:
            // TODO: remove old frame
            cpu->pc = stack->contents[-- cpu->sp];
            cpu->jumped = false; // returns are always to valid targets due to frame system

            break;
        case RVM_INST_PUSH:
            if(cpu->sp * 4 + 4 >= stack->size) rvm_mem_add_page(stack);
            stack->contents[cpu->sp ++] = *op[0];
            break;
        case RVM_INST_POP:
            // TODO: check frame limits
            if(cpu->sp == 0) {
                 printf("Stack underflow!\n");
                 exit(1);
            }
            *op[0] = stack->contents[-- cpu->sp];
            break;
        case RVM_INST_SWAP: {
            uint32_t t = *op[0];
            *op[0] = *op[1];
            *op[1] = t;
            break;
        }
        case RVM_INST_ALLOC: {
            // watermark allocator . . . sigh.
            while(heap_top + *op[0] >= heap->size) {
                heap->size += 0x1000;
                rvm_mem_add_page(heap);
            }
            *op[1] = heap_top;
            heap_top += *op[0];
            break;
        }
        case RVM_INST_FREE:
            // TODO
            break;
        default:
            printf("Instruction NYI.\n");
            break;
        }
    }

    dump_cpu_state(cpu);
}

static void dump_cpu_state(rvm_cpu_state *cpu) {
    printf("\tCPU state:\n");
    printf("\t\tPC: %x\n", cpu->pc);
    printf("\t\tSP: %x\n", cpu->sp);
    printf("\t\tFlags: %s %s\n", cpu->zflag?"ZF":"", cpu->nflag?"NF":"");
    printf("\t\tRegisters:\n");
    printf("\t\t\t%08x %08x %08x %08x\n", cpu->regs[0], cpu->regs[1],
        cpu->regs[2], cpu->regs[3]);
    printf("\t\t\t%08x %08x %08x %08x\n", cpu->regs[4], cpu->regs[5],
        cpu->regs[6], cpu->regs[7]);
}
