#include <gnuplot-iostream.h>
#include <iqdata.h>
#include <complex>
#include <vector>
#include <stdlib.h>
#include <boost/tuple/tuple.hpp>
#include <boost/foreach.hpp>

using namespace std;

typedef vector<pair<double,double>> plotdata_t;

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

plotdata_t fftFile(string fname)
{
  plotdata_t    data;
  IQStream      IQ(fname);
  fftw_plan     plan;
  fftw_complex *in, *out;
  int           N;

  N = IQ.frameSize();
  in   = (fftw_complex*) fftw_malloc(sizeof(fftw_complex)*N);
  out  = (fftw_complex*) fftw_malloc(sizeof(fftw_complex)*N);
  plan = fftw_plan_dft_1d(N,in,out,FFTW_FORWARD,FFTW_ESTIMATE);

  vector<double> av(N);
  int            count(0);

  while (IQ.readSome(in,N) == N) {
    double mag;
    fftw_execute(plan);
    fixorder(out,N);
    for (int n = 0; n < N; n++) {
      mag = (out[n][0]*out[n][0]+out[n][1]*out[n][1])/N;
      av[n] = av[n] + mag;
    }
    count = count + 1;
  }
  for (int n = 0; n < N; n++) {
    av[n] = av[n] / count;
  }
  // Grab rate and freq from header if no rate switch
  Json::Value hdr = IQ.header();
  double freq0 = hdr["center-frequency-MHz"].asDouble();
  double rate  = hdr["sample-rate-MHz"].asDouble();

  double df(rate/N);
  for (int n = 0; n < N; n++) {
    double freq = freq0 + (n - N/2)*df;
    data.push_back(make_pair(freq,av[n]));
  }

  fftw_destroy_plan(plan);
  fftw_free(in);
  fftw_free(out);
  return data;
}

string radioSN(string fn)
{
  IQStream IQ(fn);
  string sn = IQ.header()["serial-number"].asString();
  IQ.close();
  return sn;
}

int main(int argc, char *argv[])
{
  Gnuplot plt;
  plt << "set log y 10" << endl;
  plt << "set grid xtic ytic mytic" << endl;
  plt << "set xlabel \"Frequency (Mhz)\"" << endl;
  string cmd("plot ");
  for (int n = 1; n < argc; n++) {
    if (n > 1) cmd += ", ";
    cmd = cmd + "'-' w l title \"";
    cmd = cmd + radioSN(argv[n]) + "\"";
  }
  plt << cmd << endl;
  for (int n = 1; n < argc; n++) {
    plt.send1d(fftFile(argv[n]));
  }
}
