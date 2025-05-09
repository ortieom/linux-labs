#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#define DEVICE_PATH "/dev/int_stack"

#define INT_STACK_MAGIC 'S'
#define INT_STACK_SET_SIZE _IOW(INT_STACK_MAGIC, 1, unsigned int)

void print_help(void);
int set_size(int fd, int size);
int push(int fd, int value);
int pop(int fd, int *value);
int unwind(int fd);

int main(int argc, char *argv[]) {
    int fd, ret = 0;
    
    if (argc < 2) {
        print_help();
        return 1;
    }

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        if (errno == ENOENT) {
            fprintf(stderr, "error: USB key not inserted\n");
        } else {
            perror("Failed to open device");
        }
        return 1;
    }

    if (strcmp(argv[1], "set-size") == 0) {
        if (argc != 3) {
            print_help();
            close(fd);
            return 1;
        }
        int size = atoi(argv[2]);
        ret = set_size(fd, size);
    } 
    else if (strcmp(argv[1], "push") == 0) {
        if (argc != 3) {
            print_help();
            close(fd);
            return 1;
        }
        int value = atoi(argv[2]);
        ret = push(fd, value);
    } 
    else if (strcmp(argv[1], "pop") == 0) {
        if (argc != 2) {
            print_help();
            close(fd);
            return 1;
        }
        int value;
        ret = pop(fd, &value);
    } 
    else if (strcmp(argv[1], "unwind") == 0) {
        if (argc != 2) {
            print_help();
            close(fd);
            return 1;
        }
        ret = unwind(fd);
    } 
    else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        print_help();
        ret = 1;
    }

    close(fd);
    
    return ret;
}

void print_help(void) {
    printf("Usage: kernel_stack <command> [arguments]\n\n");
    printf("Commands:\n");
    printf("\tset-size <size>\tSet maximum size of the stack\n");
    printf("\tpush <value>\tPush integer value onto the stack\n");
    printf("\tpop\tPop integer from the stack\n");
    printf("\tunwind\tPop all integers from the stack\n");
}

int set_size(int fd, int size) {
    if (size <= 0) {
        fprintf(stderr, "ERROR: size should be > 0\n");
        return 1;
    }

    unsigned int new_size = (unsigned int)size;
    int ret = ioctl(fd, INT_STACK_SET_SIZE, &new_size);
    
    if (ret < 0) {
        perror("ERROR");
        return -errno;
    }
    
    return 0;
}

int push(int fd, int value) {
    int ret = write(fd, &value, sizeof(int));
    
    if (ret >= 0) {
        return 0;
    }

    if (errno == ERANGE) {
        fprintf(stderr, "ERROR: stack is full\n");
    } else {
        perror("ERROR");
    }
    
    return -errno;
}

int pop(int fd, int *value) {
    int ret = read(fd, value, sizeof(int));
    
    if (ret == 0) {
        printf("NULL\n");
        return 0;
    } else if (ret < 0) {
        perror("ERROR");
        return -errno;
    }

    printf("%d\n", *value);
    
    return 0;
}

int unwind(int fd) {
    int value;
    int ret;
    
    while (1) {
        ret = read(fd, &value, sizeof(int));
        
        if (ret == 0) {  // stack is empty, finish execution
            break;
        } else if (ret < 0) {
            perror("ERROR");
            return -errno;
        }

        printf("%d\n", value);
    }
    
    return 0;
}
