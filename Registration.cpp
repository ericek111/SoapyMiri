#include "SoapyMiri.hpp"
#include <SoapySDR/Registry.hpp>

std::vector<SoapySDR::Kwargs> SoapyMiri::findMiriSDR(const SoapySDR::Kwargs &args) {
    std::vector<SoapySDR::Kwargs> results;

    char manufact[256], product[256], serial[256];

    const size_t this_count = mirisdr_get_device_count();

    for (size_t i = 0; i < this_count; i++) {
        if (mirisdr_get_device_usb_strings(i, manufact, product, serial) != 0) {
            SoapySDR_logf(SOAPY_SDR_ERROR, "mirisdr_get_device_usb_strings(%zu) failed", i);
            continue;
        }
        SoapySDR_logf(SOAPY_SDR_DEBUG, "\tManufacturer: %s, Product Name: %s, Serial: %s", manufact, product, serial);

        std::string deviceName = mirisdr_get_device_name(i);

        SoapySDR::Kwargs devInfo;
        devInfo["label"] = std::string(deviceName) + " :: " + serial;
        // don't we need to duplicate the char buffers here?
        devInfo["product"] = product;
        devInfo["serial"] = serial;
        devInfo["manufacturer"] = manufact;
        devInfo["index"] = std::to_string(i);

        if (args.count("serial") != 0 && args.at("serial") != serial)
            continue;

        if (args.count("index") != 0 && (size_t) std::stol(args.at("index")) != i)
            continue;

        results.push_back(devInfo);
    }

    return results;
}

static SoapySDR::Device *makeMiriSDR(const SoapySDR::Kwargs &args) {
    return new SoapyMiri(args);
}

static SoapySDR::Registry registerMiri("soapyMiri", &SoapyMiri::findMiriSDR, &makeMiriSDR, SOAPY_SDR_ABI_VERSION);