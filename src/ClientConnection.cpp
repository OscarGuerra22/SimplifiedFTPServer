//****************************************************************************
//                         REDES Y SISTEMAS DISTRIBUIDOS
//                      
//                     2º de grado de Ingeniería Informática
//                       
//              This class processes an FTP transaction.
// 
//****************************************************************************



#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <cerrno>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <sys/stat.h> 
#include <iostream>
#include <dirent.h>

#include<random>
#include<algorithm>

#include "common.h"

#include "ClientConnection.h"




ClientConnection::ClientConnection(int s) {
  int sock = (int)(s);

  char buffer[MAX_BUFF];

  control_socket = s;
  fd = fdopen(s, "a+"); // Associates a file stream to the given file 
                        // descriptor (s) in read and append mode.
  if (fd == NULL) {
    std::cout << "Connection closed" << std::endl;
    fclose(fd);
    close(control_socket);
    ok = false;
    return;
  }
  
  ok = true;
  data_socket = -1;
  parar = false;
  passive_ = false;
  s_ = s;
};


ClientConnection::~ClientConnection() {
 	fclose(fd);
	close(control_socket);
}

int connect_TCP( uint32_t address,  uint16_t  port) {
  // Create the socket address 
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(address);
  addr.sin_port = htons(port); 

  int sockt = socket(AF_INET, SOCK_STREAM, 0);
  if(sockt < 0) {
    printf("No se puede crear el socket: %s\n", strerror(errno));
    return -1;
  }
  if(connect(sockt, reinterpret_cast<const sockaddr*>(&addr), 
    sizeof(addr)) < 0) {
    printf("No se puede conectar con el cliente: %s\n", strerror(errno));
    return -1;
  }
  return sockt;
}

void ClientConnection::stop() {
  close(data_socket);
  close(control_socket);
  parar = true;
}
    
unsigned GetRandomPort() {
  const unsigned first_unpriviliged_port{1024};
  const unsigned last_port{65535};
  const unsigned unprivigiled_ports_num{first_unpriviliged_port - last_port};
  return first_unpriviliged_port + (rand() % unprivigiled_ports_num);
}

bool getMyIP(std::string& ip) {
  bool error{false};
  char hostname[256];
  if (gethostname(hostname, sizeof(hostname)) != 0) error = true;

  struct hostent* host_info;
  if ((host_info = gethostbyname(hostname)) == nullptr) error = true;

  // Check the address is an IPv4 address
  if (host_info->h_addrtype != AF_INET) error = true;

  // Convert the address to a string
  char ip_address[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, host_info->h_addr_list[0], ip_address, 
  sizeof(ip_address));

  ip = ip_address;
  std::replace(ip.begin(), ip.end(), '.', ',');

  if (error) return false;
  return true;
}

/**
 * @brief receive the data from a socket into a buffer
 * 
 * @return true if the operation ended correctly
*/
bool ReceiveFromSocket(int fd, std::vector<uint8_t>& message) {
  auto result{recv(fd, message.data(), message.size(), 0)};
  if (result < 0) {
    return false;
  }
  message.resize(result);
  return true;
}

/**
 * @brief send the data from a buffer through a socket
 * 
 * @return true if the operation ended correctly
*/
bool SendToSocket(int fd, std::vector<uint8_t>& message) {
  auto result{send(fd, message.data(), message.size(), 0)};
  if (result < 0) {
    return false;
  }
  return true;
}

/**
 * @brief write the content on a buffer into a file
 *  
 * @return true if the operation ended correctly 
*/
bool WriteFile(const int fd, std::vector<uint8_t>& message) {
  if (message.size() == 0) return true;
  auto bytes_written = write(fd, message.data(), message.size());
  if (bytes_written < 0) {
    printf("\n[!] Error al escribir el archivo: %s", strerror(errno));
    return false;
  }
  return true;
}

/**
 * @brief read a file into a buffer
 * 
 * @return true if the operation ended correctly
 */
bool ReadFile(const int fd, std::vector<uint8_t>& message) {
  auto bytes_read = read(fd, message.data(), message.size());
  if (bytes_read < 0) return false;
  message.resize(bytes_read);
  return true;
}

/**
 * @brief list all files and subdirectories in a given directory
 * 
 * @param name name of the directory
 * @param fd FILE* to print the list
 * @return true if everything ended well
*/
bool ListDir(const std::string& name, FILE* fd) {
  DIR* dir;
  struct dirent *ent;
  if ((dir = opendir(name.c_str())) != NULL) {
    while ((ent = readdir (dir)) != NULL) {
      char* entry{ent->d_name};
      fprintf (fd, "%s\r\n", entry);
    }
    fflush(fd);
    closedir (dir);
    return true;
  } else {
      return false;
  }
}

/**
 * @brief list properties of a given file
 * 
 * @param name name of the file
 * @param fd FILE* to print the list
 * @return true if everything ended well
*/
bool ListFile(const std::string& name, FILE* fd) {
  struct stat path_stat;
  if (stat(name.c_str(), &path_stat) < 0) {
    return false;
  }
  fprintf(fd, "Size: %ld bytes\r\n", path_stat.st_size);
  fprintf(fd, "Permissions: %o\r\n", path_stat.st_mode & 0777);
  time_t mod_time = path_stat.st_mtime;
  std::string time_s{ctime(&mod_time)};
  time_s.pop_back();
  fprintf(fd, "Last modification: %s\r\n", time_s.c_str());
  fflush(fd);
  return true;
}

/**
 * @brief checks if a given path correspond to a file or a directory
 * 
 * @param name path to hte file/directory
 * @param error flag for indicating an error
 * @return true if it is a file
*/
bool IsAFile(const std::string& name, bool& error) {
  struct stat path_stat;
  if (stat(name.c_str(), &path_stat) < 0) {
    error = true;
    return false;
  }
  return S_ISREG(path_stat.st_mode);
}

struct ThreadData {
  FILE *file;
  char *buffer;
};

void *readFile(void *arg) {
  struct ThreadData *data = (struct ThreadData *)arg;
  fscanf(data->file, "%s", data->buffer);
  return NULL;
}


#define COMMAND(cmd) strcmp(command, cmd)==0

// This method processes the requests.
// Here you should implement the actions related to the FTP commands.
// See the example for the USER command.
// If you think that you have to add other commands feel free to do so. You 
// are allowed to add auxiliary methods if necessary.

void ClientConnection::WaitForRequests() {
  if (!ok) {
	  return;
  }
    
  fprintf(fd, "220 Service ready\n");
  
  while(!parar) {
    fscanf(fd, "%s", command);
    if (COMMAND("USER")) {
      fscanf(fd, "%s", arg);
      fprintf(fd, "331 User name ok, need password\n");
    }
    else if (COMMAND("PWD")) {}
    else if (COMMAND("PASS")) {
      fscanf(fd, "%s", arg);
      if(strcmp(arg,"1234") == 0){
          fprintf(fd, "230 User logged in\n");
      }
      else{
          fprintf(fd, "530 Not logged in.\n");
          parar = true;
      }
    }
    else if (COMMAND("PORT")) {
      fscanf(fd, "%s", arg);
      unsigned ip_v[4];
      unsigned port_v[2];
      if (sscanf(arg, "%u,%u,%u,%u,%u,%u", &ip_v[0], &ip_v[1], &ip_v[2], 
          &ip_v[3], &port_v[0], &port_v[1]) == 6) {
        uint32_t address{((ip_v[0] << 24) | (ip_v[1] << 16) | (ip_v[2] << 8) | 
                          (ip_v[3]))};
        unsigned port{(port_v[0] << 8) | port_v[1]};
        data_socket =  connect_TCP(address, static_cast<uint16_t>(port));
        if (data_socket < 0) {
          printf("Comando : %s %s\n", command, arg);
          printf("Error interno del servidor\n");
          fprintf(fd, "425 Can't open data connection.\n");
          parar = true;
          close(control_socket);
        } else 
            fprintf(fd, "200 Command okay.\n");
      } else 
          fprintf(fd, "501 Syntax error in parameters or arguments.\n");
    }
    else if (COMMAND("PASV")) {
      std::string ip{""};
      if (!getMyIP(ip)) {
        printf("Comando : %s %s\n", command, arg);
        printf("Error interno del servidor\n");
        parar = true;
      }
      unsigned port{GetRandomPort()};
      data_socket = define_socket_TCP(port);
      if (data_socket < 0) {
        printf("\n[!] No se ha podido crear el socket de datos");
        parar = true;
      }
      fprintf(fd, "227 Entering Passive Mode (%s,%u,%u).\n", ip.c_str(), 
              (port / 256), (port % 256));
      passive_ = true;
    }
    else if (COMMAND("STOR") ) {
      fscanf(fd, "%s", arg);
      int file{open(arg, O_CREAT|O_TRUNC|O_WRONLY, 0644)};
      if (file < 0) {
        printf("\n[!] No se ha podido crear el archivo\n");
        fprintf(fd, "452 Requested action not taken.\n");
        close(data_socket);
        close(file);
        continue;
      }
      fprintf(fd, "150 File creation ok, about to open data connection\n");
      fflush(fd);
      if (passive_) {
        struct sockaddr_in fsin;
        socklen_t alen = sizeof(fsin);
        int temporal_socket = accept(data_socket, (struct sockaddr *)&fsin, &alen);
        if (temporal_socket < 0) {
          printf("\n[!] No se ha podido realizar el accept\n");
          fprintf(fd, "425 Can't open data connection.\n");
          parar = true;
        }
        close(data_socket);
        data_socket = temporal_socket;
      }
      std::vector<uint8_t> buffer(64 * 1024);
      bool error{false};
      while (!buffer.empty() && !error) {
        if (!ReceiveFromSocket(data_socket, buffer) || !WriteFile(file, buffer)) {
          printf("\n[!] Error enviando el archivo\n");
          fprintf(fd, "426 Connection closed; transfer aborted.\n");
          parar = true;
          error = true;
        }
      }
      if (!error) {
        fprintf(fd, "226 Closing data connection.\n");
      }
      close(data_socket);
      close(file);
    }
    else if (COMMAND("RETR")) {
      fscanf(fd, "%s", arg);
      int file{open(arg, O_RDONLY)};
      if (file < 0) {
        fprintf(fd, "450 Requested file action not taken..\n");
        printf("\n[!] No se ha podido abrir el archivo\n");
        close(data_socket);
        close(file);
        continue;
      }
      fprintf(fd, "150 File status okay; about to open data connection\n");
      if (passive_) {
        struct sockaddr_in fsin;
        socklen_t alen = sizeof(fsin);
        int temporal_socket = accept(data_socket, (struct sockaddr *)&fsin, &alen);
        if (temporal_socket < 0) {
          printf("\n[!] No se ha podido realizar el accept\n");
          fprintf(fd, "425 Can't open data connection.\n");
          parar = true;
        }
        close(data_socket);
        data_socket = temporal_socket;
      }
      std::vector<uint8_t> buffer(64 * 1024);
      bool error{false};
      while (!buffer.empty() && !error) {
        if (!ReadFile(file, buffer) || !SendToSocket(data_socket, buffer)) {
          printf("\n[!] Error recibiendo el archivo\n");
          fprintf(fd, "426 Connection closed; transfer aborted.\n");
          parar = true;
          error = true;
        }
      }
      if (!error) {
        fprintf(fd, "226 Closing data connection.\n");
      }
      close(data_socket);
      close(file);
    }
    else if (COMMAND("LIST")) {
      if (passive_) {
        struct sockaddr_in fsin;
        socklen_t alen = sizeof(fsin);
        int temporal_socket = accept(data_socket, (struct sockaddr *)&fsin, &alen);
        if (temporal_socket < 0) {
          printf("\n[!] No se ha podido realizar el accept\n");
          fprintf(fd, "425 Can't open data connection.\n");
          parar = true;
        }
        close(data_socket);
        data_socket = temporal_socket;
      }
      FILE* data{fdopen(data_socket, "a+")};
      std::string dir_name{""};
      bool error{false};

      strcpy(arg, ".");
      struct ThreadData thread_data = {fd, arg};
      pthread_t tid;
      pthread_create(&tid, NULL, readFile, &thread_data);
      sleep(1);
      pthread_cancel(tid);
      dir_name = arg;

      if (IsAFile(dir_name, error) && !error) {
        fprintf(fd, "125 List started OK.\n");
        if (ListFile(dir_name, data)) {
          fprintf(fd, "250 List completed successfully.\n");
        } else {
            fprintf(fd, "451 Requested action aborted: local error in processing.\n");
        }
      } else if (error) {
          fprintf(fd, "451 Requested action aborted: local error in processing.\n");
      } else {
          fprintf(fd, "125 List started OK.\n");
          if (ListDir(dir_name, data)) {
            fprintf(fd, "250 List completed successfully.\n");
          } else {
              fprintf(fd, " 451 Requested action aborted: local error in processing.\n");
          }
      }
      close(data_socket);
    }
    else if (COMMAND("SYST")) {
      fprintf(fd, "215 UNIX Type: L8.\n");   
    }
    else if (COMMAND("TYPE")) {
      fscanf(fd, "%s", arg);
      fprintf(fd, "200 OK\n");   
    }
    else if (COMMAND("QUIT")) {
      fprintf(fd, "221 Service closing control connection. Logged out if appropriate.\n");
      close(control_socket);
      close(data_socket);	
      parar=true;
      break;
    }
    else  {
      fprintf(fd, "502 Command not implemented.\n"); fflush(fd);
      printf("Comando : %s %s\n", command, arg);
      printf("Error interno del servidor\n");
    }
  }
  
  fclose(fd);

  
  return;
  
};
