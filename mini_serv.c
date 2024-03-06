// Include already in main.c
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Include to add
#include <stdio.h>
#include <stdlib.h>

// Defines constants for error messages, buffer size, and the number of ports available.

#define ARGS "Wrong number of arguments\n"
#define FATAL "Fatal error\n"
#define BUFF 200000 // Set it higher if too low
#define PORTS 65000

/* Global Variables
   maxfd: Keeps track of the highest file descriptor number to manage multiple client connections efficiently.
   sockfd: The server's socket file descriptor.
   id: A counter for assigning unique IDs to clients.
   client_id_arr: An array to map client file descriptors to their IDs.
   msg_buff: A buffer for storing messages to be broadcasted.
   rset, wset, aset: File descriptor sets used by the select() function to monitor activity on sockets.*/

int maxfd, sockfd, id = 0;
int client_id_arr[PORTS];
char msg_buff[BUFF + 100];
fd_set rset, wset, aset;

/* Handles errors by closing the socket if necessary, writing an error message to stderr, and exiting the program. */
void error(char *msg)
{
	if (sockfd > 2)
		close(sockfd);
	write(2, msg, strlen(msg));
	exit(1);
}

/* Sends the content of msg_buff to all connected clients except the sender.
It iterates through the file descriptors set in wset and sends the message using the send() function. 
If an error occurs during sending, it calls the error() function. */
void replyToAll(int connfd)
{
	for (int fd = 2; fd <= maxfd; fd++)
		if (fd != connfd && FD_ISSET(fd, &wset))
			if (send(fd, msg_buff, strlen(msg_buff), 0) < 0)
				error(FATAL);
}

int main(int ac, char **av) {
	int connfd;
	socklen_t len;
	struct sockaddr_in servaddr, cli; 

	// Ensures the correct number of arguments (port number) is provided.
	if (ac != 2)
		error(ARGS);

	// Socket creation and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		error(FATAL);
	} 
	
	// Initializing socket
	bzero(&servaddr, sizeof(servaddr)); 

	// Configure socket
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1])); // Set port to argv[1]
  
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { 
		error(FATAL);
	} 
	
	// Listening for incoming client connections
	if (listen(sockfd, 4096) != 0) {
		error(FATAL);
	}

	len = sizeof(cli);
	maxfd = sockfd;
	FD_ZERO(&aset);
	FD_SET(sockfd, &aset);

	// Main event loop for handling client connections and messages
	while (1)
	{
		// Copy read, write, and exception sets to check for activity
		rset = wset = aset;

		// Wait for activity on any of the sockets
		if (select(maxfd +1, &rset, &wset, 0, 0) < 0)
			continue; // If select() fails, continue to the next iteration
		
		// Check if there is a new client connection request
		if (FD_ISSET(sockfd, &rset))
		{
			connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
			if (connfd < 0) 
	        	error(FATAL); // If accept() fails, handle the error
			sprintf(msg_buff, "server: client %d just arrived\n", id);
			client_id_arr[connfd] = id++;
			FD_SET(connfd, &aset); // Add new client's socket to the set

			replyToAll(connfd); // Notify all clients about the new client
			maxfd = (connfd > maxfd) ? connfd : maxfd; // Update maxfd if necessary
			continue; // Continue to the next iteration to handle more events
		}

		// Check for activity on all client sockets
		for (int fd = 2; fd <= maxfd; fd++)
		{
			if (FD_ISSET(fd, &rset))
			{
				int recv_return = 1;
				char msg[BUFF];
				bzero(&msg, sizeof(msg));

				// Receive message from the client
				while (recv_return == 1 && msg[strlen(msg) - 1] != '\n')
					recv_return = recv(fd, msg + strlen(msg), 1, 0);
				if (recv_return <= 0)
				{
					sprintf(msg_buff, "server: client %d just left\n", client_id_arr[fd]);
					FD_CLR(fd, &aset); // Remove client's socket from the set
					close(fd); // Close the client's socket
				}
				else
					sprintf(msg_buff, "client %d: %s", client_id_arr[fd], msg);

				replyToAll(fd); // Broadcast the message to all clients
			}
		}
	}
	return 0;
}
