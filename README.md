# Server

When communicating via TCP, before sending a message whose length was unknown to the other side of the communication, I sent the length of the upcoming message.
To implement the server, I opened two sockets - one TCP and one UDP - to receive messages from TCP and UDP clients respectively.

TCP Message Handling
When receiving a message on the TCP socket (client connection attempt):

  -  Check if a client with a similar ID is already connected

  - If exists: Send confirmation to client → client displays corresponding message

  -  If new: Add client to the vector of connected clients

## UDP Message Handling
When receiving UDP messages:

   - Translate/forward the message to corresponding TCP clients subscribed to the message's topic

## TCP Client Management
When receiving messages from TCP clients on their connection socket:

   - Handle subscribe/unsubscribe requests by adding/removing topics in the client's topic vector

   - On client disconnect:

      -  Mark as disconnected

      -  Close socket fd

      -  Keep in subscriber vector for potential reconnections

# Client

The client communicates with the server through a TCP socket.

## Connection Flow

   - Sends desired connection ID to server

   - Waits for server confirmation

## Post-Connection
### After successful connection:

   - Can send subscribe/unsubscribe requests to server

   - Waits for operation confirmation

     
### Key implementation detail: All message lengths are pre-sent before actual content transmission to handle variable-length messages.
