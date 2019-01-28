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

int getGroupName(char buf[], char groupName[]);
void getAction(char buf[], int start, char action[]);
void showStatus(struct sockaddr_in clientList[], int currSize, int maxSize);
void* listening(void* arg);
void printAddrInfo(struct sockaddr_in udpClient);
bool quit(int udpsocket, struct sockaddr_in udpClient, struct sockaddr_in clientList[], int *currSize, int *maxSize);
struct argsThread{
	int udpSocket;
	char groupName[MAXBUF];
	int *currSize;
	int *maxSize;
	struct sockaddr_in* clientList;
};

int main(int argc, char* argv[]){
	int udpSocket;
	int returnStatus = 0;
	int addrlen = 0;
	struct sockaddr_in udpServer, udpClient;
	char buf[MAXBUF] = {0};
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		exit(1);
	}
	/* create a socket */
	udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if(udpSocket == -1){
		fprintf(stderr, "Could not create a socket!\n");
		exit(1);
	}else{
		printf("Socket created.\n");
	}

	/* set up the server address and port */
	/* use INADDR_ANY to bind to all local addresses */
	udpServer.sin_family = AF_INET;
	udpServer.sin_addr.s_addr = htonl(INADDR_ANY);
	udpServer.sin_port = htons(atoi(argv[1]));

	/* bind to the socket */
	returnStatus = bind(udpSocket, (struct sockaddr*)&udpServer, sizeof(udpServer));
	if(returnStatus == 0){
		fprintf(stderr, "Bind completed!\n");
	}else{
		fprintf(stderr, "Could not bind to address!\n");
		close(udpSocket);
		exit(1);
	}
	printf("--------------------------------------\n");

	while (1) {
		char groupName[MAXBUF] = "";
		char clientNo[MAXBUF] = "";
		int maxSize = 0; // maximum number of clients
		int currSize = 0; // current number of clients
		
		// create group
		printf("Please create a new group: ");
		scanf("%s", groupName);
		
		// set the maximum number of clients
		printf("Please enter the maximum number of clients allowed: ");
		scanf("%s", clientNo);
		maxSize = atoi(clientNo);

		// struct array to store the clients(IPAddress, port number etc)
		struct sockaddr_in clientList[maxSize];
		
		// multi-threading: one for listening and the other one for getting multiple message 
		pthread_t thread1;

		// arguments for the thread function
		struct argsThread args;
		strcpy(args.groupName, groupName);//drawback: No connection between group name and clientList, improve: Put group name, clientList in an struct
		args.currSize = &currSize;
		args.maxSize = &maxSize;
		args.clientList = clientList;
		args.udpSocket = udpSocket;
		
		int s = pthread_create(&thread1, NULL, listening, &args);//thread1 executes Listening function, NULL: no args need to pass
		if(s == 0){
			printf("New thread created!\n");
		}else{
			printf("New thread failed to create!\n");
		}
		printf("--------------------------------------\n");

		/* returnStatus = recvfrom(udpSocket, buf, MAXBUF, 0, (struct sockaddr*)&udpClient, &addrlen);
		printf("buf received in main thread after pthread_create: %s\n", buf); */
		
		// main thread: read messages from user
		while(true){
			printf("Please enter the message or select action(1. status, 2.clear all): ");
			scanf("%s", buf);
			if(strcmp(buf, "1") == 0 ||strcmp(buf, "status") == 0){
				showStatus(clientList, currSize, maxSize);
			}else if(strcmp(buf, "2") == 0 || strcmp(buf, "clearall") == 0){
				strcpy(buf, "2");
				// broadcast clear all message
				for(int i = 0; i < currSize; i++){
					returnStatus = sendto(udpSocket, buf, strlen(buf) + 1, 0, (struct sockaddr*)&clientList[i], sizeof(udpClient)); 
				}
				currSize = 0;
			}else{
				if(currSize == 0) printf("No client connected yet.\n");
				else{
					// broadcast message
					for(int i = 0; i < currSize; i++){
						returnStatus = sendto(udpSocket, buf, strlen(buf) + 1, 0, (struct sockaddr*)&clientList[i], sizeof(udpClient));//send message
					}

					// if(currSize == 1) printf("Broadcast message to %d client!\n", currSize);
					// else printf("Broadcast to %d clients\n", currSize);
				}
			}
		}
		
		s = pthread_join(thread1, NULL); //join with the terminated thread
		if(s == 0){
			printf("Pthread_join succeed!\n");
		}else{
			printf("Pthread_join failed!\n");
		}
	}

	/*cleanup */
	close(udpSocket);
	return 0;
}

// thread: listen for joinning and leaving request from clients
void* listening(void* arg){
	char buf[MAXBUF] = {0};
	struct argsThread args;
	args = *(struct argsThread*)arg;
	
	int udpSocket = args.udpSocket;
	int *currSize = args.currSize;
	int *maxSize = args.maxSize;
	char groupName[MAXBUF];
	strcpy(groupName, args.groupName);

	struct sockaddr_in* clientList = args.clientList;
	int returnStatus = 0;
	int addrlen = 0;
	
	struct sockaddr_in udpClient;
	while(true){
		char tempGroupName[MAXBUF] = {0};
		char tempAction[MAXBUF] = {0};
		int addrlen = sizeof(udpClient);
		int returnStatus = recvfrom(udpSocket, buf, MAXBUF, 0, (struct sockaddr*)&udpClient, &addrlen);		

		// read group name from buf
		int start = getGroupName(buf, tempGroupName);
		getAction(buf, start, tempAction);
		
		// check the group name and option 
		// if group name valid
		if(strcmp(tempGroupName, groupName) == 0){
			if(strcmp(tempAction, "1") == 0){
				// if join
				if(*currSize != *maxSize){
					clientList[*currSize] = udpClient; //store the IPAdress to clientList 
					(*currSize)++;
					strcpy(buf, "0");
					returnStatus = sendto(udpSocket, buf, strlen(buf) + 1, 0, (struct sockaddr*)&udpClient, sizeof(udpClient));
				}else{
					// if overflow
					strcpy(buf, "3");
					returnStatus = sendto(udpSocket, buf, strlen(buf) + 1, 0, (struct sockaddr*)&udpClient, sizeof(udpClient));
				}
			}else if(strcmp(buf, "2")){
				quit(udpSocket, udpClient, clientList, currSize, maxSize);
			}	
		}else{
			// if group name invalid
			strcpy(buf, "2");
			returnStatus = sendto(udpSocket, buf, MAXBUF, 0, (struct sockaddr*)&udpClient, addrlen);
		}
	}

}

// print the IP address or port number of client
void printAddrInfo(struct sockaddr_in udpClient){
	char *ip = inet_ntoa(udpClient.sin_addr);
	int port = ntohs(udpClient.sin_port);
	printf("\nIP address: %s, port number: %d, ", ip, port);
}

void showStatus(struct sockaddr_in clientList[], int currSize, int maxSize){
	if(currSize == 0){
		printf("No client connected yet.\n");
	}

	printf("--------------------\n");
	for(int i = 0; i < currSize; i++){
		char *ip = inet_ntoa(clientList[i].sin_addr);
		int port = ntohs(clientList[i].sin_port);
		printf("Client: %d/%d\n", i + 1, maxSize);
		printf("IP address: %s\n", ip);
		printf("Port number: %d\n", port);
		printf("--------------------\n");
	}
}

bool quit(int udpSocket, struct sockaddr_in udpClient, struct sockaddr_in clientList[], int *currSize, int *maxSize){
	char buf[MAXBUF] = {0};
	int returnStatus; 
	int index = 0;
	bool flag = false; // if the client is in the clientList
	for(int i = 0; i < *currSize; i++){
		if(clientList[i].sin_port == udpClient.sin_port && clientList[i].sin_addr.s_addr == udpClient.sin_addr.s_addr){
			index = i;
			flag = true;
			break;
		}
	}

	//if the client is a member
	if(flag == true){
		if(index != *currSize-1){
			clientList[index] = clientList[*currSize-1];//shift the array to overwrite the empty place
		}

		(*currSize)--;
		strcpy(buf, "1"); // quit succeed
		returnStatus = sendto(udpSocket, buf, strlen(buf) + 1, 0, (struct sockaddr*)&udpClient, sizeof(udpClient));
	}else{
		strcpy(buf, "4"); // client does not exist
		returnStatus = sendto(udpSocket, buf, strlen(buf) + 1, 0, (struct sockaddr*)&udpClient, sizeof(udpClient));
	}
}

int getGroupName(char buf[], char groupName[]){
	int len = strlen(buf);
	for(int i = 0; i < len; i++){
		if(buf[i] != '$') groupName[i] = buf[i];
		else return i + 1;
	}
	return 0;
}

void getAction(char buf[], int start, char action[]){
	int len = strlen(buf);
	for(int i = start; i < len; i++){
		action[i - start] = buf[i];
	}
}

