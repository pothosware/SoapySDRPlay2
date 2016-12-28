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

#define RF_GAIN_IN_MENU

#include "SoapySDRPlay.hpp"

extern bool deviceSelected;    // global declared in Registration.cpp

SoapySDRPlay::SoapySDRPlay(const SoapySDR::Kwargs &args)
{
    std::string strargs = SoapySDR::KwargsToString(args);

    size_t posidx = strargs.find("SDRplay Dev");
    if (posidx == std::string::npos)
    {
        SoapySDR_logf(SOAPY_SDR_WARNING, "Can't find Dev string in args");
        return;
    }
    unsigned int devIdx = strargs.at(posidx + 11) - 0x30;
    hwVer = strargs.at(posidx + 16) - 0x30;
    serNo = strargs.substr(posidx + 16, 20);
    size_t poscom = serNo.find(",");
    if (poscom != std::string::npos)
    {
       serNo = serNo.substr(0, poscom);
    }

    mir_sdr_ApiVersion(&ver);
    if (ver != MIR_SDR_API_VERSION)
    {
        SoapySDR_logf(SOAPY_SDR_WARNING, "mir_sdr version: '%.3f' does not equal build version: '%.3f'", ver, MIR_SDR_API_VERSION);
    }

    mir_sdr_SetDeviceIdx(devIdx);
    deviceSelected = true;

    sampleRate = 2000000;
    reqSampleRate = sampleRate;
    decM = 1;
    decEnable = 0;
    centerFrequency = 100;
    ppm = 0.0;
    ifMode = mir_sdr_IF_Zero;
    bwMode = mir_sdr_BW_1_536;
    gRdB = 40;
    lnaState = (hwVer == 2)? 4: 1;

    numBuffers = DEFAULT_NUM_BUFFERS;
    bufferElems = DEFAULT_BUFFER_LENGTH;
    elementsPerSample = DEFAULT_ELEMS_PER_SAMPLE;
    shortsPerWord = 1;
    bufferLength = bufferElems * elementsPerSample;

    agcMode = mir_sdr_AGC_100HZ;
    dcOffsetMode = true;

    IQcorr = 1;
    setPoint = -30;

    antSel = mir_sdr_RSPII_ANTENNA_A;
    amPort = 0;
    extRef = 0;
    biasTen = 0;
    notechEn = 0;

    bufferedElems = 0;
    _currentBuff = 0;
    resetBuffer = false;
    useShort = true;
    
    streamActive = false;
}

SoapySDRPlay::~SoapySDRPlay(void)
{
    if (streamActive)
    {
        mir_sdr_StreamUninit();
    }
    streamActive = false;
    mir_sdr_ReleaseDeviceIdx();
    deviceSelected = false;
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
    antennas.push_back("RX");
    return antennas;
}

void SoapySDRPlay::setAntenna(const int direction, const size_t channel, const std::string &name)
{
    // TODO
}

std::string SoapySDRPlay::getAntenna(const int direction, const size_t channel) const
{
    return "RX";
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
    //enable/disable automatic DC removal
    dcOffsetMode = automatic;
    mir_sdr_DCoffsetIQimbalanceControl((unsigned int)automatic, (unsigned int)automatic);
}

bool SoapySDRPlay::getDCOffsetMode(const int direction, const size_t channel) const
{
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
#ifndef RF_GAIN_IN_MENU
    results.push_back("RFGR");
#endif

    return results;
}

bool SoapySDRPlay::hasGainMode(const int direction, const size_t channel) const
{
    return true;
}

void SoapySDRPlay::setGainMode(const int direction, const size_t channel, const bool automatic)
{
    agcMode = mir_sdr_AGC_DISABLE;
    if (automatic == true)  agcMode = mir_sdr_AGC_100HZ;
    mir_sdr_AgcControl(agcMode, setPoint, 0, 0, 0, 0, lnaState);
}

bool SoapySDRPlay::getGainMode(const int direction, const size_t channel) const
{
    return (agcMode == mir_sdr_AGC_DISABLE)? false: true;
}

void SoapySDRPlay::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
   bool doUpdate = false;

   if (name == "IFGR")
   {
      if (gRdB != (int)value)
      {
         gRdB = (int)value;
         doUpdate = true;
      }
   }
#ifndef RF_GAIN_IN_MENU
   else if (name == "RFGR")
   {
      if (lnaState != (int)value)
      {
         lnaState = (int)value;
         doUpdate = true;
      }
   }
#endif
   if ((doUpdate == true) && (streamActive))
   {
      mir_sdr_Reinit(&gRdB, 0.0, 0.0, mir_sdr_BW_Undefined, mir_sdr_IF_Undefined, mir_sdr_LO_Undefined, lnaState, &gRdBsystem, mir_sdr_USE_RSP_SET_GR, &sps, mir_sdr_CHANGE_GR);
   }
}

double SoapySDRPlay::getGain(const int direction, const size_t channel, const std::string &name) const
{
   if (name == "IFGR")
   {
      return gRdB;
   }
#ifndef RF_GAIN_IN_MENU
   else if (name == "RFGR")
   {
      return lnaState;
   }
#endif

    return 0;
}

SoapySDR::Range SoapySDRPlay::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
   if (name == "IFGR")
   {
      return SoapySDR::Range(20, 59);
   }
#ifndef RF_GAIN_IN_MENU
   else if ((name == "RFGR") && (hwVer == 1))
   {
      return SoapySDR::Range(0, 3);
   }
   else //if ((name == "RFGR") && (hwVer == 2))
   {
      return SoapySDR::Range(0, 8);
   }
#endif
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
             mir_sdr_DecimateControl(decEnable, decM, 0);
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
      if      (rate == 2048000) { *decM = 1; *decEnable = 0; return 8192000; }
   }
   else if (ifMode == mir_sdr_IF_0_450)
   {
      if      (rate == 1000000) { *decM = 1; *decEnable = 0; return 2000000; }
      else if (rate == 500000)  { *decM = 1; *decEnable = 0; return 2000000; }
      else if (rate == 250000)  { *decM = 8; *decEnable = 1; return 2000001; }
      //else if (rate == 250000)  { *decM = 1; *decEnable = 0; return 2000000; }   // not available yet
      else if (rate == 2000000) { *decM = 1; *decEnable = 0; return (rate + 1); } // this is to ensure this doesn't trigger internal down conversion
   }

   if      ((rate >= 200000)  && (rate < 500000))  { *decM = 8; *decEnable = 1; return 2000000; }
   else if ((rate >= 500000)  && (rate < 1000000)) { *decM = 4; *decEnable = 1; return 2000000; }
   else if ((rate >= 1000000) && (rate < 1536000)) { *decM = 2; *decEnable = 1; return 2000000; }
   else                                            { *decM = 1; *decEnable = 0; return rate; }
}

/*******************************************************************
* Bandwidth API
******************************************************************/

void SoapySDRPlay::setBandwidth(const int direction, const size_t channel, const double bw_in)
{
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
      if      ((rate >= 200000)  && (rate < 500000))  return mir_sdr_BW_0_200;
      else if ((rate >= 500000)  && (rate < 1000000)) return mir_sdr_BW_0_300;
      else if ((rate >= 1000000) && (rate < 1536000)) return mir_sdr_BW_0_600;
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

    if (hwVer == 2)
    {
       SoapySDR::ArgInfo AntCtrlArg;
       AntCtrlArg.key = "ant_sel";
       AntCtrlArg.value = "Antenna A";
       AntCtrlArg.name = "Antenna Select";
       AntCtrlArg.description = "Antenna Select";
       AntCtrlArg.type = SoapySDR::ArgInfo::STRING;
       AntCtrlArg.options.push_back("Antenna A");
       AntCtrlArg.options.push_back("Antenna B");
       setArgs.push_back(AntCtrlArg);

       SoapySDR::ArgInfo AmPortArg;
       AmPortArg.key = "amport_ctrl";
       AmPortArg.value = "AntA//AntB";
       AmPortArg.name = "AMport Select";
       AmPortArg.description = "AM Port Select";
       AmPortArg.type = SoapySDR::ArgInfo::STRING;
       AmPortArg.options.push_back("AntA/AntB");
       AmPortArg.options.push_back("Hi-Z");
       setArgs.push_back(AmPortArg);

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

    return setArgs;
}

void SoapySDRPlay::writeSetting(const std::string &key, const std::string &value)
{
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
      else                   lnaState = 8;
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
            mir_sdr_DecimateControl(0, 1, 0);
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
   else if (key == "ant_sel")
   {
      if (value == "Antenna A") antSel = mir_sdr_RSPII_ANTENNA_A;
      else                      antSel = mir_sdr_RSPII_ANTENNA_B;
      mir_sdr_RSPII_AntennaControl(antSel);
   }
   else if (key == "amport_ctrl")
   {
      if (value == "AntA/AntB") amPort = 0;
      else                      amPort = 1;
      mir_sdr_AmPortSelect(amPort);
   }
   else if (key == "extref_ctrl")
   {
      if (value == "false") extRef = 0;
      else                  extRef = 1;
      mir_sdr_RSPII_ExternalReferenceControl(extRef);
   }
   else if (key == "biasT_ctrl")
   {
      if (value == "false") biasTen = 0;
      else                  biasTen = 1;
      mir_sdr_RSPII_BiasTControl(biasTen);
   }
   else if (key == "rfnotch_ctrl")
   {
      if (value == "false") notechEn = 0;
      else                  notechEn = 1;
      mir_sdr_RSPII_RfNotchEnable(notechEn);
   }
}

std::string SoapySDRPlay::readSetting(const std::string &key) const
{
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
       else                    return "8";
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
    else if (key == "ant_sel")
    {
       if (antSel == mir_sdr_RSPII_ANTENNA_A) return "Antenna A";
       else                                   return "Antenna B";
    }
    else if (key == "amport_ctrl")
    {
       if (amPort == 0) return "AntA/AntB";
       else             return "Hi-Z";
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
       if (notechEn == 0) return "false";
       else               return "true";
    }

    // SoapySDR_logf(SOAPY_SDR_WARNING, "Unknown setting '%s'", key.c_str());
    return "";
}
