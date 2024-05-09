/* move.c */
char findCP(int curp, int *newpos, char cmd);
int movearound(char cmd);
int findcol(int ip, int col);
int match(int p);
char *class(char c);
int skip(char *chars, int cp, int step);
int tow(int cp, int step);
int moveword(int cp, int forwd, int toword);
int fchar(int pos, int npos);
int bchar(int pos, int npos);
int ahead(int i);
int back(int i);
int sentence(int start, int forwd);
