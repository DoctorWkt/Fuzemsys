/* mapfile.c */
void read_mapfile(char *filename);
char *get_symbol_and_offset(unsigned int addr, int *offset);
extern int mapfile_loaded;
