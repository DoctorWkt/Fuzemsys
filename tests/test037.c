#include <stdio.h>

int main() {
  int fred[6];
  char jim[6];
  char *ptr= jim;


  fred[0]= 16; printf("fred[0] is %d\n", fred[0]);
  ptr[0]= 16;  printf("ptr[0]  is %d\n",  ptr[0]);
  fred[2]= 11; printf("fred[2] is %d\n", fred[2]);
  ptr[2]= 11;  printf("ptr[2]  is %d\n",  ptr[2]);
  fred[3]= 23; printf("fred[3] is %d\n", fred[3]);
  ptr[3]= 23;  printf("ptr[3]  is %d\n",  ptr[3]);
  fred[3]= 0; printf("fred[3] is %d\n", fred[3]);
  ptr[3]=  0; printf("ptr[3]  is %d\n",  ptr[3]);
  printf("fred[0] is %d\n", fred[0]);
  printf("ptr[0]  is %d\n",  ptr[0]);
  printf("fred[2] is %d\n", fred[2]);
  printf("ptr[2]  is %d\n",  ptr[2]);
  return(0);
}
