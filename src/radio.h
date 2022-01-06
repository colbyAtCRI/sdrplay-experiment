#pragma once
#include "server.h"
#include "iqdata.h"
#include "utility.h"
#include "jsocket.h"
#include <mirsdrapi-rsp.h>
#include <pthread.h>
#include <queue>

using namespace std;
using namespace Json;

class Radio : public Task
{
public:
   enum State
   {
      NotAssigned,
      Opened,
      Streaming,
      Collecting
   };
private:
   int              mSequenceNumber;
   State            mState;
   Value            mCfg;
   pthread_mutex_t  mLock; // every radio must have it's own mutex
   static const int mBWkHz[8];
   static const int mIFkHz[4];
   uint32_t         mDecimation;
   uint32_t         mDecimationEnable;
   int              mFrameCounter;
   int              mBuffCount;
   IQFile           mFile;
   int              mBufferPipe[2];
   Device          *mSync;
   // we want to control RF switches on a network pi running a
   // gpioserver. This way the switch state may be syncronized 
   // and recorded in the brf data files.
   JSocket         *mSwitch;
   queue<Value>     mNotify;

   // Data Buffer hell
   std::queue<IQData*> mFree;
   std::queue<IQData*> mFull;

   // Taken from SoapySDRplay. 
   uint32_t computeDecimation(uint32_t rate, uint32_t *dec, mir_sdr_If_kHzT ifmode);

public:
   Radio(Server *srv, Value &cfg);
  ~Radio();

   void onBufferComplete();
   void onSwitchReply();

   // The complete radio setup will be stored in the Json object, mCfg.
   void   config(Value cfg) { mCfg = cfg; }
   Value  config() { return mCfg; }
   static Value  defaultConfig();
   string errorStr(mir_sdr_ErrT);

   bool   configSwitch();

   // To use radios as slaves one needs to have shell control of opening and
   // and starting radios.
   Value  open(Value cmd);
   Value  start(Value cmd);
   Value  stop(Value cmd);
   Value  close(Value cmd);
   Value  collect(Value cmd);
   Value  ls();

   void   jCommand(Value msg);
   IQData *getFreeBuffer();
   void    yieldFreeBuffer(IQData *buffer);
   IQData *getFullBuffer();
   void    yieldFullBuffer(IQData *buffer);
   void    setFrameCounter(int nf);
   int     currentFrame();

   // Process a file name containing data macros.
   //   %n - revision number
   //   %s - serial number
   //   %f - frequency
   //   %p - process number
   //   %w - switch state
   std::string expandName(std::string name);

   // thread safe state access
   std::string  state();
   void         state(State st);

   void    lock()   { pthread_mutex_lock(&mLock);}
   void    unlock() { pthread_mutex_unlock(&mLock);}

};
