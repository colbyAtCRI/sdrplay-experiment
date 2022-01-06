#pragma once
#include <string>
#include <sstream>
#include <string.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>

/******
      Networking functions
 *****/
std::string getEthernetAddress(std::string);

//! \brief Set up a socket on the port to listen for incoming TCP connections.
int TCPServerSocket(const char* port);

//! \brief Retrun a TCP connection socket given a connection request
//! on a given server listening socket.
//!
//! The address and port of the remote host are put into `ipaddr` and `port`
int TCPConnection(int,std::string &host,std::string &port);

//! \brief Establishes a socket for UDP communication to the given IP address and port. Global.
//!
//! UDP sockets are used for data transfer with GigE Vision cameras. Ether addrstr
//! or portstr may be NULL but not both. Usage:
//!
//!  - addrstr == NULL, port = "1535" for specific service
//!  - addrstr == "192.168.1.77", port = NULL for system selected service
//!
//! The case with both specified is valid but not used.
int UDPSocket(const char* ipaddr, const char* port);
int TCPConnectToServer(const char* ipaddr, uint32_t port);

//! Encapsulate some of the arpa things into a single struct
struct IPAddress
{
  IPAddress();
  IPAddress(std::string ip, std::string prt); //!< Fill the structure based on the strings
  struct sockaddr_in  addr; //!< Standard arpa-style structure
  socklen_t           len;
  struct sockaddr    *operator()() { return (struct sockaddr*)&addr; }
  socklen_t          &size()       { return len; }
  std::string         addrStr(); //!< Report address as a string.
  std::string         addrPort(); //!< Report port as a string
};
