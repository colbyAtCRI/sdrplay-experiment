#include <shell.h>
#include <radio.h>
#include <iostream>
#include <unistd.h>
#include <fstream>

using namespace std;
using namespace Json;

string outn(int n) 
{
   stringstream name;
   name << "stdout" << n << ".log";
   return name.str();
}

string errn(int n)
{
   stringstream name;
   name << "stderr" << n << ".log";
   return name.str();
}

int main(int argc, char *argv[])
{
  int ns(2);
  Value    cfg(Radio::defaultConfig());
  if (argc > 2) {
    ns = atoi(argv[1]);
  }
  cout << "starting " << ns << " processes" << endl;
  PipeRing ring(ns);
  for (int n = 0; n < ns; n++) {
    if (fork() == 0) {
      FILE *x, *y;
      x = freopen(outn(n).c_str(),"a+",stdout);
      y = freopen(errn(n).c_str(),"a+",stderr);
      ring.atServer(n);
      Server srv(n,ring);
      // Open a tcp listener for this radio
      ShellFactory factory;
      if (n == 0)
        factory.start(&srv,"1535");
      Radio radio(&srv,cfg);
      srv.run();
      return 0;
     }
     // Seems to help startup race condition. Unclear what the radio API
     // does to communicate between devices at load time. Here we pause
     // between radio process startup.
     usleep(100000);
  }
  return 0;
}
