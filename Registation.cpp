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
#include <SoapySDR/Registry.hpp>

static std::vector<SoapySDR::Kwargs> findSDRPlay(const SoapySDR::Kwargs &args)
{
    std::vector<SoapySDR::Kwargs> results;

    //enumerate devices, each device gets an entry in the results
    //results should contain identifying information about each device
    //like a serial number or some kind of identification number.

    //TODO filtering
    //when the user passes input args like a serial number
    //or identification number. Use this information
    //to filter the results when the specified keys do not match
    int sps;
    
    mir_sdr_SetTransferMode(mir_sdr_BULK);
    if (!mir_sdr_Init(40, 2.048, 222.064, mir_sdr_BW_1_536, mir_sdr_IF_Zero, &sps))
    {
        mir_sdr_Uninit();
        SoapySDR::Kwargs dev;

        dev["driver"] = "mir_sdr";
        dev["label"] = "SDRPlay RSP";

        results.push_back(dev);
    }
    else
    {
        mir_sdr_Uninit();
    }

    return results;
}

static SoapySDR::Device *makeSDRPlay(const SoapySDR::Kwargs &args)
{
    return new SoapySDRPlay(args);
}

static SoapySDR::Registry registerSDRPlay("sdrplay", &findSDRPlay, &makeSDRPlay, SOAPY_SDR_ABI_VERSION);
