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
    if (direction == SOAPY_SDR_RX) {
    } else {
        return NULL;
    }

    //check than channels is either empty or [0]
    //TODO: necessary?

    //check that format is a supported format,
    //any format that we want to use is going
    //to be converted from the driver's native type
    //record the format somehow so we know what format to use for readStream
    //probably want to support CF32 for complex floats
    //and CS16 for complex shorts
    if (args.at("format") == "CF32") {
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CF32.");
        convertFloat = true;
    } else if (args.at("format") == "CS16") {
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CS16.");
        convertFloat = false;
    } else {
        SoapySDR_log(SOAPY_SDR_FATAL, "Only CS16 and CF32 are supported by SoapySDRPlay, and CS16 is the native format.");
        convertFloat = false;
        return NULL;
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
    err = mir_sdr_Init(newGr, rate, centerFreq, mirGetBwMhzEnum(bw), mir_sdr_IF_Zero, &sps);

    // Alocate data buffers
    xi.resize(sps);
    xq.resize(sps);
    syncUpdate = 0;
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
        xi_buffer.insert(xi_buffer.end(),xi.begin(),xi.begin()+sps);
        xq_buffer.insert(xq_buffer.end(),xq.begin(),xq.begin()+sps);
    }

    int returnedElems = (numElems>xi_buffer.size())?xi_buffer.size():numElems;

    //step 3 convert into user's buff0
    if (convertFloat) {
        for (int i = 0; i < returnedElems; i++) {
            ((float *)buff0)[i*2] = ((float)xi_buffer[i]/(float)SHRT_MAX);
            ((float *)buff0)[i*2+1] = ((float)xq_buffer[i]/(float)SHRT_MAX);
        }
    } else {
        for (int i = 0; i < returnedElems; i++) {
            ((short *)buff0)[i*2] = xi_buffer[i];
            ((short *)buff0)[i*2+1] = xq_buffer[i];
        }
    }
    xi_buffer.erase(xi_buffer.begin(),xi_buffer.begin()+returnedElems);
    xi_buffer.erase(xq_buffer.begin(),xq_buffer.begin()+returnedElems);

    //return number of elements written to buff0
    return returnedElems;
}


