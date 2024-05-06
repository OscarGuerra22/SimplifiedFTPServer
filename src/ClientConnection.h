#if !defined ClientConnection_H
#define ClientConnection_H

#include <pthread.h>

#include <cstdio>
#include <cstdint>

const int MAX_BUFF=1000;

class ClientConnection {
 public:
  ClientConnection(int s);
  ~ClientConnection();  
  void WaitForRequests();
  void stop();
 private:  
  bool ok;  // This variable is a flag that avois that the
	        // server listens if initialization errors occured. 
  FILE *fd;	 // C file descriptor. We use it to buffer the
		 // control connection of the socket and it allows to
		 // manage it as a C file using fprintf, fscanf, etc.
  char command[MAX_BUFF];  // Buffer for saving the command.
  char arg[MAX_BUFF];      // Buffer for saving the arguments.     
  int data_socket;         // Data socket descriptor;
  int control_socket;      // Control socket descriptor;
  bool parar;
  bool passive_;
  int s_;
};

/**
 * @brief Create a TCP socket binded to a port
 * 
 * @param port port to bind the socket
 * @return socket file descriptor
*/
int define_socket_TCP(int port);

#endif
