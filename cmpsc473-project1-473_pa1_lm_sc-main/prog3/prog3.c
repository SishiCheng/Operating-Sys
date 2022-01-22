#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

void allocate(int count,int r)
{
  int x[100];
  char *b;
  r++;
  if(count >= 0)
  {
    b = malloc(pow(10,r)*sizeof(int));
    for (int i = 0 ;i<sizeof(b) ; i++)
    {
      char *ch1;
      ch1 = &b[i];
      memset(ch1,'*',sizeof(b[0])+i);
    }

    printf("Stack Address = %p     Heap Address = %p  \n\n",x,b);
    allocate(count-1,r);
  }
}

int main(int argc, char const *argv[]) {
  int count = 20;
  int r = 0;
  
  printf("Entering Function\n\n");
  allocate(count,r);
  printf("Program Terminated Successfully\n");
  return 0;
}
