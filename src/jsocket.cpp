#include "jsocket.h"
#include "network.h"

JSocket::JSocket(Task *tsk, string ip, uint32_t port) 
   : Device(tsk,-1,Device::Read), mServerIP(ip), mPort(port)
{
   mJSON.settings_["indentation"] = "";
   mJSON.settings_["commentStyle"] = "None";
   if (!open())
      log() << strerror(errno) << endl; 
}

bool JSocket::send(Value msg)
{
   int    bs;
   string sm;
   sm = writeString(mJSON,msg);
   bs = write(mSock,sm.c_str(),sm.size());
   return bs == sm.size();
}

bool JSocket::open()
{
   mSock = TCPConnectToServer(mServerIP.c_str(),mPort);
   return mSock > -1;
}; 
