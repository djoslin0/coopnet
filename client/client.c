#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <pthread.h>

#include "mpacket.h"

#define PORT 8888

int main(int argc, char *argv[]) {

    // create a master socket
    int master_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(master_socket == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // type of socket created
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // bind the socket to localhost port 8888
    if (connect(master_socket, (struct sockaddr*) &address, sizeof(address)) < 0) {
        perror("connect failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected on port %d \n", PORT);
    while (1) {}
    return 0;
}