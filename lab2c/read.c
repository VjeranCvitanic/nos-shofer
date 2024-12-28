#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>


#define CIJEV	"/dev/shofer"
#define MAXSZ	16

int fp;

void my_signal_handler(int sig)
{
	printf("Closing fd\n");
	close(fp);
	printf("EXITING");
	exit(0);
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

	fp = open(CIJEV, O_RDONLY | O_NONBLOCK);
	if (fp == -1) {
		perror("Nisam otvorio cjevovod! Greska: ");
		return -1;
	}

	while(1) {
		memset(buffer, 0, MAXSZ);
		printf("Citac %ld poziva read\n", pid);
		size = read(fp, buffer, MAXSZ);
		if (size == -1) {
			perror("Greska pri citanju! Greska: ");
			break;
		}
		printf("Citac %ld procitao (%ld): %s\n", pid, size, buffer);
		sleep(1);
	}

	return -1;
}

