/*
 * TODO license, copyright, etc
 * 
 */

#include "SoapySDRPlay.hpp"
//#include <mirsdrapi-rsp.h>

SoapySDRPlay::SoapySDRPlay(const SoapySDR::Kwargs &args)
{
    //TODO use args to instantiate correct device handle
    //like by checking the serial or enumeration number
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

void SoapySDRPlay::setAntenna(const int direction, const size_t channel, const std::string &name)
{
    //TODO delete this function or throw if name != RX...
}

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
}

void SoapySDRPlay::setDCOffsetMode(const int direction, const size_t channel, const bool automatic)
{
    //enable/disable automatic DC removal
}

bool SoapySDRPlay::getDCOffsetMode(const int direction, const size_t channel) const
{
    
}

bool SoapySDRPlay::hasDCOffset(const int direction, const size_t channel) const
{
    //is a specific DC removal value configurable?
}

void SoapySDRPlay::setDCOffset(const int direction, const size_t channel, const std::complex<double> &offset)
{
    //set a specific DC removal value
}

std::complex<double> SoapySDRPlay::getDCOffset(const int direction, const size_t channel) const
{
    
}

/*******************************************************************
 * Gain API
 ******************************************************************/

std::vector<std::string> SoapySDRPlay::listGains(const int direction, const size_t channel) const
{
    //list available gain elements,
    //the functions below have a "name" parameter
}

void SoapySDRPlay::setGainMode(const int direction, const size_t channel, const bool automatic)
{
    //enable AGC if the hardware supports it, or remove this function
}

bool SoapySDRPlay::getGainMode(const int direction, const size_t channel) const
{
    //ditto for the AGC
}

void SoapySDRPlay::setGain(const int direction, const size_t channel, const double value)
{
    //set the overall gain by distributing it across available gain elements
    //OR delete this function to use SoapySDR's default gain distribution algorithm...
}

void SoapySDRPlay::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
    //set individual gain element by name
}

double SoapySDRPlay::getGain(const int direction, const size_t channel, const std::string &name) const
{
    
}

SoapySDR::Range SoapySDRPlay::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
    
}

/*******************************************************************
 * Frequency API
 ******************************************************************/

void SoapySDRPlay::setFrequency(const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args)
{
    if (name == "RF")
    {
        
    }
}

double SoapySDRPlay::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "RF")
    {
        
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
        
    }
}

/*******************************************************************
 * Sample Rate API
 ******************************************************************/

void SoapySDRPlay::setSampleRate(const int direction, const size_t channel, const double rate)
{
    
}

double SoapySDRPlay::getSampleRate(const int direction, const size_t channel) const
{
    
}

std::vector<double> SoapySDRPlay::listSampleRates(const int direction, const size_t channel) const
{
    
}

void SoapySDRPlay::setBandwidth(const int direction, const size_t channel, const double bw)
{
    
}

double SoapySDRPlay::getBandwidth(const int direction, const size_t channel) const
{
    
}

std::vector<double> SoapySDRPlay::listBandwidths(const int direction, const size_t channel) const
{
    
}
