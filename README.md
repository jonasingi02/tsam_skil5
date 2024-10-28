# TSAM Group 43: Chat Server and Client

This project is a networked chat server and client for the T-409-TSAM Computer Networks course. The server listens on a specified port for client connections and supports multiple commands for communication and server interaction.

## Table of Contents
1. [Compilation](#compilation)
2. [Running the Server and Client](#running-the-server-and-client)
3. [Implemented Commands](#implemented-commands)
4. [Server Behavior](#server-behavior)

---

### 1. Compilation

To compile the server and client, navigate to the project directory and use the provided `Makefile`:

```bash
make
```

The `Makefile` will compile both the server and client executables. The following binaries will be created:

- `tsamgroup43`: The server executable
- `client`: The client executable

For ARM64 systems, the `Makefile` is set up to detect the architecture and compile with the appropriate flags. If needed, edit the `Makefile` to adjust compiler flags or target architecture.

To clean up compiled binaries, run:
make clean


### 2. Running the Server and Client

#### Running the Server

To start the server, run:
./tsamgroup43 <port_number>
- `<port_number>` is the port on which the server will listen for incoming client connections.

#### Running the Client

To connect a client to the server, run:
./client <server_ip> <port_number>
- `<server_ip>` is the IP address of the server.
- `<port_number>` is the port number on which the server is listening.

---

### 3. Implemented Commands

The following commands are supported by the server and can be issued by the client. Each command is sent in a specific format, and the server responds as described below.

- **HELO `<text>`**: 
  - **Client Command**: Sends a HELO message with a text string to the server.
  - **Server Response**: Responds with a greeting message containing the text string, client IP, port, and the server's unique ID.

- **SERVERS**:
  - **Client Command**: Requests a list of all active servers connected to this server.
  - **Server Response**: Returns a list of server IDs and their addresses.

- **KEEPALIVE**:
  - **Client Command**: Sends a heartbeat signal to let the server know the client is active.
  - **Server Response**: Acknowledges the KEEPALIVE and resets any disconnection timeout for the client.

- **GETMSGS**:
  - **Client Command**: Requests all messages stored on the server for the client.
  - **Server Response**: Sends a list of all messages the server has for the client.

- **SENDMSG `<recipient>` `<message>`**:
  - **Client Command**: Sends a message to a specific recipient.
  - **Server Response**: If the recipient exists, the server forwards the message; otherwise, it responds with an error message.

- **STATUSREQ**:
  - **Client Command**: Requests the status of the server.
  - **Server Response**: Returns the server's status information, such as uptime, connected clients, and server load.

- **STATUSRESP**:
  - **Server Event**: The server automatically sends status updates to clients when certain conditions are met (e.g., server overload).

*Note*: This setup only includes specific commands required for the assignment; additional commands like `MSG ALL` or `MSG <name>` are not implemented.

---

### 4. Server Behavior

The server listens for client connections and manages communication based on the commands it receives. Here’s how it handles various events and commands:

- **New Client Connections**: When a new client connects, the server assigns it a unique ID and maintains the connection in a socket set for monitoring.
  
- **HELO Command**: The server acknowledges the HELO command by sending a message that includes details about the server and client.
  
- **Message Storage and Retrieval**: When a client sends a message to another client, the server stores it for delivery. Messages can be retrieved using the `GETMSGS` command.

- **Heartbeat Handling**: The server expects periodic `KEEPALIVE` signals from connected clients to ensure they are active. If a client doesn’t send a `KEEPALIVE` within a certain timeframe, the server may disconnect the client.

- **Status Requests**: The `STATUSREQ` command allows clients to query the server’s current status, which includes uptime, load, and connected client details.

- **Disconnection Handling**: If a client disconnects, the server removes it from its active client list, and any undelivered messages may be discarded.

--- 

### 5. Other Notes

The server logs all client interactions to a file (`server_log.txt`) in append mode. This log can be used for debugging and auditing communication.

## Wireshark Trace for Client-Server Communication

The following steps were used to capture the communication between the client and server:

1. **Setup**: Wireshark was used to monitor the network interface on which the server was running.
2. **Capture**: All client commands were executed sequentially to initiate communication with the server.
3. **Filtering**: The capture was filtered using `tcp.port == 4021` to isolate relevant packets.
4. **Saving**: The filtered capture was saved as `client_server_trace.pcap`

The captured trace file is included with this submission, demonstrating the following commands:
- **LISTSERVERS**
- **SENDMSG**
- **GETMSG**

### Viewing the Trace

To view the trace, open `client_server_trace.pcap` in Wireshark. Filter by `tcp.port == 4021` to see communication specific to the server.