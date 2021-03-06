#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include "clock.hh"

static inline uint32_t ntp_diff_ms(uint64_t t1, uint64_t t2)
{
    uint32_t s_diff  = (t1 >> 32) - (t2 >> 32);
    uint32_t us_diff = (t1 & 0xffffffff) - (t2 & 0xffffffff);

    return s_diff * 1000 + (us_diff / 1000000UL);
}

uint64_t uvg_rtp::clock::ntp::now()
{
    static const uint64_t EPOCH = 2208988800ULL;
    static const uint64_t NTP_SCALE_FRAC = 4294967296ULL;

    struct timeval tv;
#ifdef _WIN32
    uvg_rtp::clock::gettimeofday(&tv, NULL);
#else
    gettimeofday(&tv, NULL);
#endif

    uint64_t tv_ntp, tv_usecs;

    tv_ntp = tv.tv_sec + EPOCH;
    tv_usecs = (NTP_SCALE_FRAC * tv.tv_usec) / 1000000UL;

    return (tv_ntp << 32) | tv_usecs;
}

uint64_t uvg_rtp::clock::ntp::diff(uint64_t ntp1, uint64_t ntp2)
{
    return ntp_diff_ms(ntp1, ntp2);
}

uint64_t uvg_rtp::clock::ntp::diff_now(uint64_t then)
{
    uint64_t now = uvg_rtp::clock::ntp::now();

    return ntp_diff_ms(now, then);
}

uvg_rtp::clock::hrc::hrc_t uvg_rtp::clock::hrc::now()
{
    return std::chrono::high_resolution_clock::now();
}

uint64_t uvg_rtp::clock::hrc::diff(hrc_t hrc1, hrc_t hrc2)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(hrc1 - hrc2).count();
}

uint64_t uvg_rtp::clock::hrc::diff_now(hrc_t then)
{
    uint64_t diff = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - then
    ).count();

    return diff;
}

uint64_t uvg_rtp::clock::hrc::diff_now_us(hrc_t& then)
{
    uint64_t diff = (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - then
    ).count();

    return diff;
}

uint64_t uvg_rtp::clock::ms_to_jiffies(uint64_t ms)
{
    return ((double)ms / 1000) * 65536;
}

uint64_t uvg_rtp::clock::jiffies_to_ms(uint64_t jiffies)
{
    return ((double)jiffies / 65536) * 1000;
}

#ifdef _WIN32
int uvg_rtp::clock::gettimeofday(struct timeval * tp, struct timezone * tzp)
{
    // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
    // This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
    // until 00:00:00 January 1, 1970
    static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime( &system_time );
    SystemTimeToFileTime( &system_time, &file_time );
    time =  ((uint64_t)file_time.dwLowDateTime )      ;
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    tp->tv_sec  = (long) ((time - EPOCH) / 10000000L);
    tp->tv_usec = (long) (system_time.wMilliseconds * 1000);
    return 0;
}
#endif
