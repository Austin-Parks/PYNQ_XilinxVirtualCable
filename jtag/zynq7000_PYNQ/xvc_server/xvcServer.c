/* This work, "xvcServer.c", is a derivative of "xvcd.c" (https://github.com/tmbinc/xvcd)
* by tmbinc, used under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/).
* "xvcServer.c" is licensed under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/)
* by Avnet and is used by Xilinx for XAPP1251.
*
*  Description : XAPP1251 Xilinx Virtual Cable Server for Linux
*   Modified By: Austin Parks
*
*   Modified to use direct /dev/mem memory map access (w/ offset to debug bridge base address)
*   The base address is now passed in dynamically via command line argument '-a' (hex format)
*   
*   Other than that change this is just the XAP1251 code....
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <pthread.h>
#include <getopt.h>
#include <ctype.h>

#define MAP_SIZE    0x10000

typedef struct {
    uint32_t length_offset;
    uint32_t tms_offset;
    uint32_t tdi_offset;
    uint32_t tdo_offset;
    uint32_t ctrl_offset;
} jtag_t;

static int verbose = 0;
static int XVC_PORT = 2542;

static int sread(int fd, void *target, int len) {
    unsigned char *t = target;
    while (len) {
        int r = read(fd, t, len);
        if (r <= 0)
            return r;
        t += r;
        len -= r;
    }
    return 1;
}

int handle_data(int fd, volatile jtag_t* ptr) {
    char xvcInfo[32];
    unsigned int bufferSize = 2048;
    
    sprintf(xvcInfo, "xvcServer_v1.0:%u\n", bufferSize);
    
    do {
        char cmd[16];
        unsigned char buffer[bufferSize], result[bufferSize / 2];
        memset(cmd, 0, 16);
        
        if (sread(fd, cmd, 2) != 1)
            return 1;
        
        if (memcmp(cmd, "ge", 2) == 0) {
            if (sread(fd, cmd, 6) != 1)
                return 1;
            memcpy(result, xvcInfo, strlen(xvcInfo));
            if (write(fd, result, strlen(xvcInfo)) != strlen(xvcInfo)) {
                perror("write");
                return 1;
            }
            if (verbose > 2) {
                printf("%u : Received command: 'getinfo'\n", (int)time(NULL));
                printf("\t Replied with %s\n", xvcInfo);
            }
            break;
        } else if (memcmp(cmd, "se", 2) == 0) {
            if (sread(fd, cmd, 9) != 1)
                return 1;
            memcpy(result, cmd + 5, 4);
            if (write(fd, result, 4) != 4) {
                perror("write");
                return 1;
            }
            if (verbose > 2) {
                printf("%u : Received command: 'settck'\n", (int)time(NULL));
                printf("\t Replied with '%.*s'\n\n", 4, cmd + 5);
            }
            break;
        } else if (memcmp(cmd, "sh", 2) == 0) {
            if (sread(fd, cmd, 4) != 1)
                return 1;
            if (verbose > 2) {
                printf("%u : Received command: 'shift'\n", (int)time(NULL));
            }
        } else {
            fprintf(stderr, "invalid cmd '%s'\n", cmd);
            return 1;
        }
        
        int len;
        if (sread(fd, &len, 4) != 1) {
            fprintf(stderr, "reading length failed\n");
            return 1;
        }
        
        int nr_bytes = (len + 7) / 8;
        if (nr_bytes * 2 > sizeof(buffer)) {
            fprintf(stderr, "buffer size exceeded\n");
            return 1;
        }
        
        if (sread(fd, buffer, nr_bytes * 2) != 1) {
            fprintf(stderr, "reading data failed\n");
            return 1;
        }
        memset(result, 0, nr_bytes);
        
        if (verbose > 2) {
            printf("\tNumber of Bits  : %d\n", len);
            printf("\tNumber of Bytes : %d \n", nr_bytes);
            printf("\n");
        }
        
        int bytesLeft = nr_bytes;
        int bitsLeft = len;
        int byteIndex = 0;
        int tdi = 0;
        int tms = 0;
        int tdo = 0;
        
        while (bytesLeft > 0) {
            int shift_num_bytes;
            int shift_num_bits = 32;
            tms = 0;
            tdi = 0;
            tdo = 0;
            
            if (bytesLeft < 4) {
                shift_num_bits = bitsLeft;
            }
            shift_num_bytes = (shift_num_bits + 7) / 8;
            
            memcpy(&tms, &buffer[byteIndex], shift_num_bytes);
            memcpy(&tdi, &buffer[byteIndex + nr_bytes], shift_num_bytes);
            
            ptr->length_offset = shift_num_bits;
            ptr->tms_offset = tms;
            ptr->tdi_offset = tdi;
            ptr->ctrl_offset = 0x01;
            
            /* Switch this to interrupt in next revision */
            while (ptr->ctrl_offset) {}
            
            tdo = ptr->tdo_offset;
            memcpy(&result[byteIndex], &tdo, shift_num_bytes);
            
            bytesLeft -= shift_num_bytes;
            bitsLeft -= shift_num_bits;
            byteIndex += shift_num_bytes;
            
            if (verbose > 2) {
                printf("LEN : 0x%08x\n", shift_num_bits);
                printf("TMS : 0x%08x\n", tms);
                printf("TDI : 0x%08x\n", tdi);
                printf("TDO : 0x%08x\n", tdo);
            }
        }
        if (write(fd, result, nr_bytes) != nr_bytes) {
            perror("write");
            return 1;
        }
    } while (1);

    /* Note: Need to fix JTAG state updates, until then no exit is allowed */
    return 0;
}

void show_usage()
{
    fprintf(stderr, "\nNote: The AXI base Address is required, use EXTREME caution here...\n"
                    "This program utilizes full /dev/mem access for direct memory access:\n\n\t"
                    "usage: ./xvcServer_mmap [-v,--verbose=no] [-p,--port <port>=2542] "
                    "[-a,--addr <AXI Base Address (Hex)>]\n\n");
}

int main(int argc, char **argv) {
    int i;
    int s;
    int c;
    unsigned int arg_base_addr = 0;
    struct sockaddr_in address;
    char hostname[256];
    
    if(argc < 3) {
        printf("\nif(argc < 3)\n");
        show_usage();
        return -1;
    }
    
    /* options descriptor */
    static struct option longopts[] = {
        { "help",    no_argument,       NULL, '?' },
        { "verbose", no_argument,       NULL, 'v' },
        { "port",    required_argument, NULL, 'p' },
        { "addr",    required_argument, NULL, 'a' },
        { NULL,      0,                 NULL, 0 }
    };
    
    opterr = 0;
    
    while ((c = getopt_long(argc, argv, "vp:a:", longopts, NULL)) != -1) {
        switch (c) {
            case 'v':
                // printf("(char *) optarg: %010X\n", (unsigned int)(optarg) );
                // if(strlen(optarg) > 0)
                //     verbose = atoi(optarg);
                // else
                //     verbose = 1;
                // break;
                verbose++;
            case 'p':
                if(optarg != NULL) {
                    if(verbose > 0) { printf("atoi(optarg) = %i\n", (int)(optarg)); }
                    XVC_PORT = atoi(optarg);
                }
                else {
                    printf("Using default port: 2542");
                    XVC_PORT = 2542;
                }
                break;
            case 'a':
                arg_base_addr = (int)strtol(optarg, NULL, 0);
                if(verbose > 0){ printf("arg_base_addr: 0x%08X\n", arg_base_addr); }
                break;
            case '?':
                printf("\ncase '?':\n");
                show_usage();
                return 1;
        }
    }
    //if(verbose > 0) {
    printf("\nxvcServer cmommand line arguments:\n");
    printf("  verbose: %i\n", verbose);
    printf("     port: %i\n", XVC_PORT);
    printf("     addr: 0x%08X\n\n", arg_base_addr);
    //}
    
    int fd_mem;
    volatile jtag_t* ptr = NULL;
    
    fd_mem = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd_mem < 1) {
        fprintf(stderr, "Failed to open /dev/mem:\n");
        return -1;
    }
    
    ptr = (volatile jtag_t*) mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_mem, arg_base_addr);
    if (ptr == MAP_FAILED) {
        fprintf(stderr, "MMAP Failed\n");
        return -1;
    }
    close(fd_mem);
    
    
    s = socket(AF_INET, SOCK_STREAM, 0);
    
    if (s < 0) {
        perror("socket");
        return 1;
    }
    
    i = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof i);
    
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(XVC_PORT);
    address.sin_family = AF_INET;
    
    if (bind(s, (struct sockaddr*) &address, sizeof(address)) < 0) {
        perror("bind");
        return 1;
    }
    
    if (listen(s, 1) < 0) {
        perror("listen");
        return 1;
    }
    
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        perror("hostname lookup");
        close(s);
        return 1;
    }
    
    printf("INFO: To connect to this xvcServer instance, use url: TCP:%s:%u\n\n", hostname, XVC_PORT);
    
    fd_set conn;
    int maxfd = 0;
    
    FD_ZERO(&conn);
    FD_SET(s, &conn);
    
    maxfd = s;
    
    while (1) {
        fd_set read = conn, except = conn;
        int fd;
        
        if (select(maxfd + 1, &read, 0, &except, 0) < 0) {
            perror("select");
            break;
        }
        
        for (fd = 0; fd <= maxfd; ++fd) {
            if (FD_ISSET(fd, &read)) {
                if (fd == s) {
                    int newfd;
                    socklen_t nsize = sizeof(address);
                    
                    newfd = accept(s, (struct sockaddr*) &address, &nsize);
                    
                    printf("connection accepted - fd %d\n", newfd);
                    if (newfd < 0) {
                        perror("accept");
                    } else {
                        printf("setting TCP_NODELAY to 1\n");
                        int flag = 1;
                        int optResult = setsockopt(newfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
                        if (optResult < 0)
                            perror("TCP_NODELAY error");
                        if (newfd > maxfd) {
                            maxfd = newfd;
                        }
                        FD_SET(newfd, &conn);
                    }
                } else if (handle_data(fd, ptr)) {
                    printf("connection closed - fd %d\n", fd);
                    close(fd);
                    FD_CLR(fd, &conn);
                }
            } else if (FD_ISSET(fd, &except)) {
                printf("connection aborted - fd %d\n", fd);
                close(fd);
                FD_CLR(fd, &conn);
                if (fd == s)
                    break;
            }
        }
    }
    
    munmap((void *) ptr, MAP_SIZE);
    
    return 0;
}
