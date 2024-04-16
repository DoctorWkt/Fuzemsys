// Code to deal with a mapfile.
// (c) 2023 Warren Toomey, GPL3.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int mapfile_loaded = 0;		// Set to 1 if loaded

struct mapentry {
  unsigned int addr;		// Address of symbol
  char *sym;			// Symbol at this address
};

// Eventually, the array of mapentries sorted by address
static struct mapentry *maparray = NULL;

// Index of the most recently used mapentry
// and the count of valid mapentries
static int cidx = 0;
static int mapcnt = 0;

// Compare mapentries by address, for qsort()
static int mapcompare(const void *a, const void *b) {
  struct mapentry *c, *d;

  c = (struct mapentry *) a;
  d = (struct mapentry *) b;
  return (c->addr - d->addr);
}

// Read the code symbols in from the mapfile
// and build the maparray.
void read_mapfile(char *filename) {
  FILE *zin;
  char buf[1024];
  char *sym;
  int i = 0;

  // Free any existing map entries in case
  // we get called multiple times
  if (maparray != NULL) {
    for (i = 0; i < mapcnt; i++)
      free(maparray[i].sym);
    free(maparray);
    cidx=mapcnt=0;
  }

  // To start with, open the file, read in
  // each line and count lines with " C "
  zin = fopen(filename, "r");
  if (zin == NULL) { perror(filename); return; }

  while (1) {
    if (fgets(buf, 1023, zin) == NULL) break;
    if ((strstr(buf, " C ")) != NULL)  mapcnt++;
  }
  fclose(zin);

  // Build the array of map entries.
  // Add room for an extra empty element.
  maparray =
    (struct mapentry *) malloc((mapcnt + 1) * sizeof(struct mapentry));
  if (maparray == NULL) { perror(filename); return; }

  // Now re-read the file, extracting the symbol and address
  zin = fopen(filename, "r");

  while (1) {
    if (fgets(buf, 1023, zin) == NULL) break;
    if ((sym = strstr(buf, " C ")) != NULL) {
      sym += 3;
      // Lose \n on end of symbol
      sym[strlen(sym) - 1] = '\0';
      maparray[i].addr = strtol(buf, NULL, 16);
      maparray[i].sym = strdup(sym);
      i++;
    }
  }
  fclose(zin);

  // Sort the array by address
  qsort(maparray, mapcnt, sizeof(struct mapentry), mapcompare);

  // Add a final entry so we can
  // index one past the end
  maparray[mapcnt].addr = 0;
  maparray[mapcnt].sym = NULL;
  mapfile_loaded = 1;
}

// Given a string, return the address of that symbol
// or -1 if the symbol is not found
int get_sym_address(char *sym) {
  int i;

  if (sym==NULL || *sym=='\0') return(-1);

  for (i = 0; i < mapcnt; i++) {
    if (!strcmp(maparray[i].sym, sym))
      return(maparray[i].addr);
  }
  return(-1);
}

// Given a string, return the end address of
// the symbol, i.e. one below the next symbol
// or -1 if the symbol is not found
int get_sym_end_address(char *sym) {
  int i;

  if (sym==NULL || *sym=='\0') return(-1);

  for (i = 0; i < mapcnt; i++) {
    if (!strcmp(maparray[i].sym, sym))
      return(maparray[i+1].addr - 1);
  }
  return(-1);
}

// Given an address, return a string which
// has the nearest symbol below the address.
// Also return, through a pointer, the offset
// of the address fom the symbol.
// If NULL is returned, there is is no nearest symbol.
char *get_symbol_and_offset(unsigned int addr, int *offset) {

  // No symbols
  if (mapcnt == 0) return (NULL);

  // Error check
  if (offset == NULL) return (NULL);

  // cidx points at the last symbol used. If the address
  // is at/between this symbol and the next one, use it
  if ((maparray[cidx].addr <= addr) && (maparray[cidx + 1].addr > addr)) {
    *offset = addr - maparray[cidx].addr;
    return (maparray[cidx].sym);
  }

  // No luck. Search the whole list to find a suitable symbol
  for (cidx = 0; cidx < mapcnt; cidx++) {
    if ((maparray[cidx].addr <= addr) && (maparray[cidx + 1].addr > addr)) {
      *offset = addr - maparray[cidx].addr;
      return (maparray[cidx].sym);
    }
  }

  // No symbol found, give up
  return (NULL);
}
