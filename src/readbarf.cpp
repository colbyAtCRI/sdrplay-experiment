#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>


using namespace std;

struct Header
{
   uint32_t  mSamples;
   uint64_t  mTimeuS;
   uint32_t  mFirstSample;
   int       mGrChange;
   int       mRfChange;
   int       mFsChange;
   uint32_t  mReset;
};

struct IQ
{
  short I;
  short Q;
};

int main(int argc, char *argv[])
{
  Header hdr;
  IQ *data(NULL);
  int frame(0);
  uint32_t lastFrame(0);
  if (argc < 2)
    cerr << "get with the program" << endl;
  int fd = open(argv[1],O_RDONLY);
  if (fd < 0) {
    cerr << strerror(errno) << endl;
    return 0;
  }
  while (read(fd,(char*)&hdr,sizeof(hdr))==sizeof(hdr)) {
    int nr = sizeof(IQ) * hdr.mSamples;
    if (data == NULL)
       data = new IQ[nr];
    if ( read(fd,(char*)data,nr) != nr )
       cerr << "yikes" << endl;
    if (frame == 0) {
       lastFrame = hdr.mFirstSample;
       frame = 1;
    }
    else {
       while (lastFrame < hdr.mFirstSample) {
          lastFrame += hdr.mSamples;
          frame += 1;
       }
    }
    cout << hdr.mSamples << " "
         << hdr.mTimeuS << " " 
         << hdr.mFirstSample << " "
         << frame << " "
         << hdr.mGrChange << " "
	 << hdr.mRfChange << " "
         << hdr.mFsChange << " "
         << hdr.mReset << endl;
  }
  delete data;
  close(fd);
  return 0;
}
