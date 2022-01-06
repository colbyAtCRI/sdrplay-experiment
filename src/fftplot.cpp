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
  string fn;
  int    N;
  double freq0;
  double rate;
  Options() {
    N     = -1;
    freq0 = 0.0;
    rate  = 0.0;
  }
};

void usage()
{
  cerr << "Usage: fftplot [options] file.brf" << endl
       << "Options are" << endl
       << "  -r rate     sample rate in MHz" << endl
       << "  -f freq     radio center frequency in MHz" << endl
       << "  -n number   size of data frame (default is radio sample #)" << endl
       << "  -h          this helpful message" << endl;
}

bool parseOptions(Options &opt, int argc, char *argv[])
{
  bool ret(true);
  int  ch;
  while ((ch = getopt(argc,argv,"hr:f:n:")) != -1) {
    switch (ch) {
      case 'r':
        opt.rate = atof(optarg);
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
  if (optind+1 != argc) {
    opt.errorMsg = "expected 1 brf file. Got ";
    opt.errorMsg += to_string(argc - optind);
    ret = false;
  }
  else {
    opt.fn = argv[optind];
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
  Options       opt;
  double        mag, phs;
  fftw_plan     plan;
  fftw_complex *in, *out;
  int           N(0);

  if (!parseOptions(opt,argc,argv)) {
    cerr << opt.errorMsg << endl;
    return -1;
  }

  IQStream IQ(opt.fn);
  if (!IQ) {
    cerr << "can't open: " << opt.fn << endl;
    return -1;
  }
  if (opt.N > 0)
    N = opt.N;
  else
    N = IQ.frameSize();

  in   = (fftw_complex*) fftw_malloc(sizeof(fftw_complex)*N);
  out  = (fftw_complex*) fftw_malloc(sizeof(fftw_complex)*N);
  plan = fftw_plan_dft_1d(N,in,out,FFTW_FORWARD,FFTW_ESTIMATE);

  vector<double> av(N);
  cplxV          iq(N);

  fill(av.begin(),av.end(),0.0);
  fill(iq.begin(),iq.end(),0.0);

  int count(0);
  while (IQ.readSome(in,N) == N) {
    fftw_execute(plan);
    fixorder(out,N);
    for (int n = 0; n < N; n++) {
      mag = (out[n][0]*out[n][0]+out[n][1]*out[n][1])/N;
      iq[n] += complex<double>(out[n][0],out[n][1])/sqrt(N);
      av[n] = av[n] + mag;
    }
    count = count + 1;
  }
  for (int n = 0; n < N; n++) {
    av[n] = av[n] / count;
    iq[n] = iq[n] / (double)count;
  }
  // Grab rate and freq from header if no rate switch
  Json::Value hdr = IQ.header();
  if (opt.rate == 0 && hdr.isMember("sample-rate-MHz")) {
    opt.freq0 = hdr["center-frequency-MHz"].asDouble();
    opt.rate  = hdr["sample-rate-MHz"].asDouble();
  }

  if (opt.rate == 0) {
    for (int n = 0; n < N; n++) {
      cout << n << " "
           << av[n] << " "
           << iq[n].real() << " "
           << iq[n].imag() << endl;
    }
  }
  else {
    double df(opt.rate/N);
    for (int n = 0; n < N; n++) {
      double freq = opt.freq0 + (n - N/2)*df;
      cout << freq << " "
           << av[n] << " "
           << iq[n].real() << " "
           << iq[n].imag() << endl;
    }
  }
  fftw_destroy_plan(plan);
  fftw_free(in);
  fftw_free(out);
  return 0;
}
