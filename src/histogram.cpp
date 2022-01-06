#include "spectrum.h"
#include "iqdata.h"
#include <iostream>

using namespace std;

int
main(int argc, char *argv[])
{
  if (argc == 1) {
    cout << "Usage: histogram file" << endl;
    return 0;
  }
  Spectrum spec;
  IQStream IQ(argv[1]);
  while (IQ) {
    IQ >> spec;
  }
  return 0;
}
