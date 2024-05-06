//****************************************************************************
//                         REDES Y SISTEMAS DISTRIBUIDOS
//                      
//                     2º de grado de Ingeniería Informática
//                       
//                        Main class of the FTP server
// 
//****************************************************************************

#include <cerrno>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

 #include <unistd.h>


#include <pthread.h>

#include <list>

#include "common.h"
#include "FTPServer.h"
#include "ClientConnection.h"

/**
 * @brief Create a TCP socket binded to a port
 * 
 * @param port port to bind the socket
 * @return socket file descriptor
*/
int define_socket_TCP(int port) {
  // Create a socket
  int sockt;
  sockt = socket(AF_INET,SOCK_STREAM, 0);
  if(sockt < 0) {
    errexit("No puedo crear el socket: %s\n", strerror(errno));
  }

  // Create the socket address 
  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY); // converts from host byte order
  address.sin_port = htons(port); // to network byte order both address an port

  // Assign (bind) the address to the socket
  if(bind(sockt, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
    errexit("No puedo hacer el bind con el puerto: %s\n", strerror(errno));
  }
  if (listen(sockt, 5) < 0) {
    errexit("Fallo en el listen: %s\n", strerror(errno));
  }
  return sockt;
}





// This function is executed when the thread is executed.
void* run_client_connection(void *c) {
    ClientConnection *connection = (ClientConnection *)c;
    connection->WaitForRequests();
    return NULL;
}


/**
 * @brief construtor of the FTPServer class
 * 
 * @param port port in which the server will operate
*/
FTPServer::FTPServer(int port) {
    this->port = port;
}


// Parada del servidor.
void FTPServer::stop() {
    close(msock);
    shutdown(msock, SHUT_RDWR);

}


// Starting of the server
void FTPServer::run() {
    struct sockaddr_in fsin;
    int ssock;
    socklen_t alen = sizeof(fsin);
    msock = define_socket_TCP(port); 
    while (1) {
	  pthread_t thread;
      ssock = accept(msock, (struct sockaddr *)&fsin, &alen);
      if(ssock < 0)
        errexit("Fallo en el accept: %s\n", strerror(errno));
	  ClientConnection *connection = new ClientConnection(ssock);
	// Here a thread is created in order to process multiple
	// requests simultaneously
	  pthread_create(&thread, NULL, run_client_connection, (void*)connection);    
    }
}
