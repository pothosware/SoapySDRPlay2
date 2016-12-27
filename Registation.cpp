/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Charles J. Cliffe

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "SoapySDRPlay.hpp"
#include <SoapySDR/Registry.hpp>

#if !defined(_M_X64) && !defined(_M_IX86)
#define sprintf_s(buffer, buffer_size, stringbuffer, ...) (sprintf(buffer, stringbuffer, __VA_ARGS__))
#endif

#define MAX_RSP_DEVICES  (4)

static mir_sdr_DeviceT rspDevs[MAX_RSP_DEVICES];
bool deviceSelected = false;

static std::vector<SoapySDR::Kwargs> findSDRPlay(const SoapySDR::Kwargs &args)
{
   std::vector<SoapySDR::Kwargs> results;
   std::string strargs = SoapySDR::KwargsToString(args);
   unsigned int nDevs = 0;
   char lblstr[128];

   if (deviceSelected == true)
   {
      mir_sdr_ReleaseDeviceIdx();
      deviceSelected = false;
   }
   mir_sdr_DebugEnable(1);
   mir_sdr_GetDevices(&rspDevs[0], &nDevs, MAX_RSP_DEVICES);

   size_t posidx = strargs.find("SDRplay Dev");
   if (posidx != std::string::npos)
   {
      unsigned int devIdx = strargs.at(posidx + 11) - 0x30;
      if ((devIdx < nDevs) && (rspDevs[devIdx].devAvail))
      {
         SoapySDR::Kwargs dev;
         dev["driver"] = "sdrplay";
         sprintf_s(lblstr, 128, "SDRplay Dev%d RSP%d %s", devIdx, rspDevs[devIdx].hwVer, rspDevs[devIdx].SerNo);
         dev["label"] = lblstr;
         results.push_back(dev);
      }
   }
   else
   {
      for (unsigned int i = 0; i < nDevs; i++)
      {
         if (rspDevs[i].devAvail)
         {
            SoapySDR::Kwargs dev;
            dev["driver"] = "sdrplay";
            sprintf_s(lblstr, 128, "SDRplay Dev%d RSP%d %s", i, rspDevs[i].hwVer, rspDevs[i].SerNo);
            dev["label"] = lblstr;
            results.push_back(dev);
         }
      }
   }
   return results;
}

static SoapySDR::Device *makeSDRPlay(const SoapySDR::Kwargs &args)
{
    return new SoapySDRPlay(args);
}

static SoapySDR::Registry registerSDRPlay("sdrplay", &findSDRPlay, &makeSDRPlay, SOAPY_SDR_ABI_VERSION);
