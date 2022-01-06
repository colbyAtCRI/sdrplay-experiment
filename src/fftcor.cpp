// Compute the copower for two radios which is the FFT of the first radio times
// the complex conjugate of the second radio. Essentially, if the two radios are
// identical (same data file) the output is the magnitude of the fft.
#include <iqdata.h>
#include <iostream>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <complex>
#include <vector>

using namespace std;

struct Options {
  string errorMsg;
  string fn1;
  string fn2;
  int    N;
  double freq0;
  double rate;
  bool   correction;
  Options() {
    N     = -1;
    freq0 = 0.0;
    rate  = 0.0;
    correction = false;
  }
};

void usage()
{
  cerr << "Usage: fftcor [options] file1.brf file2.brf" << endl
       << "Options are" << endl
       << "  -r rate     sample rate in MHz" << endl
       << "  -f freq     radio center frequency in MHz" << endl
       << "  -n number   size of data frame (default is radio sample #)" << endl
       << "  -c          do dominate peak phase correction" << endl
       << "  -h          this helpful message" << endl;
}

bool parseOptions(Options &opt, int argc, char *argv[])
{
  bool ret(true);
  int  ch;
  while ((ch = getopt(argc,argv,"chr:f:n:")) != -1) {
    switch (ch) {
      case 'r':
        opt.rate = atof(optarg);
        break;
      case 'c':
        opt.correction = true;
        break;
      case 'f':
        opt.freq0 = atof(optarg);
        break;
      case 'n':
        opt.N = atoi(optarg);
        break;
      case 'h':
      case '?':
      default:
        usage();
        ret = false;
    }
  }
  if (optind+2 != argc) {
    opt.errorMsg = "expected 2 brf files. Got ";
    opt.errorMsg += to_string(argc - optind);
    ret = false;
  }
  else {
    opt.fn1 = argv[optind];
    opt.fn2 = argv[optind+1];
  }
  return ret;
}

void fixorder(fftw_complex *out, int N)
{
  for (int n = 0; n < N/2; n++) {
    fftw_complex tmp;
    tmp[0] = out[N/2+n][0];
    tmp[1] = out[N/2+n][1];
    out[N/2+n][0] = out[n][0];
    out[N/2+n][1] = out[n][1];
    out[n][0] = tmp[0];
    out[n][1] = tmp[1];
  }
}

typedef complex<double> cplxT;
typedef vector<cplxT>   cplxV;

vector<cplxT> tocomplex(fftw_complex *ar, int N)
{
  cplxV ret(N);
  for (int n = 0; n < N; n++)
    ret[n] = cplxT(ar[n][0],ar[n][1]);
  return ret;
}

int main(int argc, char *argv[])
{
  Options opt;
  int     N;

  if (!parseOptions(opt,argc,argv)) {
    cerr << opt.errorMsg << endl;
    return -1;
  }

  IQStream IQ1(opt.fn1);
  IQStream IQ2(opt.fn2);
  if (!IQ1) {
    cerr << "can't open: " << opt.fn1 << endl;
    return -1;
  }
  if (!IQ2) {
    cerr << "can't open: " << opt.fn2 << endl;
    return -1;
  }

  if (opt.N < 0) {
    N = IQ1.frameSize();
  }
  else {
    N = opt.N;
    IQ1.frameSize(N);
    IQ2.frameSize(N);
  }

  Spectrum s1(N), s2(N), av(N);
  FFT      fft1(N), fft2(N);
  cplxT    za, zb;
  int      count(0);
  while (IQ1 && IQ2) {
    IQ1 >> fft1;
    IQ2 >> fft2;
    s1 = fft1.spectrum();
    s2 = fft2.spectrum();
    // Do the phase correction based on the max peak
    if (opt.correction) {
      int p1, p2;
      p1 = s1.peak();
      p2 = s2.peak();
      if (p1 != p2) {
        cerr << p1 << " != " << p2 << endl;
      }
      za = s1[p1];
      zb = s2[p2];
      for (int n = 0; n < N; n++) {
        s2[n] = (za/zb) * (abs(zb)/abs(za)) * s2[n];
      }
    }
    for (int n = 0; n < N; n++) {
      av[n] += s1[n] * conj(s2[n]) / cplxT(N,0.0);
    }
    count = count + 1;
  }
  for (int n = 0; n < N; n++) {
    av[n] = av[n]/cplxT(count,0.0);
  }
  Json::Value hdr = IQ1.header();
  if (opt.rate == 0 && hdr.isMember("sample-rate-MHz")) {
    opt.rate = hdr["sample-rate-MHz"].asDouble();
    opt.freq0 = hdr["center-frequency-MHz"].asDouble();
  }
  if (opt.rate == 0) {
    for (int n = 0; n < N; n++) {
      double rp  = av[n].real();
      double ip  = av[n].imag();
      double mag = sqrt(rp*rp + ip*ip);
      cout << n << "  " << mag << " " << rp << " " << ip << endl;
    }
  }
  else {
    double df(opt.rate/N);
    for (int n = 0; n < N; n++) {
      double freq = opt.freq0 + (n - N/2)*df;
      double rp  = av[n].real();
      double ip  = av[n].imag();
      double mag = sqrt(rp*rp + ip*ip);
      cout << freq << " " << mag << " " << rp << " " << ip << endl;
    }
  }
  return 0;
}
