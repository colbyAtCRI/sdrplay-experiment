#pragma once
#include <server.h>
#include <iostream>
#include <map>
#include <vector>

class Shell;

class WordList : public std::vector<std::string>
{
public:
  WordList(std::string cmd);
};

typedef void (Shell::*pCmd)(WordList&);

struct Command
{
  pCmd        mCmd;
  std::string mInfo;
  std::string mMore;
};

class CommandList : public std::map<std::string,Command>
{
public:
  bool have(std::string key) { return find(key) != end(); }
};

class Shell : public Task
{
public:
  Shell(Server *srv, int fdIn, int fdOut,std::string port);
 ~Shell();

 // device actions
 void doInput();
 void doRecv();

 void doOutput(std::string reply);
 void doOutput(Json::Value reply);
 void doCommand(std::string cmd);

 void prompt(int ns=0);
 void prompt(std::string ps1) { mPrompt = ps1; }

 void cmdHelp(WordList &wrds);
 void cmdLs(WordList &wrds);
 void cmdLt(WordList &wrd);
 void cmdLc(WordList &wrd);
 void cmdQuit(WordList &wrd);
 void cmdOpen(WordList &wrd);
 void cmdStart(WordList &wrd);
 void cmdStop(WordList &wrd);
 void cmdClose(WordList &wrd);
 void cmdCollect(WordList &wrd);
 void cmdCls(WordList &wrd);
 void cmdLoad(WordList &wrd);
 void cmdSetFreq(WordList &wrd);
 void cmdSetNotchFilter(WordList &wrd);

 void jCommand(Json::Value msg);

 std::string applyFormat(Json::Value msg);
 std::string fmtLc(Json::Value msg);
 std::string fmtLt(Json::Value msg);
 std::string fmtStart(Json::Value msg);

private:
  Device     *mIn;
  int         mOut;
  bool        mLocal;
  std::string mPrompt;
  CommandList mCmd;
  int         mSkipPrompt;
};

// TCP connection listener that makes shells for remote users.
// One may always use Shell term(&srv,0,1) for a terminal not
// connected via a socket.
class ShellFactory : public Task
{
public:
  ShellFactory();
 ~ShellFactory();
 void start(Server *srv, std::string port);
 void onConnection();
private:
  Device *mListener;
};
