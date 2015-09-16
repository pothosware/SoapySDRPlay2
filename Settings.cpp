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
        std::string errStr("mir_sdr version: '" + std::to_string(ver) + "' does not equal build version: '" + std::to_string(MIR_SDR_API_VERSION) + "'");
        SoapySDR_log(SOAPY_SDR_WARNING, errStr.c_str());
    }

    dcOffsetMode = false;

    // Initialise API and hardware for DAB demodulation: initial gain reduction of 40dB, sample
    // rate of 2.048MHz, centre frequency of 222.064MHz, double sided bandwidth of 1.536MHz and
    // a zero-IF
    // used for DAB type signals
    newGr = 40;
    oldGr = 40;
    grChangedAfter = 0;
    centerFreq = 222.064;
    rate = 2.048;
    bw = 15360000;
    syncUpdate = 0;
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
        SoapySDR_log(SOAPY_SDR_DEBUG,"Changed center frequency");
        centerFreq = frequency;
        mir_sdr_SetRf(centerFreq, 1, 0);
    }
}

double SoapySDRPlay::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "RF")
    {
        return centerFreq;
    }
}

std::vector<std::string> SoapySDRPlay::listFrequencies(const int direction, const size_t channel) const
{
    std::vector<std::string> names;
    names.push_back("RF");
    return names;
}

SoapySDR::RangeList SoapySDRPlay::getFrequencyRange(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "RF")
    {
        SoapySDR::RangeList rl;
        rl.push_back(SoapySDR::Range(100000.0,2000000000.0));
        return rl;
    }
}

/*******************************************************************
 * Sample Rate API
 ******************************************************************/

void SoapySDRPlay::setSampleRate(const int direction, const size_t channel, const double rate)
{
    SoapySDR_log(SOAPY_SDR_DEBUG,"Set sample rate");
    this->rate = rate;
    mir_sdr_SetFs(rate, 1, 0, 0);
}

double SoapySDRPlay::getSampleRate(const int direction, const size_t channel) const
{
    return this->rate;
}

std::vector<double> SoapySDRPlay::listSampleRates(const int direction, const size_t channel) const
{
    std::vector<double> rates;

    rates.push_back(200000);
    rates.push_back(8000000);
    
    return rates;
}

void SoapySDRPlay::setBandwidth(const int direction, const size_t channel, const double bw)
{
    SoapySDR_log(SOAPY_SDR_DEBUG,"Set bandwidth");
    this->bw = bw;
}

double SoapySDRPlay::getBandwidth(const int direction, const size_t channel) const
{
    return bw;
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



mir_sdr_Bw_MHzT SoapySDRPlay::mirGetBwMhzEnum(double bw) {
    if (bw == 200000) return mir_sdr_BW_0_200;
    if (bw == 300000) return mir_sdr_BW_0_300;
    if (bw == 600000) return mir_sdr_BW_0_600;
    if (bw == 1536000) return mir_sdr_BW_1_536;
    if (bw == 5000000) return mir_sdr_BW_1_536;
    if (bw == 6000000) return mir_sdr_BW_6_000;
    if (bw == 7000000) return mir_sdr_BW_7_000;
    if (bw == 8000000) return mir_sdr_BW_8_000;

    return mir_sdr_BW_1_536;
}
