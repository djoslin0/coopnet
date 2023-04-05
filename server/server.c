//Example code: A simple server side code, which echos back the received message.
//Handle multiple socket connections with select and fd_set on Linux
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
#define TRUE   1
#define FALSE  0
#define BUFFER_MAX 1025
#define CLADDR_LEN 100

uint64_t nextClientId = 1;

struct Client {
    int active;
    uint64_t id;
    struct sockaddr_in address;
    char asciiAddress[CLADDR_LEN];
    int socket;
    pthread_t thread;
    struct Client* next;
};

struct Client* clientList = NULL;

static int client_count(void) {
    int count = 0;

    struct Client* node = clientList;
    while (node) {
        count++;
        node = node->next;
    }

    return count;
}

static void client_cleanup(struct Client* client) {
    // cleanup
    client->active = FALSE;

    // remove from head
    if (clientList == client) {
        clientList = client->next;
    }

    // remove from list
    struct Client* node = clientList;
    while (node && node->next) {
        if (node->next == client) {
            node->next = client->next;
        }
        node = node->next;
    }

    // close socket
    close(client->socket);

    // free client
    free(client);
    
    printf("Client removed, count: %d\n", client_count());
}

static void* client_thread(void* voidClient) {
    // allocate memory
    struct Client* client = (struct Client*)voidClient;
    char buffer[BUFFER_MAX] = { 0 };

    printf("[%lu] Client thread started\n", client->id);

    struct MPacket* mp = mpacket_create();
    mpacket_write_u32(mp, MPACKET_USER_ID);
    mpacket_write_u64(mp, client->id);
    mpacket_send(mp, client->socket, (struct sockaddr*)&client->address);

    printf("[%lu] Sent: [%u]", client->id, mp->packetLength);
    mpacket_print(mp);
    printf("\n");

    mpacket_free(mp);

    // wait for data
    while (TRUE) {
        memset(buffer, 0, BUFFER_MAX);

        // make sure client is still active
        if (!client->active) { break; }

        socklen_t len = sizeof(struct sockaddr_in);
        int ret = recvfrom(client->socket, buffer, BUFFER_MAX, 0, (struct sockaddr *) &client->address, &len);
        int rc = errno;

        // make sure client is still active
        if (!client->active) { break; }

        // check for error
        if (ret == -1 && (rc == EAGAIN || rc == EWOULDBLOCK)) {
            printf("[%lu] continue\n", client->id);
            continue;
        } else if (ret < 0) {
            printf("[%lu] Error receiving data (%d)!\n", client->id, rc);
            break;
        }

        // check for disconnecton
        if (ret == 0) {
            printf("[%lu] Connection closed.\n", client->id);
            break;
        }

        printf("[%lu] Received data: %s\n", client->id, buffer); 

        ret = sendto(client->socket, buffer, BUFFER_MAX, 0, (struct sockaddr *) &client->address, len);   
        if (ret < 0) {  
            printf("[%lu] Error sending data!\n", client->id);
            break;
        }  

        printf("[%lu] Sent data: %s\n", client->id, buffer);
    }

    client_cleanup(client);

    return NULL;
}

int main(int argc, char *argv[]) {

    // create a master socket
    int master_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(master_socket == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // set master socket to allow multiple connections ,
    // this is just a good habit, it will work without this
    int opt = TRUE;
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // type of socket created
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr*) &address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Listener on port %d \n", PORT);

    // try to specify maximum of 5 pending connections for the master socket
    if (listen(master_socket, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // accept the incoming connection
    puts("Waiting for connections ...");

    while (TRUE) {
        // allocate and accept socket
        struct Client* client = calloc(1, sizeof(struct Client));
        socklen_t len = sizeof(struct sockaddr_in);
        client->socket = accept(master_socket, (struct sockaddr *) &client->address, &len);
        client->id = nextClientId++;
        client->active = TRUE;

        // set timeout
        struct timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        setsockopt(client->socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

        // check for errors
        if (client->socket < 0) {
            printf("Error accepting connection!\n");
            free(client);
            continue;
        }

        // convert address to ascii
        inet_ntop(AF_INET, &(address.sin_addr), client->asciiAddress, CLADDR_LEN);
        printf("[%lu] Connection accepted: %s\n", client->id, client->asciiAddress);

        // create thread
        pthread_create(&client->thread, NULL, client_thread, client);

        // add to head of list
        if (clientList == NULL) {
            clientList = client;
            continue;
        }

        // add to end of list
        struct Client* node = clientList;
        while (node && node->next) { node = node->next; }
        node->next = client;

        printf("[%lu] Client added, count: %d\n", client->id, client_count());
    }

    return 0;
}