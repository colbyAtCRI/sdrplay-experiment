#include <radio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

void streamCallback(short   *xi,
                    short   *xq,
                    uint32_t firstSampleNum,
                    int      grChange,
                    int      rfChange,
                    int      fsChange,
                    uint32_t numSamples,
                    uint32_t reset,
                    uint32_t hwChange,
                    void    *cbContext)
{
  Radio *host = (Radio*)cbContext;
  // Grab the working buffer pointer. Once we get one we keep it till
  // we're done. Then we zero it to let the calling process know we
  // are done with it.
  IQData *buffer = host->getFreeBuffer();
  if (buffer) {
    buffer->mSystemTime = systemTime();
    buffer->mFirstFrame = firstSampleNum;
    buffer->mGrChange   = grChange;
    buffer->mRfChange   = rfChange;
    buffer->mFsChange   = fsChange;
    buffer->mReset      = reset;
    buffer->grabIQ(xi,xq,numSamples);
    host->yieldFreeBuffer(buffer);
  }
}

void gainCallback(unsigned int gRidx,
                  unsigned int gRdB,
                  void        *cbContext)
{
}

// Save some typing. All functions are called with (Value cmd)
#define check(x) {\
  mir_sdr_ErrT n; \
  if((n = x)) {\
    cmd["error"][#x] = errorStr(n);\
    log() << errorStr(n) << endl;\
    return cmd;\
  } \
}

#define checkLog(x) {\
  mir_sdr_ErrT n;\
  if ((n = x)) {\
    log() << errorStr(n) << endl;\
  }\
}

string Radio::errorStr(mir_sdr_ErrT ner)
{
  static string msg[14] = {
    "Success",
    "Fail",
    "Invalid Parameter",
    "Out of range",
    "Gain update error",
    "RF update error",
    "FS update error",
    "Hardware error",
    "Aliasing error",
    "Already initialized",
    "Not initialized",
    "Not enabled",
    "Hardware version error",
    "Out of memory"};
    return msg[ner];
}

// pthread_mutex_t SDRPlay::mLock = PTHREAD_MUTEX_INITIALIZER;
const int Radio::mBWkHz[8] = {200,300,600,1536,5000,6000,7000,8000};
const int Radio::mIFkHz[4] = {0,450,1620,2048};

Radio::Radio(Server *srv, Value &cfg) : Task(srv), mCfg(cfg)
{
   // Leaving this out caused much confusion, radome errors, and strange hangs
   if (pthread_mutex_init(&mLock,NULL)) {
      server()->setError(strerror(errno));
   }
   state(NotAssigned);
   name("radio");
   server()->add(this);
   mBuffCount = 100;
   for (int n = 0; n < mBuffCount; n++)
      mFree.push(new IQData());
   mFrameCounter = 0;
   if (pipe(mBufferPipe)) {
      mCfg["error"] = strerror(errno);
      return;
  }
   mSync = new Device(this,mBufferPipe[0],Device::Read);
   mSync->setAction((Action)&Radio::onBufferComplete);
   server()->add(mSync);
   mSequenceNumber = 1;
   mSwitch = NULL;
   configSwitch();
}

bool Radio::configSwitch()
{
   if (mCfg.isMember("switch")) {
      Value sw = mCfg["switch"];
      mSwitch = new JSocket(this,sw["ip"].asString(),sw["port"].asInt());
      mSwitch->setAction((Action)&Radio::onSwitchReply);
      return mSwitch->open();
   }
   return false;
}

Radio::~Radio()
{
  if (::close(mBufferPipe[0]) || ::close(mBufferPipe[1])) {
    mCfg["error"] = "ha aha aha ha aha ha";
  }
  pthread_mutex_destroy(&mLock);
  mir_sdr_StreamUninit();
  mir_sdr_ReleaseDeviceIdx();
}

string Radio::expandName(string str)
{
  string ret;
  for (auto c = str.begin(); c != str.end(); c++) {
    if (*c != '%') {
      ret += *c;
    }
    else {
      c += 1;
      switch (*c) {
        case 'n':
          ret += to_string(mSequenceNumber++);
          break;
        case 'p':
          ret += to_string(server()->id());
          break;
        case 's':
          ret += mCfg["serial-number"].asString();
          break;
        case 'f': {
          int fm = mCfg["center-frequency-MHz"].asDouble() * 10000;
          ret += to_string(fm/10000.0);
        }
          break;
        default:
          ret = ret + '%' + *c;
          break;
      }
    }
  }
  return ret;
}

Value Radio::defaultConfig()
{
  Value cfg;
  cfg["LO-mode"] = 1;
  cfg["notch-filter"] = "ON";
  cfg["IF-kHz"] = 0;
  cfg["antenna"] = "A";
  cfg["bandwidth-kHz"] = 5000;
  cfg["biasT"] = "OFF";
  cfg["center-frequency-MHz"] = 5.5;
  cfg["correction"]["DC"] = "ON";
  cfg["correction"]["IQ"] = "ON";
  cfg["correction"]["PPM"] = 0.0;
  cfg["gain-reduction"]["IF"] = 20;
  cfg["gain-reduction"]["LNA"] = 0;
  cfg["sample-rate-MHz"] = 2.048;
  cfg["sync-out"] = "OFF";
  cfg["file-name"] = "%s.brf";
  return cfg;
}

uint32_t Radio::computeDecimation( uint32_t        rate,
                                   uint32_t       *dec,
                                   mir_sdr_If_kHzT ifmode)
{
   switch (ifmode) {
      case mir_sdr_IF_2_048: 
         if ( rate == 2048000 ) {
            *dec    = 4;
             rate   = 8192000; 
         }
         break;
      case mir_sdr_IF_0_450:
         if ( rate == 1000000 ) {
            *dec    = 2;
             rate   = 2 * rate;
         }
         if ( rate == 500000 ) {
            *dec    = 4;
            rate    = 4 * rate;
         }
         break;
      case mir_sdr_IF_Zero:
         if ( (rate >= 200000) && (rate < 500000) ) {
            *dec = 8; 
             rate = 2000000;
         }
         else if ( (rate >= 500000) && (rate < 1000000) ) {
            *dec = 4;;
            rate = 2000000;
         }
         else if ( (rate >= 1000000) && (rate < 2000000)) {
            *dec = 2;
             rate = 2000000;
         }
         break;
      default:
         *dec = 1;
         break;
   }
   return rate;
}

// Grab a free buffer from the free queue
IQData *Radio::getFreeBuffer()
{
  IQData *buffer;
  lock();
  if (mFree.empty() || mFrameCounter < 1)
    buffer = NULL;
  else {
    buffer = mFree.front();
    mFree.pop();
    mFrameCounter = mFrameCounter - 1;
  }
  unlock();
  return buffer;
}

// yield the now filled free buffer to the full queue
void Radio::yieldFreeBuffer(IQData *buffer)
{
  const char c('d');
  lock();
  mFull.push(buffer);
  // tell radio of the yielded buffer
  if (write(mBufferPipe[1],&c,1)!=1)
    mCfg["error"] = strerror(errno);
  unlock();
}

// grab a full buffer from the full queue.
IQData *Radio::getFullBuffer()
{
  IQData *buffer;
  lock();
  if (mFull.empty())
    buffer = NULL;
  else {
    buffer = mFull.front();
    mFull.pop();
  }
  unlock();
  return buffer;
}

// yield a full buffer to the free que.
void Radio::yieldFullBuffer(IQData *buffer)
{
  lock();
  mFree.push(buffer);
  unlock();
}

void Radio::setFrameCounter(int nf)
{
  lock();
  mFrameCounter = nf;
  unlock();
}

int Radio::currentFrame()
{
  int fc;
  lock();
  fc = mFrameCounter;
  unlock();
  return fc;
}

void initDevT(mir_sdr_DeviceT *device, int &found, int maxCount)
{
  found = -1;
  for (int n = 0; n < maxCount; n++) {
    device[n].SerNo = NULL;
    device[n].DevNm = NULL;
    device[n].hwVer = 0xFF;
    device[n].devAvail = 0xFF;
  }
}

bool invaidDevT(mir_sdr_DeviceT *device, int found)
{
  for (int n = 0; n < found; n++) {
    if (device[n].SerNo == NULL ||
        device[n].DevNm == NULL ||
        device[n].devAvail == 0xFF)
        return true;
  }
  return found < 0;
}

Value Radio::ls()
{
  Value           reply;
  mir_sdr_ErrT    ret;
  mir_sdr_DeviceT devices[2];
  int             found(-1);
  int             maxCount(2);

  //mir_sdr_DebugEnable(1);
  initDevT(devices,found,maxCount);
  while (invaidDevT(devices,found)) {
    ret = mir_sdr_GetDevices(devices,(uint32_t*)&found,(uint32_t)maxCount);
    if (ret) {
      reply["error"] = errorStr(ret);
      return reply;
    }
  }

  for (int n = 0; n < found; n++) {
    reply["radios"][n]["serial-number"] = devices[n].SerNo;
    reply["radios"][n]["device-number"] = devices[n].DevNm;
    reply["radios"][n]["availability"] = devices[n].devAvail;
    reply["radios"][n]["hardware-version"] = (int)devices[n].hwVer;
  }
  return reply;
}

void Radio::jCommand(Value msg)
{
  if (msg.isMember("command")) {
    string cmd = msg["command"].asString();
    if ( cmd == "lc" ) {
      msg["config"] = config();
      msg["state"] = state();
      server()->sendPipe(return_to_sender(msg));
      return;
    }
    else if (cmd == "open") {
      server()->sendPipe(return_to_sender(open(msg)));
    }
    else if (cmd == "start") {
      server()->sendPipe(return_to_sender(start(msg)));
    }
    else if (cmd == "stop") {
      server()->sendPipe(return_to_sender(stop(msg)));
    }
    else if (cmd == "close") {
      server()->sendPipe(return_to_sender(close(msg)));
    }
    else if (cmd == "collect") {
      server()->sendPipe(return_to_sender(collect(msg)));
    }
    else if (cmd == "load") {
      mCfg = msg["config"];
      msg["status"] = "thanks for the config";
      server()->sendPipe(return_to_sender(msg));
    }
    else if (cmd == "set-frequency") {
      mir_sdr_ErrT err;
      mCfg["center-frequency-MHz"] = msg["center-frequency-MHz"].asDouble();
      double freq = msg["center-frequency-MHz"].asDouble() * 1.0E6;
      log() << "set frequency " << (int)freq << " (Hz)" << endl;
      err = mir_sdr_SetRf(freq,1,0);
      if (err) {
        msg = stop(msg);
        msg = start(msg);
        log() << " Error changing frequecy: " << errorStr(err) << endl;
        msg["error"] = errorStr(err);
      }
      server()->sendPipe(return_to_sender(msg));
    }
    else if (cmd == "enable-notch-filter") {
      mir_sdr_ErrT err;
      err = mir_sdr_RSPII_RfNotchEnable(1);
      if (!err) {
        mCfg["notch-filter"] = "ON";
        log() << " Notch filter enabled " << endl;
      }
      else {
        log() << " Error setting notch filter: " << errorStr(err) << endl;
        msg["error"] = errorStr(err);
      }
      server()->sendPipe(return_to_sender(msg));
    }
    else if (cmd == "disable-notch-filter") {
      mir_sdr_ErrT err;
      err = mir_sdr_RSPII_RfNotchEnable(1);
      if (!err) {
        mCfg["notch-filter"] = "OFF";
        log() << " Notch filter disabled " << endl;
      }
      else {
        log() << " Error setting notch filter: " << errorStr(err) << endl;
        msg["error"] = errorStr(err);
      }
      server()->sendPipe(return_to_sender(msg));
    }
    else {
      msg["error"] = "unimplemented";
      server()->sendPipe(return_to_sender(msg));
    }
  }
}

Value Radio::open(Value cmd)
{
   const uint32_t  MaxRadio(4);
   uint32_t        found;
   mir_sdr_DeviceT dev[MaxRadio];
   if (mCfg.isMember("debug")) {
      if (mCfg["debug"].asBool())
         mir_sdr_DebugEnable(1);
      else 
         mir_sdr_DebugEnable(0);
   }
   if (state() != "NotAssigned") {
      cmd["state"] = state();
      cmd["error"] = string("can't open radio in state: ")+state();
      return cmd;
   }
   check(mir_sdr_GetDevices(dev,&found,MaxRadio));
   for (auto nd = 0; nd < found; nd++) {
      if (dev[nd].devAvail==1) {
         cmd["open"]["process"]   = server()->id();
         cmd["open"]["serial"]    = dev[nd].SerNo;
         cmd["open"]["device"]    = nd;
         mCfg["serial-number"]    = dev[nd].SerNo;
         mCfg["hardware-version"] = (int)dev[nd].hwVer;
         check(mir_sdr_SetDeviceIdx(nd));
         state(Opened);
         cmd["state"] = state();
         return cmd;
      }
   }
   cmd["state"] = state();
   cmd["error"] = "no devices available";
   return cmd;
}

Value Radio::start(Value cmd)
{
  int   mRdBSystem;
  int   mSamplesPerPacket;
  mir_sdr_ErrT nret;
  int   hardware(0);
  
  if (mCfg.isMember("hardware-version"))
    hardware = mCfg["hardware-version"].asInt();

  // let operator skip the open step
  if ( state() == "NotAssigned") {
    cmd = open(cmd);
  }

  if ( state() != "Opened") {
    cmd["error"] = string("can't start in state: ")+state();
    return cmd;
  }

  double mFrMHz = mCfg["center-frequency-MHz"].asDouble();
  double mFsMHz = mCfg["sample-rate-MHz"].asDouble();

  // Set for 0 gain reduction with no AGC
  int mGRdB     =    mCfg["gain-reduction"]["IF"].asInt();
  mir_sdr_Bw_MHzT    mBwType   = (mir_sdr_Bw_MHzT)mCfg["bandwidth-kHz"].asInt();
  mir_sdr_If_kHzT    mIfType   = (mir_sdr_If_kHzT)mCfg["IF-kHz"].asInt();
  int                mLNAstate = mCfg["gain-reduction"]["LNA"].asInt();
  mir_sdr_SetGrModeT mGrMode   = mir_sdr_USE_RSP_SET_GR;

  check(mir_sdr_AgcControl(mir_sdr_AGC_DISABLE,0,0,0,0,0,mLNAstate));

  //check(mir_sdr_RSP_SetGrLimits(mir_sdr_EXTENDED_MIN_GR));

  string ant = mCfg["antenna"].asString();
  if (ant == "A") {
    if (hardware == 3) {
       check(mir_sdr_rspDuo_TunerSel(mir_sdr_rspDuo_Tuner_1));
    }
    else if (hardware == 2) {
       check(mir_sdr_RSPII_AntennaControl(mir_sdr_RSPII_ANTENNA_A));
    }
    check(mir_sdr_AmPortSelect(0));
  }
  else if (ant == "B") {
    if (hardware == 3) {
       check(mir_sdr_rspDuo_TunerSel(mir_sdr_rspDuo_Tuner_2));
    }
    else if (hardware == 2) {
       check(mir_sdr_RSPII_AntennaControl(mir_sdr_RSPII_ANTENNA_B));
    }
    check(mir_sdr_AmPortSelect(0));
  }
  else if (ant == "High Z") {
    check(mir_sdr_AmPortSelect(1));
  }

  if (mCfg["sync-out"].asString() == "ON") {
    if (hardware == 3) {
       check(mir_sdr_rspDuo_ExtRef(1));
    }
    else if (hardware == 2) {
       check(mir_sdr_RSPII_ExternalReferenceControl(1));
    }
  }
  else {
    if (hardware == 3) {
       check(mir_sdr_rspDuo_ExtRef(0));
    }
    else if (hardware == 2) {
       check(mir_sdr_RSPII_ExternalReferenceControl(0));
    }
  }
  
  if (hardware == 2) {
     if (mCfg["notch-filter"].asString() == "ON") {
       check(mir_sdr_RSPII_RfNotchEnable(1));
     }
     else {
       check(mir_sdr_RSPII_RfNotchEnable(0));
     }
  }
  int dc = (mCfg["correction"]["DC"].asString()=="ON")?1:0;
  int iq = (mCfg["correction"]["IQ"].asString()=="ON")?1:0;
  check(mir_sdr_DCoffsetIQimbalanceControl(dc,iq));

  check(mir_sdr_SetPpm(mCfg["correction"]["PPM"].asDouble()));

  mir_sdr_StreamCallback_t callback;
  callback = &streamCallback;

  check(mir_sdr_StreamInit(&mGRdB,
                            mFsMHz,
                            mFrMHz,
                            mBwType,
                            mIfType,
                            mLNAstate,
                           &mRdBSystem,
                            mGrMode,
                           &mSamplesPerPacket,
                            callback,
                           &gainCallback,
                           (void*)this));
  state(Streaming);
  cmd["state"] = state();
  return cmd;
}

string Radio::state()
{
  string st;
  lock();
  switch (mState)
  {
    case NotAssigned:
      st = "NotAssigned";
      break;
    case Opened:
      st = "Opened";
      break;
    case Streaming:
      st = "Streaming";
      break;
    case Collecting:
      st = "Collecting";
      break;
    default:
      st = "Lost in Space";
  }
  unlock();
  return st;
}

void Radio::state(Radio::State st)
{
  lock();
  mState = st;
  unlock();
}

Value Radio::stop(Value cmd)
{
  if (state() == "Collecting") {
    setFrameCounter(0);
    state(Streaming);
  }
  if (state() == "Streaming") {
    check(mir_sdr_StreamUninit());
    state(Opened);
  }
  if (state() == "Opened") {
    check(mir_sdr_ReleaseDeviceIdx());
    state(NotAssigned);
  }
  cmd["state"] = state();
  return cmd;
}

Value Radio::close(Value cmd)
{
  if (state() == "Collecting" || state() == "Streaming")
    cmd = stop(cmd);
  if (state() == "Opened") {
    check(mir_sdr_ReleaseDeviceIdx());
    state(NotAssigned);
  }
  cmd["state"] = state();
  return cmd;
}

void Radio::onBufferComplete()
{
  IQData *buffer;
  char cmd[mBuffCount];
  int  nc(0);
  if ((nc = read(mBufferPipe[0],&cmd,mBuffCount)) < 1) {
    mCfg["error"] = strerror(errno);
    return;
  }
  // keep chewing until done.
  for (int k = 0; k < nc; k++) {
    buffer = getFullBuffer();
    if (buffer) {
      mFile.writeData(*buffer);
      yieldFullBuffer(buffer);
    }
  }
  // when collect complete, change state
  if (currentFrame() < 1) {
    state(Streaming);
    // Let the guy know his job is done.
    while (!mNotify.empty()) {
      mNotify.front()["notify"] = "done";
      server()->sendPipe(return_to_sender(mNotify.front()));
      mNotify.pop();
    }
  }
}

void Radio::onSwitchReply()
{
}

Value Radio::collect(Value cmd)
{
  int     nFrames(cmd["frames"].asInt());
  IQData *buffer;
  string  fname(expandName(mCfg["file-name"].asString()));
  //string  fname(mCfg["serial-number"].asString()+".brf");

  mFile.create(fname,mCfg);
  setFrameCounter(nFrames);
  // make a note on who to notify when complete.
  mNotify.push(cmd);
  state(Collecting);
  cmd["state"] = state();
  return cmd;
}
