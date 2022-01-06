#include "network.h"
#include <iostream>
using namespace std;

IPAddress::IPAddress()
{
  fill((char*)&addr,(char*)&addr+sizeof(addr),0x00);
  len = sizeof addr;
}

IPAddress::IPAddress(std::string ip, std::string prt)
{
  // Functions called here are generally from <arpa/inet.h>
  fill((char*)&addr,(char*)&addr+sizeof(addr),0x00);
  len = sizeof addr;
  addr.sin_family = AF_INET; // Address family, IPv4
  inet_pton(AF_INET,ip.c_str(),&addr.sin_addr); // Convert string ip into addr structure
  addr.sin_port = htons(atoi(prt.c_str())); // Convert port to network byte order
}

// Spit out IP address as a string
string IPAddress::addrStr()
{
  string ret;
  char   buffer[20];
  ret = inet_ntop(AF_INET,&(addr.sin_addr),buffer,20);
  ret = buffer;
  return ret;
}

// Spit out port as a string
string IPAddress::addrPort()
{
  stringstream prt;
  prt << ntohs(addr.sin_port);
  return prt.str();
}

// #COM bbj: This function has no prototype, and it's a global. Pushes the boundaries of style.
// #COM bbj: Also, its name is frighteningly close to IPAddress::addrStr()
string addrToStr(uint8_t p[])
{
  stringstream num;
  num << (int)p[0] << "."
      << (int)p[1] << "."
      << (int)p[2] << "."
      << (int)p[3];
  return num.str();
}

// #COM bbj: This function has no prototype, and it's a global. Pushes the boundaries of style.
// #COM bbj: This function doesn't appear to be used! Bite me asshole. You're commenting on
// #COM pcc: that's still becoming.
// #COM bb:  Touche', Brute'.
string getEthernetAddress(string iface)
{
  string myaddress;
  string ifname;
  struct ifaddrs *ifaddr, *ifa;
  if ( getifaddrs(&ifaddr) < 0 ) {
    return myaddress;
  }
  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    // Skip dead ones
    if ( ifa->ifa_addr == NULL )
      continue;
    // Skip bad families
    if ( ifa->ifa_addr->sa_family != AF_INET )
      continue;
    // Grab the first interface that looks like it might be ethernet, i.e. eth0,
    // eth1 or em0, em1 I think this works on most systems (ubuntu, raspian, darwin(mac))
    // The issue is that we need to tell the GigE Vision device the AF_INET address
    // and port to stream to. Telling it to stream to the loop back, 127.0.0.1, clearly
    // won't work. On the other hand, any valid interface should work but we don't want
    // the packets sent to wifi, (aka w???n), for example.
    ifname = ifa->ifa_name;
    if ( (ifname != "" && ifname == iface) || ifname[0] == 'e' ) {
      myaddress = addrToStr((uint8_t*)&((struct sockaddr_in*)ifa->ifa_addr)->sin_addr);
      break;
    }
  }
  freeifaddrs(ifaddr);
  return myaddress;
}


// UDP sockets are used for data transfer with GigE Vision cameras. Ether addrstr
// or portstr may be NULL but not both. Usage:
//
//  addrstr == NULL, port = "1535" for specific service
//  addrstr == "192.168.1.77", port = NULL for system selected service
//
// the case with both specified is valid but not used.
int UDPSocket(const char *addrstr, const char *portstr)
{
  struct addrinfo    hints;
  struct addrinfo   *results, *rp;
  int sock;
  int bc(1);

  // We've set the ip address to the local
  // ethernet one but we have no port to
  // call our own. getaddrinfo should return
  // some candidates.
  fill((char*)&hints,(char*)&hints+sizeof(addrinfo),0x00);

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_protocol = 0;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  // Find all address/port combos that match addrstr
  // and portstr, subject to constraints in `hints`.
  if ( getaddrinfo(addrstr,portstr,&hints,&results) != 0 ) {
    return -1;
  }

  // Look through the various possibilities and return
  // the first we are able to name or bind to.
  // #COM bbj - How do we know it will be the first one?
  // #COM bbj - Wouldn't it be better to check each to see if it is right?
  // #COM pcc - This was copied from some bigbird looks at sockets
  // #COM pcc - and is correct. sock < 0 says this ain't working try the
  // #COM pcc - next (hence the continue). bind fails close it and continue.
  for (rp = results; rp != NULL; rp = rp->ai_next) {
    sock = socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol);
    if ( sock < 0 )
      continue;
    if (::bind(sock,rp->ai_addr,rp->ai_addrlen) == 0 )
      break;
    close(sock);
  }

  if ( rp == NULL ) {
    return -1;
  }

  freeaddrinfo(results);

  // Set this pig up so I can blab to the local network
  if ( setsockopt(sock,SOL_SOCKET,SO_BROADCAST,&bc,sizeof(bc)) != 0 ) {
    close(sock);
    sock = -1;
  }

  return sock;
}

int TCPConnectToServer(const char* ipaddr, uint32_t port)
{
    struct sockaddr_in address;
    int sock = 0, valread;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    // Setup the server ip address and port
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, ipaddr, &serv_addr.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }
    return sock;
}

// Setup a socket on port, port, to listen for
// incoming tcp connections.
int TCPServerSocket(const char *port)
{
  struct addrinfo hints, *servinfo, *p;
  int rv;
  int sockfd;
  int yes = 1;

  fill((char*)&hints,(char*)&hints+sizeof(hints),0x00);

  hints.ai_family   = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags    = AI_PASSIVE; // use local IP

  if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
    return -1;
  }

  // loop through all the results and bind to the first we can
  // #COM bbj: How do we know the first one is best?
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      return -1;
    }

    if (::bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      continue;
    }
    break;
  }

  if (p == NULL) {
    return -1;
  }

  freeaddrinfo(servinfo); // all done with this structure

  if (listen(sockfd,20)) {
    return -1;
  }

  return sockfd;
}

char hostBuffer[4096];
char servBuffer[4096];
// Return a TCP connection socket given a connection
// request on server listening socket, ss.
int TCPConnection(int ss, string &hostRemote, string &portRemote)
{
  int              sockfd;
  socklen_t        addrlen;
  struct sockaddr  addr;

  addrlen = sizeof(struct sockaddr);
  sockfd = accept(ss,&addr,&addrlen);
  if ( sockfd < 0 ) {
    return -1;
  }

  addrlen = sizeof(struct sockaddr);
  getpeername(sockfd,&addr,&addrlen);
  getnameinfo(&addr,addrlen,hostBuffer,4096,servBuffer,4096,AF_INET);

  // These are valid for the following server command
  hostRemote = string(hostBuffer);
  portRemote = string(servBuffer);

  return sockfd;
}
