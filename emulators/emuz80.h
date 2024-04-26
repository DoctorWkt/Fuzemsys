/* emuz80.c */
uint8_t mem_read(int unused, uint16_t addr);
void mem_write(int unused, uint16_t addr, uint8_t val);
uint8_t io_read(int unused, uint16_t port);
void io_write(int unused, uint16_t port, uint8_t value);
uint8_t z80dis_byte(uint16_t addr);
uint8_t z80dis_byte_quiet(uint16_t addr);

extern Z80Context cpu_z80;
