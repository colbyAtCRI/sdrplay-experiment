#include <mirsdrapi-rsp.h>
#include <iostream>
#include <stdint.h>

using namespace std;

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

int main()
{
  mir_sdr_ErrT    ret;
  mir_sdr_DeviceT devices[2];
  int             found(-1);
  int             maxCount(2);
  const char     *green = "\033[01;32m";
  const char     *reset = "\033[00m";

  cout << "Look for SDRPlay radios" << endl;
  //mir_sdr_DebugEnable(1);
  initDevT(devices,found,maxCount);
  while (invaidDevT(devices,found)) {
    ret = mir_sdr_GetDevices(devices,(uint32_t*)&found,(uint32_t)maxCount);
    if (ret) {
      cout << "GetDevices failed: " << ret << endl;
      return -1;
    }
  }

  cout << "Found: " << found << " devices" << endl;
  for (int n = 0; n < found; n++) {
    cout << "-------------------------" << endl;
    cout << "Serial Number : " << green << devices[n].SerNo << reset << endl;
    cout << "Device Number : " << devices[n].DevNm << endl;
    cout << " Availability : " << ((devices[n].devAvail)?"Available":"In Use") << endl;
    cout << "      Version : " << (int)devices[n].hwVer << endl;
  }
  return 0;
}
