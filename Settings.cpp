#include "SoapyMiri.hpp"

/*******************************************************************
 * Identification API
 ******************************************************************/

SoapyMiri::SoapyMiri(const SoapySDR::Kwargs &args) :
        dev(nullptr),
        remainingElems(0),
        resetBuffer(false) {
    if (args.count("label") != 0) {
        SoapySDR_logf(SOAPY_SDR_INFO, "Opening %s...", args.at("label").c_str());
    }

    if (args.count("index") == 0) {
        throw std::runtime_error("No SDRplay devices supported by LibMiriSDR found!");
    }

    // TODO: This probably needs to be implemented in the API (like in `rtlsdr_get_index_by_serial`).
    // For now, we're fine with passing indices in the Kwargs.
    deviceIdx = (uint32_t) std::stoi(args.at("index"));

    SoapySDR_logf(SOAPY_SDR_DEBUG, "LibMiriSDR opening device %d", deviceIdx);
    if (mirisdr_open(&dev, deviceIdx) != 0) {
        throw std::runtime_error("Unable to open LibMiriSDR device.");
    }
}

SoapyMiri::~SoapyMiri(void) {
    if (dev) {
        mirisdr_close(dev);
    }
}

/*******************************************************************
 * Identification API
 ******************************************************************/

std::string SoapyMiri::getDriverKey(void) const {
    return "MIRI";
}

std::string SoapyMiri::getHardwareKey(void) const {
    // TODO: Lay down framework for more SDRplay devices. The API only supports RSP1 now, though.
    return "RSP1";
}

SoapySDR::Kwargs SoapyMiri::getHardwareInfo(void) const {
    SoapySDR::Kwargs args;

    args["origin"] = "https://github.com/ericek111/SoapyMiri";
    args["index"] = std::to_string(deviceIdx);

    return args;
}

/*******************************************************************
 * Channels API
 ******************************************************************/

size_t SoapyMiri::getNumChannels(const int dir) const {
    return (dir == SOAPY_SDR_RX) ? 1 : 0;
}

bool SoapyMiri::getFullDuplex(const int direction, const size_t channel) const {
    return false;
}

/*******************************************************************
 * Antenna API
 ******************************************************************/

std::vector<std::string> SoapyMiri::listAntennas(const int direction, const size_t channel) const {
    std::vector<std::string> antennas;
    antennas.push_back(getAntenna(direction, channel));
    return antennas;
}

void SoapyMiri::setAntenna(const int direction, const size_t channel, const std::string &name) {
    if (direction != SOAPY_SDR_RX) {
        throw std::runtime_error("setAntenna failed: libmirisdr only supports RX.");
    }
}

std::string SoapyMiri::getAntenna(const int direction, const size_t channel) const {
    return "RX";
}

/*******************************************************************
 * Frontend corrections API
 ******************************************************************/

bool SoapyMiri::hasDCOffsetMode(const int direction, const size_t channel) const {
    return false;
}

bool SoapyMiri::hasFrequencyCorrection(const int direction, const size_t channel) const {
    return false;
}

void SoapyMiri::setFrequencyCorrection(const int direction, const size_t channel, const double value) {
    SoapySDR_logf(SOAPY_SDR_DEBUG, "Frequency correction is not implemented yet in LibMiriSDR! Tried to set to %f.",
                  value);
}

double SoapyMiri::getFrequencyCorrection(const int direction, const size_t channel) const {
    return 0;
}

/*******************************************************************
 * Gain API
 ******************************************************************/

std::vector<std::string> SoapyMiri::listGains(const int direction, const size_t channel) const {
    std::vector<std::string> gains;
    gains.emplace_back("Automatic");
    gains.emplace_back("LNA");
    gains.emplace_back("Baseband");
    gains.emplace_back("Mixer");
    gains.emplace_back("Mixbuffer");
    return gains;
}

bool SoapyMiri::hasGainMode(const int direction, const size_t channel) const {
    return true;
}

void SoapyMiri::setGainMode(const int direction, const size_t channel, const bool automatic) {
    if (!dev)
        return;

    SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting gain mode: %s", automatic ? "Automatic" : "Manual");
    if (mirisdr_set_tuner_gain_mode(dev, automatic ? 0 : 1) != 0) {
        throw std::runtime_error("mirisdr_set_tuner_gain_mode failed");
    }
}

bool SoapyMiri::getGainMode(const int direction, const size_t channel) const {
    if (!dev)
        return false;

    return (bool) mirisdr_get_tuner_gain_mode(dev);
}

void SoapyMiri::setGain(const int direction, const size_t channel, const double value) {
    if (!dev)
        return;

    setGain(direction, channel, "Automatic", value);
}

void SoapyMiri::setGain(const int direction, const size_t channel, const std::string &name, const double value) {
    if (!dev)
        return;

    if (name == "Automatic") {
        mirisdr_set_tuner_gain(dev, (int) value);
    } else if (name == "LNA") {
        mirisdr_set_lna_gain(dev, (int) value);
    } else if (name == "Baseband") {
        mirisdr_set_baseband_gain(dev, (int) value);
    } else if (name == "Mixer") {
        mirisdr_set_mixer_gain(dev, (int) value);
    } else if (name == "Mixbuffer") {
        mirisdr_set_mixbuffer_gain(dev, (int) value);
    } else {
        SoapySDR_logf(SOAPY_SDR_WARNING, "Trying to set non-existent gain '%s' to %f!", name.c_str(), value);
    }
}

double SoapyMiri::getGain(const int direction, const size_t channel, const std::string &name) const {
    if (!dev)
        return 0.0;

    if (name == "Automatic") {
        return ((double) mirisdr_get_tuner_gain(dev));
    } else if (name == "LNA") {
        return ((double) mirisdr_get_lna_gain(dev));
    } else if (name == "Baseband") {
        return ((double) mirisdr_get_baseband_gain(dev));
    } else if (name == "Mixer") {
        return ((double) mirisdr_get_mixer_gain(dev));
    } else if (name == "Mixbuffer") {
        return ((double) mirisdr_get_mixbuffer_gain(dev));
    }

    return 0.0;
}

SoapySDR::Range SoapyMiri::getGainRange(const int direction, const size_t channel, const std::string &name) const {
    if (!dev)
        return {};

    // `mirisdr_get_tuner_gains` writes many integers into the `int* gains` parameter.
    // This is a bit ridiculous. So we'll just get the highest value and assume 0 as the lowest.
    static int highest = mirisdr_get_tuner_gains(dev, nullptr);

    if (name == "Automatic") {
        return {0, static_cast<double>(highest), 1};
    } else if (name == "LNA") {
        return {0, 1, 1}; // 24 dB
    } else if (name == "Baseband") {
        return {0, 59, 1};
    } else if (name == "Mixer") {
        return {0, 19, 19};
    } else if (name == "Mixbuffer") {
        return {0, 24, 6};
    }

    SoapySDR_logf(SOAPY_SDR_WARNING, "Unknown range '%s'", name.c_str());
    return {0, 1, 1}; // should never happen
}


/*******************************************************************
 * Frequency API
 ******************************************************************/

void SoapyMiri::setFrequency(
        const int direction,
        const size_t channel,
        const std::string &name,
        const double frequency,
        const SoapySDR::Kwargs &args
) {
    if (!dev)
        return;

    if (name == "RF") {
        SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting center freq: %d", (uint32_t) frequency);
        if (mirisdr_set_center_freq(dev, (uint32_t) frequency) != 0) {
            throw std::runtime_error("mirisdr_set_center_freq failed");
        }
    }
}

double SoapyMiri::getFrequency(const int direction, const size_t channel, const std::string &name) const {
    if (!dev)
        return 0;

    if (name == "RF") {
        return (double) mirisdr_get_center_freq(dev);
    }

    return 0;
}

std::vector<std::string> SoapyMiri::listFrequencies(const int direction, const size_t channel) const {
    std::vector<std::string> names;
    names.push_back("RF");
    return names;
}

SoapySDR::RangeList SoapyMiri::getFrequencyRange(
        const int direction,
        const size_t channel,
        const std::string &name) const {

    SoapySDR::RangeList results;

    if (name == "RF") {
        results.push_back(SoapySDR::Range(0e6, 2000e6)); // DC to 2 GHz. Because why not?
    }
    return results;
}

SoapySDR::ArgInfoList SoapyMiri::getFrequencyArgsInfo(const int direction, const size_t channel) const {
    SoapySDR::ArgInfoList freqArgs;

    // TODO: frequency arguments

    return freqArgs;
}

/*******************************************************************
 * Sample Rate API
 ******************************************************************/

void SoapyMiri::setSampleRate(const int direction, const size_t channel, const double rate) {
    if (!dev)
        return;

    SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting sample rate: %d", rate);
    if (mirisdr_set_sample_rate(dev, (uint32_t) rate) != 0) {
        throw std::runtime_error("mirisdr_set_sample_rate failed");
    }

    auto newSampleRate = (double) mirisdr_get_sample_rate(dev);
    this->sampleRate = newSampleRate;
}

double SoapyMiri::getSampleRate(const int direction, const size_t channel) const {
    if (!dev)
        return 0;

    // return (double) mirisdr_get_sample_rate(dev);
    return sampleRate;
}

std::vector<double> SoapyMiri::listSampleRates(const int direction, const size_t channel) const {
    // TODO: listSampleRates is supposedly deprecated and replaced by getSampleRateRange?

    std::vector<double> results;

    results.push_back(1300000);
    results.push_back(1536000);
    results.push_back(2048000);
    results.push_back(5e6);
    results.push_back(6e6);
    results.push_back(7e6);
    results.push_back(8e6); // known to work
    results.push_back(9e6);
    results.push_back(10e6);
    results.push_back(12e6); // 12 Msps seems to be the limit for my MSI.SDR blue clone

    return results;
}

SoapySDR::RangeList SoapyMiri::getSampleRateRange(const int direction, const size_t channel) const {
    SoapySDR::RangeList results;

    std::vector<double> listedRanges = listSampleRates(direction, channel);
    for (const auto &val : listedRanges) {
        results.push_back(SoapySDR::Range(val, val));
    }

    return results;
}

void SoapyMiri::setBandwidth(const int direction, const size_t channel, const double bw) {
    if (!dev)
        return;

    if (mirisdr_set_bandwidth(dev, (uint32_t) bw) != 0) {
        throw std::runtime_error("mirisdr_set_bandwidth failed");
    }
}

double SoapyMiri::getBandwidth(const int direction, const size_t channel) const {
    if (!dev)
        return 0;

    return (double) mirisdr_get_bandwidth(dev);
}

std::vector<double> SoapyMiri::listBandwidths(const int direction, const size_t channel) const {
    // TODO: listBandwidths is supposedly deprecated and replaced by getBandwidthRange?

    std::vector<double> results;

    // TODO: Expose bandwidths in the API.

    results.push_back(200000); // MIRISDR_BW_200KHZ
    results.push_back(300000); // MIRISDR_BW_300KHZ
    results.push_back(600000); // MIRISDR_BW_600KHZ
    results.push_back(1536000); // MIRISDR_BW_1536KHZ
    results.push_back(5000000); // MIRISDR_BW_5MHZ
    results.push_back(6000000); // MIRISDR_BW_6MHZ
    results.push_back(7000000); // MIRISDR_BW_7MHZ
    results.push_back(8000000); // MIRISDR_BW_8MHZ
    results.push_back(14000000); // MIRISDR_BW_MAX

    return results;
}

SoapySDR::RangeList SoapyMiri::getBandwidthRange(const int direction, const size_t channel) const {
    SoapySDR::RangeList results;

    std::vector<double> listedRanges = listBandwidths(direction, channel);
    for (const auto &val : listedRanges) {
        results.push_back(SoapySDR::Range(val, val));
    }

    return results;
}

/*******************************************************************
 * Settings API
 ******************************************************************/

SoapySDR::ArgInfoList SoapyMiri::getSettingInfo(void) const {
    SoapySDR::ArgInfoList setArgs;

    SoapySDR::ArgInfo offsetTuneArg;
    offsetTuneArg.key = "offset_tune";
    offsetTuneArg.value = "false";
    offsetTuneArg.name = "Offset Tune";
    offsetTuneArg.description = "MiriSDR Offset Tuning Mode";
    offsetTuneArg.type = SoapySDR::ArgInfo::BOOL;
    setArgs.push_back(offsetTuneArg);

    SoapySDR::ArgInfo biasTeeArg;
    biasTeeArg.key = "biastee";
    biasTeeArg.value = "false";
    biasTeeArg.name = "Bias Tee";
    biasTeeArg.description = "GPIO8 pin enable (bias tee)";
    biasTeeArg.type = SoapySDR::ArgInfo::BOOL;
    setArgs.push_back(biasTeeArg);

    SoapySDR::ArgInfo flavourArg;
    flavourArg.key = "flavour";
    flavourArg.value = "false";
    flavourArg.name = "HW flavour";
    flavourArg.description = "HW variant of the RSP1";
    flavourArg.type = SoapySDR::ArgInfo::STRING;
    flavourArg.options.reserve(flavourMap.size());
    for(const auto &entry : flavourMap) {
        flavourArg.options.push_back(entry.first);
    }
    setArgs.push_back(flavourArg);

    return setArgs;
}

void SoapyMiri::writeSetting(const std::string &key, const std::string &value) {
    if (!dev)
        return;

    if (key == "offset_tune") {
        bool offsetMode = (value == "true");
        SoapySDR_logf(SOAPY_SDR_DEBUG, "MiriSDR offset tuning mode: %s", offsetMode ? "450 kHz (on)" : "0 Hz (off)");
        isOffsetTuning = offsetMode;
        if (mirisdr_set_offset_tuning(dev, offsetMode ? 1 : 0) != 0) {
            throw std::runtime_error("mirisdr_set_offset_tuning failed");
        }
    } else if (key == "biastee") {
        bool enableBiasTee = (value == "true");
        SoapySDR_logf(SOAPY_SDR_DEBUG, "MiriSDR bias tee mode: %s", enableBiasTee ? "true" : "false");
        mirisdr_set_bias(dev, enableBiasTee ? 1 : 0);
    } else if (key == "flavour") {
        if (flavourMap.count(value)) {
            mirisdr_set_hw_flavour(dev, flavourMap[value]);
            hwFlavour = flavourMap[value];
        } else {
            SoapySDR_logf(SOAPY_SDR_ERROR, "MiriSDR invalid HW flavour: %s", value.c_str());
        }
    }

}

std::string SoapyMiri::readSetting(const std::string &key) const {
    if (!dev)
        return "";

    if (key == "offset_tune") {
        // TODO: create `mirisdr_get_offset_tuning`
        return isOffsetTuning ? "true" : "false";
    } else if (key == "biastee") {
        return mirisdr_get_bias(dev) ? "true" : "false";
    } else if (key == "flavour") {
        for (const auto &entry : flavourMap) {
            if (entry.second == hwFlavour) {
                return entry.first;
            }
        }
        // assert: flavour set to something impossible
        SoapySDR_logf(SOAPY_SDR_ERROR, "MiriSDR HW flavour set to unknown value: %d", hwFlavour);
        return "";
    }

    SoapySDR_logf(SOAPY_SDR_WARNING, "Unknown setting '%s'", key.c_str());
    return "";
}
