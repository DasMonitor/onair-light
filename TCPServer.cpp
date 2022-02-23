// ============================================================================
//   Author: Kenneth Perkins
//   Date:  Jan 8, 2014
//   Taken From: http://programmingnotes.org/
//   File: ServerTCP.cpp
//   Description: This program implements a simple network application in
//     which a client sends text to a server and the server replies back to the
//     client. This is the server program, and has the following functionality:
//       1. Listen for incoming connections on a specified port.
//       2. When a client tries to connect, the connection is accepted.
//       3. When a connection is created, text is received from the client.
//       4. Text is sent back to the client from the server.
//       5. Close the connection and go back to waiting for more connections.
// ============================================================================
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdexcept>
#include <stdio.h>
#include <string>
using namespace std;

// Compile & Run
// g++ ServerTCP.cpp -o ServerTCP
// ./ServerTCP 1234

// the maximum size for sending/receiving text
const int MSG_SIZE = 100;


string exec(string command) {
   char buffer[128];
   string result = "";

   // Open pipe to file
   FILE* pipe = popen(command.c_str(), "r");
   if (!pipe) {
      return "popen failed!";
   }

   // read till end of process:
   while (!feof(pipe)) {

      // use buffer to read and add to result
      if (fgets(buffer, 128, pipe) != NULL)
         result += buffer;
   }

   pclose(pipe);
   return result;
}


int main(int argc, char* argv[])
{
    // declare variables
    // the port number
    int port = -1;

    // the integer to store the file descriptor number
    // which will represent a socket on which the server
    // will be listening
    int listenfd = -1;

    // the file descriptor representing the connection to the client
    int connfd = -1;

    // the number of bytes read
    int numRead = -1;

    // the buffer to store text
    char data[MSG_SIZE];

    // the structures representing the server and client
    // addresses respectively
    sockaddr_in serverAddr, cliAddr;

    // stores the size of the client's address
    socklen_t cliLen = sizeof(cliAddr);

    // make sure the user has provided the port number to listen on
    if(argc < 2)
    {
        cerr<<"\n** ERROR NOT ENOUGH ARGS!\n"
            <<"USAGE: "<<argv[0]<<" <SERVER PORT #>\n\n";
        exit(1);
    }

    // get the port number
    port = atoi(argv[1]);

    // make sure that the port is within a valid range
    if(port < 0 || port > 65535)
    {
        cerr<<"Invalid port number\n";
        exit(1);
    }

    // create a socket that uses
    // IPV4 addressing scheme (AF_INET),
    // Supports reliable data transfer (SOCK_STREAM),
    // and choose the default protocol that provides
    // reliable service (i.e. 0); usually TCP
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        exit(1);
    }

    // set the structure to all zeros
    memset(&serverAddr, 0, sizeof(serverAddr));

    // convert the port number to network representation
    serverAddr.sin_port = htons(port);

    // set the server family
    serverAddr.sin_family = AF_INET;

    // retrieve packets without having to know your IP address,
    // and retrieve packets from all network interfaces if the
    // machine has multiple ones
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // associate the address with the socket
    if(bind(listenfd, (sockaddr *) &serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("bind");
        exit(1);
    }

    // listen for connections on socket listenfd.
    // allow no more than 100 pending clients.
    if(listen(listenfd, 100) < 0)
    {
        perror("listen");
        exit(1);
    }

    cerr<<"\nServer started!\n";

    // wait forever for connections to come
    while(true)
    {
        cerr<<"\nWaiting for someone to connect..\n";

        // a structure to store the client address
        if((connfd = accept(listenfd, (sockaddr *)&cliAddr, &cliLen)) < 0)
        {
            perror("accept");
            exit(1);
        }

        // receive whatever the client sends
        if((numRead = read(connfd, data, sizeof(data))) < 0)
        {
            perror("read");
            exit(1);
        }

        // NULL terminate the received string
        data[numRead] = '\0';

        cerr<<"\nRECEIVED: '"<<data<<"' from the client\n";
	cout << data << endl;
	if (strcmp(data, "1") == 0 ) {
		string result = exec("uhubctl --ports 2 --action on");
		cout << result << endl;
	}
	else if (strcmp(data, "0") == 0 ) {
		string result = exec("uhubctl --ports 2 --action off");
		cout << result << endl;
	}
        //cerr<<"Bytes read: "<<numRead<<endl;

        // set the array to all zeros
        memset(&data, 0, sizeof(data));

        // retrieve the time
        time_t rawtime;
        time(&rawtime);
        struct tm* timeinfo;
        timeinfo = localtime(&rawtime);
        strftime(data, sizeof(data),"%a, %b %d %Y, %I:%M:%S %p", timeinfo);

        cerr<<"\nSENDING: '"<<data<<"' to the client\n";

        // send the client a message
        if(write(connfd, data, strlen(data)+1) < 0)
        {
            perror("write");
            exit(1);
        }

        // close the socket
        close(connfd);
    }

    return 0;
}// http://programmingnotes.org/
