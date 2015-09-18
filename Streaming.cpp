/*
 * TODO license, copyright, etc
 * 
 */

#include "SoapySDRPlay.hpp"


SoapySDR::Stream *SoapySDRPlay::setupStream(
    const int direction,
    const std::string &format,
    const std::vector<size_t> &channels,
    const SoapySDR::Kwargs &args)
{
    //check that direction is SOAPY_SDR_RX
    if (direction != SOAPY_SDR_RX) {
        return NULL;
    }

    //check the channel configuration
        //check than channels is either empty or [0]
    if (channels.size() > 1 or (channels.size() > 0 and channels.at(0) != 0))
    {
        throw std::runtime_error("setupStream invalid channel selection");
    }

    //check the format
    if (format == "CF32")
    {
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CF32.");
        rxFloat = true;
    }
    else if (format == "CS16")
    {
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CS16.");
        rxFloat = false;
    }
    else
    {
        throw std::runtime_error("setupStream invalid format '" + format + "' -- Only CS16 and CF32 are supported by SoapySDRPlay, and CS16 is the native format.");
    }

    //use args to specify optional things like:
    //integer to float scale factors
    //number of transfers and or transfer size
    //depends what the hardware supports...

    //TODO: add optional SDRPlay stream flags here

    //this device probably only supports one stream
    //so the stream pointer probably doesn't matter
    //return (SoapySDR::Stream *)42;
    return (SoapySDR::Stream *)this;
}

void SoapySDRPlay::closeStream(SoapySDR::Stream *stream)
{
    xi.erase(xi.begin(), xi.end());
    xq.erase(xi.begin(), xi.end());
}

size_t SoapySDRPlay::getStreamMTU(SoapySDR::Stream *stream) const
{
    //how large is a transfer?
    //this value helps users to allocate buffers that will match the hardware transfer size
    return xi.size();
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

    // Configure DC tracking in tuner
    mir_sdr_ErrT err;
    err = mir_sdr_SetDcMode(4,0);
    err = mir_sdr_SetDcTrackTime(63);
    err = mir_sdr_Init(newGr, rate/1000000.0, centerFreq/1000000.0, mirGetBwMhzEnum(bw), mir_sdr_IF_Zero, &sps);

    SoapySDR_logf(SOAPY_SDR_DEBUG, "stream sps: %d", sps);

    // Alocate data buffers
    xi.resize(sps);
    xq.resize(sps);
    syncUpdate = 0;

    return 0;
}

int SoapySDRPlay::deactivateStream(
    SoapySDR::Stream *stream,
    const int flags,
    const long long timeNs)
{
    //same idea as activateStream,
    //but disable streaming in the hardware
    mir_sdr_ErrT err;
    err = mir_sdr_Uninit();

    return 0;
}

int SoapySDRPlay::readStream(
    SoapySDR::Stream *stream,
    void * const *buffs,
    const size_t numElems,
    int &flags,
    long long &timeNs,
    const long timeoutUs)
{
    mir_sdr_ErrT err;

    //this is the user's buffer for channel 0
    void *buff0 = buffs[0];

    //step 1 check for data with timeout
    // TODO: Can we do?

    // Prevent stalling if we've already buffered enough data..
    if (xi_buffer.size() < numElems) {
        //step 2 receive into temporary buffer
        err = mir_sdr_ReadPacket(&xi[0], &xq[0], &fs, &grc, &rfc, &fsc);

        //return SOAPY_SDR_TIMEOUT when timeout occurs
        if (err != 0) {
            return SOAPY_SDR_TIMEOUT;
        }

        //was numElems < than the hardware transfer size?
        //may have to keep part of that temporary buffer
        //around for the next call into readStream...
        xi_buffer.insert(xi_buffer.end(),xi.begin(),xi.end());
        xq_buffer.insert(xq_buffer.end(),xq.begin(),xq.end());
    }

    int returnedElems = (numElems>xi_buffer.size())?xi_buffer.size():numElems;

    //step 3 convert into user's buff0
    if (rxFloat) {
        float *ftarget = (float *)buff0;
        for (int i = 0; i < returnedElems; i++) {
            ftarget[i*2] = ((float)xi_buffer[i]/(float)SHRT_MAX);
            ftarget[i*2+1] = ((float)xq_buffer[i]/(float)SHRT_MAX);
        }
    } else {
        short *starget = (short *)buff0;
        for (int i = 0; i < returnedElems; i++) {
            starget[i*2] = xi_buffer[i];
            starget[i*2+1] = xq_buffer[i];
        }
    }

    xi_buffer.erase(xi_buffer.begin(),xi_buffer.begin()+returnedElems);
    xq_buffer.erase(xq_buffer.begin(),xq_buffer.begin()+returnedElems);

    //return number of elements written to buff0
    return returnedElems;
}


