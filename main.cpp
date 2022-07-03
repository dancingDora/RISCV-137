#include <iostream>
#include <cstdlib>
#include <string>

using namespace std;

//循环执行符 : cycles
int CYCLES{-1};

//为了循环队列戳掉泡泡
bool isBong = true;
unsigned popoBong;

template<class T = unsigned int>
struct LoopQueue {
    T val[100];
    int last_i = 0;

    T &operator[](int i) {
        //printf("delta:%2d ", i - last_i);
        //last_i = i;
//        int delta = CYCLES - i;
//        if (abs(delta) > 5) {
//            printf("i:%d CYCLES:%d\n", i, CYCLES);
//            throw -1;
//        }
        return val[i % 100];
    }

    LoopQueue() {
        memset(val, 0, sizeof(val));
    }
};

//指令集
enum opt {
    LUI, AUIPC, JAL, JALR, BEQ, BNE, BLT, BGE, BLTU, BGEU, LB, LH, LW, LBU, LHU, SB, SH,
    SW, ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI, ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND
};

LoopQueue<opt> option;


LoopQueue<unsigned> resEXE;

struct MEM_WB {
    unsigned load;
};
LoopQueue<MEM_WB> mem_wb;

//指令实体
LoopQueue<> inst;

//指令中各参数
LoopQueue<> opcode, rd, fct3, rs1, rs2, fct7;
LoopQueue<> imm11_0, imm11_5, imm4_0, imm12, imm10_5, imm4_1, imm11, imm31_12, imm20, imm10_1, i11, imm19_12;
LoopQueue<> immI, immS, immB, immU, immJ;

LoopQueue<> regrs1, regrs2;

//寄存器，内存，代码
unsigned int reg[32]{};
unsigned int pc = 0;
unsigned char mem[500000]{};
LoopQueue<bool> code[32];

unsigned int tmpReg[32]{};

unsigned pos = 0;

//第一位为1，符号上位均为1
//第一位为0，符号上位均为0
unsigned int SignExtend(unsigned int x, unsigned char hh) {
    if (x >> (hh - 1) & 1) x |= 0xFFFFFFFFu << hh;
    return x;
}

//全局判符 : isEXE, isMEM, isWB
int isEXE{-1}, isMEM{-1}, isWB{-1}, isID{-1}, isIF{-1};
bool isEnd{false};
unsigned int popo = 0;
//插入两个泡泡
/* ID时插入
 * 原因：
 *   '该'指令会更改 MEM ，为了之后的指令可以顺利调用 `mem[]` ，
 *       所以只要是 load&store 中的 store 操作就 insertBubble
 * 操作：
 *   ID，IF 下两回合不执行
 *   EXE 执行一回合 下两回合不执行
 *   MEM 执行两回合 下两回合不执行
 *   WB  执行三回合 下两回合不执行
 */

/*
   WB  WB  WB  WB  WB  WB  WB  | WB  WB  WB  **  **  WB  WB  WB  WB  WB  WB  WB
   MEM MEM MEM MEM MEM MEM MEM | MEM MEM **  **  MEM MEM MEM MEM MEM MEM MEM MEM
   EXE EXE EXE EXE EXE EXE EXE | EXE **  **  EXE EXE EXE EXE EXE EXE EXE EXE EXE
   ID  ID  ID  ID  ID  ID 'ID' | **  **  ID  ID  ID  ID  ID  ID  ID  ID  ID  ID
   IF  IF  IF  IF  IF  IF  **  | **  IF  IF  IF  IF  IF  IF  IF  IF  IF  IF  IF
 */

void insertBubble() {
    isWB = 4;
    isMEM = 3;
    isEXE = 2;
    isID = 1;
    isIF = 1;
}

void insertLastBubble() {
    isWB = 4;
    isMEM = 3;
    isEXE = 2;
    isID = 1;
    isEnd = true;
}
//插入一个泡泡
/* IF时插入
 * 原因：
 *   IF 用到的 pc 可能在上'一'条指令之中发生了更改，此时读到不应该此时读到的IF
 *   所以应该不在此时读入IF
 * 处理逻辑：
 *   因为为 MEM 单独定制的插入两个泡泡处理机制，
 *   所以此次插入泡泡为改为让 inst[] 读取特殊值，再下几次操作中进行特判
 */

/*
   WB  WB  WB  WB  WB  WB  WB  WB  | WB  WB  WB  **  WB  WB  WB  WB  WB  WB  WB
   MEM MEM MEM MEM MEM MEM MEM MEM | MEM MEM **  MEM MEM MEM MEM MEM MEM MEM MEM
   EXE EXE EXE EXE EXE EXE EXE EXE | EXE **  EXE EXE EXE EXE EXE EXE EXE EXE EXE
   ID  ID  ID  ID  ID  ID  ID  ID  | **  ID  ID  ID  ID  ID  ID  ID  ID  ID  ID
   IF  IF  IF  IF  IF  IF  IF '**' | IF  IF  IF  IF  IF  IF  IF  IF  IF  IF  IF
 */
void insertPopo(const int &pp) {
    inst[pp] = (unsigned int) 1;
    popoBong = pp;
    isBong = false;
    return;
}

void readle(std::istream &Istream) {
    string input;
    char *nxt;
    while (Istream >> input) {
        if (input[0] == '@')
            pos = strtoul(input.c_str() + 1, &nxt, 16);
        else
            mem[pos++] = strtoul(input.c_str(), &nxt, 16);
    }
}

void regZero() {
    reg[0] = 0;
    tmpReg[0] = 0;
}

/*
 12 32 43 12 "37 17 00 00" 43 12 43 12 43
    take4token
 37 17 00 00
    TaiChi
 00 00 17 37
    FtoS (2 * 0x16 * 4 -> 4 * bool * 8)
0000 0000 0000 0000 0001 0111 0011 0111
    changeLanguage
|----------|-----------|--------|-----|-----------|---------|----------------|----------|
|    31    |30       25| 24   21| 20  |19      15 |14    12 |11            7 |6       0 |
|----------|-----------|--------|-----|-----------|---------|----------------|----------|
|                      |              |           |         |                |          |
|R :       fct7        |    rs2       |    rs1    |  fct3   |       rd       |  opcode  |
|                      |              |           |         |                |          |
|----------------------|--------------+-----------+---------+----------------|----------|
|                                     |           |         |                |          |
|I :          imm[11:0]               |    rs1    |  fct3   |       rd       |  opcode  |
|                                     |           |         |                |          |
|----------------------|--------------|-----------|---------|----------------|----------|
|                      |              |           |         |                |          |
|S :     imm[11:5]     |    rs2       |    rs1    |  fct3   |    imm[4:0]    |  opcode  |
|                      |              |           |         |                |          |
|----------------------|--------------|-----------|---------|--------|-------|----------|
|                      |              |           |         |        |       |          |
|B :imm[12] |imm[10:5] |    rs2       |    rs1    |  fct3   |imm[4:1]|imm[11]|  opcode  |
|                      |              |           |         |        |       |          |
|----------------------|--------------|-----------|---------|--------|-------|----------|
|                                                           |                |          |
|U :                imm[31:12]                              |      rd        |  opcode  |
|                                                           |                |          |
|----------|--------------------|-----|---------------------+----------------|----------|
|          |                    |     |                     |                |          |
|J :imm[20]|      imm[10:1]     |i[11]|    imm[19:12]       |      rd        |  opcode  |
|          |                    |     |                     |                |          |
|----------|--------------------|-----|---------------------|----------------|----------|
 */

/** what IF do
十六进制 -> 二进制 打表


          37     17    00     00
                     |
                    \|/
          00     00    17     37
                     |
                    \|/
0000·0000  0000·0000   0001·0111 0011·0111
/|\
 L____存在bool数组里
 **/
const unsigned int twoN[33] = {
        1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768,
        65536,
        131072, 262144, 524288, 1048576, 2097152, 4194304, 8388608, 16777216, 33554432,
        67108864,
        134217728, 268435456, 536870912, 1073741824, 2147483648
};


bool IF(int cycles) {
//    && inst[cycles - 1] != 1 && !inst[cycles - 1]
    regZero();
    if(inst[cycles - 5] == 1) inst[cycles - 5] = 2;
    if (cycles > 0) {
        if (option[cycles - 1] == JAL ||
            option[cycles - 1] == JALR ||
            option[cycles - 1] == BEQ ||
            option[cycles - 1] == BNE ||
            option[cycles - 1] == BLT ||
            option[cycles - 1] == BGE ||
            option[cycles - 1] == BLTU ||
            option[cycles - 1] == BGEU
                ) {
            insertPopo(cycles);
            return true;
        }
    }
    resEXE[cycles] = mem_wb[cycles].load = opcode[cycles] = rd[cycles] = fct3[cycles] = rs1[cycles] = rs2[cycles] = fct7[cycles] = imm11_0[cycles] = imm11[cycles] = imm11_5[cycles] = imm4_0[cycles] = imm4_1[cycles] = imm12[cycles] = imm10_1[cycles] = imm10_5[cycles] = imm19_12[cycles] = imm20[cycles] = imm31_12[cycles] = i11[cycles] = immI[cycles] = immS[cycles] = immB[cycles] = immU[cycles] = immJ[cycles] = regrs1[cycles] = regrs2[cycles] = 0;
    resEXE[cycles + 1] = mem_wb[cycles + 1].load = opcode[cycles + 1] = rd[cycles + 1] = fct3[cycles + 1] = rs1[cycles + 1] = rs2[cycles + 1] = fct7[cycles + 1] = imm11_0[cycles + 1] = imm11[cycles + 1] = imm11_5[cycles + 1] = imm4_0[cycles + 1] = imm4_1[cycles + 1] = imm12[cycles + 1] = imm10_1[cycles + 1] = imm10_5[cycles + 1] = imm19_12[cycles + 1] = imm20[cycles + 1] = imm31_12[cycles + 1] = i11[cycles + 1] = immI[cycles + 1] = immS[cycles + 1] = immB[cycles + 1] = immU[cycles + 1] = immJ[cycles + 1] = regrs1[cycles + 1] = regrs2[cycles + 1] = 0;
    resEXE[cycles + 2] = mem_wb[cycles + 2].load = opcode[cycles + 2] = rd[cycles + 2] = fct3[cycles + 2] = rs1[cycles + 2] = rs2[cycles + 2] = fct7[cycles + 2] = imm11_0[cycles + 2] = imm11[cycles + 2] = imm11_5[cycles + 2] = imm4_0[cycles + 2] = imm4_1[cycles + 2] = imm12[cycles + 2] = imm10_1[cycles + 2] = imm10_5[cycles + 2] = imm19_12[cycles + 2] = imm20[cycles + 2] = imm31_12[cycles + 2] = i11[cycles + 2] = immI[cycles + 2] = immS[cycles + 2] = immB[cycles + 2] = immU[cycles + 2] = immJ[cycles + 2] = regrs1[cycles + 2] = regrs2[cycles + 2] = 0;
    resEXE[cycles + 3] = mem_wb[cycles + 3].load = opcode[cycles + 3] = rd[cycles + 3] = fct3[cycles + 3] = rs1[cycles + 3] = rs2[cycles + 3] = fct7[cycles + 3] = imm11_0[cycles + 3] = imm11[cycles + 3] = imm11_5[cycles + 3] = imm4_0[cycles + 3] = imm4_1[cycles + 3] = imm12[cycles + 3] = imm10_1[cycles + 3] = imm10_5[cycles + 3] = imm19_12[cycles + 3] = imm20[cycles + 3] = imm31_12[cycles + 3] = i11[cycles + 3] = immI[cycles + 3] = immS[cycles + 3] = immB[cycles + 3] = immU[cycles + 3] = immJ[cycles + 3] = regrs1[cycles + 3] = regrs2[cycles + 3] = 0;
    resEXE[cycles + 4] = mem_wb[cycles + 4].load = opcode[cycles + 4] = rd[cycles + 4] = fct3[cycles + 4] = rs1[cycles + 4] = rs2[cycles + 4] = fct7[cycles + 4] = imm11_0[cycles + 4] = imm11[cycles + 4] = imm11_5[cycles + 4] = imm4_0[cycles + 4] = imm4_1[cycles + 4] = imm12[cycles + 4] = imm10_1[cycles + 4] = imm10_5[cycles + 4] = imm19_12[cycles + 4] = imm20[cycles + 4] = imm31_12[cycles + 4] = i11[cycles + 4] = immI[cycles + 4] = immS[cycles + 4] = immB[cycles + 4] = immU[cycles + 4] = immJ[cycles + 4] = regrs1[cycles + 4] = regrs2[cycles + 4] = 0;
    inst[cycles] =
            (unsigned int) mem[pc] | (unsigned int) mem[pc + 1] << 8 |
            (unsigned int) mem[pc + 2] << 16 | (unsigned int) mem[pc + 3] << 24;
    unsigned tmpInst = inst[cycles];
    pc += 4;
    if (inst[cycles] == 0x0ff00513u) {
        printf("%d", reg[10] & 255u);
//        insertLastBubble();
        return false;
    }
    for (int i = 0; i <= 31; i++) code[i][cycles] = false;
    for (int i = 31; i >= 0; i--)
        if (tmpInst >= twoN[i]) {
            tmpInst -= twoN[i];
            code[i][cycles] = true;
        } else continue;
    return true;
}

void ID(int cycles) {
    regZero();
    if (cycles < 0) return;
    if (inst[cycles] == 1) return;
//    先从code中得到各参数
    opcode[cycles] = (code[0][cycles]) | (code[1][cycles] << 1) | (code[2][cycles] << 2) | (code[3][cycles] << 3) |
                     (code[4][cycles] << 4) | (code[5][cycles] << 5) |
                     (code[6][cycles] << 6);
    imm11[cycles] = (code[7][cycles] << 11);
    imm4_1[cycles] =
            (code[8][cycles] | (code[9][cycles] << 1) | (code[10][cycles] << 2) | (code[11][cycles] << 3)) << 1;
    rd[cycles] = code[7][cycles] | (code[8][cycles] << 1) | (code[9][cycles] << 2) | (code[10][cycles] << 3) |
                 (code[11][cycles] << 4);
    imm4_0[cycles] = rd[cycles];
    fct3[cycles] = code[12][cycles] | (code[13][cycles] << 1) | (code[14][cycles] << 2);
    rs1[cycles] = (code[15][cycles] | (code[16][cycles] << 1) | (code[17][cycles] << 2) | (code[18][cycles] << 3) |
                   (code[19][cycles] << 4));
    rs2[cycles] = (code[20][cycles] | (code[21][cycles] << 1) | (code[22][cycles] << 2) | (code[23][cycles] << 3) |
                   (code[24][cycles] << 4));
    imm11_5[cycles] = (code[25][cycles] | (code[26][cycles] << 1) | (code[27][cycles] << 2) | (code[28][cycles] << 3) |
                       (code[29][cycles] << 4) | (code[30][cycles] << 5) |
                       (code[31][cycles] << 6)) << 5;
    fct7[cycles] = code[30][cycles];
    imm11_0[cycles] = rs2[cycles] | imm11_5[cycles];
    imm10_5[cycles] = imm11_5[cycles] & 0b0111'1111'1111u;
    imm12[cycles] = (code[31][cycles] << 12);
    imm19_12[cycles] = (fct3[cycles] | (rs1[cycles] << 3)) << 12;
    imm31_12[cycles] = imm19_12[cycles] | (imm11_0[cycles] << 20);
    i11[cycles] = code[20][cycles] << 11;
    imm10_1[cycles] = (code[21][cycles] | (code[22][cycles] << 1) | (code[23][cycles] << 2) | (code[24][cycles] << 3) |
                       (code[25][cycles] << 4) | (code[26][cycles] << 5) |
                       (code[27][cycles] << 6) | (code[28][cycles] << 7) | (code[29][cycles] << 8) |
                       (code[30][cycles] << 9)) << 1;
    imm20[cycles] = code[31][cycles] << 20;

    immI[cycles] = SignExtend(imm11_0[cycles], 12);
    immS[cycles] = SignExtend(imm11_5[cycles] | imm4_0[cycles], 12);
    immB[cycles] = SignExtend(imm12[cycles] | imm11[cycles] | imm10_5[cycles] | imm4_1[cycles], 13);
    immU[cycles] = imm31_12[cycles];
    immJ[cycles] = SignExtend(imm20[cycles] | imm19_12[cycles] | i11[cycles] | imm10_1[cycles], 21);

    regrs1[cycles] = reg[rs1[cycles]];
    regrs2[cycles] = reg[rs2[cycles]];
    if (cycles > 1 &&
        (opcode[cycles - 2] != SB && opcode[cycles - 2] != SH &&
         opcode[cycles - 2] != SW && opcode[cycles - 2] != LB &&
         opcode[cycles - 2] != LH && opcode[cycles - 2] != LW &&
         opcode[cycles - 2] != BEQ && opcode[cycles - 2] != BNE &&
         opcode[cycles - 2] != BLT && opcode[cycles - 2] != BGE &&
         opcode[cycles - 2] != LBU && opcode[cycles - 2] != LHU &&
         opcode[cycles - 2] != BLTU && opcode[cycles - 2] != BGEU)
            ) {
        if (rs2[cycles] == rd[cycles - 2]) regrs2[cycles] = tmpReg[rd[cycles - 2]];
        if (rs1[cycles] == rd[cycles - 2]) regrs1[cycles] = tmpReg[rd[cycles - 2]];
    }
    if (cycles > 0 &&
        (opcode[cycles - 1] != SB && opcode[cycles - 1] != SH &&
         opcode[cycles - 1] != SW && opcode[cycles - 1] != LB &&
         opcode[cycles - 1] != LH && opcode[cycles - 1] != LW &&
         opcode[cycles - 1] != BEQ && opcode[cycles - 1] != BNE &&
         opcode[cycles - 1] != BLT && opcode[cycles - 1] != BGE &&
         opcode[cycles - 1] != LBU && opcode[cycles - 1] != LHU &&
         opcode[cycles - 1] != BLTU && opcode[cycles - 1] != BGEU)
            ) {
        if (rs1[cycles] == rd[cycles - 1]) regrs1[cycles] = tmpReg[rd[cycles - 1]];
        if (rs2[cycles] == rd[cycles - 1]) regrs2[cycles] = tmpReg[rd[cycles - 1]];
    }
//    EXECUTE
    switch (opcode[cycles]) {
        //0110111 LUI
        case 55:
            option[cycles] = LUI;
            break;
            //0010111 AUIPC
        case 23:
            option[cycles] = AUIPC;
            break;
            //1101111 JAL
        case 111:
            option[cycles] = JAL;
            break;
            //1100111 JALR
        case 103:
            option[cycles] = JALR;
            break;
            //1100011 - 000 BEQ, 001 BNE, 100 BLT, 101 BGE, 110 BLTU, 111 BGEU;
        case 99:
            if (fct3[cycles] == 0) {
                option[cycles] = BEQ;
                break;
            } else if (fct3[cycles] == 1) {
                option[cycles] = BNE;
                break;
            } else if (fct3[cycles] == 4) {
                option[cycles] = BLT;
                break;
            } else if (fct3[cycles] == 5) {
                option[cycles] = BGE;
                break;
            } else if (fct3[cycles] == 6) {
                option[cycles] = BLTU;
                break;
            } else if (fct3[cycles] == 7) {
                option[cycles] = BGEU;
                break;
            }
            //0000011 - 000 LB, 001 LH, 010 LW, 100 LBU, 101 LHU;
        case 3:
            insertBubble();
            if (fct3[cycles] == 0) {
                option[cycles] = LB;
                break;
            } else if (fct3[cycles] == 1) {
                option[cycles] = LH;
                break;
            } else if (fct3[cycles] == 2) {
                option[cycles] = LW;
                break;
            } else if (fct3[cycles] == 4) {
                option[cycles] = LBU;
                break;
            } else if (fct3[cycles] == 5) {
                option[cycles] = LHU;
                break;
            }
            //0100011 - 000 SB, 001 SH, 010 SW;
        case 35:
            insertBubble();
            if (fct3[cycles] == 0) {
                option[cycles] = SB;
                break;
            } else if (fct3[cycles] == 1) {
                option[cycles] = SH;
                break;
            } else if (fct3[cycles] == 2) {
                option[cycles] = SW;
                break;
            }
            //0010011 - 000 ADDI, 010 SLTI, 011 SLTIU, 100 XORI, 110 ORI, 111 ANDI, 001 SLLI, 101-0 SRLI, 101-1 SRAI;
        case 19:
            if (fct3[cycles] == 0) {
                option[cycles] = ADDI;
                break;
            } else if (fct3[cycles] == 2) {
                option[cycles] = SLTI;
                break;
            } else if (fct3[cycles] == 3) {
                option[cycles] = SLTIU;
                break;
            } else if (fct3[cycles] == 4) {
                option[cycles] = XORI;
                break;
            } else if (fct3[cycles] == 6) {
                option[cycles] = ORI;
                break;
            } else if (fct3[cycles] == 7) {
                option[cycles] = ANDI;
                break;
            } else if (fct3[cycles] == 1) {
                option[cycles] = SLLI;
                break;
            } else if (fct3[cycles] == 5 && !fct7[cycles]) {
                option[cycles] = SRLI;
                break;
            } else if (fct3[cycles] == 5 && fct7[cycles]) {
                option[cycles] = SRAI;
                break;
            }
            //0110011 - 000-0 ADD, 000-1 SUB, 001 SLL, 010 SLT, 011 SLTU, 100 XOR, 101-0 SRL, 101-1 SRA, 110 OR, 111 AND;
        case 51:
            if (fct3[cycles] == 0 && !fct7[cycles]) {
                option[cycles] = ADD;
                break;
            } else if (fct3[cycles] == 0 && fct7[cycles]) {
                option[cycles] = SUB;
                break;
            } else if (fct3[cycles] == 1) {
                option[cycles] = SLL;
                break;
            } else if (fct3[cycles] == 2) {
                option[cycles] = SLT;
                break;
            } else if (fct3[cycles] == 3) {
                option[cycles] = SLTU;
                break;
            } else if (fct3[cycles] == 4) {
                option[cycles] = XOR;
                break;
            } else if (fct3[cycles] == 5 && !fct7[cycles]) {
                option[cycles] = SRL;
                break;
            } else if (fct3[cycles] == 5 && fct7[cycles]) {
                option[cycles] = SRA;
                break;
            } else if (fct3[cycles] == 6) {
                option[cycles] = OR;
                break;
            } else if (fct3[cycles] == 7) {
                option[cycles] = AND;
                break;
            }
            regZero();
    }
//    switch(opcode[cycles]) {
//        case BEQ:
//        case BNE:
//        case BLT:
//        case BGE:
//        case BLTU:
//        case BGEU:
//        case SB:
//        case SH:
//        case SW:
//            return;
//        default:
//            rd[cycles] = jump;
//    }
}

void EXE(int cycles) {
    regZero();
    if (cycles < 0) return;
    if (inst[cycles] == 1) return;
    switch (option[cycles]) {
        case LUI:
            tmpReg[rd[cycles]] = resEXE[cycles] = immU[cycles];
            break;
        case AUIPC:
            tmpReg[rd[cycles]] = resEXE[cycles] = pc + immU[cycles] - 4;
            break;
        case JAL:
            tmpReg[rd[cycles]] = resEXE[cycles] = pc;
            pc += immJ[cycles];
            pc -= 4;
            break;
        case JALR:
            tmpReg[rd[cycles]] = resEXE[cycles] = pc;
            pc = (regrs1[cycles] + immI[cycles]) & ~(unsigned int) 1;
            break;
        case BEQ:
            if (regrs1[cycles] == regrs2[cycles]) {
                pc += immB[cycles];
                pc -= 4;
            }
            break;
        case BNE:
            if (regrs1[cycles] != regrs2[cycles]) {
                pc += immB[cycles];
                pc -= 4;
            }
            break;
        case BLT:
            if ((int) regrs1[cycles] < (int) regrs2[cycles]) {
                pc += immB[cycles];
                pc -= 4;
            }
            break;
        case BGE:
            if ((int) regrs1[cycles] >= (int) regrs2[cycles]) {
                pc += immB[cycles];
                pc -= 4;
            }
            break;
        case BLTU:
            if (regrs1[cycles] < regrs2[cycles]) {
                pc += immB[cycles];
                pc -= 4;
            }
            break;
        case BGEU:
            if (regrs1[cycles] >= regrs2[cycles]) {
                pc += immB[cycles];
                pc -= 4;
            }
            break;
        case ADDI:
            tmpReg[rd[cycles]] = resEXE[cycles] = regrs1[cycles] + immI[cycles];
            break;
        case SLTI:
            tmpReg[rd[cycles]] = resEXE[cycles] = (int) regrs1[cycles] < (int) immI[cycles];
            break;
        case SLTIU:
            tmpReg[rd[cycles]] = resEXE[cycles] = regrs1[cycles] < immI[cycles];
            break;
        case XORI:
            tmpReg[rd[cycles]] = resEXE[cycles] = regrs1[cycles] ^ immI[cycles];
            break;
        case ORI:
            tmpReg[rd[cycles]] = resEXE[cycles] = regrs1[cycles] | immI[cycles];
            break;
        case ANDI:
            tmpReg[rd[cycles]] = resEXE[cycles] = regrs1[cycles] & immI[cycles];
            break;
        case SLLI:
            tmpReg[rd[cycles]] = resEXE[cycles] = regrs1[cycles] << (immI[cycles] & 31u);
            break;
        case SRLI:
            tmpReg[rd[cycles]] = resEXE[cycles] = regrs1[cycles] >> (immI[cycles] & 31u);
            break;
        case SRAI:
            tmpReg[rd[cycles]] = resEXE[cycles] = SignExtend(regrs1[cycles] >> (immI[cycles] & 31u),
                                                             32 - (immI[cycles] & 31u));
            break;
        case ADD:
            tmpReg[rd[cycles]] = resEXE[cycles] = regrs1[cycles] + regrs2[cycles];
            break;
        case SUB:
            tmpReg[rd[cycles]] = resEXE[cycles] = regrs1[cycles] - regrs2[cycles];
            break;
        case SLL:
            tmpReg[rd[cycles]] = resEXE[cycles] = regrs1[cycles] << (regrs2[cycles] & 31u);
            break;
        case SLT:
            tmpReg[rd[cycles]] = resEXE[cycles] = (int) regrs1[cycles] < (int) regrs2[cycles];
            break;
        case SLTU:
            tmpReg[rd[cycles]] = resEXE[cycles] = regrs1[cycles] < regrs2[cycles];
            break;
        case XOR:
            tmpReg[rd[cycles]] = resEXE[cycles] = regrs1[cycles] ^ regrs2[cycles];
            break;
        case SRL:
            tmpReg[rd[cycles]] = resEXE[cycles] = regrs1[cycles] >> (regrs2[cycles] & 31u);
            break;
        case SRA:
            tmpReg[rd[cycles]] = resEXE[cycles] = SignExtend(regrs1[cycles] >> (regrs2[cycles] & 31u),
                                                             32 - (regrs2[cycles] & 31u));
            break;
        case OR:
            tmpReg[rd[cycles]] = resEXE[cycles] = regrs1[cycles] | regrs2[cycles];
            break;
        case AND:
            tmpReg[rd[cycles]] = resEXE[cycles] = regrs1[cycles] & regrs2[cycles];
            break;
        case LB:
        case LH:
        case LW:
        case LBU:
        case LHU:
        case SB:
        case SH:
        case SW:
            break;
    }
    regZero();
}

void MEM(int cycles) {
    regZero();
    if (cycles < 0) return;
    if (inst[cycles] == 1 || inst[cycles] == 0) return;
    switch (option[cycles]) {
        //load
        case LB:
            mem_wb[cycles].load = SignExtend(mem[regrs1[cycles]] + immI[cycles], 8);
            break;
        case LH:
            mem_wb[cycles].load = SignExtend((unsigned int) mem[regrs1[cycles] + immI[cycles]] |
                                             (unsigned int) mem[regrs1[cycles] + immI[cycles] + 1] << 8, 16);
            break;
        case LW:
            mem_wb[cycles].load = (unsigned int) mem[regrs1[cycles] + immI[cycles]] |
                                  (unsigned int) mem[regrs1[cycles] + immI[cycles] + 1] << 8 |
                                  (unsigned int) mem[regrs1[cycles] + immI[cycles] + 2] << 16 |
                                  (unsigned int) mem[regrs1[cycles] + immI[cycles] + 3] << 24;
            break;
        case LBU:
            mem_wb[cycles].load = mem[regrs1[cycles] + immI[cycles]];
            break;
        case LHU:
            mem_wb[cycles].load = (unsigned int) mem[regrs1[cycles] + immI[cycles]] |
                                  (unsigned int) mem[regrs1[cycles] + immI[cycles] + 1] << 8;
            break;
            //store
        case SB:
            mem[regrs1[cycles] + immS[cycles]] = (unsigned char) regrs2[cycles];
            break;
        case SH:
            mem[regrs1[cycles] + immS[cycles]] = (unsigned char) regrs2[cycles];
            mem[regrs1[cycles] + immS[cycles] + 1] = (unsigned char) (regrs2[cycles] >> 8);
            break;
        case SW:
            mem[(int) regrs1[cycles] + (int) immS[cycles] + 0] = (unsigned char) regrs2[cycles];
            mem[(int) regrs1[cycles] + (int) immS[cycles] + 1] = (unsigned char) (regrs2[cycles] >> 8);
            mem[(int) regrs1[cycles] + (int) immS[cycles] + 2] = (unsigned char) (regrs2[cycles] >> 16);
            mem[(int) regrs1[cycles] + (int) immS[cycles] + 3] = (unsigned char) (regrs2[cycles] >> 24);
            break;
        default:
            return;
    }
}

void de_bug() {
    cout << "reg_test : " << " ";
    for (int i = 0; i < 32; ++i) {
        cout << reg[i] << " ";
    }
    cout << endl;
//    printf("inst: %08X", inst[CYCLES]);
//    cout << endl;
}

void WB(int cycles) {
    regZero();
    if (inst[cycles] == 1) return;
    if (cycles < 0) return;
    switch (option[cycles]) {
        case BEQ:
        case BNE:
        case BLT:
        case BGE:
        case BLTU:
        case BGEU:
        case SB:
        case SH:
        case SW:
            return;
        case LB:
        case LH:
        case LW:
        case LBU:
        case LHU:
            reg[rd[cycles]] = mem_wb[cycles].load;
            regZero();
//            de_bug();
//            cout << CYCLES << endl;
            return;
        default:
            reg[rd[cycles]] = resEXE[cycles];
            regZero();
//            de_bug();
//            cout << CYCLES << endl;

//            cout << CYCLES << endl;
            return;
    }
}

int cnt = 0;

int main() {
//    freopen("testcases_for_riscv 2/testcases/pi.data", "r", stdin);
//    freopen("test.data", "w", stdout);
    readle(std::cin);
    while (true) {
        CYCLES++;
//        cout << ++cnt;
        if (CYCLES >= 4 && ((isWB--) < 0 || (isWB + 1) > 1))
            WB(CYCLES - 4);
        if (CYCLES >= 3 && ((isMEM--) < 0 || (isMEM + 1) > 1))
            MEM(CYCLES - 3);
        if (CYCLES >= 2 && ((isEXE--) < 0 || (isEXE + 1) > 1))
            EXE(CYCLES - 2);
        if (CYCLES >= 1 && ((isID--) < 0 || (isID + 1) > 1))
            ID(CYCLES - 1);
        if (CYCLES >= 0 && ((isIF--) < 0 || (isIF + 1) > 1))
            if (!IF(CYCLES)) break;
        reg[0] = 0;
    }
    return 0;
}
