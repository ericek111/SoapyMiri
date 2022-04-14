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

    // we only support SDRplay
    mirisdr_set_hw_flavour(dev, MIRISDR_HW_SDRPLAY);

    mirisdr_reset(dev);
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
    gains.push_back("LNA");
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

    mirisdr_set_tuner_gain(dev, (int) (value * 10.0));
}

void SoapyMiri::setGain(const int direction, const size_t channel, const std::string &name, const double value) {
    if (!dev)
        return;

    if (name == "LNA") {
        // The only implemented gain control at this moment is for LNA.
        setGain(direction, channel, value);
    } else {
        SoapySDR_logf(SOAPY_SDR_WARNING, "Trying to set non-existent gain '%s' to %f!", name.c_str(), value);
    }
}

double SoapyMiri::getGain(const int direction, const size_t channel, const std::string &name) const {
    if (!dev)
        return 0.0;

    if (name == "LNA") {
        return ((double) mirisdr_get_tuner_gain(dev)) / 10.0;
    }

    return 0.0;
}

SoapySDR::Range SoapyMiri::getGainRange(const int direction, const size_t channel, const std::string &name) const {
    if (!dev)
        return {};

    // `mirisdr_get_tuner_gains` writes many integers into the `int* gains` parameter.
    // This is a bit ridiculous. So we'll just get the highest value and assume 0 as the lowest.
    int highest = mirisdr_get_tuner_gains(dev, nullptr);
    return SoapySDR::Range(0, highest);
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

    results.push_back(8e6); // known to work

    return results;
}

SoapySDR::RangeList SoapyMiri::getSampleRateRange(const int direction, const size_t channel) const {
    SoapySDR::RangeList results;

    std::vector<double> listedRanges = listBandwidths(direction, channel);
    for (const auto &val : listedRanges) {
        results.push_back(SoapySDR::Range(val, val));
    }

    results.push_back(SoapySDR::Range(0, 8e6)); // known to work

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
    }
}

std::string SoapyMiri::readSetting(const std::string &key) const {
    if (!dev)
        return "";

    if (key == "offset_tune") {
        // TODO: create `mirisdr_get_offset_tuning`
        return isOffsetTuning ? "true" : "false";
    }

    SoapySDR_logf(SOAPY_SDR_WARNING, "Unknown setting '%s'", key.c_str());
    return "";
}
