/*
 * TODO license, copyright, etc
 * 
 */

#include "SoapySDRPlay.hpp"
//#include <mirsdrapi-rsp.h>

SoapySDR::Stream *SoapySDRPlay::setupStream(
    const int direction,
    const std::string &format,
    const std::vector<size_t> &channels,
    const SoapySDR::Kwargs &args)
{
    //check that direction is SOAPY_SDR_RX
    //check than channels is either empty or [0]

    //check that format is a supported format,
    //any format that we want to use is going
    //to be converted from the driver's native type
    //record the format somehow so we know what format to use for readStream
    //probably want to support CF32 for complex floats
    //and CS16 for complex shorts

    //use args to specify optional things like:
    //integer to float scale factors
    //number of transfers and or transfer size
    //depends what the hardware supports...

    //this device probably only supports one stream
    //so the stream pointer probably doesn't matter
    //return (SoapySDR::Stream *)42;
    //return (SoapySDR::Stream *)this;
}

void SoapySDRPlay::closeStream(SoapySDR::Stream *stream)
{
    
}

size_t SoapySDRPlay::getStreamMTU(SoapySDR::Stream *stream) const
{
    //how large is a transfer?
    //this value helps users to allocate buffers that will match the hardware transfer size
}

int SoapySDRPlay::activateStream(
    SoapySDR::Stream *stream,
    const int flags,
    const long long timeNs,
    const size_t numElems)
{
    //probably can ignore flags and time and numElems
    //these are for timed burst features
    //however it might be nice to return SOAPY_SDR_NOT_SUPPORTED
    //when flags != or or numElems != 0

    //this function should actually enable the hardware to stream
}

int SoapySDRPlay::deactivateStream(
    SoapySDR::Stream *stream,
    const int flags,
    const long long timeNs)
{
    //same idea as activateStream,
    //but disable streaming in the hardware
}

int SoapySDRPlay::readStream(
    SoapySDR::Stream *stream,
    void * const *buffs,
    const size_t numElems,
    int &flags,
    long long &timeNs,
    const long timeoutUs)
{
    //this is the user's buffer for channel 0
    void *buff0 = buffs[0];

    //step 1 check for data with timeout
    //return SOAPY_SDR_TIMEOUT when timeout occurs

    //step 2 receive into temporary buffer

    //step 3 convert into user's buff0

    //was numElems < than the hardware transfer size?
    //may have to keep part of that temporary buffer
    //around for the next call into readStream...

    //return number of elements written to buff0
}
