/*
   SESC: Super ESCalar simulator
   Copyright (C) 2003 University of Illinois.

   Contributed by Jose Renau
                  Luis Ceze

This file is part of SESC.

SESC is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2, or (at your option) any later version.

SESC is    distributed in the  hope that  it will  be  useful, but  WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should  have received a copy of  the GNU General  Public License along with
SESC; see the file COPYING.  If not, write to the  Free Software Foundation, 59
Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/* This code is inspired in smt:scodes.c It translates MIPS codes to a
 * easier to manipulate format. MIPSInstruction.cpp, Instruction.h,
 * and ExecutionFlow.h should be the only files to modify if VESC is
 * ported to another fronted (embra,i386...)
 *
 */

#include "Instruction.h"
#include "SescConf.h"

#include "libemul/EmulInit.h"
int32_t isFirstInFuncCall(uint32_t addr);

// iBJUncond is also true for all the conditional instruction
// marked as likely. If the compiler tells me that most of the
// time is taken. Do you believe the compiler?
// For crafty, the results are worse.

// Following the ISA advice can backfire through two places:
//
// 1-You have a very good predictor, and it's better than the ISA
// suggested.
//
// After playing with it for a while, I recommend not to follow the
// ISA advice
//
//#define FOLLOW_MIPSPRO_ADVICE 1

static const char *opcode2NameTable[] = {
    "iOpInvalid",
    "iALU",
    "iMult",
    "iDiv",
    "iBJ",
    "iLoad",
    "iStore",
    "fpALU",
    "fpMult",
    "fpDiv",
    "iFence",
    "iEvent"
};


int32_t Instruction::maxFuncID=0;

Instruction *Instruction::InstTable = 0;
Instruction::InstHash Instruction::instHash;

size_t Instruction::InstTableSize = 0;

void Instruction::initialize(int32_t argc
                             ,char **argv
                             ,char **envp)
{
    emulInit(argc, argv, envp);
}


void Instruction::finalize()
{
    free(InstTable);
    InstTable = 0;
}

const char *Instruction::opcode2Name(InstType op)
{
    return opcode2NameTable[op];
}

void Instruction::dump(const char *str) const
{
    MSG("%s:0x%8x: reg[%2d] = reg[%2d] [%8s:%2d] reg[%2d] (uEvent=%d)", str,
        (int)getAddr(), dest, src1, opcode2Name(opcode), subCode, src2, uEvent);
}

