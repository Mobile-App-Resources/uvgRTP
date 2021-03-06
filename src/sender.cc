#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#if defined(__MINGW32__) || defined(__MINGW64__)
#include "mingw_inet.hh"
using namespace uvg_rtp;
using namespace mingw;
#endif

#include <cstring>
#include <iostream>

#include "debug.hh"
#include "dispatch.hh"
#include "sender.hh"

#include "formats/opus.hh"
#include "formats/hevc.hh"
#include "formats/generic.hh"

uvg_rtp::sender::sender(uvg_rtp::socket& socket, rtp_ctx_conf& conf, rtp_format_t fmt, uvg_rtp::rtp *rtp):
    socket_(socket),
    rtp_(rtp),
    conf_(conf),
    fmt_(fmt)
{
}

uvg_rtp::sender::~sender()
{
    delete dispatcher_;
    delete fqueue_;
}

rtp_error_t uvg_rtp::sender::destroy()
{
    if (fmt_ == RTP_FORMAT_HEVC && conf_.flags & RCE_SYSTEM_CALL_DISPATCHER) {
        while (dispatcher_->stop() != RTP_OK) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    return RTP_OK;
}

rtp_error_t uvg_rtp::sender::init()
{
    rtp_error_t ret  = RTP_OK;
    ssize_t buf_size = 4 * 1000 * 1000;

    if ((ret = socket_.setsockopt(SOL_SOCKET, SO_SNDBUF, (const char *)&buf_size, sizeof(int))) != RTP_OK)
        return ret;

#ifndef _WIN32
    if (fmt_ == RTP_FORMAT_HEVC && conf_.flags & RCE_SYSTEM_CALL_DISPATCHER) {
        dispatcher_ = new uvg_rtp::dispatcher(&socket_);
        fqueue_     = new uvg_rtp::frame_queue(fmt_, dispatcher_);

        if (dispatcher_)
            dispatcher_->start();
    } else {
#endif
        fqueue_     = new uvg_rtp::frame_queue(fmt_);
        dispatcher_ = nullptr;
#ifndef _WIN32
    }
#endif

    return ret;
}

rtp_error_t uvg_rtp::sender::__push_frame(uint8_t *data, size_t data_len, int flags)
{
    switch (fmt_) {
        case RTP_FORMAT_HEVC:
            return uvg_rtp::hevc::push_frame(this, data, data_len, flags);

        case RTP_FORMAT_OPUS:
            return uvg_rtp::opus::push_frame(this, data, data_len, flags);

        default:
            LOG_DEBUG("Format not recognized, pushing the frame as generic");
            return uvg_rtp::generic::push_frame(this, data, data_len, flags);
    }
}

rtp_error_t uvg_rtp::sender::__push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int flags)
{
    switch (fmt_) {
        case RTP_FORMAT_HEVC:
            return uvg_rtp::hevc::push_frame(this, std::move(data), data_len, flags);

        case RTP_FORMAT_OPUS:
            return uvg_rtp::opus::push_frame(this, std::move(data), data_len, flags);

        default:
            LOG_DEBUG("Format not recognized, pushing the frame as generic");
            return uvg_rtp::generic::push_frame(this, std::move(data), data_len, flags);
    }
}

rtp_error_t uvg_rtp::sender::push_frame(uint8_t *data, size_t data_len, int flags)
{
    if (flags & RTP_COPY || (conf_.flags & RCE_SRTP && !(conf_.flags & RCE_INPLACE_ENCRYPTION))) {
        std::unique_ptr<uint8_t[]> data_ptr = std::unique_ptr<uint8_t[]>(new uint8_t[data_len]);
        std::memcpy(data_ptr.get(), data, data_len);

        return __push_frame(std::move(data_ptr), data_len, flags);
    }

    return __push_frame(data, data_len, flags);
}

rtp_error_t uvg_rtp::sender::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int flags)
{
    std::unique_ptr<uint8_t[]> data_ptr = std::move(data);

    if (flags & RTP_COPY || (conf_.flags & RCE_SRTP && !(conf_.flags & RCE_INPLACE_ENCRYPTION))) {
        data_ptr = std::unique_ptr<uint8_t[]>(new uint8_t[data_len]);
        std::memcpy(data_ptr.get(), data.get(), data_len);
    }

    return __push_frame(std::move(data_ptr), data_len, flags);
}

uvg_rtp::frame_queue *uvg_rtp::sender::get_frame_queue()
{
    return fqueue_;
}

uvg_rtp::socket& uvg_rtp::sender::get_socket()
{
    return socket_;
}

uvg_rtp::rtp *uvg_rtp::sender::get_rtp_ctx()
{
    return rtp_;
}

void uvg_rtp::sender::install_dealloc_hook(void (*dealloc_hook)(void *))
{
    if (!fqueue_)
        return;

    fqueue_->install_dealloc_hook(dealloc_hook);
}

rtp_ctx_conf& uvg_rtp::sender::get_conf()
{
    return conf_;
}
