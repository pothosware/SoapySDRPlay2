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

std::vector<std::string> SoapySDRPlay::getStreamFormats(const int direction, const size_t channel) const 
{
    std::vector<std::string> formats;

    formats.push_back("CS16");
    formats.push_back("CF32");

    return formats;
}

std::string SoapySDRPlay::getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const 
{
     fullScale = 32767;
     return "CS16";
}

SoapySDR::ArgInfoList SoapySDRPlay::getStreamArgsInfo(const int direction, const size_t channel) const 
{
    SoapySDR::ArgInfoList streamArgs;

    return streamArgs;
}

/*******************************************************************
 * Async thread work
 ******************************************************************/

static void _rx_callback_A(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
                           unsigned int numSamples, unsigned int reset, void *cbContext)
{
    SoapySDRPlay *self = (SoapySDRPlay *)cbContext;
    return self->rx_callback(xi, xq, numSamples, sdrplay_api_Tuner_A);
}

static void _rx_callback_B(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
                           unsigned int numSamples, unsigned int reset, void *cbContext)
{
    SoapySDRPlay *self = (SoapySDRPlay *)cbContext;
    return self->rx_callback(xi, xq, numSamples, sdrplay_api_Tuner_B);
}

static void _ev_callback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner,
                         sdrplay_api_EventParamsT *params, void *cbContext)
{
    SoapySDRPlay *self = (SoapySDRPlay *)cbContext;
    return self->ev_callback(eventId, tuner, params);
}

void SoapySDRPlay::rx_callback(short *xi, short *xq, unsigned int numSamples, sdrplay_api_TunerSelectT tuner)
{
    Buffer *buf = 0;
    if (tuner == sdrplay_api_Tuner_A)      { buf = _bufA; }
    else if (tuner == sdrplay_api_Tuner_B) { buf = _bufB; }
    std::lock_guard<std::mutex> lock(buf->mutex);

    if (buf->count == numBuffers)
    {
        buf->overflowEvent = true;
        return;
    }

    int spaceReqd = numSamples * elementsPerSample * shortsPerWord;
    if ((buf->buffs[buf->tail].size() + spaceReqd) >= (bufferLength / chParams->ctrlParams.decimation.decimationFactor))
    {
       // increment the tail pointer and buffer count
       buf->tail = (buf->tail + 1) % numBuffers;
       buf->count++;

       // notify readStream()
       buf->cond.notify_one();
    }

    // get current fill buffer
    auto &buff = buf->buffs[buf->tail];
    buff.resize(buff.size() + spaceReqd);

    // copy into the buffer queue
    unsigned int i = 0;

    if (useShort)
    {
       short *dptr = buff.data();
       dptr += (buff.size() - spaceReqd);
       for (i = 0; i < numSamples; i++)
       {
           *dptr++ = xi[i];
           *dptr++ = xq[i];
        }
    }
    else
    {
       float *dptr = (float *)buff.data();
       dptr += ((buff.size() - spaceReqd) / shortsPerWord);
       for (i = 0; i < numSamples; i++)
       {
          *dptr++ = (float)xi[i] / 32768.0f;
          *dptr++ = (float)xq[i] / 32768.0f;
       }
    }

    return;
}

void SoapySDRPlay::ev_callback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT *params)
{
    if (eventId == sdrplay_api_GainChange)
    {
        //Beware, lnaGRdB is really the LNA GR, NOT the LNA state !
        //sdrplay_api_GainCbParamT gainParams = params->gainParams;
        //unsigned int gRdB = gainParams.gRdB;
        //unsigned int lnaGRdB = gainParams.lnaGRdB;
        // gainParams.currGain is a calibrated gain value
        //if (gRdB < 200)
        //{
        //    current_gRdB = gRdB;
        //}
    }
    else if (eventId == sdrplay_api_PowerOverloadChange)
    {
        sdrplay_api_PowerOverloadCbEventIdT powerOverloadChangeType = params->powerOverloadParams.powerOverloadChangeType;
        if (powerOverloadChangeType == sdrplay_api_Overload_Detected)
        {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Ctrl_OverloadMsgAck, sdrplay_api_Update_Ext1_None);
            // OVERLOAD DETECTED
        }
        else if (powerOverloadChangeType == sdrplay_api_Overload_Corrected)
        {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Ctrl_OverloadMsgAck, sdrplay_api_Update_Ext1_None);
            // OVERLOAD CORRECTED
        }
    }
}

/*******************************************************************
 * Stream API
 ******************************************************************/

SoapySDRPlay::Buffer::Buffer(size_t numBuffers, unsigned long bufferLength)
{
    std::lock_guard<std::mutex> lock(mutex);

    // clear async fifo counts
    tail = 0;
    head = 0;
    count = 0;

    // allocate buffers
    buffs.resize(numBuffers);
    for (auto &buff : buffs) buff.reserve(bufferLength);
    for (auto &buff : buffs) buff.clear();
}

SoapySDRPlay::Buffer::~Buffer()
{
}

SoapySDR::Stream *SoapySDRPlay::setupStream(const int direction,
                                             const std::string &format,
                                             const std::vector<size_t> &channels,
                                             const SoapySDR::Kwargs &args)
{
    if (device.hwVer == SDRPLAY_RSPduo_ID && device.rspDuoMode == sdrplay_api_RspDuoMode_Dual_Tuner)
    {
        nchannels = 2;
    } else {
        nchannels = 1;
    }

    // check the channel configuration
    int channels_size = (int) channels.size();
    if (channels_size != 0 and channels_size != nchannels)
    {
        throw std::runtime_error("setupStream invalid channel selection");
    }
    int i;
    for (i = 0; i < channels_size; ++i)
    {
        if ((int) channels.at(i) != i)
        {
            throw std::runtime_error("setupStream invalid channel selection");
        }
    }

    // check the format
    if (format == "CS16") 
    {
        useShort = true;
        shortsPerWord = 1;
        bufferLength = bufferElems * elementsPerSample * shortsPerWord;
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CS16.");
    } 
    else if (format == "CF32") 
    {
        useShort = false;
        shortsPerWord = sizeof(float) / sizeof(short);
        bufferLength = bufferElems * elementsPerSample * shortsPerWord;  // allocate enough space for floats instead of shorts
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CF32.");
    } 
    else 
    {
       throw std::runtime_error( "setupStream invalid format '" + format +
                                  "' -- Only CS16 or CF32 are supported by the SoapySDRPlay module.");
    }

    if (nchannels >= 1) _bufA = new Buffer(numBuffers, bufferLength);
    if (nchannels >= 2) _bufB = new Buffer(numBuffers, bufferLength);

    return (SoapySDR::Stream *) this;
}

void SoapySDRPlay::closeStream(SoapySDR::Stream *stream)
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    if (streamActive)
    {
        sdrplay_api_Uninit(device.dev);
    }
    streamActive = false;
}

size_t SoapySDRPlay::getStreamMTU(SoapySDR::Stream *stream) const
{
    // is a constant in practice
    return bufferElems;
}

int SoapySDRPlay::activateStream(SoapySDR::Stream *stream,
                                 const int flags,
                                 const long long timeNs,
                                 const size_t numElems)
{
    if (flags != 0) 
    {
        return SOAPY_SDR_NOT_SUPPORTED;
    }
   
    if (_bufA)
    {
        _bufA->reset = true;
        _bufA->nElems = 0;
    }
    if (_bufB)
    {
        _bufB->reset = true;
        _bufB->nElems = 0;
    }
    
    sdrplay_api_ErrT err;
    
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    // Enable (= sdrplay_api_DbgLvl_Verbose) API calls tracing,
    // but only for debug purposes due to its performance impact.
    sdrplay_api_DebugEnable(device.dev, sdrplay_api_DbgLvl_Disable);

    // temporary fix for ARM targets.
#if defined(__arm__) || defined(__aarch64__)
    sdrplay_api_SetTransferMode(sdrplay_api_BULK);
#endif

    chParams->tunerParams.dcOffsetTuner.dcCal = 4;
    chParams->tunerParams.dcOffsetTuner.speedUp = 0;
    chParams->tunerParams.dcOffsetTuner.trackTime = 63;

    sdrplay_api_CallbackFnsT cbFns;
    cbFns.StreamACbFn = _rx_callback_A;
    cbFns.StreamBCbFn = _rx_callback_B;
    cbFns.EventCbFn = _ev_callback;

    err = sdrplay_api_Init(device.dev, &cbFns, (void *)this);
    if (err != sdrplay_api_Success)
    {
       //throw std::runtime_error("Init Error: " + std::to_string(err));
       return SOAPY_SDR_NOT_SUPPORTED;
    }

    streamActive = true;
    
    return 0;
}

int SoapySDRPlay::deactivateStream(SoapySDR::Stream *stream, const int flags, const long long timeNs)
{
    if (flags != 0)
    {
        return SOAPY_SDR_NOT_SUPPORTED;
    }

    std::lock_guard <std::mutex> lock(_general_state_mutex);

    if (streamActive)
    {
        sdrplay_api_Uninit(device.dev);
    }

    streamActive = false;
    
    return 0;
}

int SoapySDRPlay::readStream(SoapySDR::Stream *stream,
                             void * const *buffs,
                             const size_t numElems,
                             int &flags,
                             long long &timeNs,
                             const long timeoutUs)
{
    if (!streamActive) 
    {
        return 0;
    }

    int retA = readChannel(stream, buffs[0], numElems, flags, timeNs, timeoutUs, _bufA);
    if (nchannels <= 1) {
       return retA;
    }
    int retB = readChannel(stream, buffs[1], numElems, flags, timeNs, timeoutUs, _bufB);
    if (retA < 0)
    {
        return retA;
    }
    else if (retB < 0)
    {
        return retB;
    }
    if (retA != retB)
    {
        SoapySDR_logf(SOAPY_SDR_WARNING, "Channel A returned %d elements; channel B returned %d elements", retA, retB);
    }
    return std::min(retA, retB);
}

int SoapySDRPlay::readChannel(SoapySDR::Stream *stream,
                               void *buff,
                               const size_t numElems,
                               int &flags,
                               long long &timeNs,
                               const long timeoutUs,
                               Buffer *daBuf)
{
    // are elements left in the buffer? if not, do a new read.
    if (daBuf->nElems == 0)
    {
        int ret = this->acquireReadBuffer(stream, daBuf->currentHandle, (const void **)&daBuf->currentBuff, flags, timeNs, timeoutUs, daBuf);
  
        if (ret < 0)
        {
            return ret;
        }
        daBuf->nElems = ret;
    }

    size_t returnedElems = std::min(daBuf->nElems.load(), numElems);

    // copy into user's buff
    if (useShort)
    {
        std::memcpy(buff, daBuf->currentBuff, returnedElems * 2 * sizeof(short));
    }
    else
    {
        std::memcpy(buff, (float *)daBuf->currentBuff, returnedElems * 2 * sizeof(float));
    }

    // bump variables for next call into readStream
    daBuf->nElems -= returnedElems;

    // scope lock here to update daBuf->currentBuff position
    {
        std::lock_guard <std::mutex> lock(daBuf->mutex);
        daBuf->currentBuff += returnedElems * elementsPerSample * shortsPerWord;
    }

    // return number of elements written to buff
    if (daBuf->nElems != 0)
    {
        flags |= SOAPY_SDR_MORE_FRAGMENTS;
    }
    else
    {
        this->releaseReadBuffer(stream, daBuf->currentHandle);
    }
    return (int)returnedElems;
}

/*******************************************************************
 * Direct buffer access API
 ******************************************************************/

size_t SoapySDRPlay::getNumDirectAccessBuffers(SoapySDR::Stream *stream)
{
    std::lock_guard <std::mutex> lockA(_bufA->mutex);
    size_t retA = _bufA->buffs.size();
    if (!_bufB) { return retA; }
    std::lock_guard <std::mutex> lockB(_bufB->mutex);
    size_t retB = _bufB->buffs.size();
    return std::min(retA, retB);
}

int SoapySDRPlay::getDirectAccessBufferAddrs(SoapySDR::Stream *stream, const size_t handle, void **buffs)
{
    std::lock_guard <std::mutex> lockA(_bufA->mutex);
    buffs[0] = (void *)_bufA->buffs[handle].data();
    if (nchannels > 1)
    {
        std::lock_guard <std::mutex> lockB(_bufB->mutex);
        buffs[0] = (void *)_bufB->buffs[handle].data();
    }
    return 0;
}

int SoapySDRPlay::acquireReadBuffer(SoapySDR::Stream *stream,
                                     size_t &handle,
                                     const void **buffs,
                                     int &flags,
                                     long long &timeNs,
                                     const long timeoutUs,
                                     Buffer *daBuf)
{
    std::unique_lock <std::mutex> lock(daBuf->mutex);

    // reset is issued by various settings
    // overflow set in the rx callback thread
    if (daBuf->reset || daBuf->overflowEvent)
    {
        // drain all buffers from the fifo
        daBuf->tail = 0;
        daBuf->head = 0;
        daBuf->count = 0;
        for (auto &buff : daBuf->buffs) buff.clear();
        daBuf->overflowEvent = false;
        if (daBuf->reset)
        {
           daBuf->reset = false;
        }
        else
        {
           SoapySDR_log(SOAPY_SDR_SSI, "O");
           return SOAPY_SDR_OVERFLOW;
        }
    }

    // wait for a buffer to become available
    if (daBuf->count == 0)
    {
        daBuf->cond.wait_for(lock, std::chrono::microseconds(timeoutUs));
        if (daBuf->count == 0) 
        {
           return SOAPY_SDR_TIMEOUT;
        }
    }

    // extract handle and buffer
    handle = daBuf->head;
    buffs[0] = (void *)daBuf->buffs[handle].data();
    flags = 0;

    daBuf->head = (daBuf->head + 1) % numBuffers;

    // return number available
    return (int)(daBuf->buffs[handle].size() / (elementsPerSample * shortsPerWord));
}

void SoapySDRPlay::releaseReadBuffer(SoapySDR::Stream *stream, const size_t handle)
{
    std::lock_guard <std::mutex> lockA(_bufA->mutex);
    _bufA->buffs[handle].clear();
    _bufA->count--;
    if (nchannels > 1)
    {
        std::lock_guard <std::mutex> lockA(_bufB->mutex);
        _bufB->buffs[handle].clear();
        _bufB->count--;
    }
}
