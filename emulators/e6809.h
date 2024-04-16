#ifndef __E6809_H
#define __E6809_H

#include <stdint.h>

extern FILE *logfile;

/* user defined read and write functions */
extern unsigned char e6809_read8(unsigned address);
extern void e6809_write8(unsigned address, unsigned char data);

void e6809_reset (uint16_t sp, uint16_t pc);
uint16_t e6809_get_pc(void);
void e6809_set_pc(uint16_t pc);
const char *e6809_get_flagstr(void);
void e6809_get_statestr(char *buffer);
unsigned e6809_sstep (unsigned irq_i, unsigned irq_f);

struct reg6809 {
    uint16_t pc;
    uint16_t x,y,u,s;
    uint8_t a,b,dp,cc;
};

struct reg6809 *e6809_get_regs(void);

#endif
