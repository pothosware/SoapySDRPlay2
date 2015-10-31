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

std::vector<std::string> SoapySDRPlay::getStreamFormats(const int direction, const size_t channel) const {
    std::vector<std::string> formats;

    formats.push_back("CS16");
    formats.push_back("CF32");

    return formats;
}

std::string SoapySDRPlay::getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const {
    //check that direction is SOAPY_SDR_RX
     if (direction != SOAPY_SDR_RX) {
         throw std::runtime_error("SDRPlay is RX only, use SOAPY_SDR_RX");
     }

     fullScale = 2048;
     return "CS16";
}

SoapySDR::ArgInfoList SoapySDRPlay::getStreamArgsInfo(const int direction, const size_t channel) const {
    //check that direction is SOAPY_SDR_RX
     if (direction != SOAPY_SDR_RX) {
         throw std::runtime_error("SDRPlay is RX only, use SOAPY_SDR_RX");
     }

    SoapySDR::ArgInfoList streamArgs;

    //TODO: add optional SDRPlay stream flags here

    return streamArgs;
}


SoapySDR::Stream *SoapySDRPlay::setupStream(
    const int direction,
    const std::string &format,
    const std::vector<size_t> &channels,
    const SoapySDR::Kwargs &args)
{
    //check that direction is SOAPY_SDR_RX
    if (direction != SOAPY_SDR_RX) {
        throw std::runtime_error("SDRPlay is RX only, use SOAPY_SDR_RX");
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


//    if (args.count("buffer_packets") != 0) {
//        int numPackets_in = std::stoi(args.at("buffer_packets"));
//        if (std::isnan(numPackets_in) || numPackets_in == 0) {
//            SoapySDR_logf(SOAPY_SDR_DEBUG, "numPackets is 0 or not a number; defaulting to %d", numPackets);
//        } else {
//            numPackets = numPackets_in;
//        }
//    }
//    SoapySDR_logf(SOAPY_SDR_DEBUG, "Set numPackets to %d", numPackets);

    //TODO: add optional SDRPlay stream flags here

    bufferedElems = 0;
    bufferedElemOffset = 0;
    resetBuffer = false;
    oldGr = oldLnaGr = 0;
    newGr = 40;
    newLnaGr = 24;

    for (int k = 0; k < GR_FILTER_STEPS; k++) {
        grFilter[k] = newGr;
    }

    double dbFs = -10.0;
    double dbFsRange = 5;

    adcLow = expf((double)(dbFs-dbFsRange)/10.0);
    adcTarget = expf((double)dbFs/10.0);
    adcHigh = expf((double)(dbFs+dbFsRange)/10.0);

    grMisses = 0;

    SoapySDR_logf(SOAPY_SDR_DEBUG, "ADC gain targets min/target/max: %f, %f, %f", adcLow, adcTarget, adcHigh);

    return (SoapySDR::Stream *)this;
}

void SoapySDRPlay::closeStream(SoapySDR::Stream *stream)
{

}

size_t SoapySDRPlay::getStreamMTU(SoapySDR::Stream *stream) const
{
    return sps*numPackets;
}

double SoapySDRPlay::getHWRate() {
    /*
    if (abs(rate-200000)<10000) return 2000000;
    if (abs(rate-300000)<10000) return 3000000;
    if (abs(rate-400000)<10000) return 2000000;
    if (abs(rate-500000)<10000) return 2000000;
    if (abs(rate-600000)<10000) return 3000000;
    if (abs(rate-750000)<10000) return 3000000;
    if (abs(rate-800000)<10000) return 4000000;
    if (abs(rate-1000000)<10000) return 2000000;
    */
    return rate*getDSFactor();
}

int SoapySDRPlay::getDSFactor() {
    double threshold=rate*0.03;    // 30000 ppm allowed error for detecting downsampling factor
    if (abs(rate-222222.222)<threshold) return 9;
    if (abs(rate-333333.333)<threshold) return 6;
    if (abs(rate-428571.428)<threshold) return 7;
    if (abs(rate-500000)<threshold) return 4;
    if (abs(rate-571428.571)<threshold) return 7;
    if (abs(rate-750000)<threshold) return 4;
    if (abs(rate-875000)<threshold) return 8;
    if (abs(rate-1000000)<threshold) return 2;
    return 1;
}

std::vector<short>::size_type SoapySDRPlay::getOwnBufferSize()
{
    return sps*numPackets+getDSFactor()*sps;
    //sps is downsampled readpacket size
    //i add one unit of real readpacket size to make sure
    //the there is enough space for the readpacket call when the wrapper
    //is called to fill the end of the buffer
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

    if (centerFreqChanged) {
        centerFreq = newCenterFreq;
        centerFreqChanged = false;
    }


    if (SoapySDRPlay::gainPrefs.empty()) {
        gainPrefs.push_back(SDRPlayGainPref(SDRPLAY_LO_120, 100000, 12000000));
        gainPrefs.push_back(SDRPlayGainPref(SDRPLAY_LO_120, 12000000, 30000000));
        gainPrefs.push_back(SDRPlayGainPref(SDRPLAY_LO_120, 30000000, 60000000));
        gainPrefs.push_back(SDRPlayGainPref(SDRPLAY_LO_120, 60000000, 120000000));
        gainPrefs.push_back(SDRPlayGainPref(SDRPLAY_LO_120, 120000000, 250000000));
        gainPrefs.push_back(SDRPlayGainPref(SDRPLAY_LO_120, 250000000, 375000000));
        gainPrefs.push_back(SDRPlayGainPref(SDRPLAY_LO_144, 375000000, 395000000));
        gainPrefs.push_back(SDRPlayGainPref(SDRPLAY_LO_168, 395000000, 420000000));
        gainPrefs.push_back(SDRPlayGainPref(SDRPLAY_LO_120, 420000000, 1000000000, 40, 7, 85));
        gainPrefs.push_back(SDRPlayGainPref(SDRPLAY_LO_120, 1000000000, 2000000000, 40, 5, 85));
    }

    activeGainPref = &gainPrefs[0];
    checkGainPref(centerFreq);
    gainPrefChanged = false;

    if (rateChanged) {
        rate = newRate;
        rateChanged = false;

        mir_sdr_Bw_MHzT bwCheck = getBwEnumForRate(rate);
        double bwCheckVal = getBwValueFromEnum(bwCheck);
        if (bwCheckVal != bw) {
            bw = bwCheckVal;
            bwChanged = false;
            SoapySDR_logf(SOAPY_SDR_DEBUG, "Changed bandwidth for rate %f to %f", rate, bw);
        }
    } else if (bwChanged) {
        mir_sdr_Bw_MHzT eBw = getBwEnumForRate(newBw);
        bw = getBwValueFromEnum(eBw);
        bwChanged = false;
    }

    // Configure DC tracking in tuner
    mir_sdr_ErrT err;
    mir_sdr_SetParam(101, activeGainPref->loFreq);
    // convert rate to sdrplay rate here with getHWRate() (downsampling support)
    err = mir_sdr_Init(newGr, getHWRate()/1000000.0, centerFreq/1000000.0, mirGetBwMhzEnum(bw), mir_sdr_IF_Zero, &sps);

    if (err != 0) {
        throw std::runtime_error("activateStream failed.");
    }
    int realspp=sps;
    sps/=getDSFactor();         // sps holds the downsampled readpacket size
    if (realspp!=getDSFactor()*sps) {
        throw std::runtime_error("mir_sdr_ReadPacket() packet size incompatibility.");
    }

    if (dcOffsetMode) {
        mir_sdr_SetDcMode(1, 1);
    }

    mir_sdr_SetGrParams(0, activeGainPref->grLNA);

    numPackets = getOptimalPacketsForRate(rate, sps);
    SoapySDR_logf(SOAPY_SDR_DEBUG, "Set numPackets to %d", numPackets);

    SoapySDR_logf(SOAPY_SDR_DEBUG, "stream sps: %d", sps);
    SoapySDR_logf(SOAPY_SDR_DEBUG, "stream numPackets*sps: %d", (numPackets*sps));


    // Allocate data buffers
    xi_buffer.resize(getOwnBufferSize());
    xq_buffer.resize(getOwnBufferSize());
    downsample_buffer.resize(sps*getDSFactor());
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

mir_sdr_ErrT SoapySDRPlay::ds_mir_sdr_ReadPacket(short *xi, short *xq, unsigned int *firstSampleNum, int *grChanged, int *rfChanged, int *fsChanged)
{
    mir_sdr_ErrT rv;
    int dsf=getDSFactor();
    int realspp=dsf*sps;
    rv=mir_sdr_ReadPacket(xi, xq, firstSampleNum, grChanged, rfChanged, fsChanged);
    if (dsf==1) return rv;
    // stupid decimation.... do averaging instead and we may be able to increase ENOB!
    int s=0;
    for (int t=1;t<sps;t++) {
        xi[t]=xi[t*dsf];
        xq[t]=xq[t*dsf];
    }
    return rv;
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

    //check for data with timeout
    // TODO: Can we do?

    bool reInit = false;

    if (resetBuffer)
    {
        resetBuffer = false;
        bufferedElems = 0;
    }

    if (rateChanged)
    {
        rate = newRate;
        rateChanged = false;

        mir_sdr_Bw_MHzT bwCheck = getBwEnumForRate(rate);
        double bwCheckVal = getBwValueFromEnum(bwCheck);

        bwChanged = true;
        newBw = bwCheckVal;

        SoapySDR_log(SOAPY_SDR_DEBUG,"Changed sample rate");
    }

    if (centerFreqChanged)
    {
        double freqDiff = std::abs(centerFreq-newCenterFreq);
        centerFreq = newCenterFreq;
        centerFreqChanged = false;

        checkGainPref(centerFreq);

        if (!gainPrefChanged && (freqDiff < rate/2.0)) {
            mir_sdr_SetRf(centerFreq, 1, 0);
        } else {
            gainPrefChanged = false;
            reInit = true;
        }

        SoapySDR_log(SOAPY_SDR_DEBUG,"Changed center frequency");
    }

    if (bwChanged)
    {
        bw = newBw;
        bwChanged = false;
        reInit = true;
    }

    if (reInit)
    {
        int newSps;

        // "For large ADC sample frequency changes a mir_sdr_Uninit and mir_sdr_Init at the new sample rate must be performed."
        err = mir_sdr_Uninit();
        mir_sdr_SetParam(101, activeGainPref->loFreq);
        // convert rate to sdrplay rate here with getHWRate() (downsampling support)
        err = mir_sdr_Init(newGr, getHWRate()/1000000.0, centerFreq/1000000.0, mirGetBwMhzEnum(bw), mir_sdr_IF_Zero, &newSps);

        if (err != 0) {
            throw std::runtime_error("Error resetting mir_sdr interface for bandwidth change.");
        }
        int realspp=newSps;
        newSps/=getDSFactor();         // sps holds the downsampled readpacket size
        if (realspp!=getDSFactor()*newSps) {
            throw std::runtime_error("mir_sdr_ReadPacket() packet size incompatibility.");
        }

        if (dcOffsetMode) {
            mir_sdr_SetDcMode(1, 1);
        }

        mir_sdr_SetGrParams(0, activeGainPref->grLNA);

        int newPackets = getOptimalPacketsForRate(rate, sps);

        if (newPackets != numPackets || newSps != sps) {
            numPackets = newPackets;
            sps = newSps;
            SoapySDR_logf(SOAPY_SDR_DEBUG, "stream re-init sps: %d", sps);
            SoapySDR_logf(SOAPY_SDR_DEBUG, "stream re-init numPackets*sps: %d", (numPackets*sps));

            xi_buffer.resize(getOwnBufferSize());
            xq_buffer.resize(getOwnBufferSize());
            downsample_buffer.resize(sps*getDSFactor());
        }

        bufferedElems = 0;
        bufferedElemOffset = 0;
        resetBuffer = false;
        grChanged = false;
        gainPrefChanged = false;
    }


    //are elements left in the buffer? if not, do a new read.
    if (bufferedElems == 0)
    {
        bufferedElemOffset = 0;
        int gainElemOfs = 0;

        //receive into temporary buffer
        for (int i = 0; i < numPackets; i++)
        {
            err = ds_mir_sdr_ReadPacket(&xi_buffer[sps*i], &xq_buffer[sps*i], &fs, &grc, &rfc, &fsc);
            //return SOAPY_SDR_TIMEOUT when timeout occurs
            if (err != 0)
            {
                return SOAPY_SDR_TIMEOUT;
            }
            if (grc) {
                SoapySDR_logf(SOAPY_SDR_DEBUG, "Gain change acknowledged from device. packet: %d", i);
                grChanged = false; // indicate where change occurred
                mir_sdr_ResetUpdateFlags(1,0,0);
                gainElemOfs = i * sps;
            }
            if (rfc) {
                SoapySDR_logf(SOAPY_SDR_DEBUG, "Center frequency change acknowledged from device. packet: %d", i);
                mir_sdr_ResetUpdateFlags(0,1,0);
                bufferedElemOffset = i * sps;
                gainElemOfs = i * sps;
            }
            if (fsc) {  // This shouldn't happen now but leaving it to see..
                SoapySDR_logf(SOAPY_SDR_DEBUG, "Rate change acknowledged from device. packet: %d", i);
                mir_sdr_ResetUpdateFlags(0,0,1);
                bufferedElemOffset = i * sps;
                gainElemOfs = i * sps;
            }
        }

        bufferedElems = (numPackets*sps) - bufferedElemOffset;

        // Run AGC unless AGC is waiting for update
        if (agcEnabled && bufferedElems && !gainElemOfs && !grChanged)
        {
            double adcPower = 0, ival, qval;
            for (int j = 0; j < bufferedElems; j++)
            {
                ival = (float)xi_buffer[j]/32767.0;
                qval = (float)xq_buffer[j]/32767.0;
                adcPower += (ival*ival) + (qval*qval);
            }
            double avgPower = adcPower / double(bufferedElems);

            if (adcPower && ((avgPower >= adcHigh) || (avgPower <= adcLow))) {
                grFilter[0] = 10.0 * log10(adcPower / adcTarget);
                for (int k = 1; k < GR_FILTER_STEPS; k++) {
                    grFilter[k] += (grFilter[0] - grFilter[k]) * 0.10;
                }
                newGr = grFilter[GR_FILTER_STEPS-1];

                if (newGr > activeGainPref->grMax) {
                    newGr = activeGainPref->grMax;
                }
            }
//            else {
//                SoapySDR_logf(SOAPY_SDR_DEBUG, "power: low: %f, targ: %f, high: %f, avpow: %f, adpower: %f", adcLow, adcTarget, adcHigh, avgPower, adcPower);
//            }

            // only update if change is required
            if (newGr != oldGr) {
                // use absolute value
                SoapySDR_logf(SOAPY_SDR_DEBUG, "AGC: Gain reduction changed from %d to %d", oldGr, newGr);
                oldLnaGr = newLnaGr = activeGainPref->grLNA;
                err = mir_sdr_SetGr(newGr, 1, syncUpdate);
                err = mir_sdr_SetGrParams(0, activeGainPref->grLNA);
                oldGr = newGr;
                grChanged = true;
            }
        } else if (!agcEnabled && !grChanged && (newGr != oldGr || newLnaGr != oldLnaGr)) {
            err = mir_sdr_SetGr(newGr, 1, syncUpdate);
            err = mir_sdr_SetGrParams(0, newLnaGr);
            oldGr = newGr;
            oldLnaGr = newLnaGr;
            grChanged = true;
        } else if (grChanged) {
            grMisses++;
            if (grMisses >= 10) {
                SoapySDR_logf(SOAPY_SDR_DEBUG, "AGC missed %d updates, giving it a kick.", grMisses);
                mir_sdr_ResetUpdateFlags(0,1,0);
                grMisses = 0;
                grChanged = false;
            }
        }

    }

    int returnedElems = (numElems>bufferedElems)?bufferedElems:numElems;

    //convert into user's buff0
    if (rxFloat)
    {
        float *ftarget = (float *)buff0;
        for (int i = 0; i < returnedElems; i++)
        {
            ftarget[i*2] = ((float)xi_buffer[bufferedElemOffset+i]/32767.0);
            ftarget[i*2+1] = ((float)xq_buffer[bufferedElemOffset+i]/32767.0);
        }
    }
    else
    {
        short *starget = (short *)buff0;
        for (int i = 0; i < returnedElems; i++)
        {
            starget[i*2] = xi_buffer[bufferedElemOffset+i];
            starget[i*2+1] = xq_buffer[bufferedElemOffset+i];
        }
    }

    //bump variables for next call into readStream
    bufferedElems -= returnedElems;
    bufferedElemOffset += returnedElems;

    //return number of elements written to buff0
    return returnedElems;
}

int SoapySDRPlay::getOptimalPacketsForRate(double rate_in, int sps_in) {
    return ceil((rate_in / 30.0)/(float)sps_in);
}

void SoapySDRPlay::checkGainPref(double frequency) {
    for (int i = 0, iMax = gainPrefs.size(); i < iMax; i++) {
        if (frequency >= gainPrefs[i].freqMin && frequency < gainPrefs[i].freqMax) {
            if (&gainPrefs[i] != activeGainPref) {
                activeGainPref = &gainPrefs[i];
                SoapySDR_logf(SOAPY_SDR_DEBUG, "Changed gain preference LO %f", activeGainPref->loFreq);
                gainPrefChanged = true;
            }
        }
    }
}

