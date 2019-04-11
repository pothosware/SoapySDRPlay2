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

std::set<std::string> &SoapySDRPlay_getClaimedSerials(void)
{
	static std::set<std::string> serials;
	return serials;
}

SoapySDRPlay::SoapySDRPlay(const SoapySDR::Kwargs &args)
{
    if (args.count("serial") == 0) throw std::runtime_error("no sdrplay device found");

    serNo = args.at("serial");

    // retreive hwVer and device index by API
    unsigned int nDevs = 0;

    mir_sdr_DeviceT rspDevs[MAX_RSP_DEVICES];
    mir_sdr_GetDevices(&rspDevs[0], &nDevs, MAX_RSP_DEVICES);

    unsigned devIdx = MAX_RSP_DEVICES;
    for (unsigned int i = 0; i < nDevs; i++)
    {
        if (rspDevs[i].devAvail and rspDevs[i].SerNo == serNo) devIdx = i;
    }
    if (devIdx == MAX_RSP_DEVICES) throw std::runtime_error("no sdrplay device matches");

    hwVer = rspDevs[devIdx].hwVer;

    mir_sdr_ApiVersion(&ver);
    if (ver != MIR_SDR_API_VERSION)
    {
        SoapySDR_logf(SOAPY_SDR_WARNING, "mir_sdr version: '%.3f' does not equal build version: '%.3f'", ver, MIR_SDR_API_VERSION);
    }

    mir_sdr_SetDeviceIdx(devIdx);

    sampleRate = 2000000;
    reqSampleRate = sampleRate;
    decM = 1;
    decEnable = 0;
    centerFrequency = 100;
    ppm = 0.0;
    ifMode = mir_sdr_IF_Zero;
    bwMode = mir_sdr_BW_1_536;
    gRdB = 40;
    lnaState = (hwVer == 2 || hwVer == 3 || hwVer > 253)? 4: 1;

    //this may change later according to format
    shortsPerWord = 1;
    bufferLength = bufferElems * elementsPerSample * shortsPerWord;

    agcMode = mir_sdr_AGC_100HZ;
    dcOffsetMode = true;

    IQcorr = 1;
    setPoint = -30;

    antSel = mir_sdr_RSPII_ANTENNA_A;
    tunSel = mir_sdr_rspDuo_Tuner_1;
    amPort = 0;
    extRef = 0;
    biasTen = 0;
    notchEn = 0;
    dabNotchEn = 0;

    bufferedElems = 0;
    _currentBuff = 0;
    resetBuffer = false;
    useShort = true;
    
    streamActive = false;
    SoapySDRPlay_getClaimedSerials().insert(serNo);
}

SoapySDRPlay::~SoapySDRPlay(void)
{
    SoapySDRPlay_getClaimedSerials().erase(serNo);
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    if (streamActive)
    {
        mir_sdr_StreamUninit();
    }
    streamActive = false;
    mir_sdr_ReleaseDeviceIdx();
}

/*******************************************************************
 * Identification API
 ******************************************************************/

std::string SoapySDRPlay::getDriverKey(void) const
{
    return "SDRplay";
}

std::string SoapySDRPlay::getHardwareKey(void) const
{
    return serNo;
}

SoapySDR::Kwargs SoapySDRPlay::getHardwareInfo(void) const
{
    //key/value pairs for any useful information
    //this also gets printed in --probe
    SoapySDR::Kwargs hwArgs;

    hwArgs["mir_sdr_api_version"] = std::to_string(ver);
    hwArgs["mir_sdr_hw_version"] = std::to_string(hwVer);

    return hwArgs;
}

/*******************************************************************
 * Channels API
 ******************************************************************/

size_t SoapySDRPlay::getNumChannels(const int dir) const
{
    return (dir == SOAPY_SDR_RX) ? 1 : 0;
}

/*******************************************************************
 * Antenna API
 ******************************************************************/

std::vector<std::string> SoapySDRPlay::listAntennas(const int direction, const size_t channel) const
{
    std::vector<std::string> antennas;

    if (direction == SOAPY_SDR_TX) {
        return antennas;
    }

    if (hwVer == 1 || hwVer > 253) {
        antennas.push_back("RX");
    }
    else if (hwVer == 2) {
        antennas.push_back("Antenna A");
        antennas.push_back("Antenna B");
        antennas.push_back("Hi-Z");
    }
    else if (hwVer == 3) {
        antennas.push_back("Tuner 1 50 ohm");
        antennas.push_back("Tuner 2 50 ohm");
        antennas.push_back("Tuner 1 Hi-Z");
    }
    return antennas;
}

void SoapySDRPlay::setAntenna(const int direction, const size_t channel, const std::string &name)
{
    // Check direction
    if ((direction != SOAPY_SDR_RX) || (hwVer == 1) || (hwVer > 253)) {
        return;       
    }

    std::lock_guard <std::mutex> lock(_general_state_mutex);

    if (hwVer == 2)
    {
        bool changeToAntennaA_B = false;

        if (name == "Antenna A")
        {
            antSel = mir_sdr_RSPII_ANTENNA_A;
            changeToAntennaA_B = true;
        }
        else if (name == "Antenna B")
        {
            antSel = mir_sdr_RSPII_ANTENNA_B;
            changeToAntennaA_B = true;
        }
        else if (name == "Hi-Z")
        {
            amPort = 1;
            mir_sdr_AmPortSelect(amPort);

            if (streamActive)
            {
                mir_sdr_Reinit(&gRdB, 0.0, 0.0, mir_sdr_BW_Undefined, mir_sdr_IF_Undefined, mir_sdr_LO_Undefined, lnaState, &gRdBsystem, mir_sdr_USE_RSP_SET_GR, &sps, mir_sdr_CHANGE_AM_PORT);
            }
        }

        if (changeToAntennaA_B)
        {
        
            //if we are currently High_Z, make the switch first.
            if (amPort == 1)
            {
                amPort = 0;
                mir_sdr_AmPortSelect(amPort);
            
                mir_sdr_RSPII_AntennaControl(antSel);

                if (streamActive)
                {
                    mir_sdr_Reinit(&gRdB, 0.0, 0.0, mir_sdr_BW_Undefined, mir_sdr_IF_Undefined, mir_sdr_LO_Undefined, lnaState, &gRdBsystem, mir_sdr_USE_RSP_SET_GR, &sps, mir_sdr_CHANGE_AM_PORT);
                }
            }
            else
            {
                mir_sdr_RSPII_AntennaControl(antSel);
            }
        }
    }
    else if (hwVer == 3)
    {
        bool changeToTuner1_2 = false;
        if (name == "Tuner 1 50 ohm")
        {
            amPort = 0;
            if (tunSel != mir_sdr_rspDuo_Tuner_1)
            {
                tunSel = mir_sdr_rspDuo_Tuner_1;
                changeToTuner1_2 = true;
            }
        }
        else if (name == "Tuner 2 50 ohm")
        {
            amPort = 0;
            if (tunSel != mir_sdr_rspDuo_Tuner_2)
            {
                tunSel = mir_sdr_rspDuo_Tuner_2;
                changeToTuner1_2 = true;
            }
        }
        else if (name == "Tuner 1 HiZ")
        {
            amPort = 1;
            if (tunSel != mir_sdr_rspDuo_Tuner_1)
            {
                tunSel = mir_sdr_rspDuo_Tuner_1;
                changeToTuner1_2 = true;
            }
        }

        if (changeToTuner1_2)
        {
            changeToTuner1_2 = false;
            mir_sdr_rspDuo_TunerSel(tunSel);
        }

        mir_sdr_AmPortSelect(amPort);

        if (streamActive)
        {
            mir_sdr_Reinit(&gRdB, 0.0, 0.0, mir_sdr_BW_Undefined, mir_sdr_IF_Undefined, mir_sdr_LO_Undefined, lnaState, &gRdBsystem, mir_sdr_USE_RSP_SET_GR, &sps, mir_sdr_CHANGE_AM_PORT);
        }
    }
}

std::string SoapySDRPlay::getAntenna(const int direction, const size_t channel) const
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    if (direction == SOAPY_SDR_TX)
    {
        return "";
    }

    if (hwVer == 2)
    {
        if (amPort == 1) {
            return "Hi-Z";
        }
        else if (antSel == mir_sdr_RSPII_ANTENNA_A) {
            return "Antenna A";
        }
        else {
            return "Antenna B";  
        }
    }
    else if (hwVer == 3)
    {
        if (amPort == 1) {
            return "Tuner 1 Hi-Z";
        }
        else if (tunSel == mir_sdr_rspDuo_Tuner_1) {
            return "Tuner 1 50 ohm";
        }
        else {
            return "Tuner 2 50 ohm";  
        }
    }
    else
    {
        return "RX";
    }
}

/*******************************************************************
 * Frontend corrections API
 ******************************************************************/

bool SoapySDRPlay::hasDCOffsetMode(const int direction, const size_t channel) const
{
    return true;
}

void SoapySDRPlay::setDCOffsetMode(const int direction, const size_t channel, const bool automatic)
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    //enable/disable automatic DC removal
    dcOffsetMode = automatic;
    mir_sdr_DCoffsetIQimbalanceControl((unsigned int)automatic, (unsigned int)automatic);
}

bool SoapySDRPlay::getDCOffsetMode(const int direction, const size_t channel) const
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    return dcOffsetMode;
}

bool SoapySDRPlay::hasDCOffset(const int direction, const size_t channel) const
{
    //is a specific DC removal value configurable?
    return false;
}

/*******************************************************************
 * Gain API
 ******************************************************************/

std::vector<std::string> SoapySDRPlay::listGains(const int direction, const size_t channel) const
{
    //list available gain elements,
    //the functions below have a "name" parameter
    std::vector<std::string> results;

    results.push_back("IFGR");
    results.push_back("RFGR");

    return results;
}

bool SoapySDRPlay::hasGainMode(const int direction, const size_t channel) const
{
    return true;
}

void SoapySDRPlay::setGainMode(const int direction, const size_t channel, const bool automatic)
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    agcMode = mir_sdr_AGC_DISABLE;

    if (automatic == true) {
        agcMode = mir_sdr_AGC_100HZ;
        //align known agc values with current value before starting AGC.
        current_gRdB = gRdB;
    }
    mir_sdr_AgcControl(agcMode, setPoint, 0, 0, 0, 0, lnaState);
}

bool SoapySDRPlay::getGainMode(const int direction, const size_t channel) const
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    return (agcMode == mir_sdr_AGC_DISABLE)? false: true;
}

void SoapySDRPlay::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

   bool doUpdate = false;

   if (name == "IFGR")
   {
      //Depending of the previously used AGC context, the real applied 
      // gain may be either gRdB or current_gRdB, so apply the change if required value is different 
      //from one of them.
      if ((gRdB != (int)value) || (current_gRdB != (int)value))
      {
         gRdB = (int)value;
         current_gRdB = (int)value;
         doUpdate = true;
      }
   }
   else if (name == "RFGR")
   {
      if (lnaState != (int)value) {

          lnaState = (int)value;
          doUpdate = true;
      }
   }
   if ((doUpdate == true) && (streamActive))
   {
      mir_sdr_Reinit(&gRdB, 0.0, 0.0, mir_sdr_BW_Undefined, mir_sdr_IF_Undefined, mir_sdr_LO_Undefined, lnaState, &gRdBsystem, mir_sdr_USE_RSP_SET_GR, &sps, mir_sdr_CHANGE_GR);
   }
}

double SoapySDRPlay::getGain(const int direction, const size_t channel, const std::string &name) const
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

   if (name == "IFGR")
   {
       return current_gRdB;
   }
   else if (name == "RFGR")
   {
      return lnaState;
   }

   return 0;
}

SoapySDR::Range SoapySDRPlay::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
   if (name == "IFGR")
   {
      return SoapySDR::Range(20, 59);
   }
   else if ((name == "RFGR") && (hwVer == 1))
   {
      return SoapySDR::Range(0, 3);
   }
   else if ((name == "RFGR") && (hwVer == 2))
   {
      return SoapySDR::Range(0, 8);
   }
   else if ((name == "RFGR") && (hwVer == 3))
   {
      return SoapySDR::Range(0, 9);
   }
   else if ((name == "RFGR") && (hwVer > 253))
   {
      return SoapySDR::Range(0, 9);
   }
    return SoapySDR::Range(20, 59);
}

/*******************************************************************
 * Frequency API
 ******************************************************************/

void SoapySDRPlay::setFrequency(const int direction,
                                const size_t channel,
                                const std::string &name,
                                const double frequency,
                                const SoapySDR::Kwargs &args)
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

   if (direction == SOAPY_SDR_RX)
   {
      if ((name == "RF") && (centerFrequency != (uint32_t)frequency))
      {
         centerFrequency = (uint32_t)frequency;
         if (streamActive)
         {
            mir_sdr_Reinit(&gRdB, 0.0, frequency / 1e6, mir_sdr_BW_Undefined, mir_sdr_IF_Undefined, mir_sdr_LO_Undefined, lnaState, &gRdBsystem, mir_sdr_USE_RSP_SET_GR, &sps, mir_sdr_CHANGE_RF_FREQ);
         }
      }
      else if ((name == "CORR") && (ppm != frequency))
      {
         ppm = frequency;
         mir_sdr_SetPpm(ppm);
      }
   }
}

double SoapySDRPlay::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    if (name == "RF")
    {
        return (double)centerFrequency;
    }
    else if (name == "CORR")
    {
        return ppm;
    }

    return 0;
}

std::vector<std::string> SoapySDRPlay::listFrequencies(const int direction, const size_t channel) const
{
    std::vector<std::string> names;
    names.push_back("RF");
    names.push_back("CORR");
    return names;
}

SoapySDR::RangeList SoapySDRPlay::getFrequencyRange(const int direction, const size_t channel,  const std::string &name) const
{
    SoapySDR::RangeList results;
    if (name == "RF")
    {
       results.push_back(SoapySDR::Range(10000, 2000000000));
    }
    return results;
}

SoapySDR::ArgInfoList SoapySDRPlay::getFrequencyArgsInfo(const int direction, const size_t channel) const
{
    SoapySDR::ArgInfoList freqArgs;

    return freqArgs;
}

/*******************************************************************
 * Sample Rate API
 ******************************************************************/

void SoapySDRPlay::setSampleRate(const int direction, const size_t channel, const double rate)
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting sample rate: %d", sampleRate);

    if (direction == SOAPY_SDR_RX)
    {
       unsigned int decMp = decM;
       reqSampleRate = (uint32_t)rate;
       uint32_t currSampleRate = sampleRate;

       sampleRate = getInputSampleRateAndDecimation(reqSampleRate, &decM, &decEnable, ifMode);
       bwMode = getBwEnumForRate(rate, ifMode);

       if ((sampleRate != currSampleRate) || (decM != decMp) || (reqSampleRate != sampleRate))
       {
          resetBuffer = true;
          if (streamActive)
          {
             mir_sdr_Reinit(&gRdB, sampleRate / 1e6, 0.0, bwMode, mir_sdr_IF_Undefined, mir_sdr_LO_Undefined, lnaState, &gRdBsystem, mir_sdr_USE_RSP_SET_GR, &sps, (mir_sdr_ReasonForReinitT)(mir_sdr_CHANGE_FS_FREQ | mir_sdr_CHANGE_BW_TYPE));
             if (ifMode == mir_sdr_IF_Zero)
             {
                mir_sdr_DecimateControl(decEnable, decM, 1);
             }
          }
       }
    }
}

double SoapySDRPlay::getSampleRate(const int direction, const size_t channel) const
{
   return reqSampleRate;
}

std::vector<double> SoapySDRPlay::listSampleRates(const int direction, const size_t channel) const
{
    std::vector<double> rates;

    rates.push_back(250000);
    rates.push_back(500000);
    rates.push_back(1000000);
    rates.push_back(2000000);
    rates.push_back(2048000);
    rates.push_back(3000000);
    rates.push_back(4000000);
    rates.push_back(5000000);
    rates.push_back(6000000);
    rates.push_back(7000000);
    rates.push_back(8000000);
    rates.push_back(9000000);
    rates.push_back(10000000);
    
    return rates;
}

uint32_t SoapySDRPlay::getInputSampleRateAndDecimation(uint32_t rate, unsigned int *decM, unsigned int *decEnable, mir_sdr_If_kHzT ifMode)
{
   if (ifMode == mir_sdr_IF_2_048)
   {
      if      (rate == 2048000) { *decM = 4; *decEnable = 1; return 8192000; }
   }
   else if (ifMode == mir_sdr_IF_0_450)
   {
      if      (rate == 1000000) { *decM = 2; *decEnable = 1; return 2000000; }
      else if (rate == 500000)  { *decM = 4; *decEnable = 1; return 2000000; }
   }
   else if (ifMode == mir_sdr_IF_Zero)
   {

      if      ((rate >= 200000)  && (rate < 500000))  { *decM = 8; *decEnable = 1; return 2000000; }
      else if ((rate >= 500000)  && (rate < 1000000)) { *decM = 4; *decEnable = 1; return 2000000; }
      else if ((rate >= 1000000) && (rate < 2000000)) { *decM = 2; *decEnable = 1; return 2000000; }
      else                                            { *decM = 1; *decEnable = 0; return rate; }
   }

   // this is invalid, but return something
   *decM = 1; *decEnable = 0; return rate;
}

/*******************************************************************
* Bandwidth API
******************************************************************/

void SoapySDRPlay::setBandwidth(const int direction, const size_t channel, const double bw_in)
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

   if (direction == SOAPY_SDR_RX) 
   {
      if (getBwValueFromEnum(bwMode) != bw_in)
      {
         bwMode = mirGetBwMhzEnum(bw_in);
         if (streamActive)
         {
            mir_sdr_Reinit(&gRdB, 0.0, 0.0, bwMode, mir_sdr_IF_Undefined, mir_sdr_LO_Undefined, lnaState, &gRdBsystem, mir_sdr_USE_RSP_SET_GR, &sps, mir_sdr_CHANGE_BW_TYPE);
         }
      }
   }
}

double SoapySDRPlay::getBandwidth(const int direction, const size_t channel) const
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

   if (direction == SOAPY_SDR_RX)
   {
      return getBwValueFromEnum(bwMode);
   }
   return 0;
}

std::vector<double> SoapySDRPlay::listBandwidths(const int direction, const size_t channel) const
{
   std::vector<double> bandwidths;
   bandwidths.push_back(200000);
   bandwidths.push_back(300000);
   bandwidths.push_back(600000);
   bandwidths.push_back(1536000);
   bandwidths.push_back(5000000);
   bandwidths.push_back(6000000);
   bandwidths.push_back(7000000);
   bandwidths.push_back(8000000);
   return bandwidths;
}

SoapySDR::RangeList SoapySDRPlay::getBandwidthRange(const int direction, const size_t channel) const
{
   SoapySDR::RangeList results;
   //call into the older deprecated listBandwidths() call
   for (auto &bw : this->listBandwidths(direction, channel))
   {
     results.push_back(SoapySDR::Range(bw, bw));
   }
   return results;
}

double SoapySDRPlay::getRateForBwEnum(mir_sdr_Bw_MHzT bwEnum)
{
   if (bwEnum == mir_sdr_BW_0_200) return 250000;
   else if (bwEnum == mir_sdr_BW_0_300) return 500000;
   else if (bwEnum == mir_sdr_BW_0_600) return 1000000;
   else if (bwEnum == mir_sdr_BW_1_536) return 2000000;
   else if (bwEnum == mir_sdr_BW_5_000) return 5000000;
   else if (bwEnum == mir_sdr_BW_6_000) return 6000000;
   else if (bwEnum == mir_sdr_BW_7_000) return 7000000;
   else if (bwEnum == mir_sdr_BW_8_000) return 8000000;
   else return 0;
}


mir_sdr_Bw_MHzT SoapySDRPlay::getBwEnumForRate(double rate, mir_sdr_If_kHzT ifMode)
{
   if (ifMode == mir_sdr_IF_Zero)
   {
      if      ((rate >= 200000)  && (rate < 300000))  return mir_sdr_BW_0_200;
      else if ((rate >= 300000)  && (rate < 600000))  return mir_sdr_BW_0_300;
      else if ((rate >= 600000)  && (rate < 1536000)) return mir_sdr_BW_0_600;
      else if ((rate >= 1536000) && (rate < 5000000)) return mir_sdr_BW_1_536;
      else if ((rate >= 5000000) && (rate < 6000000)) return mir_sdr_BW_5_000;
      else if ((rate >= 6000000) && (rate < 7000000)) return mir_sdr_BW_6_000;
      else if ((rate >= 7000000) && (rate < 8000000)) return mir_sdr_BW_7_000;
      else                                            return mir_sdr_BW_8_000;
   }
   else if ((ifMode == mir_sdr_IF_0_450) || (ifMode == mir_sdr_IF_1_620))
   {
      if      ((rate >= 200000)  && (rate < 500000))  return mir_sdr_BW_0_200;
      else if ((rate >= 500000)  && (rate < 1000000)) return mir_sdr_BW_0_300;
      else                                            return mir_sdr_BW_0_600;
   }
   else
   {
      if      ((rate >= 200000)  && (rate < 500000))  return mir_sdr_BW_0_200;
      else if ((rate >= 500000)  && (rate < 1000000)) return mir_sdr_BW_0_300;
      else if ((rate >= 1000000) && (rate < 1536000)) return mir_sdr_BW_0_600;
      else                                            return mir_sdr_BW_1_536;
   }
}


double SoapySDRPlay::getBwValueFromEnum(mir_sdr_Bw_MHzT bwEnum)
{
   if      (bwEnum == mir_sdr_BW_0_200) return 200000;
   else if (bwEnum == mir_sdr_BW_0_300) return 300000;
   else if (bwEnum == mir_sdr_BW_0_600) return 600000;
   else if (bwEnum == mir_sdr_BW_1_536) return 1536000;
   else if (bwEnum == mir_sdr_BW_5_000) return 5000000;
   else if (bwEnum == mir_sdr_BW_6_000) return 6000000;
   else if (bwEnum == mir_sdr_BW_7_000) return 7000000;
   else if (bwEnum == mir_sdr_BW_8_000) return 8000000;
   else return 0;
}


mir_sdr_Bw_MHzT SoapySDRPlay::mirGetBwMhzEnum(double bw)
{
   if      (bw == 200000) return mir_sdr_BW_0_200;
   else if (bw == 300000) return mir_sdr_BW_0_300;
   else if (bw == 600000) return mir_sdr_BW_0_600;
   else if (bw == 1536000) return mir_sdr_BW_1_536;
   else if (bw == 5000000) return mir_sdr_BW_5_000;
   else if (bw == 6000000) return mir_sdr_BW_6_000;
   else if (bw == 7000000) return mir_sdr_BW_7_000;
   else if (bw == 8000000) return mir_sdr_BW_8_000;
   else return mir_sdr_BW_0_200;
}

/*******************************************************************
* Settings API
******************************************************************/

mir_sdr_If_kHzT SoapySDRPlay::stringToIF(std::string ifMode)
{
   if (ifMode == "Zero-IF")
   {
      return mir_sdr_IF_Zero;
   }
   else if (ifMode == "450kHz")
   {
      return mir_sdr_IF_0_450;
   }
   else if (ifMode == "1620kHz")
   {
      return mir_sdr_IF_1_620;
   }
   else if (ifMode == "2048kHz")
   {
      return mir_sdr_IF_2_048;
   }
   return mir_sdr_IF_Zero;
}

std::string SoapySDRPlay::IFtoString(mir_sdr_If_kHzT ifkHzT)
{
   switch (ifkHzT)
   {
   case mir_sdr_IF_Zero:
      return "Zero-IF";
      break;
   case mir_sdr_IF_0_450:
      return "450kHz";
      break;
   case mir_sdr_IF_1_620:
      return "1620kHz";
      break;
   case mir_sdr_IF_2_048:
      return "2048kHz";
      break;
   case mir_sdr_IF_Undefined:
      return "";
      break;
   }
   return "";
}

SoapySDR::ArgInfoList SoapySDRPlay::getSettingInfo(void) const
{
    SoapySDR::ArgInfoList setArgs;
 
#ifdef RF_GAIN_IN_MENU
    if (hwVer == 2)
    {
       SoapySDR::ArgInfo RfGainArg;
       RfGainArg.key = "rfgain_sel";
       RfGainArg.value = "4";
       RfGainArg.name = "RF Gain Select";
       RfGainArg.description = "RF Gain Select";
       RfGainArg.type = SoapySDR::ArgInfo::STRING;
       RfGainArg.options.push_back("0");
       RfGainArg.options.push_back("1");
       RfGainArg.options.push_back("2");
       RfGainArg.options.push_back("3");
       RfGainArg.options.push_back("4");
       RfGainArg.options.push_back("5");
       RfGainArg.options.push_back("6");
       RfGainArg.options.push_back("7");
       RfGainArg.options.push_back("8");
       setArgs.push_back(RfGainArg);
    }
    else if (hwVer == 3)
    {
       SoapySDR::ArgInfo RfGainArg;
       RfGainArg.key = "rfgain_sel";
       RfGainArg.value = "4";
       RfGainArg.name = "RF Gain Select";
       RfGainArg.description = "RF Gain Select";
       RfGainArg.type = SoapySDR::ArgInfo::STRING;
       RfGainArg.options.push_back("0");
       RfGainArg.options.push_back("1");
       RfGainArg.options.push_back("2");
       RfGainArg.options.push_back("3");
       RfGainArg.options.push_back("4");
       RfGainArg.options.push_back("5");
       RfGainArg.options.push_back("6");
       RfGainArg.options.push_back("7");
       RfGainArg.options.push_back("8");
       RfGainArg.options.push_back("9");
       setArgs.push_back(RfGainArg);
    }
    else if (hwVer > 253)
    {
       SoapySDR::ArgInfo RfGainArg;
       RfGainArg.key = "rfgain_sel";
       RfGainArg.value = "4";
       RfGainArg.name = "RF Gain Select";
       RfGainArg.description = "RF Gain Select";
       RfGainArg.type = SoapySDR::ArgInfo::STRING;
       RfGainArg.options.push_back("0");
       RfGainArg.options.push_back("1");
       RfGainArg.options.push_back("2");
       RfGainArg.options.push_back("3");
       RfGainArg.options.push_back("4");
       RfGainArg.options.push_back("5");
       RfGainArg.options.push_back("6");
       RfGainArg.options.push_back("7");
       RfGainArg.options.push_back("8");
       RfGainArg.options.push_back("9");
       setArgs.push_back(RfGainArg);
    }
    else
    {
       SoapySDR::ArgInfo RfGainArg;
       RfGainArg.key = "rfgain_sel";
       RfGainArg.value = "1";
       RfGainArg.name = "RF Gain Select";
       RfGainArg.description = "RF Gain Select";
       RfGainArg.type = SoapySDR::ArgInfo::STRING;
       RfGainArg.options.push_back("0");
       RfGainArg.options.push_back("1");
       RfGainArg.options.push_back("2");
       RfGainArg.options.push_back("3");
       setArgs.push_back(RfGainArg);
    }
#endif
    
    SoapySDR::ArgInfo AIFArg;
    AIFArg.key = "if_mode";
    AIFArg.value = IFtoString(ifMode);
    AIFArg.name = "IF Mode";
    AIFArg.description = "IF frequency in kHz";
    AIFArg.type = SoapySDR::ArgInfo::STRING;
    AIFArg.options.push_back(IFtoString(mir_sdr_IF_Zero));
    AIFArg.options.push_back(IFtoString(mir_sdr_IF_0_450));
    AIFArg.options.push_back(IFtoString(mir_sdr_IF_1_620));
    AIFArg.options.push_back(IFtoString(mir_sdr_IF_2_048));
    setArgs.push_back(AIFArg);

    SoapySDR::ArgInfo IQcorrArg;
    IQcorrArg.key = "iqcorr_ctrl";
    IQcorrArg.value = "true";
    IQcorrArg.name = "IQ Correction";
    IQcorrArg.description = "IQ Correction Control";
    IQcorrArg.type = SoapySDR::ArgInfo::BOOL;
    setArgs.push_back(IQcorrArg);

    SoapySDR::ArgInfo SetPointArg;
    SetPointArg.key = "agc_setpoint";
    SetPointArg.value = "-30";
    SetPointArg.name = "AGC Setpoint";
    SetPointArg.description = "AGC Setpoint (dBfs)";
    SetPointArg.type = SoapySDR::ArgInfo::INT;
    SetPointArg.range = SoapySDR::Range(-60, 0);
    setArgs.push_back(SetPointArg);

    if (hwVer == 2) // RSP2/RSP2pro
    {
       SoapySDR::ArgInfo ExtRefArg;
       ExtRefArg.key = "extref_ctrl";
       ExtRefArg.value = "true";
       ExtRefArg.name = "ExtRef Enable";
       ExtRefArg.description = "External Reference Control";
       ExtRefArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(ExtRefArg);

       SoapySDR::ArgInfo BiasTArg;
       BiasTArg.key = "biasT_ctrl";
       BiasTArg.value = "true";
       BiasTArg.name = "BiasT Enable";
       BiasTArg.description = "BiasT Control";
       BiasTArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(BiasTArg);

       SoapySDR::ArgInfo RfNotchArg;
       RfNotchArg.key = "rfnotch_ctrl";
       RfNotchArg.value = "true";
       RfNotchArg.name = "RfNotch Enable";
       RfNotchArg.description = "RF Notch Filter Control";
       RfNotchArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(RfNotchArg);
    }
    else if (hwVer == 3) // RSPduo
    {
       SoapySDR::ArgInfo ExtRefArg;
       ExtRefArg.key = "extref_ctrl";
       ExtRefArg.value = "true";
       ExtRefArg.name = "ExtRef Enable";
       ExtRefArg.description = "External Reference Control";
       ExtRefArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(ExtRefArg);

       SoapySDR::ArgInfo BiasTArg;
       BiasTArg.key = "biasT_ctrl";
       BiasTArg.value = "true";
       BiasTArg.name = "BiasT Enable";
       BiasTArg.description = "BiasT Control";
       BiasTArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(BiasTArg);

       SoapySDR::ArgInfo RfNotchArg;
       RfNotchArg.key = "rfnotch_ctrl";
       RfNotchArg.value = "true";
       RfNotchArg.name = "RfNotch Enable";
       RfNotchArg.description = "RF Notch Filter Control";
       RfNotchArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(RfNotchArg);

       SoapySDR::ArgInfo DabNotchArg;
       DabNotchArg.key = "dabnotch_ctrl";
       DabNotchArg.value = "true";
       DabNotchArg.name = "DabNotch Enable";
       DabNotchArg.description = "DAB Notch Filter Control";
       DabNotchArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(DabNotchArg);
    }
    else if (hwVer > 253) // RSP1A
    {
       SoapySDR::ArgInfo BiasTArg;
       BiasTArg.key = "biasT_ctrl";
       BiasTArg.value = "true";
       BiasTArg.name = "BiasT Enable";
       BiasTArg.description = "BiasT Control";
       BiasTArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(BiasTArg);

       SoapySDR::ArgInfo RfNotchArg;
       RfNotchArg.key = "rfnotch_ctrl";
       RfNotchArg.value = "true";
       RfNotchArg.name = "RfNotch Enable";
       RfNotchArg.description = "RF Notch Filter Control";
       RfNotchArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(RfNotchArg);

       SoapySDR::ArgInfo DabNotchArg;
       DabNotchArg.key = "dabnotch_ctrl";
       DabNotchArg.value = "true";
       DabNotchArg.name = "DabNotch Enable";
       DabNotchArg.description = "DAB Notch Filter Control";
       DabNotchArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(DabNotchArg);
    }

    return setArgs;
}

void SoapySDRPlay::writeSetting(const std::string &key, const std::string &value)
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

#ifdef RF_GAIN_IN_MENU
   if (key == "rfgain_sel")
   {
      if      (value == "0") lnaState = 0;
      else if (value == "1") lnaState = 1;
      else if (value == "2") lnaState = 2;
      else if (value == "3") lnaState = 3;
      else if (value == "4") lnaState = 4;
      else if (value == "5") lnaState = 5;
      else if (value == "6") lnaState = 6;
      else if (value == "7") lnaState = 7;
      else if (value == "8") lnaState = 8;
      else                   lnaState = 9;
      if (agcMode != mir_sdr_AGC_DISABLE)
      {
         mir_sdr_AgcControl(agcMode, setPoint, 0, 0, 0, 0, lnaState);
      }
      else
      {
         mir_sdr_Reinit(&gRdB, 0.0, 0.0, mir_sdr_BW_Undefined, mir_sdr_IF_Undefined, mir_sdr_LO_Undefined, lnaState, &gRdBsystem, mir_sdr_USE_RSP_SET_GR, &sps, mir_sdr_CHANGE_GR);
      }
   }
   else
#endif
   if (key == "if_mode")
   {
      if (ifMode != stringToIF(value))
      {
         ifMode = stringToIF(value);
         sampleRate = getInputSampleRateAndDecimation(reqSampleRate, &decM, &decEnable, ifMode);
         bwMode = getBwEnumForRate(reqSampleRate, ifMode);
         if (streamActive)
         {
            mir_sdr_DecimateControl(0, 1, 1);
            mir_sdr_Reinit(&gRdB, sampleRate / 1e6, 0.0, bwMode, ifMode, mir_sdr_LO_Undefined, lnaState, &gRdBsystem, mir_sdr_USE_RSP_SET_GR, &sps, (mir_sdr_ReasonForReinitT)(mir_sdr_CHANGE_FS_FREQ | mir_sdr_CHANGE_BW_TYPE | mir_sdr_CHANGE_IF_TYPE));
         }
      }
   }
   else if (key == "iqcorr_ctrl")
   {
      if (value == "false") IQcorr = 0;
      else                  IQcorr = 1;
      mir_sdr_DCoffsetIQimbalanceControl(1, IQcorr);
      //mir_sdr_DCoffsetIQimbalanceControl(IQcorr, IQcorr);
   }
   else if (key == "agc_setpoint")
   {
      setPoint = stoi(value);
      mir_sdr_AgcControl(agcMode, setPoint, 0, 0, 0, 0, lnaState);
   }
   else if (key == "extref_ctrl")
   {
      if (value == "false") extRef = 0;
      else                  extRef = 1;
      if (hwVer == 2) mir_sdr_RSPII_ExternalReferenceControl(extRef);
      if (hwVer == 3) mir_sdr_rspDuo_ExtRef(extRef);
   }
   else if (key == "biasT_ctrl")
   {
      if (value == "false") biasTen = 0;
      else                  biasTen = 1;
      if (hwVer == 2) mir_sdr_RSPII_BiasTControl(biasTen);
      if (hwVer == 3) mir_sdr_rspDuo_BiasT(biasTen);
      if (hwVer > 253) mir_sdr_rsp1a_BiasT(biasTen);
   }
   else if (key == "rfnotch_ctrl")
   {
      if (value == "false") notchEn = 0;
      else                  notchEn = 1;
      if (hwVer == 2) mir_sdr_RSPII_RfNotchEnable(notchEn);
      if (hwVer == 3)
      {
        if (tunSel == mir_sdr_rspDuo_Tuner_1 && amPort == 1) mir_sdr_rspDuo_Tuner1AmNotch(notchEn);
        if (amPort == 0) mir_sdr_rspDuo_BroadcastNotch(notchEn);
      }
      if (hwVer > 253) mir_sdr_rsp1a_BroadcastNotch(notchEn);
   }
   else if (key == "dabnotch_ctrl")
   {
      if (value == "false") dabNotchEn = 0;
      else                  dabNotchEn = 1;
      if (hwVer == 3) mir_sdr_rspDuo_DabNotch(dabNotchEn);
      if (hwVer > 253) mir_sdr_rsp1a_DabNotch(dabNotchEn);
   }
}

std::string SoapySDRPlay::readSetting(const std::string &key) const
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

#ifdef RF_GAIN_IN_MENU
    if (key == "rfgain_sel")
    {
       if      (lnaState == 0) return "0";
       else if (lnaState == 1) return "1";
       else if (lnaState == 2) return "2";
       else if (lnaState == 3) return "3";
       else if (lnaState == 4) return "4";
       else if (lnaState == 5) return "5";
       else if (lnaState == 6) return "6";
       else if (lnaState == 7) return "7";
       else if (lnaState == 8) return "8";
       else                    return "9";
    }
    else
#endif
    if (key == "if_mode")
    {
        return IFtoString(ifMode);
    }
    else if (key == "iqcorr_ctrl")
    {
       if (IQcorr == 0) return "false";
       else             return "true";
    }
    else if (key == "agc_setpoint")
    {
       return std::to_string(setPoint);
    }
    else if (key == "extref_ctrl")
    {
       if (extRef == 0) return "false";
       else             return "true";
    }
    else if (key == "biasT_ctrl")
    {
       if (biasTen == 0) return "false";
       else              return "true";
    }
    else if (key == "rfnotch_ctrl")
    {
       if (notchEn == 0) return "false";
       else              return "true";
    }
    else if (key == "dabnotch_ctrl")
    {
       if (dabNotchEn == 0) return "false";
       else                 return "true";
    }

    // SoapySDR_logf(SOAPY_SDR_WARNING, "Unknown setting '%s'", key.c_str());
    return "";
}
