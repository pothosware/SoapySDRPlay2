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

#pragma once

#include <SoapySDR/Logger.h>
#include <SoapySDR/Device.hpp>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <thread>

#ifdef _WIN32
	#include <mir_sdr.h>
#else
	#include <mirsdrapi-rsp.h>
#endif

#define DEFAULT_NUM_PACKETS 200
#define GR_FILTER_STEPS 5

#define SDRPLAY_LO_120 24576000
#define SDRPLAY_LO_144 22000000
#define SDRPLAY_LO_168 19200000

class SDRPlayGainPref {
public:
    double loFreq;
    double freqMin, freqMax;
    int grTarget, grMax, grLNA;

    SDRPlayGainPref(double lo, double min, double max, int targetGain = 40, int lnaGain = 24, int maxGain = 102) : loFreq(lo), freqMin(min), freqMax(max), grTarget(targetGain), grLNA(lnaGain), grMax(maxGain) { }
};

class SoapySDRPlay : public SoapySDR::Device
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

    SoapySDR::Stream *setupStream(
        const int direction,
        const std::string &format,
        const std::vector<size_t> &channels = std::vector<size_t>(),
        const SoapySDR::Kwargs &args = SoapySDR::Kwargs());

    void closeStream(SoapySDR::Stream *stream);

    size_t getStreamMTU(SoapySDR::Stream *stream) const;

    int activateStream(
        SoapySDR::Stream *stream,
        const int flags = 0,
        const long long timeNs = 0,
        const size_t numElems = 0);

    int deactivateStream(
        SoapySDR::Stream *stream,
        const int flags = 0,
        const long long timeNs = 0);

    int readStream(
        SoapySDR::Stream *stream,
        void * const *buffs,
        const size_t numElems,
        int &flags,
        long long &timeNs,
        const long timeoutUs = 100000);

    /*******************************************************************
     * Antenna API
     ******************************************************************/

    std::vector<std::string> listAntennas(const int direction, const size_t channel) const;

//    void setAntenna(const int direction, const size_t channel, const std::string &name);

    std::string getAntenna(const int direction, const size_t channel) const;

    /*******************************************************************
     * Frontend corrections API
     ******************************************************************/

    bool hasDCOffsetMode(const int direction, const size_t channel) const;

    void setDCOffsetMode(const int direction, const size_t channel, const bool automatic);

    bool getDCOffsetMode(const int direction, const size_t channel) const;

    bool hasDCOffset(const int direction, const size_t channel) const;

    void setDCOffset(const int direction, const size_t channel, const std::complex<double> &offset);

//    std::complex<double> getDCOffset(const int direction, const size_t channel) const;

    /*******************************************************************
     * Gain API
     ******************************************************************/

    bool hasGainMode(const int direction, const size_t channel) const;

    std::vector<std::string> listGains(const int direction, const size_t channel) const;

    void setGainMode(const int direction, const size_t channel, const bool automatic);

    bool getGainMode(const int direction, const size_t channel) const;

    void setGain(const int direction, const size_t channel, const double value);

    void setGain(const int direction, const size_t channel, const std::string &name, const double value);

    double getGain(const int direction, const size_t channel, const std::string &name) const;

    SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const;

    /*******************************************************************
     * Frequency API
     ******************************************************************/

    void setFrequency(const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args = SoapySDR::Kwargs());

    double getFrequency(const int direction, const size_t channel, const std::string &name) const;

    std::vector<std::string> listFrequencies(const int direction, const size_t channel) const;

    SoapySDR::RangeList getFrequencyRange(const int direction, const size_t channel, const std::string &name) const;

    SoapySDR::ArgInfoList getFrequencyArgsInfo(const int direction, const size_t channel) const;

    /*******************************************************************
     * Sample Rate API
     ******************************************************************/

    void setSampleRate(const int direction, const size_t channel, const double rate);

    double getSampleRate(const int direction, const size_t channel) const;

    std::vector<double> listSampleRates(const int direction, const size_t channel) const;

    void setBandwidth(const int direction, const size_t channel, const double bw);

    double getBandwidth(const int direction, const size_t channel) const;

    std::vector<double> listBandwidths(const int direction, const size_t channel) const;

    /*******************************************************************
     * Settings API
     ******************************************************************/

    SoapySDR::ArgInfoList getSettingInfo(void) const;

    void writeSetting(const std::string &key, const std::string &value);

    std::string readSetting(const std::string &key) const;

private:

    mir_sdr_Bw_MHzT mirGetBwMhzEnum(double bw);
    static mir_sdr_Bw_MHzT getBwEnumForRate(double rate);
    static mir_sdr_If_kHzT stringToIF(std::string ifMode);
    static std::string IFtoString(mir_sdr_If_kHzT ifkHzT);
    static double getBwValueFromEnum(mir_sdr_Bw_MHzT bwEnum);
    static int getOptimalPacketsForRate(double rate, int sps);
    std::vector<short>::size_type getOwnBufferSize();
    double getHWRate();
    int getDSFactor();
    mir_sdr_ErrT ds_mir_sdr_ReadPacket(short *xi, short *xq, unsigned int *firstSampleNum, int *grChanged, int *rfChanged, int *fsChanged);
    void initDS();

    SDRPlayGainPref *activeGainPref;
    bool gainPrefChanged;
    std::vector<SDRPlayGainPref> gainPrefs;

    void checkGainPref(double frequency);
    bool freqBandChanged(double currentFreq, double newFreq);
    int getFreqBand(double frequency);

    //device handle

    //stream
    int bufferedElems, bufferedElemOffset;
    bool resetBuffer;
    std::vector<short> xi_buffer;
    std::vector<short> xq_buffer;
    std::vector<short> downsample_buffer;
    unsigned int fs;
    int syncUpdate;

    //cached settings
    float ver;
    bool dcOffsetMode;
    int sps;
    int grc, rfc, fsc;
    int newGr, newLnaGr;
    double adcLow, adcHigh, adcTarget;
    double grFilter[GR_FILTER_STEPS];
    int oldGr, oldLnaGr, grMisses;
    int numPackets;
    bool agcEnabled, grChanged;
    double centerFreq, newCenterFreq;
    double rate, newRate;
    double bw, newBw;
    mir_sdr_If_kHzT ifMode;
    bool tryLowIF, newTryLowIF,tryLowIFChanged;
    int totalDSF;
    int mirDSF;
    int ownDSF;
    bool streamActive;

    bool rxFloat, centerFreqChanged, rateChanged, bwChanged;
};
