#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "computer.h"
#undef mips			/* gcc already has a def for mips */

unsigned int endianSwap(unsigned int);

void PrintInfo (int changedReg, int changedMem);
unsigned int Fetch (int);
void Decode (unsigned int, DecodedInstr*, RegVals*);
int Execute (DecodedInstr*, RegVals*);
int Mem(DecodedInstr*, int, int *);
void RegWrite(DecodedInstr*, int, int *);
void UpdatePC(DecodedInstr*, int);
void PrintInstruction (DecodedInstr*);

/*Globally accessible Computer variable*/
Computer mips;
RegVals rVals;

/*
 *  Return an initialized computer with the stack pointer set to the
 *  address of the end of data memory, the remaining registers initialized
 *  to zero, and the instructions read from the given file.
 *  The other arguments govern how the program interacts with the user.
 */
void InitComputer (FILE* filein, int printingRegisters, int printingMemory,
  int debugging, int interactive) {
    int k;
    unsigned int instr;

    /* Initialize registers and memory */

    for (k=0; k<32; k++) {
        mips.registers[k] = 0;
    }

    /* stack pointer - Initialize to highest address of data segment */
    mips.registers[29] = 0x00400000 + (MAXNUMINSTRS+MAXNUMDATA)*4;

    for (k=0; k<MAXNUMINSTRS+MAXNUMDATA; k++) {
        mips.memory[k] = 0;
    }

    k = 0;

    while (fread(&instr, 4, 1, filein)) {
	/*swap to big endian, convert to host byte order. Ignore this.*/
        mips.memory[k] = ntohl(endianSwap(instr));
        k++;
        if (k>MAXNUMINSTRS) {
            fprintf (stderr, "Program too big.\n");
            exit (1);
        }
    }

    mips.printingRegisters = printingRegisters;
    mips.printingMemory = printingMemory;
    mips.interactive = interactive;
    mips.debugging = debugging;
}

unsigned int endianSwap(unsigned int i) {
    return (i>>24)|(i>>8&0x0000ff00)|(i<<8&0x00ff0000)|(i<<24);
}

/*
 *  Run the simulation.
 */
void Simulate () {
    char s[40];  /* used for handling interactive input */
    unsigned int instr;
    int changedReg=-1, changedMem=-1, val;
    DecodedInstr d;
    
    /* Initialize the PC to the start of the code section */
    mips.pc = 0x00400000;
    while (1) {
        if (mips.interactive) {
            printf ("> ");
            fgets (s,sizeof(s),stdin);
            if (s[0] == 'q') {
                return;
            }
        }

        /* Fetch instr at mips.pc, returning it in instr */
        instr = Fetch (mips.pc);

        printf ("Executing instruction at %8.8x: %8.8x\n", mips.pc, instr);

        /* 
	 * Decode instr, putting decoded instr in d
	 * Note that we reuse the d struct for each instruction.
	 */
        Decode (instr, &d, &rVals);

        /*Print decoded instruction*/
        PrintInstruction(&d);

        /* 
	 * Perform computation needed to execute d, returning computed value 
	 * in val 
	 */
        val = Execute(&d, &rVals);

	UpdatePC(&d,val);

        /* 
	 * Perform memory load or store. Place the
	 * address of any updated memory in *changedMem, 
	 * otherwise put -1 in *changedMem. 
	 * Return any memory value that is read, otherwise return -1.
         */
        val = Mem(&d, val, &changedMem);

        /* 
	 * Write back to register. If the instruction modified a register--
	 * (including jal, which modifies $ra) --
         * put the index of the modified register in *changedReg,
         * otherwise put -1 in *changedReg.
         */
        RegWrite(&d, val, &changedReg);

        PrintInfo (changedReg, changedMem);
    }
}

/*
 *  Print relevant information about the state of the computer.
 *  changedReg is the index of the register changed by the instruction
 *  being simulated, otherwise -1.
 *  changedMem is the address of the memory location changed by the
 *  simulated instruction, otherwise -1.
 *  Previously initialized flags indicate whether to print all the
 *  registers or just the one that changed, and whether to print
 *  all the nonzero memory or just the memory location that changed.
 */
void PrintInfo ( int changedReg, int changedMem) {
    int k, addr;
    printf ("New pc = %8.8x\n", mips.pc);
    if (!mips.printingRegisters && changedReg == -1) {
        printf ("No register was updated.\n");
    } else if (!mips.printingRegisters) {
        printf ("Updated r%2.2d to %8.8x\n",
        changedReg, mips.registers[changedReg]);
    } else {
        for (k=0; k<32; k++) {
            printf ("r%2.2d: %8.8x  ", k, mips.registers[k]);
            if ((k+1)%4 == 0) {
                printf ("\n");
            }
        }
    }
    if (!mips.printingMemory && changedMem == -1) {
        printf ("No memory location was updated.\n");
    } else if (!mips.printingMemory) {
        printf ("Updated memory at address %8.8x to %8.8x\n",
        changedMem, Fetch (changedMem));
    } else {
        printf ("Nonzero memory\n");
        printf ("ADDR	  CONTENTS\n");
        for (addr = 0x00400000+4*MAXNUMINSTRS;
             addr < 0x00400000+4*(MAXNUMINSTRS+MAXNUMDATA);
             addr = addr+4) {
            if (Fetch (addr) != 0) {
                printf ("%8.8x  %8.8x\n", addr, Fetch (addr));
            }
        }
    }
}

/*
 *  Return the contents of memory at the given address. Simulates
 *  instruction fetch. 
 */
unsigned int Fetch ( int addr) {
    printf("%d\n", mips.memory[(addr-0x00400000)/4]);
    return mips.memory[(addr-0x00400000)/4];
}


/* Decode instr, returning decoded instruction. */
void Decode ( unsigned int instr, DecodedInstr* d, RegVals* rVals) {
    /* Your code goes here */
    int opMask = 0xfc000000; // 1111 1100 0000 0000 0000 0000 0000 0000
    int rsMask = 0x3e00000;  // 0000 0011 1110 0000 0000 0000 0000 0000
    int rtMask = 0x1f0000;   // 0000 0000 0001 1111 0000 0000 0000 0000
    int rdMask = 0xf800;     // 0000 0000 0000 0000 1111 1000 0000 0000
    int shamtMask = 0x7c0;   // 0000 0000 0000 0000 0000 0111 1100 0000
    int functMask = 0x3f;    // 0000 0000 0000 0000 0000 0000 0011 1111
    int immedMask = 0xffff;    // 0000 0000 0000 0000 1111 1111 1111 1111
    int addressMask = 0x3ffffff; // 0000 0011 1111 1111 1111 1111 1111 1111

    int signMask = 0x00008000; //used for determining if 16 bit immed is + or -

    int opcode = (instr & opMask) >> 26;
    int rs = (instr & rsMask) >> 21;
    int rt = (instr & rtMask) >> 16;
    int rd = (instr & rdMask) >> 11;
    int shamt = (instr & shamtMask) >> 6;
    int funct = instr & functMask;
    int immed = instr & immedMask;
    int address = instr & addressMask;

    d->op = opcode;

    switch(opcode) {
        case 0: //R type instruction
            // Store the fields of the R type instructions
            d->type = R;
            d->regs.r.rs = rs;
            d->regs.r.rt = rt;
            d->regs.r.rd = rd;
            d->regs.r.shamt = shamt;
            d->regs.r.funct = funct;

            rVals->R_rs = mips.registers[rs];
            rVals->R_rt = mips.registers[rt];
            break;
        case 2: // J type instructions
        case 3:
            // Store the fields of the J type instructions
            d->type = J;

            int pcMask = 0xf0000000;
            int pcfour = (mips.pc & pcMask);     //4 most significant bits of pc

            switch(opcode) {
                case 0x2:   // j
                case 0x3:   // jal
                    address = address << 2; // add 00 as the least significant bits
                    address += pcfour;
                    break;
                default:
                    break;
            }

            d->regs.j.target = address;
            break;
        case 16: // Unused coprocessor instructions
        case 17:
        case 18:
        case 19:
            break;
        default:    // I type instructions
            // Store the fields of the I type instructions
            d->type = I;
            d->regs.i.rs = rs;
            d->regs.i.rt = rt;

            rVals->R_rs = mips.registers[rs];
            rVals->R_rt = mips.registers[rt];

            switch(opcode) {
                case 0x4:   // beq
                case 0x5:   // bne
                case 0x9:   // addiu
                case 0x23:  // lw
                case 0x2b:  // sw
                    if(signMask & instr) //if 16th bit is 1 -> pad with 1's
                        immed += 0xffff0000;
                    break;
                /* The next two should be in the correct format already */
                case 0xc:   // andi
                case 0xd:   // ori
                    break;
                /* Pad the immediate with 16 0's on the right*/
                case 0xf:   // lui
                    immed = immed << 16;
                    break;
                default:
                    break;
            }

            d->regs.i.addr_or_immed = immed;
    }

}

/*
 *  Print the disassembled version of the given instruction
 *  followed by a newline.
 */
void PrintInstruction ( DecodedInstr* d) {
    char* instruction_name[] = {"addu", "subu", "and", "or", "slt", "sll", "srl",
                                 "jr", "j", "jal", "beq", "bne", "addiu", "andi",
                                 "ori", "lui", "lw", "sw"};

}

/* Perform computation needed to execute d, returning computed value */
int Execute ( DecodedInstr* d, RegVals* rVals) {
    switch(d->op) {
        case 0: //Instruction is R-type
            switch (funct) {
                case 0x21:  // addu
                    mips.registers[d->regs.r.rd] = rVals->R_rs + rVals->R_rt;
                    break;
                case 0x23:  // subu
                    mips.registers[d->regs.r.rd] = rVals->R_rs - rVals->R_rt;
                    break;
                case 0x24:  // and
                    mips.registers[d->regs.r.rd] = rVals->R_rs & rVals->R_rt;
                    break;
                case 0x25:  // or
                    mips.registers[d->regs.r.rd] = rVals->R_rs | rVals->R_rt;
                    break;
                case 0x2a:  // slt
                    (rVals->R_rs < rVals->R_rt) ? mips.registers[d->regs.r.rd] = 1 : mips.registers[d->regs.r.rd] = 0;
                    break;
                case 0x00:  // sll
                    mips.registers[d->regs.r.rd] = rVals->R_rt << d->regs.r.shamt;
                    break;
                case 0x02:  // srl
                    mips.registers[d->regs.r.rd] = rVals->R_rt >> d->regs.r.shamt;
                    break;
                case 0x08:  // jr
                    UpdatePC(d, mips.registers[d->regs.r.rs]);
                    break;
            }
        case 2: /* j and jal */
        case 3:
            UpdatePC(d, d->regs.j.target);
            break;
        case 16: // Coprocessor instructions (unused)
        case 17:
        case 18:
        case 19:
            break;
        default:    // I-Type instructions
            switch(d->op) {
                case 0x4:   // beq
                    if(d->regs.i.rs == d->regs.i.rt) {
                        UpdatePC(d, regs.i.addr_or_immed);
                    }
                    break;
                case 0x5:   // bne
                    if(d->regs.i.rs != d->regs.i.rt) {
                        UpdatePC(d, regs.i.addr_or_immed);
                    }
                    break;
                case 0x9:   // addiu
                    mips.registers[d->regs.i.rt] = mips.registers[d->regs.i.rs] + d->regs.i.addr_or_immed;
                    break;
                case 0x23:  // lw
                    int i = mips.registers[d->regs.i.rs] + d->regs.i.addr_or_immed;
                    rVals->R_rt = mips.memory[i];
                    break;
                case 0x2b:  // sw
                    break;
                case 0xc:   // andi
                case 0xd:   // ori
                    break;
                case 0xf:   // lui
                    break;
            break;
    }

    return 0;
}

/* 
 * Update the program counter based on the current instruction. For
 * instructions other than branches and jumps, for example, the PC
 * increments by 4 (which we have provided).
 */
void UpdatePC ( DecodedInstr* d, int val) {
    mips.pc+=4;

    if(d->type == R && d->regs.r.funct == 0x08) {
        mips.pc = val;
        return;
    }

    if(d->type == J) {
        if (d->op == 3) {
            mips.registers[31] = mips.pc + 4;
        }
        mips.pc = val;
    }

    /* Your code goes here */
}

/*
 * Perform memory load or store. Place the address of any updated memory 
 * in *changedMem, otherwise put -1 in *changedMem. Return any memory value 
 * that is read, otherwise return -1. 
 *
 * Remember that we're mapping MIPS addresses to indices in the mips.memory 
 * array. mips.memory[0] corresponds with address 0x00400000, mips.memory[1] 
 * with address 0x00400004, and so forth.
 *
 */
int Mem( DecodedInstr* d, int val, int *changedMem) {
    /* Your code goes here */
  return 0;
}

/* 
 * Write back to register. If the instruction modified a register--
 * (including jal, which modifies $ra) --
 * put the index of the modified register in *changedReg,
 * otherwise put -1 in *changedReg.
 */
void RegWrite( DecodedInstr* d, int val, int *changedReg) {
    /* Your code goes here */
}
