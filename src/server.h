#pragma once
#include <vector>
#include <list>
#include <map>
#include <string>
#include <stddef.h>
#include <json/json.h>
#include "logger.h"

class Task;
class Server;
class Device;

typedef void (Task::*Action)(void);

// Basically a unix file number to be managed by a Server
class Device : public Logger
{
public:
  enum Type {
    Read,
    Write
  };
private:
  Task  *mTask;
  int    mFd;
  Type   mType;
  Action mAction;
public:
  Device(Task *tsk, int fd, Device::Type typ)
    : mTask(tsk), mFd(fd), mType(typ), mAction(NULL) {}
 ~Device() {}
  Type type()   { return mType; }
  int  device();
  void action();
  void setAction(Action act) { mAction = act; }
  std::ofstream &log();
};

// A task is a user shell or radio or data logger or ...
class Task : public Logger
{
  Json::Value mInfo;
  std::string mName;
  Server     *mServer;
public:
      Task(Server *srv);
     ~Task();
     Server *server() { return mServer; }
     // Tasks need to be able to find other tasks and manipulate them.
     // Each server will maintain a list of these tasks. What we have
     // in mind is that each process will control a radio through a
     // RadioHead task. This way login shells will be able to access
     // radios while in operation.
     std::string name() { return mName; }
     void        name(std::string name) { mName = name; }
     // Every task can take and reply to commands so it must provide
     // a list of commands.
     Json::Value  commands();
     virtual void jCommand(Json::Value cmd);

     Json::Value  return_to_sender(Json::Value);
     Json::Value  broadcastTo(std::string task);
     Json::Value  sendTo(int proc, std::string task);

     friend class Radio;
     friend class Shell;
     friend class ShellFactory;

     std::ofstream &log();
};

typedef std::map<std::string,Task*> TaskList;
typedef std::list<Device*> DeviceList;

// We need to be able to tell all servers to do something like
// terminate or start its radio collection. We link all servers
// with a pipe ring so that server 0 talks to 1, 1 to 2 ... n-1
// to 0.

class PipeRing : public Logger
{
  int mNServer;
  int mReadFd;
  int mWriteFd;
  int (*mFd)[2];
  std::string mError;
public:
   PipeRing(int NServer);
  ~PipeRing();
  // determins which server we're at. Closes all unused file descriptors
  // keeping only the 2 relavent ones open.
  void atServer(int n);
  // Each server makes its own device connected to its own action
  int readFd() { return mReadFd; }
  // use this to talk to the next server in the ring.
  int writeFd() { return mWriteFd; }
  int size() { return mNServer; }
  std::ofstream &log();
};

// A thin layer on unix select call.
class Server : public std::list<Device*>, public Task
{
  int        mId;
  int        mBroadcast;
  bool       mKeepRunning;
  DeviceList mPending;
  DeviceList mToDelete;
  TaskList   mTasks;
  int        mWritePipe;
  Device    *mReadPipe;
  std::string mError;
public:
        Server(int id);
        Server(int id, PipeRing &);
       ~Server();
  void  setError(std::string err) { mError += err; }
  void  add(Device *dev);
  void  add(Task *tsk);
  void  del(Device *dev);
  void  del(Task *tsk);
  Task *task(std::string name);
  void  gameOver();
  void  sendPipe(Json::Value msg);
  void  recvPipe();
  int   broadcast() { return mBroadcast; }
  int   id() { return mId; }
  void  jCommand(Json::Value msg);
  void  run();
  std::ofstream &log() { return Task::log(); }
};
