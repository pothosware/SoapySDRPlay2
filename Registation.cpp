/*
 * TODO license, copyright, etc
 * 
 */

#include "SoapySDRPlay.hpp"
#include <SoapySDR/Registry.hpp>

static std::vector<SoapySDR::Kwargs> findSDRPlay(const SoapySDR::Kwargs &args)
{
    std::vector<SoapySDR::Kwargs> results;

    //TODO enumerate devices, each device gets an entry in the results
    //results should contain identifying information about each device
    //like a serial number or some kind of identification number.

    //TODO filtering
    //when the user passes input args like a serial number
    //or identification number. Use this information
    //to filter the results when the specified keys do not match
    int sps;

    if (!mir_sdr_Init(40, 2.048, 222.064, mir_sdr_BW_1_536, mir_sdr_IF_Zero, &sps))
    {
        mir_sdr_Uninit();
        SoapySDR::Kwargs dev;

        dev["driver"] = "mir_sdr";
        dev["label"] = "SDRPlay Device (experimental)";

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
