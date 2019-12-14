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
#pragma once

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Logger.h>
#include <SoapySDR/Types.h>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <string>
#include <cstring>
#include <algorithm>

#include <sdrplay_api.h>

#define DEFAULT_BUFFER_LENGTH     (65536)
#define DEFAULT_NUM_BUFFERS       (8)
#define DEFAULT_ELEMS_PER_SAMPLE  (2)

class SoapySDRPlay: public SoapySDR::Device
{
public:
    SoapySDRPlay(const SoapySDR::Kwargs &args);

    ~SoapySDRPlay(void);

    /*******************************************************************
     * Identification API
     ******************************************************************/

    std::string getDriverKey(void) const;

    std::string getHardwareKey(void) const;

    SoapySDR::Kwargs getHardwareInfo(void) const;

    /*******************************************************************
     * Channels API
     ******************************************************************/

    size_t getNumChannels(const int) const;

    /*******************************************************************
     * Stream API
     ******************************************************************/

    std::vector<std::string> getStreamFormats(const int direction, const size_t channel) const;

    std::string getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const;

    SoapySDR::ArgInfoList getStreamArgsInfo(const int direction, const size_t channel) const;

    SoapySDR::Stream *setupStream(const int direction, 
                                  const std::string &format, 
                                  const std::vector<size_t> &channels = std::vector<size_t>(), 
                                  const SoapySDR::Kwargs &args = SoapySDR::Kwargs());

    void closeStream(SoapySDR::Stream *stream);

    size_t getStreamMTU(SoapySDR::Stream *stream) const;

    int activateStream(SoapySDR::Stream *stream,
                       const int flags = 0,
                       const long long timeNs = 0,
                       const size_t numElems = 0);

    int deactivateStream(SoapySDR::Stream *stream, const int flags = 0, const long long timeNs = 0);

    int readStream(SoapySDR::Stream *stream,
                   void * const *buffs,
                   const size_t numElems,
                   int &flags,
                   long long &timeNs,
                   const long timeoutUs = 200000);

    class Buffer;
    int readChannel(SoapySDR::Stream *stream,
                    void *buff,
                    const size_t numElems,
                    int &flags,
                    long long &timeNs,
                    const long timeoutUs,
                    Buffer *daBuf);

    /*******************************************************************
     * Direct buffer access API
     ******************************************************************/

    size_t getNumDirectAccessBuffers(SoapySDR::Stream *stream);

    int getDirectAccessBufferAddrs(SoapySDR::Stream *stream, const size_t handle, void **buffs);

    int acquireReadBuffer(SoapySDR::Stream *stream,
                          size_t &handle,
                          const void **buffs,
                          int &flags,
                          long long &timeNs,
                          const long timeoutUs = 100000,
                          Buffer *daBuf = 0);

    void releaseReadBuffer(SoapySDR::Stream *stream, const size_t handle);

    /*******************************************************************
     * Antenna API
     ******************************************************************/

    std::vector<std::string> listAntennas(const int direction, const size_t channel) const;

    void setAntenna(const int direction, const size_t channel, const std::string &name);

    std::string getAntenna(const int direction, const size_t channel) const;

    /*******************************************************************
     * Frontend corrections API
     ******************************************************************/

    bool hasDCOffsetMode(const int direction, const size_t channel) const;

    /*******************************************************************
     * Gain API
     ******************************************************************/

    std::vector<std::string> listGains(const int direction, const size_t channel) const;

    bool hasGainMode(const int direction, const size_t channel) const;

    void setGainMode(const int direction, const size_t channel, const bool automatic);

    bool getGainMode(const int direction, const size_t channel) const;

    void setGain(const int direction, const size_t channel, const std::string &name, const double value);

    double getGain(const int direction, const size_t channel, const std::string &name) const;

    SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const;

    /*******************************************************************
     * Frequency API
     ******************************************************************/

    void setFrequency(const int direction,
                      const size_t channel,
                      const std::string &name,
                      const double frequency,
                      const SoapySDR::Kwargs &args = SoapySDR::Kwargs());

    double getFrequency(const int direction, const size_t channel, const std::string &name) const;

    SoapySDR::RangeList getBandwidthRange(const int direction, const size_t channel) const;
    
    std::vector<std::string> listFrequencies(const int direction, const size_t channel) const;

    SoapySDR::RangeList getFrequencyRange(const int direction, const size_t channel, const std::string &name) const;

    SoapySDR::ArgInfoList getFrequencyArgsInfo(const int direction, const size_t channel) const;

    /*******************************************************************
     * Sample Rate API
     ******************************************************************/

    void setSampleRate(const int direction, const size_t channel, const double rate);

    double getSampleRate(const int direction, const size_t channel) const;

    std::vector<double> listSampleRates(const int direction, const size_t channel) const;

    /*******************************************************************
    * Bandwidth API
    ******************************************************************/

    void setBandwidth(const int direction, const size_t channel, const double bw);

    double getBandwidth(const int direction, const size_t channel) const;

    std::vector<double> listBandwidths(const int direction, const size_t channel) const;
    
    void setDCOffsetMode(const int direction, const size_t channel, const bool automatic);
    
    bool getDCOffsetMode(const int direction, const size_t channel) const;
    
    bool hasDCOffset(const int direction, const size_t channel) const;

    /*******************************************************************
     * Settings API
     ******************************************************************/

    SoapySDR::ArgInfoList getSettingInfo(void) const;

    void writeSetting(const std::string &key, const std::string &value);

    void changeRspDuoMode(const std::string &rspDuoModeString);

    std::string readSetting(const std::string &key) const;

    /*******************************************************************
     * Async API
     ******************************************************************/

    void rx_callback(short *xi, short *xq, unsigned int numSamples, sdrplay_api_TunerSelectT tuner);

    void ev_callback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT *params);

private:

    /*******************************************************************
     * Internal functions
     ******************************************************************/

    static double getRateForBwEnum(sdrplay_api_Bw_MHzT bwEnum);

    static uint32_t getInputSampleRateAndDecimation(uint32_t rate, unsigned int *decM, unsigned int *decEnable, sdrplay_api_If_kHzT ifMode);

    static sdrplay_api_Bw_MHzT getBwEnumForRate(double rate, sdrplay_api_If_kHzT ifMode);

    static  double getBwValueFromEnum(sdrplay_api_Bw_MHzT bwEnum);

    static sdrplay_api_Bw_MHzT sdrPlayGetBwMhzEnum(double bw);

    static sdrplay_api_TunerSelectT rspDuoModeStringToTuner(std::string rspDuoMode);

    static sdrplay_api_RspDuoModeT rspDuoModeStringToRspDuoMode(std::string rspDuoMode);

    static std::string rspDuoModetoString(sdrplay_api_TunerSelectT tuner, sdrplay_api_RspDuoModeT rspDuoMode);

    static sdrplay_api_If_kHzT stringToIF(std::string ifMode);

    static std::string IFtoString(sdrplay_api_If_kHzT ifkHzT);

    /*******************************************************************
     * Private variables
     ******************************************************************/
    //device settings
    sdrplay_api_DeviceT device;
    sdrplay_api_DeviceParamsT *deviceParams;
    sdrplay_api_RxChannelParamsT *chParams;
    float ver;

    //cached settings
    uint32_t reqSampleRate;
    std::atomic_ulong bufferLength;

    //numBuffers, bufferElems, elementsPerSample
    //are indeed constants
    const size_t numBuffers = DEFAULT_NUM_BUFFERS;
    const unsigned int bufferElems = DEFAULT_BUFFER_LENGTH;
    const int elementsPerSample = DEFAULT_ELEMS_PER_SAMPLE;

    std::atomic_uint shortsPerWord;
 
    std::atomic_bool streamActive;

    std::atomic_bool useShort;

    int nchannels;

public:

   /*******************************************************************
    * Public variables
    ******************************************************************/
    
    mutable std::mutex _general_state_mutex;

#if 0
    std::mutex _buf_mutex;
    std::condition_variable _buf_cond;

    std::vector<std::vector<short> > _buffs;
    size_t	_buf_head;
    size_t	_buf_tail;
    size_t	_buf_count;
    short *_currentBuff;
    bool _overflowEvent;
    std::atomic_size_t bufferedElems;
    size_t _currentHandle;
    std::atomic_bool resetBuffer;
#endif

    class Buffer
    {
    public:
        Buffer(size_t numBuffers, unsigned long bufferLength);
        ~Buffer(void);

        std::mutex mutex;
        std::condition_variable cond;

        std::vector<std::vector<short> > buffs;
        size_t      head;
        size_t      tail;
        size_t      count;
        short *currentBuff;
        bool overflowEvent;
        std::atomic_size_t nElems;
        size_t currentHandle;
        std::atomic_bool reset;
    };

    Buffer *_bufA, *_bufB;
};
