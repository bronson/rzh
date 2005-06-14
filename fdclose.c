// let's see what fds are open by default.
// Apparently gdb changes this significantly.

#include <stdio.h>
#include <unistd.h>

int main()
{
	int i, n;

	for(i=2; i<128; i++) {
		n = close(i);
		printf("%d: %d\n", i, n);
		if(n == -1) {
			if(i > 16) {
				break;
			}
		}
	}

	return 0;
}
