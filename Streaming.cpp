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

static void _rx_callback(short *xi, short *xq, unsigned int firstSampleNum, int grChanged, int rfChanged, 
                         int fsChanged, unsigned int numSamples, unsigned int reset, void *cbContext)
{
    SoapySDRPlay *self = (SoapySDRPlay *)cbContext;
    return self->rx_callback(xi, xq, numSamples);
}

static void _gr_callback(unsigned int gRdB, unsigned int lnaGRdB, void *cbContext)
{
    SoapySDRPlay *self = (SoapySDRPlay *)cbContext;
    return self->gr_callback(gRdB, lnaGRdB);
}

void SoapySDRPlay::rx_callback(short *xi, short *xq, unsigned int numSamples)
{
    std::unique_lock<std::mutex> lock(_buf_mutex);

    if (_buf_count == numBuffers)
    {
        _overflowEvent = true;
        return;
    }
    
    int spaceReqd = numSamples * elementsPerSample * shortsPerWord;
    if ((_buffs[_buf_tail].size() + spaceReqd) >= (bufferLength / decM))
    {
       // increment the tail pointer and buffer count
       _buf_tail = (_buf_tail + 1) % numBuffers;
       _buf_count++;

       // notify readStream()
       _buf_cond.notify_one();
    }

    // get current fill buffer
    auto &buff = _buffs[_buf_tail];
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

void SoapySDRPlay::gr_callback(unsigned int gRdB, unsigned int lnaGRdB)
{
    return;
}

/*******************************************************************
 * Stream API
 ******************************************************************/

SoapySDR::Stream *SoapySDRPlay::setupStream(const int direction,
                                            const std::string &format,
                                            const std::vector<size_t> &channels,
                                            const SoapySDR::Kwargs &args)
{
    // check the channel configuration
    if (channels.size() > 1 or (channels.size() > 0 and channels.at(0) != 0)) 
    {
       throw std::runtime_error("setupStream invalid channel selection");
    }

    bufferLength = bufferElems * elementsPerSample;

    // check the format
    if (format == "CS16") 
    {
        useShort = true;
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CS16.");
    } 
    else if (format == "CF32") 
    {
        useShort = false;
        shortsPerWord = sizeof(float) / sizeof(short);
        bufferLength *= shortsPerWord;  // allocate enough space for floats instead of shorts
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CF32.");
    } 
    else 
    {
       throw std::runtime_error( "setupStream invalid format '" + format +
                                  "' -- Only CS16 or CF32 are supported by the SoapySDRPlay module.");
    }

    // clear async fifo counts
    _buf_tail = 0;
    _buf_head = 0;
    _buf_count = 0;

    // allocate buffers
    _buffs.resize(numBuffers);
    for (auto &buff : _buffs) buff.reserve(bufferLength);
    for (auto &buff : _buffs) buff.clear();

    return (SoapySDR::Stream *) this;
}

void SoapySDRPlay::closeStream(SoapySDR::Stream *stream)
{
    if (streamActive)
    {
        mir_sdr_StreamUninit();
    }
    streamActive = false;
    return;
}

size_t SoapySDRPlay::getStreamMTU(SoapySDR::Stream *stream) const
{
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
    
    resetBuffer = true;
    bufferedElems = 0;
    
    mir_sdr_ErrT err;
    
    mir_sdr_DebugEnable(1);

    err = mir_sdr_StreamInit(&gRdB, sampleRate / 1e6, centerFrequency / 1e6, bwMode,
                             ifMode, lnaState, &gRdBsystem, mir_sdr_USE_RSP_SET_GR, &sps,
                             _rx_callback, _gr_callback, (void *)this);
    if (err != mir_sdr_Success)
    {
       //throw std::runtime_error("StreamInit Error: " + std::to_string(err));
       return SOAPY_SDR_NOT_SUPPORTED;
    }
    mir_sdr_DecimateControl(decEnable, decM, 0);

    mir_sdr_SetDcMode(4,0);
    mir_sdr_SetDcTrackTime(63);
    
    streamActive = true;
    
    return 0;
}

int SoapySDRPlay::deactivateStream(SoapySDR::Stream *stream, const int flags, const long long timeNs)
{
    if (flags != 0)
    {
        return SOAPY_SDR_NOT_SUPPORTED;
    }

    if (streamActive)
    {
        mir_sdr_StreamUninit();
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
    
    // this is the user's buffer for channel 0
    void *buff0 = buffs[0];

    // are elements left in the buffer? if not, do a new read.
    if (bufferedElems == 0)
    {
        int ret = this->acquireReadBuffer(stream, _currentHandle, (const void **)&_currentBuff, flags, timeNs, timeoutUs);
        if (ret < 0)
        {
            return ret;
        }
        bufferedElems = ret;
    }

    size_t returnedElems = std::min(bufferedElems, numElems);

    // copy into user's buff0
    if (useShort)
    {
        std::memcpy(buff0, _currentBuff, returnedElems * 2 * sizeof(short));
    }
    else
    {
        std::memcpy(buff0, (float *)_currentBuff, returnedElems * 2 * sizeof(float));
    }
    
    // bump variables for next call into readStream
    bufferedElems -= returnedElems;
    _currentBuff += returnedElems * elementsPerSample * shortsPerWord;

    // return number of elements written to buff0
    if (bufferedElems != 0)
    {
        flags |= SOAPY_SDR_MORE_FRAGMENTS;
    }
    else
    {
        this->releaseReadBuffer(stream, _currentHandle);
    }
    return (int)returnedElems;
}

/*******************************************************************
 * Direct buffer access API
 ******************************************************************/

size_t SoapySDRPlay::getNumDirectAccessBuffers(SoapySDR::Stream *stream)
{
    return _buffs.size();
}

int SoapySDRPlay::getDirectAccessBufferAddrs(SoapySDR::Stream *stream, const size_t handle, void **buffs)
{
    buffs[0] = (void *)_buffs[handle].data();
    return 0;
}

int SoapySDRPlay::acquireReadBuffer(SoapySDR::Stream *stream,
                                    size_t &handle,
                                    const void **buffs,
                                    int &flags,
                                    long long &timeNs,
                                    const long timeoutUs)
{
    std::unique_lock <std::mutex> lock(_buf_mutex);

    // reset is issued by various settings
    // overflow set in the rx callback thread
    if (resetBuffer || _overflowEvent)
    {
        // drain all buffers from the fifo
        _buf_tail = 0;
        _buf_head = 0;
        _buf_count = 0;
        for (auto &buff : _buffs) buff.clear();
        _overflowEvent = false;
        if (resetBuffer)
        {
           resetBuffer = false;
        }
        else
        {
           SoapySDR_log(SOAPY_SDR_SSI, "O");
           return SOAPY_SDR_OVERFLOW;
        }
    }

    // wait for a buffer to become available
    if (_buf_count == 0)
    {
        _buf_cond.wait_for(lock, std::chrono::microseconds(timeoutUs));
        if (_buf_count == 0) 
        {
           return SOAPY_SDR_TIMEOUT;
        }
    }

    // extract handle and buffer
    handle = _buf_head;
    buffs[0] = (void *)_buffs[handle].data();
    flags = 0;

    _buf_head = (_buf_head + 1) % numBuffers;

    // return number available
    return (int)(_buffs[handle].size() / (elementsPerSample * shortsPerWord));
}

void SoapySDRPlay::releaseReadBuffer(SoapySDR::Stream *stream, const size_t handle)
{
    std::unique_lock <std::mutex> lock(_buf_mutex);
    _buffs[handle].clear();
    _buf_count--;
}
