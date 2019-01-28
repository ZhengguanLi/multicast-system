#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>
#include <pthread.h>
#define MAXBUF 1024

typedef int bool;
enum{false, true};
struct argsThread{
	int udpSocket;
};
void* receiveMessage(void *args);
void sendMessage(int updSocket, struct sockaddr_in udpServer, char groupName[MAXBUF]);

int main(int argc, char* argv[]){
	int udpSocket;
	int returnStatus;
	int addrlen;
	struct sockaddr_in udpClient, udpServer;
	char groupName[MAXBUF] = {0};
	char action[MAXBUF] = {0};
	char buf[MAXBUF] = {0};
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <ip address> <port>\n", argv[0]);
		exit(1);
	}
	
	/* create a socket */
	udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if(udpSocket == -1) {
		fprintf(stderr, "Could not create a socket!\n");
		exit(1);
	}else {
		printf("Socket created!\n");
	}

	/* client address */
	/* use INADDR_ANY to use all local addresses */
	udpClient.sin_family = AF_INET;
	udpClient.sin_addr.s_addr = INADDR_ANY;
	udpClient.sin_port = 0;
	returnStatus = bind(udpSocket, (struct sockaddr*)&udpClient, sizeof(udpClient));
	if(returnStatus == 0){
		fprintf(stderr, "Bind completed!\n");
	}else{
		fprintf(stderr, "Could not bind to address!\n");
		close(udpSocket);
		exit(1);
	}
	printf("--------------------------------------\n");

	/* server address */
	/* use the command-line arguments */
	udpServer.sin_family = AF_INET;
	udpServer.sin_addr.s_addr = inet_addr(argv[1]);
	udpServer.sin_port = htons(atoi(argv[2]));

	while(true){
		while(true){
			// read group name
			printf("Please enter the group name you want to join: ");
			scanf("%s", groupName);
			strcpy(buf, groupName); 

			// read action
			printf("Please choose action(1. join, 2. quit): ");
			scanf("%s", action);
			if(strcmp(action, "1") == 0 || strcmp(action, "join") == 0){
				bzero(action, MAXBUF);
				strcpy(action, "1");
				break;
			}else if(strcmp(action, "2") == 0|| strcmp(action, "quit") == 0){
				strcpy(action, "2");
				break;
			}else{
				printf("Action invalid!\n\n");
			}
		}

		strcat(buf, "$");
		strcat(buf, action); // group name $ action 
		returnStatus = sendto(udpSocket, buf, strlen(buf) + 1, 0, (struct sockaddr*)&udpServer, sizeof(udpServer));
		
		// read acknowledgement(0:join succeed, 1:quit done, 2:invalid group name, 3:overflow, 4.quit invalid)
		bzero(buf, MAXBUF);
		returnStatus = recvfrom(udpSocket, buf, MAXBUF, 0, (struct sockaddr*)&udpServer, &addrlen);
		if(strcmp(buf, "0") == 0){
			printf("You joined the group!\n");
			break;
		}else if(strcmp(buf, "1") == 0){
			printf("You quitted from the group!\n");
		}else if(strcmp(buf, "2") == 0){
			printf("Invalid group name, please try again!\n");
		}else if(strcmp(buf, "3") == 0){
			printf("Maximum number of clients reached!\n");
		}else if(strcmp(buf, "4") == 0){
			printf("Client does not exist!\n");
		}
		printf("--------------------------------------\n");
	}

	printf("--------------------------------------\n");
	pthread_t thread1;
	struct argsThread args;
	args.udpSocket = udpSocket;

	// create new thread
	int s = pthread_create(&thread1, NULL, receiveMessage, &args);
	if(s != 0){
		printf("New thread creation failed!\n\n");
	}

	// main thread: start up accepting message
	sendMessage(udpSocket, udpServer, groupName);
	s = pthread_join(thread1, NULL); // join with the terminated thread
	if(s == 0){
		printf("Pthread join succeed!\n");
	}else{
		printf("Pthread join failed!\n");
	}

	/* cleanup */
	close(udpSocket);
	return 0;
}

void sendMessage(int udpSocket, struct sockaddr_in udpServer, char groupName[MAXBUF]){
	char buf[MAXBUF] = {0};		
	while(true){
		scanf("%s", buf);		
		if(strcmp(buf, "2") == 0 || strcmp(buf, "quit") == 0){
			strcpy(buf, groupName);
			strcat(buf, "$2");
			int returnStatus = sendto(udpSocket, buf, strlen(buf) + 1, 0, (struct sockaddr*)&udpServer, sizeof(udpServer));
			if(returnStatus == -1){
				fprintf(stderr, "Couldn't send message!\n");
			}else{
				// confirmation
				int addrlen = sizeof(udpServer);
				returnStatus = recvfrom(udpSocket, buf, MAXBUF, 0, (struct sockaddr*)&udpServer, &addrlen);
			}
		}
	}
}

void* receiveMessage(void* args){
	struct argsThread arg = *(struct argsThread*)args;
	int udpSocket = arg.udpSocket;
	struct sockaddr_in udpServer;
	char buf[MAXBUF];
	int addrlen = sizeof(udpServer);
	printf("Receiving messages: \n");
	while(true){
		int returnStatus = recvfrom(udpSocket, buf, MAXBUF, 0, (struct sockaddr*)&udpServer, &addrlen);
		if(returnStatus == -1){
			fprintf(stderr, "Couldn't receive message!\n");
		}else{
			if(strcmp(buf, "2") == 0){
				printf("Stop receiving message from Server!\n");
				break;
			}else{
				printf("%s\n", buf);
			}
		}
	}
}
