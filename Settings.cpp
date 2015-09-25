/*
 * TODO license, copyright, etc
 * 
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

    gr = 40;
    newGr = 40;
    oldGr = 40;
    adcLow = int(exp(double(gr-2)*log(10.0)));
    adcTarget = int(exp(double(gr)*log(10.0)));
    adcHigh = int(exp(double(gr+2)*log(10.0)));
    grChangedAfter = 0;
    centerFreq = 100000000;
    rate = 2048000;
    bw = getBwValueFromEnum(getBwEnumForRate(rate));
    centerFreqChanged = false;
    rateChanged = false;
    syncUpdate = 0;
    numPackets = DEFAULT_NUM_PACKETS;
    bwChanged = false;

//    SoapySDR_logf(SOAPY_SDR_DEBUG, "Bandwidth for 200000: %f", getBwValueFromEnum(getBwEnumForRate(200000)));
//    SoapySDR_logf(SOAPY_SDR_DEBUG, "Bandwidth for 500000: %f", getBwValueFromEnum(getBwEnumForRate(500000)));
//    SoapySDR_logf(SOAPY_SDR_DEBUG, "Bandwidth for 1024000: %f", getBwValueFromEnum(getBwEnumForRate(1024000)));
//    SoapySDR_logf(SOAPY_SDR_DEBUG, "Bandwidth for 2048000: %f", getBwValueFromEnum(getBwEnumForRate(2048000)));
//    SoapySDR_logf(SOAPY_SDR_DEBUG, "Bandwidth for 4096000: %f", getBwValueFromEnum(getBwEnumForRate(4096000)));
//    SoapySDR_logf(SOAPY_SDR_DEBUG, "Bandwidth for 8192000: %f", getBwValueFromEnum(getBwEnumForRate(8192000)));
//    SoapySDR_logf(SOAPY_SDR_DEBUG, "Bandwidth for 10000000: %f", getBwValueFromEnum(getBwEnumForRate(10000000)));
//    SoapySDR_logf(SOAPY_SDR_DEBUG, "Bandwidth for 12000000: %f", getBwValueFromEnum(getBwEnumForRate(12000000)));
//    SoapySDR_logf(SOAPY_SDR_DEBUG, "Bandwidth for 13000000: %f", getBwValueFromEnum(getBwEnumForRate(13000000)));
//    SoapySDR_logf(SOAPY_SDR_DEBUG, "Bandwidth for 14000000: %f", getBwValueFromEnum(getBwEnumForRate(14000000)));
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
    if (dcOffsetMode)
    {
        mir_sdr_SetDcMode(4,0);
    }
    else
    {
        mir_sdr_SetDcMode(0,0);
    }
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

void SoapySDRPlay::setDCOffset(const int direction, const size_t channel, const std::complex<double> &offset)
{
    //set a specific DC removal value
}

//std::complex<double> SoapySDRPlay::getDCOffset(const int direction, const size_t channel) const
//{
//
//}

/*******************************************************************
 * Gain API
 ******************************************************************/

//std::vector<std::string> SoapySDRPlay::listGains(const int direction, const size_t channel) const
//{
//    //list available gain elements,
//    //the functions below have a "name" parameter
//}
//
//void SoapySDRPlay::setGainMode(const int direction, const size_t channel, const bool automatic)
//{
//    //enable AGC if the hardware supports it, or remove this function
//}
//
//bool SoapySDRPlay::getGainMode(const int direction, const size_t channel) const
//{
//    //ditto for the AGC
//}
//
//void SoapySDRPlay::setGain(const int direction, const size_t channel, const double value)
//{
//    //set the overall gain by distributing it across available gain elements
//    //OR delete this function to use SoapySDR's default gain distribution algorithm...
//}
//
//void SoapySDRPlay::setGain(const int direction, const size_t channel, const std::string &name, const double value)
//{
//    //set individual gain element by name
//}
//
//double SoapySDRPlay::getGain(const int direction, const size_t channel, const std::string &name) const
//{
//
//}
//
//SoapySDR::Range SoapySDRPlay::getGainRange(const int direction, const size_t channel, const std::string &name) const
//{
//
//}

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

/*******************************************************************
 * Sample Rate API
 ******************************************************************/

void SoapySDRPlay::setSampleRate(const int direction, const size_t channel, const double rate_in)
{
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

    rates.push_back(2000000);
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
    if (rate <= (200000*2)) return mir_sdr_BW_0_200;
    else if ((rate >= 200000*2) && rate <= (300000*2)) return mir_sdr_BW_0_300;
    else if ((rate >= 300000*2) && rate <= (600000*2)) return mir_sdr_BW_0_600;
    else if ((rate >= 600000*2) && rate <= (1536000*2)) return mir_sdr_BW_1_536;
    else if ((rate >= 1536000*2) && rate <= (5000000*2)) return mir_sdr_BW_5_000;
    else if ((rate >= 5000000*2) && rate <= (6000000*2)) return mir_sdr_BW_6_000;
    else if ((rate >= 6000000*2) && rate <= (7000000*2)) return mir_sdr_BW_7_000;
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
    else if (bw == 5000000) return mir_sdr_BW_1_536;
    else if (bw == 6000000) return mir_sdr_BW_6_000;
    else if (bw == 7000000) return mir_sdr_BW_7_000;
    else if (bw == 8000000) return mir_sdr_BW_8_000;

    return getBwEnumForRate(bw*2.0);
}
