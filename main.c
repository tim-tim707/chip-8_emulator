#include <stdlib.h> // system()
#include <unistd.h> // sleep()
#include <stdio.h> // printf()
#include <time.h> // time() for random
#include <stdint.h> // uintx_t
#include <err.h> // errx()

// Terminal colors for pretty print
#define BLK   "\x1B[40m"
#define WHT   "\x1B[47m"
#define RED   "\x1B[41m"
#define GRN   "\x1B[42m"
#define RESET "\x1B[0m"

// Chip constants
#define MEM_SIZE        0x2000
#define PROGRAM_START   0x0000 // is 0x0200 usually
#define STACK_SIZE      0x10
#define NB_REG          0x10
#define FLAGS           (NB_REG - 1)

#define VIDEO_START     0x1E00
#define VIDEO_SIZE      (MEM_SIZE - VIDEO_START)
#define VIDEO_WIDTH     0x10
#define VIDEO_HEIGHT    (VIDEO_SIZE / VIDEO_WIDTH)

typedef uint8_t byte;
typedef uint16_t word;

typedef struct CHIP {
    byte mem[MEM_SIZE];

    // 1bit pixel, from 0,0 to 63, 31

    word pc;

    byte sp;
    word stack[STACK_SIZE];

    // registers
    word I; // index register = void pointer
    byte reg[NB_REG];
    // note : the vF register is reserved for flags

}chip;

byte read_byte(chip *c)
{
    return c->mem[c->pc++];
}

word read_word(chip *c)
{
    byte first = read_byte(c);
    byte second = read_byte(c);

    return ( (word)first<<8 ) + second;
}

byte first_byte(word w)
{
    return w & 0x00FF;
}

byte first_reg(word w)
{
    return (w & 0x0F00) >> 8;
}

byte second_reg(word w)
{
    return (w & 0x00F0) >> 4;
}

void print_bit(byte bit)
{
    if(bit)
        printf(WHT "%d" RESET, bit);
    else
        printf(" ");
}

void print_byte(byte b)
{
    print_bit(b>>7);
    print_bit((b & 0b01000000) >> 6);
    print_bit((b & 0b00100000) >> 5);
    print_bit((b & 0b00010000) >> 4);
    print_bit((b & 0b00001000) >> 3);
    print_bit((b & 0b00000100) >> 2);
    print_bit((b & 0b00000010) >> 1);
    print_bit((b & 0b00000001));
}

void print_screen(chip *c)
{
    for(unsigned int y = 0; y < VIDEO_HEIGHT; y++)
    {
        for(unsigned int x = 0; x < VIDEO_WIDTH; x++)
        {
            print_byte(c->mem[VIDEO_START + x + y*VIDEO_WIDTH]);
        }
        printf("\n");
    }
}

void fill_screen(chip *c, byte value)
{
    for(unsigned int i = VIDEO_START; i < MEM_SIZE; i++)
        c->mem[i] = value;
}

void print_mem(chip *c)
{
    for(unsigned int i = 0; i < MEM_SIZE; i++)
    {
        if(i == PROGRAM_START)
            printf(RED "\nPROGRAM START\n" RESET);
        if(i == VIDEO_START)
            printf(GRN "\nVIDEO START\n" RESET);
        printf("%02X ", c->mem[i]);
    }
    printf("\n");
}

void print_chip(chip *c)
{
    printf("PC : %X, I : %X, SP : %X\n", c->pc, c->I, c->sp);

    for(unsigned int i = 0; i < NB_REG; i++)
        printf("V%x : %d ", i, c->reg[i]);
    printf("\n");

    for(unsigned int i = 0; i < STACK_SIZE; i++)
        printf("Stack level %d: %X\n",
                STACK_SIZE - 1 - i, c->stack[STACK_SIZE - 1 - i]);
    printf("\n");
}

void CLS(chip *c)
{
    system("clear");
    fill_screen(c, 0);
    print_screen(c);
}

void RET(chip *c)
{
    c->pc = c->stack[c->sp--];
}

void SYS(chip *c, word instruction)
{
    word result = instruction & 0x0FFF;

    if(result >= MEM_SIZE)
    {
        printf("PC out of bound. previous pc : %d\n", c->pc);
        errx(EXIT_FAILURE, "PC out of bound");
    }

    c->pc = result;
}

void JP(chip *c, word instruction)
{
    SYS(c, instruction);
}

void CALL(chip *c, word instruction)
{
    if(c->sp == STACK_SIZE - 1)
        errx(EXIT_FAILURE, "STACK_OVERFLOW pc : %X", c->pc);

    c->sp++;
    c->stack[c->sp] = c->pc;
    SYS(c, instruction);
}

void SEI(chip *c, word instruction)
{
    if(c->reg[first_reg(instruction)] == first_byte(instruction) )
        c->pc++;
}

void SNEI(chip *c, word instruction)
{
    if( !(c->reg[first_reg(instruction)] == first_byte(instruction)) )
        c->pc++;
}
void SE(chip *c, word instruction)
{
    if( c->reg[first_reg(instruction)] == c->reg[second_reg(instruction)] )
        c->pc++;
}

void SNE(chip *c, word instruction)
{
    if( !(c->reg[first_reg(instruction)] == c->reg[second_reg(instruction)]) )
        c->pc++;
}

void LD(chip *c, word instruction)
{
    c->reg[first_reg(instruction)] == first_byte(instruction);
}

void ADDI(chip *c, word instruction)
{
    c->reg[first_reg(instruction)] += first_byte(instruction);
}

void ALU(chip *c, word instruction)
{
    byte last_four_bits = instruction & 0x000F;
    byte regx = first_reg(instruction);
    byte regy = second_reg(instruction);

    switch(last_four_bits)
    {
        case 0:
            c->reg[regx] = c->reg[regy];
            break;
        case 1:
            c->reg[regx] |= c->reg[regy];
            break;
        case 2:
            c->reg[regx] &= c->reg[regy];
            break;
        case 3:
            c->reg[regx] ^= c->reg[regy];
            break;
        case 4: ;
            byte result = c->reg[regx]
                + c->reg[regy];
            if(result > 255)
                c->reg[FLAGS] = 1;
            else
                c->reg[FLAGS] = 0;
            c->reg[regx] = result;
            break;
        case 5:
            if(c->reg[regx] > c->reg[regy])
                c->reg[FLAGS] = 1;
            else
                c->reg[FLAGS] = 0;
            c->reg[regx] = c->reg[regx] - c->reg[regy];
            break;
        case 6:
            if( (c->reg[regx] & 0x0001) == 1)
                c->reg[FLAGS] = 1;
            else
                c->reg[FLAGS] = 0;
            c->reg[regx] >>= 1;
            break;
        case 7:
            if(c->reg[regx] < c->reg[regy])
                c->reg[FLAGS] = 1;
            else
                c->reg[FLAGS] = 0;
            c->reg[regx] = c->reg[regy] - c->reg[regx];
            break;
        case 14:
            if( (c->reg[regx] >> 7) == 1)
                c->reg[FLAGS] = 1;
            else
                c->reg[FLAGS] = 0;
            c->reg[regx] <<= 1;
            break;
        default:
            errx(EXIT_FAILURE, "Instruction not parsed %X at pc : %X",
                    instruction, c->pc);
    }
}

void LDP(chip *c, word instruction)
{
    c->I = instruction & 0x0FFF;
}

void JPR(chip *c, word instruction)
{
    c->pc = instruction & 0x0FFF + c->reg[0];
}

void RND(chip *c, word instruction)
{
    byte regx = first_reg(instruction);
    c->reg[regx] = first_byte(instruction) & (byte)rand();
}

void DRW(chip *c, word instruction)
{
    printf("DRW\n");
}

void SKP(chip *c, word instruction)
{
    printf("SKP\n");
}

void SKNP(chip *c, word instruction)
{
    printf("SKNP\n");
}

void ADDP(chip *c, word instruction)
{
    printf("ADDP\n");
}

void LD_write(chip *c, word instruction)
{
    byte nb_registers = first_reg(instruction);
    if(c->I + nb_registers > MEM_SIZE)
        errx(EXIT_FAILURE, "OUT OF MEMORY WRITE pc : %X starting address = %X"
                , c->pc, c->I);
    for(unsigned int i = 0; i <= nb_registers; i++)
        c->mem[c->I + i] = c->reg[i];
}

void LD_read(chip *c, word instruction)
{
    byte nb_registers = first_reg(instruction);
    if(c->I + nb_registers > MEM_SIZE)
        errx(EXIT_FAILURE, "OUT OF MEMORY READ pc : %X starting address = %X"
                , c->pc, c->I);

    for(unsigned int i = 0; i <= nb_registers; i++)
        c->reg[i] = c->mem[c->I + i];
}

void execute_instruction(chip *c) // instruction is located under pc
{
    word instruction = read_word(c);
    byte first_four_bits = instruction>>12;

    switch(first_four_bits)
    {
        case 0:
            if(instruction == 0x00E0)
                CLS(c);
            else if(instruction == 0X00EE)
                RET(c);
            else
                SYS(c, instruction);
            break;
        case 1:
            JP(c, instruction);
            break;
        case 2:
            CALL(c, instruction);
            break;
        case 3:
            SEI(c, instruction);
            break;
        case 4:
            SNEI(c, instruction);
            break;
        case 5:
            SE(c, instruction);
            break;
        case 6:
            LD(c, instruction);
            break;
        case 7:
            ADDI(c, instruction);
            break;
        case 8: ;
                ALU(c, instruction);
                break;
        case 9:
                SNE(c, instruction);
                break;
        case 10:
                LDP(c, instruction);
                break;
        case 11:
                JPR(c, instruction);
                break;
        case 12:
                RND(c, instruction);
                break;
        case 13:
                DRW(c, instruction);
                break;
        case 14:
                if( (instruction & 0x00FF ) == 0x9E)
                    SKP(c, instruction);
                else if( (instruction & 0x00FF) == 0xA1)
                    SKNP(c, instruction);
                else
                    goto default_label;
                break;
        case 15:
                if( (instruction & 0x00FF) == 0x1E)
                    ADDP(c, instruction);
                else if( (instruction & 0x00FF) == 0x55 )
                    LD_write(c, instruction);
                else if( (instruction & 0x00FF) == 0x65)
                    LD_read(c, instruction);
                else
                    goto default_label;
                break;
        default:
default_label:
                errx(EXIT_FAILURE, "Instruction not parsed %X at pc : %X",
                        instruction, c->pc);
                break;
    }
}

void load_program(chip *c, char *program)
{
    unsigned int i = 0;
    while(program[i] != '\0')
    {
        i ++;
    }
}

int main(void)
{
    srand(time(NULL));

    chip c = {
        .mem = {0},
        .pc = PROGRAM_START,
        .sp = 0x00,
        .stack = {0},
        .I = 0,
        .reg = {0}
    };

    print_chip(&c);
    execute_instruction(&c);
    print_chip(&c);
    // printf("VIDEO_WIDTH : %d, VIDEO_HEIGHT : %d\n", VIDEO_WIDTH, VIDEO_HEIGHT);
    return 0;
}
