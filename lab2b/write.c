/* poll_input.c
Licensed under GNU General Public License v2 or later.
*/
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
			} while (0)

int main(int argc, char *argv[])
{
	int            ready;
	char           buf[10] = "HELLO1234";
  	ssize_t        s;
	struct pollfd  pfds;


    pfds.fd = open("/dev/shofer_in", O_WRONLY);
    if (pfds.fd == -1)
                errExit("open");

    printf("Opened /dev/shofer_in on fd %d\n", pfds.fd);

    pfds.events = POLLOUT;

    while(1)
    {
        printf("About to poll()\n");
        ready = poll(&pfds, 1, -1);
        if (ready == -1)
                errExit("poll");

        printf("Ready: %d\n", ready);

        if (pfds.revents != 0) {
            printf("  fd=%d; events: %s%s%s\n", pfds.fd,
                    (pfds.revents & POLLOUT)  ? "POLLOUT "  : "",
                    (pfds.revents & POLLHUP) ? "POLLHUP " : "",
                    (pfds.revents & POLLERR) ? "POLLERR " : "");

            if (pfds.revents & POLLOUT) {
                s = write(pfds.fd, buf, sizeof(buf));
                if (s == -1)
                    errExit("write");
                printf("    write %zd bytes: %.*s\n",
                        s, (int) s, buf);
            } else {                /* POLLERR | POLLHUP */
                printf("    closing fd %d\n", pfds.fd);
                if (close(pfds.fd) == -1)
                    errExit("close");
                goto end;
            }
        }
        sleep(5);
    }
    
end:
	printf("File descriptor closed; bye\n");
	exit(EXIT_SUCCESS);
}
