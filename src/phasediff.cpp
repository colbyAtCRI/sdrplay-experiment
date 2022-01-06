#include <iqdata.h>
#include <iostream>
#include <math.h>
#include <complex>
#include <fstream>
#include <unistd.h>
#include <vector>
#include <gnuplot-iostream.h>
#include <boost/tuple/tuple.hpp>
#include <boost/foreach.hpp>

using namespace std;

int main(int argc, char *argv[])
{
  Gnuplot plt;
  if ( argc != 3) {
    cerr << "Usage: phasediff file1 file2" << endl;
    return 0;
  }
  vector<double> data;
  IQStream IQA(argv[1]);
  IQStream IQB(argv[2]);
  if (!IQA || !IQB) {
    cerr << "missing radio data" << endl;
    return -1;
  }
  int N(IQA.frameSize());
  FFT fftA(N);
  FFT fftB(N);
  int count(0);
  Spectrum sa, sb;
  cplxT    za, zb;
  while (IQA && IQB) {
    IQA >> fftA;
    sa = fftA.spectrum();
    IQB >> fftB;
    sb = fftB.spectrum();
    za =  sa[sa.peak()];
    zb = sb[sb.peak()];
    data.push_back(180.0*arg(za/zb)/M_PI);
  }
  plt << "set yrange [-180:180]" << endl;
  plt << "set ylabel \"Phase (deg)\"" << endl;
  plt << "plot '-' w l" << endl;
  plt.send1d(data);
  return 0;
}
