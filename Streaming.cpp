#include "SoapyMiri.hpp"
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Formats.hpp>
#include <cstring>

std::vector<std::string> SoapyMiri::getStreamFormats(const int direction, const size_t channel) const {
    std::vector<std::string> formats;

    formats.push_back(SOAPY_SDR_CF32);

    return formats;
}

std::string SoapyMiri::getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const {
     fullScale = 128;
     return SOAPY_SDR_CU16;
}

SoapySDR::ArgInfoList SoapyMiri::getStreamArgsInfo(const int direction, const size_t channel) const {
    SoapySDR::ArgInfoList streamArgs;

    SoapySDR::ArgInfo bufflenArg;
    bufflenArg.key = "bufflen";
    bufflenArg.value = std::to_string(DEFAULT_BUFFER_LENGTH);
    bufflenArg.name = "Buffer Size";
    bufflenArg.description = "Number of bytes per buffer, multiples of 512 only.";
    bufflenArg.units = "bytes";
    bufflenArg.type = SoapySDR::ArgInfo::INT;

    streamArgs.push_back(bufflenArg);

    SoapySDR::ArgInfo buffersArg;
    buffersArg.key = "buffers";
    buffersArg.value = std::to_string(DEFAULT_NUM_BUFFERS);
    buffersArg.name = "Ring buffers";
    buffersArg.description = "Number of buffers in the ring.";
    buffersArg.units = "buffers";
    buffersArg.type = SoapySDR::ArgInfo::INT;

    streamArgs.push_back(buffersArg);

    SoapySDR::ArgInfo asyncbuffsArg;
    asyncbuffsArg.key = "asyncBuffs";
    asyncbuffsArg.value = "0";
    asyncbuffsArg.name = "Async buffers";
    asyncbuffsArg.description = "Number of async usb buffers (advanced).";
    asyncbuffsArg.units = "buffers";
    asyncbuffsArg.type = SoapySDR::ArgInfo::INT;

    streamArgs.push_back(asyncbuffsArg);

    return streamArgs;
}

/*******************************************************************
 * Async thread work
 ******************************************************************/

static void _rx_callback(unsigned char *buf, uint32_t len, void *ctx)
{
    auto *self = (SoapyMiri *) ctx;
    self->rx_callback(buf, len);
}

void SoapyMiri::rx_async_operation(void)
{
    mirisdr_read_async(dev, &_rx_callback, this, optNumBuffers, optBufferLength);
}

void SoapyMiri::rx_callback(unsigned char *buf, uint32_t len)
{
    //printf("_rx_callback %d _buf_head=%d, optNumBuffers=%d\n", len, _buf_head, _buf_tail);

    // atomically add len to ticks but return the previous value
    // unsigned long long tick = ticks.fetch_add(len);

    // overflow condition: the caller is not reading fast enough
    if (_buf_count == optNumBuffers) {
        _overflowEvent = true;
        return;
    }

    // copy into the buffer queue
    auto &buff = buffs[_buf_tail];
    // buff.tick = tick;
    buff.data.resize(len);
    std::memcpy(buff.data.data(), buf, len);

    // increment the tail pointer
    _buf_tail = (_buf_tail + 1) % optNumBuffers;

    // increment buffers available under lock to avoid race in acquireReadBuffer wait
    {
        std::lock_guard<std::mutex> lock(_buf_mutex);
        _buf_count++;
    }

    //notify readStream()
    _buf_cond.notify_one();
}

/*******************************************************************
 * Stream API
 ******************************************************************/

SoapySDR::Stream *SoapyMiri::setupStream(
        const int direction,
        const std::string &format,
        const std::vector<size_t> &channels,
        const SoapySDR::Kwargs &args)
{
    if (direction != SOAPY_SDR_RX) {
        throw std::runtime_error("LibMiriSDR supports only RX.");
    }

    if (channels.size() > 1 || (channels.size() > 0 && channels.at(0) != 0)) {
        throw std::runtime_error("setupStream invalid channel selection");
    }

    if (format != SOAPY_SDR_CF32) {
        throw std::runtime_error("setupStream: invalid format '" + format + "', only CF32 is supported by SoapyMiri.");
    }

    sampleFormat = MIRI_FORMAT_CF32;

    optBufferLength = DEFAULT_BUFFER_LENGTH;
    if (args.count("bufflen") != 0)
    {
        try
        {
            int bufferLength_in = std::stoi(args.at("bufflen"));
            if (bufferLength_in > 0)
            {
                optBufferLength = bufferLength_in;
            }
        }
        catch (const std::invalid_argument &){}
    }
    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR Using buffer length %d", optBufferLength);

    optNumBuffers = DEFAULT_NUM_BUFFERS;
    if (args.count("buffers") != 0) {
        try {
            int numBuffers_in = std::stoi(args.at("buffers"));
            if (numBuffers_in > 0) {
                optNumBuffers = numBuffers_in;
            }
        }
        catch (const std::invalid_argument &){}
    }
    SoapySDR_logf(SOAPY_SDR_DEBUG, "SoapyMiri Using %d buffers", optNumBuffers);

    // clear async fifo counts
    _buf_tail = 0;
    _buf_count = 0;
    _buf_head = 0;

    // allocate buffers
    buffs.resize(optNumBuffers);
    for (auto &buff : buffs) {
        buff.data.reserve(optBufferLength);
        buff.data.resize(optBufferLength);
    }

    return (SoapySDR::Stream *) this;
}

void SoapyMiri::closeStream(SoapySDR::Stream *stream)
{
    this->deactivateStream(stream, 0, 0);
    buffs.clear();
}

size_t SoapyMiri::getStreamMTU(SoapySDR::Stream *stream) const
{
    return optBufferLength / BYTES_PER_SAMPLE;
}

int SoapyMiri::activateStream(
        SoapySDR::Stream *stream,
        const int flags,
        const long long timeNs,
        const size_t numElems)
{
    if (!dev)
        return 0;

    resetBuffer = true;
    remainingElems = 0;

    if (!_rx_async_thread.joinable())
    {
        mirisdr_reset_buffer(dev);
        _rx_async_thread = std::thread(&SoapyMiri::rx_async_operation, this);
    }

    return 0;
}

int SoapyMiri::deactivateStream(SoapySDR::Stream *stream, const int flags, const long long timeNs)
{
    if (!dev)
        return 0;

    if (_rx_async_thread.joinable())
    {
        mirisdr_cancel_async(dev);
        _rx_async_thread.join();
    }
    return 0;
}

int SoapyMiri::readStream(
        SoapySDR::Stream *stream,
        void * const *buffs,
        const size_t numElems,
        int &flags,
        long long &timeNs,
        const long timeoutUs)
{
    // drop remainder buffer on reset
    if (resetBuffer && remainingElems != 0)
    {
        remainingElems = 0;
        this->releaseReadBuffer(stream, _currentHandle);
    }

    // this is the user's buffer for channel 0
    void* buff0 = buffs[0];

    // are elements left in the buffer? if not, do a new read.
    if (remainingElems == 0)
    {
        int ret = this->acquireReadBuffer(stream, _currentHandle, (const void **)&_currentBuff, flags, timeNs, timeoutUs);
        if (ret < 0) {
            return ret;
        }
        remainingElems = ret;
    }

    size_t returnedElems = std::min(remainingElems, numElems);

    // convert into user's buff0
    if (sampleFormat == MIRI_FORMAT_CF32)
    {
        float* ftarget = (float*) buff0;
        for (size_t i = 0; i < returnedElems; i++) {
            ftarget[i * 2 + 0] = (float) _currentBuff[i * 2 + 0] * (1.0f/4096.0f);
            ftarget[i * 2 + 1] = (float) _currentBuff[i * 2 + 1] * (1.0f/4096.0f);
        }
    }

    // bump variables for next call into readStream
    remainingElems -= returnedElems;
    _currentBuff += returnedElems * 2; // BYTES_PER_SAMPLE is handled by _currentBuff being uint16_t*, only account for I/Q.

    if (remainingElems != 0) {
        flags |= SOAPY_SDR_MORE_FRAGMENTS;
    } else {
        this->releaseReadBuffer(stream, _currentHandle);
    }

    // return number of elements written to buff0
    return returnedElems;
}

/*******************************************************************
 * Direct buffer access API
 ******************************************************************/

size_t SoapyMiri::getNumDirectAccessBuffers(SoapySDR::Stream *stream)
{
    return buffs.size();
}

int SoapyMiri::getDirectAccessBufferAddrs(SoapySDR::Stream *stream, const size_t handle, void **outBuffs)
{
    outBuffs[0] = (void *)buffs[handle].data.data();
    return 0;
}

int SoapyMiri::acquireReadBuffer(
    SoapySDR::Stream *stream,
    size_t &handle,
    const void **outBuffs,
    int &flags,
    long long &timeNs,
    const long timeoutUs)
{
    // reset is issued by various settings to drain old data out of the queue
    if (resetBuffer)
    {
        // drain all buffers from the fifo
        _buf_head = (_buf_head + _buf_count.exchange(0)) % optNumBuffers;
        resetBuffer = false;
        _overflowEvent = false;
    }

    // handle overflow from the rx callback thread
    if (_overflowEvent)
    {
        // drain the old buffers from the fifo
        _buf_head = (_buf_head + _buf_count.exchange(0)) % optNumBuffers;
        _overflowEvent = false;
        SoapySDR::log(SOAPY_SDR_SSI, "O");
        return SOAPY_SDR_OVERFLOW;
    }

    // wait for a buffer to become available
    if (_buf_count == 0)
    {
        std::unique_lock <std::mutex> lock(_buf_mutex);
        _buf_cond.wait_for(lock, std::chrono::microseconds(timeoutUs), [this]{ return _buf_count != 0; });
        if (_buf_count == 0) {
            return SOAPY_SDR_TIMEOUT;
        }
    }

    // extract handle and buffer
    handle = _buf_head;
    _buf_head = (_buf_head + 1) % optNumBuffers;
    outBuffs[0] = (void *)buffs[handle].data.data();

    // return number available
    return buffs[handle].data.size() / BYTES_PER_SAMPLE / 2; // 2 = I/Q, BYTES_PER_SAMPLE = 2 for uint16_t
                                                             // (since we only return 12-bit values wrapped in shorts)
}

void SoapyMiri::releaseReadBuffer(
    SoapySDR::Stream *stream,
    const size_t handle)
{
    //TODO this wont handle out of order releases
    _buf_count--;
}