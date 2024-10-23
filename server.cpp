//
// Simple chat server for TSAM-409
//
// Command line: ./chat_server 4000 
//
// Author: Jacky Mallett (jacky@ru.is)
//
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <vector>
#include <list>
#include <iostream>
#include <sstream>
#include <thread>
#include <map>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <unistd.h>

// fix SOCK_NONBLOCK for OSX
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#define BACKLOG  5          // Allowed length of queue of waiting connections

const char SOH = '\x01'; // Start of Header character
const char EOT = '\x04'; // End of Transmission character


// Simple class for handling connections from clients.
// Client(int socket) - socket to send/receive traffic from client.
class Client {
public:
    int sock;                        // socket of client connection
    std::string name;                // Client's user name
    struct sockaddr_in addr;         // Client's address information

    Client(int socket, struct sockaddr_in address) : sock(socket), addr(address) {}

    ~Client() {}                     // Destructor for cleanup
};

struct sockaddr_in clientAddress;

// Message struct to store messages for groups
struct Message {
    std::string fromGroupID;
    std::string content;

    Message(const std::string& from, const std::string& msg) 
        : fromGroupID(from), content(msg) {}
};

// Map to store messages for each group
std::map<std::string, std::queue<Message>> messageQueue;



// Note: map is not necessarily the most efficient method to use here,
// especially for a server with large numbers of simulataneous connections,
// where performance is also expected to be an issue.
//
// Quite often a simple array can be used as a lookup table, 
// (indexed on socket no.) sacrificing memory for speed.

std::map<int, Client*> clients; // Lookup table for per Client information

// Open socket for specified port.
//
// Returns -1 if unable to create the socket for any reason.

int open_socket(int portno)
{
   struct sockaddr_in sk_addr;   // address settings for bind()
   int sock;                     // socket opened for this port
   int set = 1;                  // for setsockopt

   // Create socket for connection. Set to be non-blocking, so recv will
   // return immediately if there isn't anything waiting to be read.
#ifdef __APPLE__     
   if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
      perror("Failed to open socket");
      return(-1);
   }
#else
   if((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
   {
     perror("Failed to open socket");
    return(-1);
   }
#endif

   // Turn on SO_REUSEADDR to allow socket to be quickly reused after 
   // program exit.

   if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
   {
      perror("Failed to set SO_REUSEADDR:");
   }
   set = 1;
#ifdef __APPLE__     
   if(setsockopt(sock, SOL_SOCKET, SOCK_NONBLOCK, &set, sizeof(set)) < 0)
   {
     perror("Failed to set SOCK_NOBBLOCK");
   }
#endif
   memset(&sk_addr, 0, sizeof(sk_addr));

   sk_addr.sin_family      = AF_INET;
   sk_addr.sin_addr.s_addr = INADDR_ANY;
   sk_addr.sin_port        = htons(portno);

   // Bind to socket to listen for connections from clients

   if(bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0)
   {
      perror("Failed to bind to socket:");
      return(-1);
   }
   else
   {
      return(sock);
   }
}

// Close a client's connection, remove it from the client list, and
// tidy up select sockets afterwards.
void closeClient(int clientSocket, fd_set *openSockets, int *maxfds)
{

     printf("Client closed connection: %d\n", clientSocket);

     // If this client's socket is maxfds then the next lowest
     // one has to be determined. Socket fd's can be reused by the Kernel,
     // so there aren't any nice ways to do this.

     close(clientSocket);      

     if(*maxfds == clientSocket)
     {
        for(auto const& p : clients)
        {
            *maxfds = std::max(*maxfds, p.second->sock);
        }
     }

     // And remove from the list of open sockets.

     FD_CLR(clientSocket, openSockets);

}

// Store a message in the message queue for a group
void storeMessage(const std::string& toGroupID, const std::string& fromGroupID, const std::string& content) {
    Message newMessage(fromGroupID, content);
    messageQueue[toGroupID].push(newMessage);
    std::cout << "Stored message for group " << toGroupID << ": " << content << std::endl;
}

// Get all messages for a group from the message queue
std::vector<Message> getMessages(const std::string& groupID) {
    std::vector<Message> messages;

    // Check if the group has any messages in the queue and add them to the messages vector
    if(messageQueue.find(groupID) != messageQueue.end()) {
        while(!messageQueue[groupID].empty()) {
            messages.push_back(messageQueue[groupID].front());
            messageQueue[groupID].pop();
        }
    }
    return messages;
}

// Get the number of messages in the message queue for a group
int getMessageCount(const std::string& groupID) {
    if(messageQueue.find(groupID) != messageQueue.end()) {
        return messageQueue[groupID].size();
    }
    return 0;
}

// Get the current timestamp in the format "YYYY-MM-DD HH:MM:SS"
std::string getTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}
// Log the command received from a client
void logCommand(int clientSocket, const std::string& command) {
    std::string timestamp = getTimestamp();
    std::cout << "[" << timestamp << "] Client " << clientSocket << ": " << command << std::endl;
}

// Join a vector of strings into a single string
std::string join(const std::vector<std::string>& vec, const std::string& delimiter) {
    std::ostringstream oss;
    for (size_t i = 0; i < vec.size(); ++i) {
        oss << vec[i];
        if (i != vec.size() - 1) {
            oss << delimiter;
        }
    }
    return oss.str();
}

// Construct a command with a command name and parameters also with SOH and EOT characters
std::string constructCommand(const std::string& command, const std::vector<std::string>& params) {
    std::string cmd = std::string(1, SOH) + command;

    if (!params.empty()) {
        cmd += "," + join(params, ","); 
    }

    cmd += EOT; // Append the End of Transmission character
    return cmd;
}



// Process command from client on the server
void clientCommand(int clientSocket, fd_set *openSockets, int *maxfds, 
                  char *buffer) 
{
  if (buffer[0] != SOH || buffer[strlen(buffer) - 1] != EOT) {
      std::cerr << "Invalid command format: missing SOH or EOT." << std::endl;
      return; // Exit if the format is incorrect
  }  

  // Create a string from the buffer and remove SOH and EOT
  std::string command(buffer + 1, strlen(buffer) - 2); // Exclude SOH and EOT
  std::vector<std::string> tokens;
  std::string token;  

  // Split command into tokens for parsing
  std::stringstream stream(command);
  while (std::getline(stream, token, ',')) {
      tokens.push_back(token); // Split by comma
  }

  // Log command
  logCommand(clientSocket, buffer);

  // Close the socket
  if (tokens[0].compare("LEAVE") == 0)
  {
      // Close the socket, and leave the socket handling
      // code to deal with tidying up clients etc. when
      // select() detects the OS has torn down the connection.
 
      closeClient(clientSocket, openSockets, maxfds);
  }
  // First message sent by server
  else if(tokens[0].compare("HELO") == 0 && tokens.size() == 2)
  {
    // Check if the client exists
    auto it = clients.find(clientSocket);
    if (it != clients.end()) {
        // Client found, get the client info
        Client* client = it->second;

        std::string response = "SERVERS,";
        response += "A5 " + std::to_string(client->sock) + "," + 
                    inet_ntoa(client->addr.sin_addr) + "," + 
                    std::to_string(ntohs(client->addr.sin_port)); // Port needs to be converted

        // Append additional 1-hop server connections
        for (auto const& pair : clients)
        {
            if (pair.second->sock != client->sock)
            {
                response += ";" + std::to_string(pair.second->sock) + "," + 
                            inet_ntoa(pair.second->addr.sin_addr) + "," + 
                            std::to_string(ntohs(pair.second->addr.sin_port)); // Convert port
            }
        }

        send(clientSocket, response.c_str(), response.length(), 0);
    } else {
        std::cerr << "Client not found for HELO response." << std::endl;
    }
  }

  // List all connected servers
  else if(tokens[0].compare("LISTSERVERS") == 0)
  {
    std::string response = "SERVERS,";
    bool first = true; // To handle the first entry differently

    // Append all server connections 
    for (auto const& pair : clients)
    {
        if (!first)
        {
            response += ";"; 
        }
        first = false;

        response += "A5 " + std::to_string(pair.second->sock) + "," + 
                    inet_ntoa(pair.second->addr.sin_addr) + "," + 
                    std::to_string(ntohs(pair.second->addr.sin_port)); // Convert port
    }

    send(clientSocket, response.c_str(), response.length(), 0);
  }
  // Send a message to a group
  else if(tokens[0].compare("SENDMSG") == 0 && tokens.size() >= 4)
  {
    std::string toGroupID = tokens[1];
    std::string fromGroupID = tokens[2];
    
    // Construct the message from the tokens starting from index 3
    std::string message;
    for(auto i = tokens.begin() + 3; i != tokens.end(); i++) 
    {
        message += *i + " ";
    }

    // Remove the trailing space if message is not empty
    if (!message.empty()) {
        message.pop_back();
    }

    // Construct the full SENDMSG command
    std::string sendMsgCommand = "SENDMSG," + toGroupID + "," + fromGroupID + "," + message;

    // Check if the command exceeds the 5000-byte limit
    if (sendMsgCommand.length() > 5000)
    {
        std::cerr << "SENDMSG command exceeds the 5000-byte limit." << std::endl;

        // Send an error message back to the client
        std::string errorMsg = "Error: Message exceeds the 5000-byte limit.";
        send(clientSocket, errorMsg.c_str(), errorMsg.length(), 0);
        return; // Exit to avoid sending the message 
    }

    // Store the message for the target group if within limits
    storeMessage(toGroupID, fromGroupID, message);
  }

  else if(tokens[0].compare("GETMSGS") == 0 && tokens.size() == 2)
  {
    std::string groupID = tokens[1];
    std::vector<Message> messages = getMessages(groupID);

    // Send the messages back to the client
    for(const auto& msg : messages)
    {
        std::string response = "From " + msg.fromGroupID + ": " + msg.content;
        send(clientSocket, response.c_str(), response.length(), 0);
    }
  }

  else if(tokens[0].compare("KEEPALIVE") == 0 && tokens.size() == 2)
  {
    std::string toGroupID = tokens[1];
    int pendingCount = getMessageCount(toGroupID);

    std::string response = "KEEPALIVE," + std::to_string(pendingCount);
    send(clientSocket, response.c_str(), response.length(), 0);
  }

  else
  {
      std::cout << "Unknown command from client:" << buffer << std::endl;
  }
     
}

// Assume you have filled clientAddress with the appropriate information


int main(int argc, char* argv[])
{
    bool finished;
    int listenSock;                 // Socket for connections to server
    int clientSock;                 // Socket of connecting client
    fd_set openSockets;             // Current open sockets 
    fd_set readSockets;             // Socket list for select()        
    fd_set exceptSockets;           // Exception socket list
    int maxfds;                     // Passed to select() as max fd in set
    struct sockaddr_in client;
    socklen_t clientLen;
    char buffer[1025];              // buffer for reading from clients

    if(argc != 2)
    {
        printf("Usage: chat_server <ip port>\n");
        exit(0);
    }

    // Setup socket for server to listen to

    listenSock = open_socket(atoi(argv[1])); 
    printf("Listening on port: %d\n", atoi(argv[1]));

    if(listen(listenSock, BACKLOG) < 0)
    {
        printf("Listen failed on port %s\n", argv[1]);
        exit(0);
    }
    else 
    // Add listen socket to socket set we are monitoring
    {
        FD_ZERO(&openSockets);
        FD_SET(listenSock, &openSockets);
        maxfds = listenSock;
    }

    finished = false;

    while(!finished)
    {
        // Get modifiable copy of readSockets
        readSockets = exceptSockets = openSockets;
        memset(buffer, 0, sizeof(buffer));

        // Look at sockets and see which ones have something to be read()
        int n = select(maxfds + 1, &readSockets, NULL, &exceptSockets, NULL);

        if(n < 0)
        {
            perror("select failed - closing down\n");
            finished = true;
        }
        else
        {
            // First, accept  any new connections to the server on the listening socket
            if(FD_ISSET(listenSock, &readSockets))
            {

               clientSock = accept(listenSock, (struct sockaddr *)&client,
                                   &clientLen);
               printf("accept***\n");
               // Add new client to the list of open sockets
               FD_SET(clientSock, &openSockets);

               // And update the maximum file descriptor
               maxfds = std::max(maxfds, clientSock) ;

               // create a new client to store information.

               clients[clientSock] = new Client(clientSock, client);

               // Decrement the number of sockets waiting to be dealt with
               n--;

               printf("Client connected on server: %d\n", clientSock);
            }
            // Now check for commands from clients
            std::list<Client *> disconnectedClients;  
            while(n-- > 0)
            {
               for(auto const& pair : clients)
               {
                  Client *client = pair.second;

                  if(FD_ISSET(client->sock, &readSockets))
                  {
                      // recv() == 0 means client has closed connection
                      if(recv(client->sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0)
                      {
                          disconnectedClients.push_back(client);
                          closeClient(client->sock, &openSockets, &maxfds);

                      }
                      // We don't check for -1 (nothing received) because select()
                      // only triggers if there is something on the socket for us.
                      else
                      {
                          std::cout << buffer << std::endl;
                          clientCommand(client->sock, &openSockets, &maxfds, buffer);
                      }
                  }
               }
               // Remove client from the clients list
               for(auto const& c : disconnectedClients)
                  clients.erase(c->sock);
            }
        }
    }
}
