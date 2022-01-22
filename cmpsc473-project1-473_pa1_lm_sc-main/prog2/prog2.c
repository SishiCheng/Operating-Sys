#include <stdio.h>
#include <stdlib.h>

void allocate(int count)
{
  int x[300000];
  char *c;

  if(count >= 0)
  {
    c = malloc (30000);
    printf("Stack Address  = %p    Heap Address = %p\n\n",x,c);
    allocate(count - 1);
  }
}

int main(int argc, char const *argv[]) {
  int count = 10;

  printf("Entering Function \n\n");
  allocate(count);
  printf("Press Enter to continue\n");
  getchar();
  printf("Program Terminated Successfully\n");
  return 0;
}
