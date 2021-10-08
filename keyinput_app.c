#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <linux/input.h>

int main(int argc, char *argv[])
{
        int ret = 0;
        int fd;
        unsigned char buf[1] = {0};
        char *filename = NULL;
        struct input_event event;

        if (argc != 2) {
                printf("Error Usage!\n"
                       "Usage: %s filename\n", argv[0]);
                ret = -1;
                goto error;
        }

        filename = argv[1];
        fd = open(filename, O_RDWR);
        if (fd == -1) {
                perror("open failed!\n");
                ret = -1;
                goto error;
        }

        while (1) {
                ret = read(fd, &event, sizeof(event));
                if (ret < 0) {
                        perror("read error");
                        goto error;
                }

                switch (event.type) {
                case EV_SYN:
                        break;
                case EV_REP:
                        break;
                case EV_KEY:
                        if (event.code < BTN_MISC) {
                                printf("key %d %s\n",
                                       event.code,
                                       event.value ? "press" : "release");
                        } else {
                                printf("button %d %s\n",
                                       event.code,
                                       event.value ? "press" : "release");
                        }
                        break;
                }
        }

error:
        close(fd);
        return ret;
}
