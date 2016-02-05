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


SoapySDRPlay::SoapySDRPlay(const SoapySDR::Kwargs &args)
{
    //TODO use args to instantiate correct device handle
    //like by checking the serial or enumeration number
    mir_sdr_ErrT err;

    err = mir_sdr_ApiVersion(&ver);
    if (ver != MIR_SDR_API_VERSION)
    {
        SoapySDR_logf(SOAPY_SDR_WARNING, "mir_sdr version: '%.3f' does not equal build version: '%.3f'", ver, MIR_SDR_API_VERSION);
    }

    dcOffsetMode = false;

    centerFreq = 100000000;
    rate = 2048000;
    bw = getBwValueFromEnum(getBwEnumForRate(rate));
    centerFreqChanged = false;
    rateChanged = false;
    syncUpdate = 0;
    numPackets = DEFAULT_NUM_PACKETS;
    bwChanged = false;
    tryLowIF=true;
    tryLowIFChanged=false;
    ifMode = mir_sdr_IF_Zero;
    streamActive = false;
}

SoapySDRPlay::~SoapySDRPlay(void)
{
    //cleanup device handles
}

/*******************************************************************
 * Identification API
 ******************************************************************/

std::string SoapySDRPlay::getDriverKey(void) const
{
    return "SDRPlay";
}

std::string SoapySDRPlay::getHardwareKey(void) const
{
    return "SDRPlay";
}

SoapySDR::Kwargs SoapySDRPlay::getHardwareInfo(void) const
{
    //key/value pairs for any useful information
    //this also gets printed in --probe
    SoapySDR::Kwargs hwArgs;

    mir_sdr_ErrT err;



    hwArgs["mir_sdr_version"] = std::to_string(ver);

    return hwArgs;
}

/*******************************************************************
 * Channels API
 ******************************************************************/

size_t SoapySDRPlay::getNumChannels(const int dir) const
{
    return (dir == SOAPY_SDR_RX)?1:0;
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

//void SoapySDRPlay::setAntenna(const int direction, const size_t channel, const std::string &name)
//{
    //TODO delete this function or throw if name != RX...
//}

std::string SoapySDRPlay::getAntenna(const int direction, const size_t channel) const
{
    return "RX";
}

/*******************************************************************
 * Frontend corrections API
 ******************************************************************/

//TODO fill these in or delete any that do not apply...

bool SoapySDRPlay::hasDCOffsetMode(const int direction, const size_t channel) const
{
    //is automatic DC offset removal supported in the hardware
    return true;
}

void SoapySDRPlay::setDCOffsetMode(const int direction, const size_t channel, const bool automatic)
{
    //enable/disable automatic DC removal
    dcOffsetMode = automatic;
//    if (dcOffsetMode)
//    {
//        mir_sdr_SetDcMode(4,1);
//        mir_sdr_SetDcTrackTime(1);
//    }
//    else
//    {
//        mir_sdr_SetDcMode(0,0);
//    }
}

bool SoapySDRPlay::getDCOffsetMode(const int direction, const size_t channel) const
{
    return dcOffsetMode;
}

bool SoapySDRPlay::hasDCOffset(const int direction, const size_t channel) const
{
    //is a specific DC removal value configurable?
    return true;
}

void SoapySDRPlay::setDCOffset(const int direction, const size_t channel, const std::complex<double> &offset)
{
    //set a specific DC removal value
    if (direction == SOAPY_SDR_RX) {

    }
}

//std::complex<double> SoapySDRPlay::getDCOffset(const int direction, const size_t channel) const
//{
//
//}

/*******************************************************************
 * Gain API
 ******************************************************************/

std::vector<std::string> SoapySDRPlay::listGains(const int direction, const size_t channel) const
{
    std::vector<std::string> gains;

    gains.push_back("TUNER");
    gains.push_back("LNAT");

    return gains;
}

void SoapySDRPlay::setGainMode(const int direction, const size_t channel, const bool automatic)
{
    agcEnabled = automatic;
    if (!automatic) {
        newLnaGr = activeGainPref->grLNA;
    }
}

bool SoapySDRPlay::getGainMode(const int direction, const size_t channel) const
{
   return agcEnabled;
}

void SoapySDRPlay::setGain(const int direction, const size_t channel, const double value)
{
    setGain(direction,channel,"TUNER",value);
}

void SoapySDRPlay::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
    if (name == "TUNER") {
        newGr = 102-value;
    } else if (name == "LNAT") {
        newLnaGr = 102-value;
    } else {
        throw std::runtime_error("getGainRange -- unknown gain name '"+ name + "'");
    }
}

double SoapySDRPlay::getGain(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "TUNER") {
        return 102-newGr;
    } else if (name == "LNAT") {
        return 102-newLnaGr;
    } else {
        throw std::runtime_error("getGainRange -- unknown gain name '"+ name + "'");
    }
}

SoapySDR::Range SoapySDRPlay::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "TUNER") {
        return SoapySDR::Range(0,102);
    } else if (name == "LNAT") {
        return SoapySDR::Range(0,102);
    } else {
        throw std::runtime_error("getGainRange -- unknown gain name '"+ name + "'");
    }
}

/*******************************************************************
 * Frequency API
 ******************************************************************/

void SoapySDRPlay::setFrequency(const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args)
{
    if (name == "RF")
    {
        newCenterFreq = frequency;
        centerFreqChanged = true;
    }
}

double SoapySDRPlay::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "RF")
    {
        if (centerFreqChanged) {
            return newCenterFreq;
        }
        return centerFreq;
    }

    return 0;
}

std::vector<std::string> SoapySDRPlay::listFrequencies(const int direction, const size_t channel) const
{
    std::vector<std::string> names;
    names.push_back("RF");
    return names;
}

SoapySDR::RangeList SoapySDRPlay::getFrequencyRange(const int direction, const size_t channel, const std::string &name) const
{
    SoapySDR::RangeList rl;
    if (name == "RF")
    {
        rl.push_back(SoapySDR::Range(100000.0,2000000000.0));
    }
    return rl;
}

SoapySDR::ArgInfoList SoapySDRPlay::getFrequencyArgsInfo(const int direction, const size_t channel) const {
    SoapySDR::ArgInfoList freqArgs;

    // TODO: frequency arguments

    return freqArgs;
}

/*******************************************************************
 * Sample Rate API
 ******************************************************************/

void SoapySDRPlay::setSampleRate(const int direction, const size_t channel, const double rate_in)
{
    // TODO implement clamping to allowed sample rates 1MHz and below (200k, 300k, 400k, 500k, 600k, 750k, 800k, 1M)
    // BUT also allow X ppm deviation! (see mir_sdr_SetFs doc)
    // X has to be determined by solving a nice catch-22 that i just can not describe now :)
    // newRate can deviate from allowed valies as described.
    newRate = rate_in;
    rateChanged = true;
}

double SoapySDRPlay::getSampleRate(const int direction, const size_t channel) const
{
    if (direction == SOAPY_SDR_RX) {
        if (rateChanged) {
            return newRate;
        }

        return rate;
    }

    return 0;
}

std::vector<double> SoapySDRPlay::listSampleRates(const int direction, const size_t channel) const
{
    std::vector<double> rates;

    rates.push_back(222222.222);
    rates.push_back(333333.333);
    rates.push_back(428571.428);
    rates.push_back(500000);
    rates.push_back(571428.571);
    rates.push_back(750000);
    rates.push_back(875000);
    rates.push_back(1000000);
    rates.push_back(1536000);
//    rates.push_back(2000000);
    rates.push_back(2048000);
    rates.push_back(5000000);
    // rates.push_back(5500000);
    rates.push_back(6000000);
    // rates.push_back(6500000);
    rates.push_back(7000000);
    // rates.push_back(7500000);
    rates.push_back(8000000);
    // rates.push_back(8500000);
    rates.push_back(9000000);
    // rates.push_back(9500000);
    rates.push_back(10000000);
    // rates.push_back(10500000);
    rates.push_back(11000000);
    // rates.push_back(11500000);
    rates.push_back(12000000);
    
    return rates;
}

void SoapySDRPlay::setBandwidth(const int direction, const size_t channel, const double bw_in)
{
    if (direction == SOAPY_SDR_RX) {
        newBw = bw_in;
        bwChanged = true;
    }
}

double SoapySDRPlay::getBandwidth(const int direction, const size_t channel) const
{
    if (direction == SOAPY_SDR_RX) {
        if (bwChanged) {
            return newBw;
        }
        return bw;
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


mir_sdr_Bw_MHzT SoapySDRPlay::getBwEnumForRate(double rate)
{
    if (rate >= (200000) && (rate < 300000)) return mir_sdr_BW_0_200;
    else if ((rate >= 300000) && rate < (600000)) return mir_sdr_BW_0_300;
    else if ((rate >= 600000) && rate < (1536000)) return mir_sdr_BW_0_600;
    else if ((rate >= 1536000) && rate < (5000000)) return mir_sdr_BW_1_536;
    else if ((rate >= 5000000) && rate < (6000000)) return mir_sdr_BW_5_000;
    else if ((rate >= 6000000) && rate < (7000000)) return mir_sdr_BW_6_000;
    else if ((rate >= 7000000) && rate < (8000000)) return mir_sdr_BW_7_000;
    else return mir_sdr_BW_8_000;
}


double SoapySDRPlay::getBwValueFromEnum(mir_sdr_Bw_MHzT bwEnum)
{
    if (bwEnum == mir_sdr_BW_0_200) return 200000;
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
    if (bw == 200000) return mir_sdr_BW_0_200;
    else if (bw == 300000) return mir_sdr_BW_0_300;
    else if (bw == 600000) return mir_sdr_BW_0_600;
    else if (bw == 1536000) return mir_sdr_BW_1_536;
    else if (bw == 5000000) return mir_sdr_BW_5_000;
    else if (bw == 6000000) return mir_sdr_BW_6_000;
    else if (bw == 7000000) return mir_sdr_BW_7_000;
    else if (bw == 8000000) return mir_sdr_BW_8_000;

    return getBwEnumForRate(bw);
}

mir_sdr_If_kHzT SoapySDRPlay::stringToIF(std::string ifMode) {
    if (ifMode == "Zero-IF") {
        return mir_sdr_IF_Zero;
    } else if (ifMode == "450kHz") {
        return mir_sdr_IF_0_450;
    } else if (ifMode == "1620kHz") {
        return mir_sdr_IF_1_620;
    } else if (ifMode == "2048kHz") {
        return mir_sdr_IF_2_048;
    }
    return mir_sdr_IF_Zero;
}

std::string SoapySDRPlay::IFtoString(mir_sdr_If_kHzT ifkHzT) {
    switch (ifkHzT) {
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
    }
    return "";
}


/*******************************************************************
 * Settings API
 ******************************************************************/

SoapySDR::ArgInfoList SoapySDRPlay::getSettingInfo(void) const {
    SoapySDR::ArgInfoList setArgs;

    // TODO: Settings
    SoapySDR::ArgInfo IFArg;
    IFArg.key="use_low_if";
    IFArg.value="true";
    IFArg.name = "Low IF (when available)";
    IFArg.description = "Use low IF when available: 0.5MHz SR with 200 and 300khz BW, 1MHz SR with 600kHz BW, 2048kHz SR with 1536kHz BW";
    IFArg.type=SoapySDR::ArgInfo::BOOL;
    setArgs.push_back(IFArg);

    SoapySDR::ArgInfo AIFArg;
    AIFArg.key="if_mode";
    AIFArg.value=IFtoString(ifMode);
    AIFArg.name = "IF Mode";
    AIFArg.description = "IF frequency in kHz";
    AIFArg.type=SoapySDR::ArgInfo::STRING;
    AIFArg.options.push_back(IFtoString(mir_sdr_IF_Zero));
    AIFArg.options.push_back(IFtoString(mir_sdr_IF_0_450));
    AIFArg.options.push_back(IFtoString(mir_sdr_IF_1_620));
    AIFArg.options.push_back(IFtoString(mir_sdr_IF_2_048));
    setArgs.push_back(AIFArg);

    return setArgs;
}

void SoapySDRPlay::writeSetting(const std::string &key, const std::string &value) {
    if (key=="use_low_if") {
        newTryLowIF = (value == "true") ? true : false;
        tryLowIFChanged=true;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "SDRPlay setting tryLowIF: %s", newTryLowIF ? "true" : "false");
    }
    if (key=="if_mode") { // not sure if this is correct..
        ifMode = stringToIF(value);
        newTryLowIF = true;
        tryLowIFChanged = true;
        SoapySDR_logf(SOAPY_SDR_DEBUG, "SDRPlay setting IFMode: %s", IFtoString(ifMode).c_str());
    }
}

std::string SoapySDRPlay::readSetting(const std::string &key) const {
    if (key=="use_low_if" && tryLowIFChanged) return newTryLowIF?"true":"false";
    if (key=="use_low_if" && !tryLowIFChanged) return tryLowIF?"true":"false";
    if (key=="if_mode") return IFtoString(ifMode);
    return "";
}

