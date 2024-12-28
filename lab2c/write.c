#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>


#define RED	"/dev/shofer"
#define MAXSZ	64

int fp;

void my_signal_handler(int sig)
{
	printf("Closing fd %d\n", fp);
	close(fp);
	printf("EXITING");
	exit(0);
}


void gen_text(char *text, size_t size)
{
	int i;

	for (i = 0; i < size; i++)
	{
		text[i] = 'A' + random() % ('Z' - 'A');
		if(i % 8 == 0)
			text[i] = ' ';	
	}
}

int main(int argc, char *argv[])
{
	char buffer[MAXSZ];
	size_t size;
	long pid = (long) getpid();

	struct sigaction sa = {{0}};
    sa.sa_handler = &my_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) != 0)
    {
        fprintf(stderr, "sigaction error\n");
        return -1;
    }

	fp = open(RED, O_WRONLY);
	if (fp == -1) {
		perror("Nisam otvorio cjevovod! Greska: ");
		return -1;
	}

	srandom(pid);
	while(1) {
		memset(buffer, 0, MAXSZ);
		size = MAXSZ / 3 + random() % (2 * MAXSZ / 3);
		gen_text(buffer, size);
		printf("Pisac %ld poziva write s tekstom (%ld): %s\n", pid, size, buffer);
		size = write(fp, buffer, size);
		if (size == -1) {
			perror("Greska pri pisanju! Greska: ");
			break;
		}
		printf("Pisac %ld poslao (%ld): %s\n", pid, size, buffer);
		sleep(2);
	}

	return -1;
}
