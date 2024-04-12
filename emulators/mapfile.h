/* mapfile.c */
void read_mapfile(char *filename);
int get_sym_address(char *sym);
char *get_symbol_and_offset(unsigned int addr, int *offset);
extern int mapfile_loaded;
