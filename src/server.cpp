#include <server.h>
#include <sys/select.h>
#include <algorithm>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sstream>

using namespace std;
using namespace Json;

// Here we call the Task member which is associated with this device.
void Device::action()
{
  if (mAction) (mTask->*mAction)();
}

int Device::device()
{
  return mFd;
}

Task::Task(Server *srv) : mServer(srv)
{
  mInfo["task"] = "task";
}

std::ofstream &Device::log()
{
  std::ofstream &lg(Logger::log());
  lg << ",device(" << mFd << ")";
  return lg;
}

Task::~Task() {}

std::ofstream &Task::log()
{
  std::ofstream &lg(Logger::log());
  lg << ",pid(" << server()->id()
     << "),task(" << name() << ")";
  return lg;
}

void Task::jCommand(Value msg)
{
  Value reply(msg);
  reply = server()->return_to_sender(msg);
  reply["error"] = "not implemented";
  log() << ",command("
        << msg["command"].asString()
        << "), not implemented"
        << endl;
  server()->sendPipe(reply);
}

Value Task::return_to_sender(Value msg)
{
  Value reply(msg);
  reply["to"]   = msg["from"];
  reply["from"] = msg["to"];
  // reply as yourself, not as broadcast
  if (reply["from"]["process"].asInt() == server()->broadcast())
    reply["from"]["process"] = server()->id();
  return reply;
}

Value Task::sendTo(int proc, string task)
{
  Value msg;
  msg["from"]["process"] = server()->id();
  msg["from"]["task"]    = name();
  msg["to"]["process"]   = proc;
  msg["to"]["task"]      = task;
  return msg;
}

Value Task::broadcastTo(string task)
{
  return sendTo(server()->broadcast(),task);
}

std::ofstream &PipeRing::log()
{
  std::ofstream &lg(Logger::log());
  lg << ",pipe ring error: ";
  return lg;
}

PipeRing::PipeRing(int Ns) : mNServer(Ns)
{
  mReadFd  = -1;
  mWriteFd = -1;
  mFd = new int[Ns][2];
  for (int n = 0; n < mNServer; n++) {
    if (pipe(mFd[n])) {
      log() << strerror(errno) << endl;
      mError = strerror(errno);
      return;
    }
  }
  // each pipe connects server n to n+1. We need to sort our device makrix
  // so that mFd[n][0] is what server n reads from and mFd[n][1] what it
  // should write too. We need to write to the next server in the ring,
  //
  //      mFd[n][1] -> mFd[n+1][1]
  //
  // in order to form a ring connecting all servers, the last must be the
  // start of the ring.
  int start = mFd[0][1];
  for (int n = 0; n < mNServer; n++) {
      if (n+1 < mNServer)
        mFd[n][1] = mFd[n+1][1];
      else
        mFd[n][1] = start;
  }
}

PipeRing::~PipeRing()
{
  if (mReadFd < 0 && mWriteFd < 0)
    for (int n = 0; n < mNServer; n++) {
      close(mFd[n][0]);
      close(mFd[n][1]);
    }
  if (mReadFd > 0)
    close(mReadFd);
  if (mWriteFd > 0)
    close(mWriteFd);
}

void PipeRing::atServer(int n)
{
  if (n < mNServer && n > -1) {
    // set aside in and out fd's
    mReadFd  = mFd[n][0];
    mWriteFd = mFd[n][1];
    // close all other fd's we don't need
    // in this process.
    for (int k = 0; k < mNServer; k++)
      if ( k != n ) {
        close(mFd[k][0]);
        close(mFd[k][1]);
      }
  }
}

Server::Server(int id) : mId(id), Task(this)
{
  name("server");
  add(this);
  mKeepRunning = true;
}

Server::Server(int id, PipeRing &ring) : mId(id), Task(this)
{
  name("server");
  add(this);
  mKeepRunning = true;
  mReadPipe = new Device(this,ring.readFd(),Device::Read);
  mReadPipe->setAction((Action)&Server::recvPipe);
  add(mReadPipe);
  mWritePipe = ring.writeFd();
  mBroadcast = ring.size();
}

// Message structure has several required fields. These are the "to" and "from"
// addresses. An address is an object with a "process" field as an integer and
// a "task" field as a string. Thus,
//
//    msg["to"]   = {"process" : 1, "task" : "radio"}
//    msg["from"] = {"process" : 0, "task": "shell-xxxxxx"}
//
// any fields may be added to msg and all additional fields are copied verbatim.
// Every task must have a jCommand(Value) member to parse and act on pipe-ring
// messages. Messages sent to mBroadcast are sent to all.
void Server::recvPipe()
{
  int nb;
  char buffer[4096];
  Json::Value msg;
  fill(buffer,buffer+4096,0x00);
  nb = read(mReadPipe->device(),buffer,4096);
  if (nb > 0) {
    stringstream str(buffer);
    str >> msg;
    // Pass the message on if you didn't send it.
    if (msg["from"]["process"].asInt() != mId)
      nb = write(mWritePipe,buffer,nb);
  }
  int to = msg["to"]["process"].asInt();
  if (to == mId || to == mBroadcast) {
    Task *tsk = task(msg["to"]["task"].asString());
    if (tsk == NULL) {
      Value reply = return_to_sender(msg);
      reply["from"]["task"] = name();
      reply["from"]["process"] = id();
      reply["error"] = "task not found";
      sendPipe(reply);
      return;
    }
    tsk->jCommand(msg);
  }
}

void Server::sendPipe(Json::Value msg)
{
  int nb;
  stringstream sstr;
  string str;
  sstr << msg;
  str = sstr.str();
  nb = write(mWritePipe,str.c_str(),str.size());
  if (nb < 0) {
    log() << "pipe ring error: " << strerror(errno) << endl;
    mError = strerror(errno);
  }
}

void Server::jCommand(Value msg)
{
  if (msg.isMember("command")) {
    string cmd = msg["command"].asString();
    if (cmd == "quit") {
      gameOver();
      return;
    }
    if (cmd == "lt") {
      Value reply;
      reply = return_to_sender(msg);
      int n(0);
      for (auto c = mTasks.begin(); c != mTasks.end(); c++)
      {
        reply["tasks"][n++] = c->first;
      }
      sendPipe(reply);
      return;
    }
  }
}

Server::~Server()
{
}

void Server::add(Device *dev)
{
  mPending.push_back(dev);
}

void Server::add(Task *tsk)
{
  mTasks[tsk->name()] = tsk;
}

void Server::del(Device *dev)
{
  mToDelete.push_back(dev);
}

void Server::del(Task* tsk)
{
  for (auto tk = mTasks.begin(); tk != mTasks.end(); tk++) {
    if (tk->second == tsk) {
      mTasks.erase(tk);
    }
  }
}

Task *Server::task(string name)
{
  TaskList::iterator tsk = mTasks.find(name);
  if (tsk == mTasks.end())
    return NULL;
  else
    return tsk->second;
}

void Server::gameOver()
{
  mKeepRunning = false;
}

void Server::run()
{
  int    ret;
  fd_set rlst;
  fd_set wlst;
  int    maxfd;
  while (mKeepRunning) {
    FD_ZERO(&rlst);
    FD_ZERO(&wlst);
    // Device actions will change the device list.
    // This really screws with the device loops.
    // The same will be true for deletions.
    while (!mPending.empty()) {
      push_back(mPending.back());
      mPending.pop_back();
    }
    while (!mToDelete.empty()) {
      Device *dev = mToDelete.back();
      iterator plc = find(begin(),end(),dev);
      if (plc != end())
        erase(plc);
      mToDelete.pop_back();
      delete dev;
    }
    // Fill the device read and write lists. i
    maxfd = 0;
    for (auto dev = begin(); dev != end(); dev++) {
      // Must skip invalid device numbers.
      if ((*dev)->device() < 0)
         continue;
      if ((*dev)->type() == Device::Read) {
        FD_SET((*dev)->device(),&rlst);
        if (maxfd < (*dev)->device())
          maxfd = (*dev)->device();
      }
      if ((*dev)->type() == Device::Write) {
        FD_SET((*dev)->device(),&wlst);
        if (maxfd < (*dev)->device())
          maxfd = (*dev)->device();
      }
    }
    ret = select(maxfd+1,&rlst,&wlst,NULL,NULL);
    if (ret < 0) {
      log() << "Fatal error: " << strerror(errno) << endl;
      gameOver();
     }
    for (auto dev = begin(); dev != end(); dev++) {
      if ((*dev)->type() == Device::Read) {
        if (FD_ISSET((*dev)->device(),&rlst)) {
          (*dev)->action();
        }
      }
      if ((*dev)->type() == Device::Write) {
        if (FD_ISSET((*dev)->device(),&wlst)) {
          (*dev)->action();
        }
      }
    }
  }
}
