/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Charles J. Cliffe
 * Copyright (c) 2019 Franco Venturi - changes for SDRplay API version 3
 *                                     and Dual Tuner for RSPduo

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

static bool isAtExitRegistered = false;
static sdrplay_api_DeviceT rspDevs[SDRPLAY_MAX_DEVICES];
bool isSdrplayApiOpen = false;
sdrplay_api_DeviceT *deviceSelected = nullptr;

static void close_sdrplay_api(void)
{
   if (deviceSelected)
   {
      sdrplay_api_ReleaseDevice(deviceSelected);
      deviceSelected = nullptr;
   }
   if (isSdrplayApiOpen == true)
   {
      sdrplay_api_Close();
      isSdrplayApiOpen = false;
   }
}

static std::vector<SoapySDR::Kwargs> findSDRPlay(const SoapySDR::Kwargs &args)
{
   std::vector<SoapySDR::Kwargs> results;
   std::string labelHint;
   if (args.count("label") != 0) labelHint = args.at("label");
   unsigned int nDevs = 0;
   char lblstr[128];

   if (isAtExitRegistered == false)
   {
       atexit(close_sdrplay_api);
       isAtExitRegistered = true;
   }

   if (isSdrplayApiOpen == false)
   {
      sdrplay_api_ErrT err;
      if ((err = sdrplay_api_Open()) != sdrplay_api_Success)
      {
          return results;
      }
      isSdrplayApiOpen = true;
   }

   if (deviceSelected)
   {
      sdrplay_api_ReleaseDevice(deviceSelected);
      deviceSelected = nullptr;
   }

   std::string baseLabel = "SDRplay Dev";

   // list devices by API
   sdrplay_api_LockDeviceApi();
   sdrplay_api_GetDevices(&rspDevs[0], &nDevs, SDRPLAY_MAX_DEVICES);

   size_t posidx = labelHint.find(baseLabel);

   if (posidx != std::string::npos)
   {
      unsigned int devIdx = labelHint.at(posidx + baseLabel.length()) - 0x30;

      if (devIdx < nDevs)
      {
         SoapySDR::Kwargs dev;
         dev["driver"] = "sdrplay";
         if (rspDevs[devIdx].hwVer == SDRPLAY_RSP1A_ID)
         {
             sprintf_s(lblstr, 128, "SDRplay Dev%d RSP1A %s", devIdx, rspDevs[devIdx].SerNo);
         }
         else if (rspDevs[devIdx].hwVer == SDRPLAY_RSPduo_ID)
         {
             sprintf_s(lblstr, 128, "SDRplay Dev%d RSPduo %s", devIdx, rspDevs[devIdx].SerNo);
         }
         else
         {
             sprintf_s(lblstr, 128, "SDRplay Dev%d RSP%d %s", devIdx, rspDevs[devIdx].hwVer, rspDevs[devIdx].SerNo);
         }
         dev["label"] = lblstr;
         results.push_back(dev);
      }
   }
   else
   {
      for (unsigned int i = 0; i < nDevs; i++)
      {
         SoapySDR::Kwargs dev;
         dev["driver"] = "sdrplay";
         if (rspDevs[i].hwVer == SDRPLAY_RSP1A_ID)
         {
            sprintf_s(lblstr, 128, "SDRplay Dev%d RSP1A %s", i, rspDevs[i].SerNo);
         }
         else if (rspDevs[i].hwVer == SDRPLAY_RSPduo_ID)
         {
            sprintf_s(lblstr, 128, "SDRplay Dev%d RSPduo %s", i, rspDevs[i].SerNo);
         }
         else
         {
            sprintf_s(lblstr, 128, "SDRplay Dev%d RSP%d %s", i, rspDevs[i].hwVer, rspDevs[i].SerNo);
         }
         dev["label"] = lblstr;
         results.push_back(dev);
      }
   }

   // unlock sdrplay API and close it in case some other driver needs it
   sdrplay_api_UnlockDeviceApi();
   if (isSdrplayApiOpen == true)
   {
      sdrplay_api_Close();
      isSdrplayApiOpen = false;
   }
   return results;
}

static SoapySDR::Device *makeSDRPlay(const SoapySDR::Kwargs &args)
{
    return new SoapySDRPlay(args);
}

static SoapySDR::Registry registerSDRPlay("sdrPlay", &findSDRPlay, &makeSDRPlay, SOAPY_SDR_ABI_VERSION);
