/* poll_input.c

    Licensed under GNU General Public License v2 or later.
*/
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

int choose_random_fd(int* ready_pfds, int nfds);

int main(int argc, char *argv[])
{
    int            ready;
    char           buf[10] = "HELLO";
    nfds_t         num_open_fds, nfds;
    int ret_val = 0;
    ssize_t s;

    struct pollfd  *pfds;
    int *ready_pfds;
    int *null_ready_pfds;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s file...\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    num_open_fds = nfds = argc - 1;
    pfds = calloc(nfds, sizeof(struct pollfd));
    if (pfds == NULL)
        errExit("malloc");
    ready_pfds = calloc(nfds, sizeof(int));
    if (ready_pfds == NULL)
        errExit("malloc");
    null_ready_pfds = calloc(nfds, sizeof(int));
    if (null_ready_pfds == NULL)
        errExit("malloc");

    /* Open each file on command line, and add it to 'pfds' array. */

    for (nfds_t j = 0; j < nfds; j++) {
        pfds[j].fd = open(argv[j + 1], O_WRONLY);
        if (pfds[j].fd == -1)
            errExit("open");

        printf("Opened \"%s\" on fd %d\n", argv[j + 1], pfds[j].fd);

        pfds[j].events = POLLOUT;
    }

    /* Keep calling poll() as long as at least one file descriptor is
        open. */

    while (num_open_fds > 0) {
        printf("About to poll()\n");
        ready = poll(pfds, nfds, -1);
        if (ready == -1)
            errExit("poll");

        printf("Ready: %d\n", ready);

        /* Deal with array returned by poll(). */

        for (nfds_t j = 0; j < nfds; j++) {
            if (pfds[j].revents != 0) {
                printf("  fd=%d; events: %s%s%s\n", pfds[j].fd,
                        (pfds[j].revents & POLLOUT)  ? "POLLOUT "  : "",
                        (pfds[j].revents & POLLHUP) ? "POLLHUP " : "",
                        (pfds[j].revents & POLLERR) ? "POLLERR " : "");

                if (pfds[j].revents & POLLOUT) {
                    ready_pfds[j] = 1;
                } else {                /* POLLERR | POLLHUP */
                    printf("    closing fd %d\n", pfds[j].fd);
                    if (close(pfds[j].fd) == -1)
                        errExit("close");
                    num_open_fds--;
                }
            }
        }
        ret_val = choose_random_fd(ready_pfds, nfds);
        if(ret_val == -1)
            perror("RetVAl = -1");
        else
        {
            printf("ret_Val: %d", ret_val);
            s = write(pfds[ret_val].fd, buf, sizeof(buf));
            if (s == -1)
                errExit("write");
            printf("    wrote %zd bytes: %.*s\n",
                    s, (int) s, buf);
        }
        sleep(5);
        memcpy(ready_pfds, null_ready_pfds, sizeof(null_ready_pfds));
    }

    printf("All file descriptors closed; bye\n");
    exit(EXIT_SUCCESS);
}

int choose_random_fd(int* ready_pfds, int nfds)
{
    int cnt = 0;
    for(int i = 0; i < nfds; i++)
    {
        if(*ready_pfds == 1)
            cnt++;
    }

    if(cnt == 0)
        return -1;

    srand(time(NULL));
    int random = rand() % cnt;

    for(int i = 0; i < nfds; i++)
    {
        if(random == 0)
        {
            return i;
        }
        if(*ready_pfds == 1)
            random--;
    }

    return -1;
}
