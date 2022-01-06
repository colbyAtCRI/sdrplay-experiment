#include <shell.h>
#include <unistd.h>
#include <network.h>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <radio.h>
#include <glob.h>

using namespace std;

string trim(string str)
{
  string::reverse_iterator c;
  for (c = str.rbegin(); c != str.rend(); c++) {
    if (!isspace(*c))
      break;
  }
  string ret(c,str.rend());
  reverse(ret.begin(),ret.end());
  return ret;
}

string remove_tabs(string str)
{
  string ret;
  for (auto c = str.begin(); c != str.end(); c++) {
    if (*c != '\t') {
      ret.push_back(*c);
    }
    else {
      for (int n = 0; n < 3; n++)
        ret.push_back(' ');
      }
  }
  return ret;
}

string pad_to(string str, int wid)
{
  string ret(str);
  while (ret.size() < wid)
    ret.push_back(' ');
  return ret;
}

WordList::WordList(string cmd)
{
  cmd = trim(cmd);
  // split off space delimited strings
  istringstream tokens(cmd);
  while (tokens) {
    string wrd;
    tokens >> wrd;
    if (wrd.size() > 0)
      push_back(wrd);
  }
}

Shell::Shell(Server *srv, int in, int out, string port) : Task(srv), mOut(out)
{
  // detect a socket by its bidirectionality
  // lame but I don't see a down side.
  name(string("shell@")+port);
  server()->add(this);
  mLocal = in != out;
  mIn = new Device(this,  in, Device::Read);
  if (mLocal) {
    mIn->setAction((Action)(&Shell::doInput));
  }
  else {
    mIn->setAction((Action)&Shell::doRecv);
  }
  server()->add(mIn);
  mCmd["help"].mCmd   = &Shell::cmdHelp;
  mCmd["help"].mInfo  = "List all commands";
  mCmd["help"].mMore  = "That's pretty much it";
  mCmd["ls"].mCmd     = &Shell::cmdLs;
  mCmd["ls"].mInfo    = "list all SDRplay radios";
  mCmd["quit"].mCmd   = &Shell::cmdQuit;
  mCmd["quit"].mInfo  = "kill device server";
  mCmd["lt"].mCmd     = &Shell::cmdLt;
  mCmd["lt"].mInfo    = "list server tasks";
  mCmd["lc"].mCmd     = &Shell::cmdLc;
  mCmd["lc"].mInfo    = "list radio configurations";
  mCmd["open"].mCmd   = &Shell::cmdOpen;
  mCmd["open"].mInfo  = "open all radios (depricated implied by start)";
  mCmd["start"].mCmd  = &Shell::cmdStart;
  mCmd["start"].mInfo = "start (and open) all radio data streams";
  mCmd["stop"].mCmd   = &Shell::cmdStop;
  mCmd["stop"].mInfo  = "stop (and close) all radio streams";
  mCmd["close"].mCmd  = &Shell::cmdClose;
  mCmd["close"].mInfo = "close all radios (depricated impled by stop)";
  mCmd["collect"].mCmd = &Shell::cmdCollect;
  mCmd["collect"].mInfo = "collect frames";
  mCmd["cls"].mCmd     = &Shell::cmdCls;
  mCmd["cls"].mInfo    = "clear the screen";
  mCmd["load"].mCmd    = &Shell::cmdLoad;
  mCmd["load"].mInfo   = "load radio configuration";
  mCmd["freq"].mCmd    = &Shell::cmdSetFreq;
  mCmd["freq"].mInfo   = "set center frequency (MHz)";
  mCmd["filter"].mCmd  = &Shell::cmdSetNotchFilter;
  mCmd["filter"].mInfo = "set notch filter [on|off]";

  mSkipPrompt = 0;
  mPrompt = "sdr> ";
  prompt();
}

Shell::~Shell()
{
  server()->del(this);
  if (!mLocal)
    close(mOut);
}

void Shell::cmdLoad(WordList &wrd)
{
  Value cfg;
  Value cmd;
  glob_t files;
  if (glob("*.json",0,NULL,&files)) {
    string reply = strerror(errno);
    reply += "\n";
    doOutput(reply);
    prompt();
    return;
  }
  stringstream reply;
  if (files.gl_pathc == 0) {
    reply << "no json files found" << endl;
    doOutput(reply.str());
    prompt();
    return;
  }
  switch (wrd.size()) {
    case 1: {
      reply << "choose one of" << endl << endl;
      for (int n = 0; n < files.gl_pathc; n++) {
        reply << "   " << n << "\t.... " << files.gl_pathv[n] << endl;
      }
      reply << endl << "issue 'load <file number>' for all radios" << endl;
      reply << "issue 'load <file number> <process number>' for just one radio." << endl;
      doOutput(reply.str());
      prompt();
      return;
    }
    case 2: {
      int nf = atoi(wrd[1].c_str());
      ifstream file(files.gl_pathv[nf]);
      file >> cfg;
      cmd = broadcastTo("radio");
      cmd["command"] = "load";
      cmd["config"] = cfg;
      file.close();
      prompt(server()->broadcast());
      break;
    }
    case 3: {
      int nf = atoi(wrd[1].c_str());
      int np = atoi(wrd[2].c_str());
      ifstream file(files.gl_pathv[nf]);
      file >> cfg;
      cmd = sendTo(np,"radio");
      cmd["command"] = "load";
      cmd["config"] = cfg;
      prompt(1);
      file.close();
      break;
    }
  }
  server()->sendPipe(cmd);
}

void Shell::cmdSetFreq(WordList &wrd)
{
  Value cmd;
  stringstream reply;
  switch (wrd.size()) {
    case 1: {
      reply << "what" << endl;
      doOutput(reply.str());
      prompt();
      return;
    }
    case 2: {
      cmd = broadcastTo("radio");
      cmd["command"] = "set-frequency";
      cmd["center-frequency-MHz"] = atof(wrd[1].c_str());
      server()->sendPipe(cmd);
      prompt(1);
      return;
    }
    case 3: {
      cmd = sendTo(atoi(wrd[2].c_str()),"radio");
      cmd["command"] = "set-frequency";
      cmd["center-frequency-MHz"] = atof(wrd[1].c_str());
      server()->sendPipe(cmd);
      prompt();
      return;
    }
  }
}

void Shell::cmdSetNotchFilter(WordList &wrd)
{
  Value        cmd;
  switch (wrd.size()) {
    case 1:
      doOutput(string("?"));
      prompt();
      return;
    case 2:
      cmd = broadcastTo("radio");
      if (wrd[1] == "on") {
        cmd["command"] = "enable-notch-filter";
      }
      else {
        cmd["command"] = "disable-notch-filter";
      }
      server()->sendPipe(cmd);
      prompt(1);
      return;
    case 3:
      cmd = sendTo(atoi(wrd[2].c_str()),"radio");
      if (wrd[1] == "on") {
        cmd["command"] = "enable-notch-filter";
      }
      else {
        cmd["command"] = "disable-notch-filter";
      }
      server()->sendPipe(cmd);
      prompt();
      return;
  }
}

void Shell::cmdCls(WordList &wrd)
{
  const string cls("\e[2J\e[0;0H");
  doOutput(cls+mPrompt);
}

void Shell::cmdOpen(WordList &wrd)
{
  Value cmd;
  if (wrd.size()==1) {
    cmd = broadcastTo("radio");
    prompt(server()->broadcast());
  }
  else {
    int proc = atoi(wrd[1].c_str());
    if (proc > -1 && proc < server()->broadcast()) {
      cmd = sendTo(proc,"radio");
      prompt(1);
    }
    else {
      doOutput(("process index out of bound: ")+wrd[1]);
      return;
    }
  }
  cmd["command"] = "open";
  server()->sendPipe(cmd);
}

void Shell::cmdStart(WordList &wrd)
{
  Value cmd;
  if (wrd.size()==1) {
    cmd = broadcastTo("radio");
    prompt(server()->broadcast());
  }
  else {
    int proc = atoi(wrd[1].c_str());
    if (proc > -1 && proc < server()->broadcast()) {
      cmd = sendTo(proc,"radio");
      prompt(1);
    }
    else {
      doOutput(("process index out of bound: ")+wrd[1]);
      return;
    }
  }
  cmd["command"] = "start";
  server()->sendPipe(cmd);
}

void Shell::cmdStop(WordList &wrd)
{
  Value cmd;
  if (wrd.size()==1) {
    cmd = broadcastTo("radio");
    prompt(server()->broadcast());
  }
  else {
    int proc = atoi(wrd[1].c_str());
    if (proc > -1 && proc < server()->broadcast()) {
      cmd = sendTo(proc,"radio");
      prompt(1);
    }
    else {
      doOutput(("process index out of bound: ")+wrd[1]);
      return;
    }
  }
  cmd["command"] = "stop";
  server()->sendPipe(cmd);
}

void Shell::cmdClose(WordList &wrd)
{
  Value cmd;
  if (wrd.size()==1) {
    cmd = broadcastTo("radio");
    prompt(server()->broadcast());
  }
  else {
    int proc = atoi(wrd[1].c_str());
    if (proc > -1 && proc < server()->broadcast()) {
      cmd = sendTo(proc,"radio");
      prompt(1);
    }
    else {
      doOutput(("process index out of bound: ")+wrd[1]);
      return;
    }
  }
  cmd["command"] = "close";
  server()->sendPipe(cmd);
}

void Shell::cmdCollect(WordList &wrd)
{
  Value cmd = broadcastTo("radio");

  if (wrd.size() < 2) {
    cmd["frames"] = 100;
  }
  else {
    cmd["frames"] = atoi(wrd[1].c_str());
  }
  cmd["command"] = "collect";
  server()->sendPipe(cmd);
  prompt(server()->broadcast());
}

void Shell::cmdLs(WordList &wrd)
{
  Radio *radio = (Radio*)server()->task("radio");
  if (radio == NULL) {
    doOutput((string)"No radio task");
    return;
  }
  Value dir;
  dir = radio->ls();
  doOutput(dir);
  prompt();
}

void Shell::cmdHelp(WordList &wrds)
{
  stringstream reply;
  int maxwid(0);
  for (auto c = mCmd.begin(); c != mCmd.end(); c++)
    if (c->first.size() > maxwid)
      maxwid = c->first.size();
  if (wrds.size()==1) {
    reply << "I know " << mCmd.size() << " commands" << endl;
    for (auto c = mCmd.begin(); c != mCmd.end(); c++) {
      reply << "   " << pad_to(c->first,maxwid) << " - " << c->second.mInfo << endl;
    }
  }
  else {
    for (int n = 1; n < wrds.size(); n++) {
      if (mCmd.have(wrds[n])) {
        reply << wrds[n] << " - " << mCmd[wrds[n]].mInfo << endl;
        if (mCmd[wrds[n]].mMore != "")
          reply << mCmd[wrds[n]].mMore << endl;
      }
      else {
        reply << wrds[n] << " - " << " command unknown" << endl;
      }
    }
  }
  doOutput(reply.str());
  prompt();
}

void Shell::cmdQuit(WordList &wrd)
{
  Value cmd;
  cmd = broadcastTo("server");
  cmd["command"] = "quit";
  server()->sendPipe(cmd);
}

void Shell::cmdLt(WordList &wrd)
{
  Value cmd;
  stringstream reply;
  cmd = broadcastTo("server");
  cmd["command"] = "lt";
  server()->sendPipe(cmd);
  prompt(server()->broadcast());
}

void Shell::cmdLc(WordList &wrd)
{
   Value cmd;
   stringstream reply;
   cmd = broadcastTo("radio");
   cmd["command"] = "lc";
   server()->sendPipe(cmd);
   prompt(server()->broadcast());
}

void Shell::doInput()
{
  int  nb;
  char buffer[4096];
  fill(buffer,buffer+4096,0x00);
  nb = read(mIn->device(),buffer,4096);
  if (nb == 0)
    server()->gameOver();
  if (nb > 0)
    doCommand(buffer);
}

void Shell::doRecv()
{
  int nb;
  char buffer[4096];
  fill(buffer,buffer+4096,0x00);
  nb = recv(mIn->device(),buffer,4096,0);
  if (nb == 0 || buffer[0]==0x04) {
    server()->del(mIn);
    delete this;
    return;
  }
  if (nb > 0)
    doCommand(buffer);
}

void Shell::doCommand(string cmd)
{
  WordList wrds(cmd);
  if (!wrds.empty() && mCmd.have(wrds[0])) {
    pCmd cmd = mCmd[wrds[0]].mCmd;
    (this->*cmd)(wrds);
  }
  else {
    stringstream reply;
    if (wrds.empty()) {
      mSkipPrompt = 0;
      prompt();
    }
    else {
      reply << wrds[0] << " - unknown command" << endl;
      doOutput(reply.str());
    }
  }
}

struct DotList : map<JSONCPP_STRING,Value>
{
  void add(Value val,string name="") {
    if (val.isObject()) {
      for (auto const& id : val.getMemberNames()) {
        if (val[id].isObject())
          add(val[id],id+".");
        else {
          (*this)[name+id] = val[id];
        }
      }
    }
  }

  int maxLen() {
    int len(0);
    for (auto const& p : *this) {
      if (p.first.size() > len)
        len = p.first.size();
    }
    return len;
  }

  string format() {
    stringstream ret;
    int          mlen = maxLen()+5;
    for (auto const& p : *this) {
      ret << "   " << p.first << spacedot(p.first,mlen) << p.second << endl;
    }
    return ret.str();
  }

  string spacedot(string name, int width) {
    string ret;
    ret += ' ';
    for (int n = 0; n < width - name.size(); n++)
      ret += '.';
    ret += ' ';
    return ret;
  }
};

string Shell::fmtLc(Value msg)
{
  stringstream rep;
  rep << "Process " << msg["from"]["process"] << endl;
  Value cfg = msg["config"];
  DotList lst;
  lst.add(cfg);
  return rep.str()+lst.format();
}

string Shell::fmtLt(Value msg)
{
  stringstream rep;
  rep << "Process " << msg["from"]["process"] << " : ";
  for (auto const& p : msg["tasks"]) {
    rep << p.asString() << " ";
  }
  rep << endl;
  return rep.str();
}

string Shell::fmtStart(Value msg)
{
  stringstream rep;
  rep << "Process " << msg["from"]["process"]
      << " " << msg["state"].asString() << endl;
  return rep.str();
}

string Shell::applyFormat(Value msg)
{
  if (!msg.isMember("command")) {
    stringstream ack;
    ack << msg << endl;
    return remove_tabs(ack.str());
  }
  string cmd = msg["command"].asString();
  if (cmd == "lc")
    return fmtLc(msg);
  else if (cmd == "lt")
    return fmtLt(msg);
  else if (cmd == "start" || cmd == "stop" || cmd == "open" || cmd == "close")
    return fmtStart(msg);
  else {
    stringstream ack;
    ack << msg << endl;
    return remove_tabs(ack.str());
  }
}

// Basically every json message comming into a shell is an ack or a notification
// The whole waitReply debacle is an attempt to make the shell prompts work
// smoothly. This is stupid and is removed.
void Shell::jCommand(Value msg)
{
  //stringstream ack;
  // If notification move to the line beyond prompt. If this happens while
  // user is typing things will become confused. Oh well. Candy coat later.
  //if (msg.isMember("notify"))
  //  ack << endl;
  //ack << msg << endl;
  //doOutput(remove_tabs(ack.str()));
  doOutput(applyFormat(msg));
  prompt();
}

// If a command expects ns acks then skip ns prompts.
void Shell::prompt(int ns)
{
  if (ns > 0) {
    mSkipPrompt = ns+1;
  }
  if (mSkipPrompt > 0) {
    mSkipPrompt--;
  }
  if (mSkipPrompt==0) {
    doOutput(mPrompt);
  }
}

void Shell::doOutput(string reply)
{
  int nb;
  if (mLocal) {
    nb = write(mOut,reply.c_str(),reply.size());
  }
  else {
    nb = send(mOut,reply.c_str(),reply.size(),0);
  }
  if (nb != reply.size())
    cout << "fuck" << endl;
}

void Shell::doOutput(Value cmd)
{
  stringstream out;
  out << cmd << endl;
  doOutput(remove_tabs(out.str()));
}

ShellFactory::ShellFactory() : Task(NULL)
{
}

void ShellFactory::start(Server *srv, string port)
{
  mServer = srv;
  name("tcp-listener");
  server()->add(this);
  mListener = new Device(this, TCPServerSocket(port.c_str()),Device::Read);
  mListener->setAction((Action)&ShellFactory::onConnection);
  server()->add(mListener);
}

ShellFactory::~ShellFactory()
{
  if (server())
    server()->del(this);
}

void ShellFactory::onConnection()
{
  string host;
  string port;
  Shell *shell;
  if (server()) {
    int fd = TCPConnection(mListener->device(),host,port);
    if (fd > 0) {
      shell = new Shell(server(),fd,fd,host+":"+port);
    }
  }
}
