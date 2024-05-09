/* find.c */
int REmatch(char *pattern, int start, int end);
int omatch(char *pattern, char **cp, char *endp);
int amatch(char *pattern, char *start, char *endp);
int patsize(char **pattern);
int locate(char *pattern, char *linep);
int concatch(char c);
char esc(char **s);
char *dodash(char *src);
char *badccl(char *src);
char *makepat(char *string, char delim);
int findfwd(char *pattern, int start, int endp);
int findback(char *pattern, int start, int endp);
char *search(char *pat, int *start);
char *findparse(char *src, int *idx, int start);
int nextline(int advance, int dest, int count);
int fseekeol(int origin);
int bseekeol(int origin);
int lvgetcontext(char c, int begline);
