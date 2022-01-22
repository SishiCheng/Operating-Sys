#include <stdio.h>
#include <stdlib.h>

void address_print (int *count) {
	if (*count >= 10) {
		return;
	} else {
		int a[49500];
		char *b;

		b = (char *) malloc (500 * sizeof(char));

		printf ("Address 1 = %p    Address 2 = %p\n", a, b);

		*count += 1;

		address_print (count);
	}
}

int main(int argc, char const *argv[])
{
	int count = 0;

	address_print (&count);

	printf("Enter any key to exit\n");
	getchar();

	return 0;
}
