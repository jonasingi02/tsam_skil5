//
// Simple chat client for TSAM-409
//
// Command line: ./chat_client 4000 
//
// Author: Jacky Mallett (jacky@ru.is)
//
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctime>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <cstring>
#include <vector>
#include <thread>
#include <iostream>
#include <regex>
#include <sstream>
#include <thread>
#include <map>

// Threaded function for handling responss from server

const char SOH = '\x01'; // Start of Header character
const char EOT = '\x04'; // End of Transmission character

void listenServer(int serverSocket)
{
    int nread;                                  // Bytes read from socket
    char buffer[1025];                          // Buffer for reading input

    while(true)
    {
       memset(buffer, 0, sizeof(buffer));
       nread = read(serverSocket, buffer, sizeof(buffer));

       if(nread == 0)                      // Server has dropped us
       {
          printf("Over and Out\n");
          close(serverSocket);
          exit(0);
       }
       else if(nread > 0)
       {
          printf("%s\n", buffer);
       }
    }
}

void printTimestamp() {
    // Get current time
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    // Print time in a readable format
    std::cout << "[" << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S") << "] ";
}

void get_message_from_server(int sock, const std::string& group_id) {
    // Send the GETMSG command to the server
    std::string command = "\x01GETMSG," + group_id + "\x04";
    send(sock, command.c_str(), command.size(), 0);

    // Log sent command
    printTimestamp();
    std::cout << "Sent: " << "GETMSG," << group_id << std::endl;

    // Receive server's response
    char buffer[5000];
    int valread = read(sock, buffer, 5000);
    if (valread > 0) {
        buffer[valread] = '\0';
        printTimestamp();
        std::cout << "Received: " << buffer << std::endl;
    }
}

void send_messege_to_group()

int main(int argc, char* argv[])
{
   struct addrinfo hints, *svr;              // Network host entry for server
   struct sockaddr_in serv_addr;           // Socket address for server
   int serverSocket;                         // Socket used for server 
   int nwrite;                               // No. bytes written to server
   char buffer[1025];                        // buffer for writing to server
   bool finished;                   
   int set = 1;                              // Toggle for setsockopt

   if(argc != 3)
   {
        printf("Usage: chat_client <ip  port>\n");
        printf("Ctrl-C to terminate\n");
        exit(0);
   }

   hints.ai_family   = AF_INET;            // IPv4 only addresses
   hints.ai_socktype = SOCK_STREAM;

   memset(&hints,   0, sizeof(hints));

   if(getaddrinfo(argv[1], argv[2], &hints, &svr) != 0)
   {
       perror("getaddrinfo failed: ");
       exit(0);
   }

   struct hostent *server;
   server = gethostbyname(argv[1]);

   if (!server) {
        perror("Error, no such host.");
        exit(0);
    }

    bzero((char*)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy((char*)&serv_addr.sin_addr.s_addr, (char*)server->h_addr, server->h_length);
    serv_addr.sin_port = htons(atoi(argv[2]));

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket < 0)
    {
        perror("Error opening socket.");
        exit(0);
    }

   // Turn on SO_REUSEADDR to allow socket to be quickly reused after 
   // program exit.

   if(setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
   {
       printf("Failed to set SO_REUSEADDR for port %s\n", argv[2]);
       perror("setsockopt failed: ");
   }

   
   if(connect(serverSocket, (struct sockaddr *)&serv_addr, sizeof(serv_addr) )< 0)
   {
       // EINPROGRESS means that the connection is still being setup. Typically this
       // only occurs with non-blocking sockets. (The serverSocket above is explicitly
       // not in non-blocking mode, so this check here is just an example of how to
       // handle this properly.)
       if(errno != EINPROGRESS)
       {
         printf("Failed to open socket to server: %s\n", argv[1]);
         perror("Connect failed: ");
         close(serverSocket);
         exit(0);
       }
   }

    // Listen and print replies from server
   std::thread serverThread(listenServer, serverSocket);

   finished = false;
   while(!finished)
   {
       bzero(buffer, sizeof(buffer));

       fgets(buffer, sizeof(buffer), stdin);
       buffer[strcspn(buffer, "\n")] = 0; // Remove newline character

       if (strncmp(buffer, "/quit", 5) == 0)
        {
            finished = true;
            printf("Exiting chat...\n");
            close(serverSocket);  
            break;
        }
       
       // WRAP WITH SOH AND EOT AND SEND
       std::string formattedMessage = std::string(1, SOH) + buffer + std::string(1, EOT);
       
       std::regex getmsgRegex("^GETMSG,\\d+$");
       std::cmatch match;

       if (std::regex_match(buffer, match, getmsgRegex))
        {
            std::string group_id = match[1].str();

            get_message_from_server(serverSocket, group_id);
        }
       nwrite = send(serverSocket, formattedMessage.c_str(), formattedMessage.length(), 0);

       if(nwrite  == -1)
       {
           perror("send() to server failed: ");
           finished = true;
       }

   }

   serverThread.join();
   return 0;
}
