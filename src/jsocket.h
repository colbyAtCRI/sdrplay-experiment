#pragma once
// We need radios to be able to manipulate GPIO on 
// a pi running on the network. A switch will be 
// a device that the radio task will manage. 
//
// So we have GPIOserver which runs on a pi which
// consumes and serves json objects. This is a 
// theme. So we define a jsocket for tasks to 
// use to send and respond to json object data.
#include "server.h"

using namespace std;
using namespace Json;

class JSocket : public Device
{
   int                 mSock;
   string              mServerIP;
   uint32_t            mPort;
   Reader              mReader;
   StreamWriterBuilder mJSON;
public:
    JSocket(Task *tsk, string ip, uint32_t port);
   ~JSocket();
   bool open();
   bool send(Value msg);
};

