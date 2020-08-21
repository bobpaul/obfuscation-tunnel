#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>

#define LOCAL_ADDR "0.0.0.0"
#define LOCAL_PORT 8080
#define REMOTE_ADDR "rolisoft.go.ro"
#define REMOTE_PORT 8080
//#define REMOTE_ADDR "127.0.0.1"
//#define REMOTE_PORT 8081
#define MTU_SIZE 1500

#define min(X, Y) (((X) < (Y)) ? (X) : (Y))

static inline void obfuscate_message(char* message, int length)
{
    int process = min(16, length);

    if (length > 32)
    {
        for (int i = 0; i < process; i++)
        {
            message[i] ^= 'a' ^ message[i + 16];
        }
    }
    else
    {
        for (int i = 0; i < process; i++)
        {
            message[i] ^= 'a';
        }
    }
}

static inline unsigned short read_14bit(int fd)
{
    int shift = 0;
    unsigned short value = 0;
    unsigned char current = 0;
    
    do
    {
        if (shift == 2 * 7) // cap at 16383
        {
            printf("Size header seems to be corrupted, abandoning read.\n");
            break;
        }

        socklen_t msglen = read(fd, &current, sizeof(unsigned char));

        if (msglen == 0)
        {
            // propagate TCP closed event
            return 0;
        }

        value |= (current & 0x7f) << shift;
        shift += 7;
    }
    while ((current & 0x80) != 0);

    return value;
}

static inline void write_14bit(unsigned short size, char* buffer, int* length)
{
    *length = 0;
    unsigned short value = size;

    while (value >= 0x80)
    {
        buffer[(*length)++] = value | 0x80;
        value >>= 7;
    }

    buffer[(*length)++] = value;
}

int main()
{/*
    unsigned short size = 163821;
    
    printf("%02hhx %02hhx\n", ((unsigned char*)&size)[0], ((unsigned char*)&size)[1]);

    int cmplen = 0;
    char cmpbuf[2];
    unsigned short cmpval = size;

    while (cmpval >= 0x80)
    {
        cmpbuf[cmplen++] = cmpval | 0x80;
        cmpval >>= 7;
    }

    cmpbuf[cmplen++] = cmpval;

    printf("encoded bytes are: %d %02hhx %02hhx %02hhx\n", cmplen, cmpbuf[0], cmpbuf[1], cmpbuf[2]);

    unsigned short decmpval = 0;
    int decshift = 0;
    unsigned char deccurrent = 0;
    int j = 0;
    do
    {
        printf("looping\n");
        if (decshift == 2 * 7) // cap at 16383
        {
            printf("Size header seems to be corrupted, abandoning read.\n");
            //break;
        }

        deccurrent = cmpbuf[j++];
        decmpval |= (deccurrent & 0x7f) << decshift;
        decshift += 7;
    }
    while ((deccurrent & 0x80) != 0);

    printf("decoded size is: %d\n", decmpval);

    return 0;*/
    int verbose = 0, obfuscate = 1, remotebound = 0, res;
    int serverfd, remotefd;
    struct pollfd fds[2];
    char buffer[MTU_SIZE + sizeof(unsigned short)];
    struct sockaddr_in localaddr, clientaddr, remoteaddr;
    int clientaddrlen = sizeof(clientaddr), remoteaddrlen = sizeof(remoteaddr);

    if ((serverfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    { 
        perror("server socket creation failed");
        return EXIT_FAILURE;
    }

    if ((remotefd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    { 
        perror("gateway socket creation failed");
        return EXIT_FAILURE;
    }

    memset(&localaddr, 0, sizeof(localaddr));
    memset(&clientaddr, 0, sizeof(clientaddr));
    memset(&remoteaddr, 0, sizeof(remoteaddr));

    localaddr.sin_family = AF_INET;
    localaddr.sin_port = htons(LOCAL_PORT);
    inet_pton(AF_INET, LOCAL_ADDR, &localaddr.sin_addr);

    struct hostent* remotehost = gethostbyname(REMOTE_ADDR);

    if (remotehost == NULL)
    {
        perror("failed to resolve remote host");
        return EXIT_FAILURE;
    }

    memcpy(&remoteaddr.sin_addr, remotehost->h_addr_list[0], remotehost->h_length);
    remoteaddr.sin_family = AF_INET;
    remoteaddr.sin_port = htons(REMOTE_PORT);

    if (bind(serverfd, (const struct sockaddr *)&localaddr, sizeof(localaddr)) < 0)
    {
        perror("bind failed");
        return EXIT_FAILURE;
    }

    printf("Connecting to remote server...\n");

    if ((res = connect(remotefd, (const struct sockaddr *)&remoteaddr, remoteaddrlen)) != 0)
    {
        perror("failed to connect to remote host");
        return EXIT_FAILURE;
    }

    memset(fds, 0 , sizeof(fds));
    fds[0].fd = serverfd;
    fds[0].events = POLLIN;
    fds[1].fd = remotefd;
    fds[1].events = POLLIN;

    int i = 1;
    setsockopt(serverfd, IPPROTO_TCP, TCP_NODELAY, (void *)&i, sizeof(i));
#ifdef TCP_QUICKACK
    setsockopt(serverfd, IPPROTO_TCP, TCP_QUICKACK, (void *)&i, sizeof(i));
#endif

    //fcntl(remotefd, F_SETFL, O_NONBLOCK);

    if (obfuscate) printf("Header obfuscation enabled.\n");

    while (1)
    {
        if (verbose) printf("Polling...\n");

        res = poll(fds, 2, (3 * 60 * 1000));

        if (res == 0)
        {
            continue;
        }
        else if (res < 0)
        {
            perror("poll failed");
            return EXIT_FAILURE;
        }

        if (fds[1].revents & POLLHUP || fds[1].revents & POLLERR)
        {
            printf("TCP connection to remote lost\n");
            return EXIT_FAILURE;
        }

        if (fds[0].revents & POLLIN)
        {
            // udp -> tcp

            socklen_t msglen = recvfrom(serverfd, ((char*)buffer) + sizeof(unsigned short), MTU_SIZE, MSG_WAITALL, (struct sockaddr*)&clientaddr, (unsigned int*)&clientaddrlen);

            if (verbose) printf("Received %d bytes from client\n", msglen);
            if (obfuscate) obfuscate_message(((char*)buffer) + sizeof(unsigned short), msglen);

            int sizelen = 0;
            write_14bit(msglen, (char*)buffer, &sizelen);
            int sizediff = sizeof(unsigned short) - sizelen;

            if (sizediff == 1)
            {
                buffer[1] = buffer[0];
            }

            res = write(remotefd, (char*)buffer + sizediff, msglen + sizelen);
        }

        if (fds[1].revents & POLLIN)
        {
            // tcp -> udp

            unsigned short toread = read_14bit(remotefd);

            if (toread == 0)
            {
                printf("TCP connection to remote lost\n");
                return EXIT_FAILURE;
            }

            if (toread > MTU_SIZE)
            {
                printf("Incorrect size read from buffer, abandoning read.\n");
                continue;
            }

            unsigned short readsize = toread;

            while (toread > 0)
            {
                socklen_t msglen = read(remotefd, (char*)buffer + (readsize - toread), toread);

                if (verbose && toread != msglen)
                {
                    printf("Read partially, need %u more bytes.\n", toread - msglen);
                }

                toread -= msglen;
            }

            if (verbose) printf("Received %d bytes from remote\n", readsize);
            if (obfuscate) obfuscate_message(buffer, readsize);

            res = sendto(serverfd, (char*)buffer, readsize, 0, (const struct sockaddr *)&clientaddr, clientaddrlen);
        }
    }

    close(remotefd);

    return 0;
}