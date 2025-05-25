#ifndef LIBLOADER_H
#define LIBLOADER_H

#ifndef __linux__
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

#include <cstdint>
#include <string>

#include "common.h"

#ifndef __linux__
#  define COLIBRI_NANO_API __stdcall
#else
#  define COLIBRI_NANO_API
#endif

/**
 * \mainpage ColibriNANO receiver library.
 *
 * Internal ADC clock frequency 122.88 MHz, it determines the Nyquist zone bandwidth, ADC clock/2 = 61.44 MHz.
 * ColibriNANO can receive only in one Nyquist zone at the same time e.g. 0-61.44 MHz, 61.44-122.88 MHz, 122.88-184.32 MHz etc.
 * In the first Nyquist zone internal LPF works automatically, in the second and higher Nyquist zones LPF is disabled.
 * This library allows you to control ColibriNANO receiver, change tuning frequency, preamplifier gain/attenuation
 * and receive IQ stream with adjustable sample rate.
 * ColibriNANO receiver supports the following sample rates:
 *   - 48 kHz
 *   - 96 kHz
 *   - 192 kHz
 *   - 384 kHz
 *   - 768 kHz
 *   - 1536 kHz
 *   - 1920 kHz
 *   - 2560 kHz
 *   - 3072 kHz
 *
 *   Some PC won't be able to support highest sample rates values, it depends on the USB port quality.
 */

class LibLoader {
    typedef void (COLIBRI_NANO_API *pVersion)(u32&, u32&, u32&);
    typedef void (COLIBRI_NANO_API *pInformation)(char**);
    typedef void (COLIBRI_NANO_API *pDevices)(u32&);
    typedef void (COLIBRI_NANO_API *pFunc1)(void);
    typedef bool (COLIBRI_NANO_API *pOpen)(Descriptor*, const u32);
    typedef void (COLIBRI_NANO_API *pClose)(Descriptor);
    typedef bool (COLIBRI_NANO_API *pStart)(Descriptor, SampleRateIndex, pCallbackRx, void*);
    typedef bool (COLIBRI_NANO_API *pStop)(Descriptor);
    typedef bool (COLIBRI_NANO_API *pSetPreamp)(Descriptor, f32);
    typedef bool (COLIBRI_NANO_API *pSetFrequency)(Descriptor, u32);

public:
    LibLoader() = default;

    /**
     * \brief Library load.
     * \param file - full file name.
     * \return if success return true, else false.
     */
#ifndef __linux__
    bool load(const wstring &file);
#else
    bool load(const char *file);
#endif

    /**
     * \brief Library initialization.
     * \details Required once before start of operation.
     */
    void initialize ();

    /**
     * \brief Finish library operation.
     *
     * \details Required to call once before the whole program close.
     * All objects created in open function will be remove.
     */
    void finalize ();

    /**
     * \brief Read library version.
     * \param major
     * \param minor
     * \param patch
     */
    void version (u32 &major, u32 &minor, u32 &patch);

    /**
     * \brief Return line info about the library.
     */
    string information ();

    /**
     * \brief Return amount of found ColibriNANO receivers.
     */
    u32 devices ();


    /**
     * \brief Open the ColibriNANO receiver according to its periodic number.
     * \param pDev - Pointer to the receiver descriptor.
     * \param devIndex - Receiver's periodic number.
     * \return if success return true, else false.
     *
     * \details If function ended with an error, then descriptor will be equal nullptr.
     */
    bool open(Descriptor *pDev, const u32 devIndex);

    /**
     * \brief Close the ColibriNANO receiver.
     * \param dev - Receiver's descriptor.
     */
    void close(Descriptor dev);

    /**
     * \brief Start IQ stream.
     * \param dev - Receiver's descriptor.
     * \return if success return true, else false.
     */
    bool start(Descriptor dev, SampleRateIndex sr, pCallbackRx p, void *pUserData);

    /**
     * \brief Stop IQ stream.
     * \param dev - Receiver's descriptor.
     * \return if success return true, else false.
     */
    bool stop(Descriptor dev);

    /**
     * \brief Set preamplifier gain/attenuation value.
     * \param dev - Receiver's descriptor.
     * \param value - gain/attenuation, dB.
     * \return if success return true, else false.
     *
     * \details Preamplifier range from -31.5 up to 6 dB with 0.5 dB step.
     */
    bool setPream(Descriptor dev, f32 value);

    /**
     * \brief Set the tuning frequency for ColibriNANO receiver.
     * \param dev - Receiver's descriptor.
     * \param value - tuning frequency, Hz.
     * \return if success return true, else false.
     *
     * \details Tuning frequency range from 0.5*SampleRate Hz up to 500 MHz - 0.5 SampleRate.
     */
    bool setFrequency(Descriptor dev, u32 value);

private:
#ifndef __linux__
    HMODULE hDLL                              { nullptr };
#else
    void *hDLL                                { nullptr };
#endif
    pVersion m_version                        { nullptr };
    pInformation m_information                { nullptr };
    pDevices m_devices                        { nullptr };
    pFunc1 m_initialize                       { nullptr };
    pFunc1 m_finalize                         { nullptr };
    pOpen  m_open                             { nullptr };
    pClose m_close                            { nullptr };
    pStart m_start                            { nullptr };
    pStop  m_stop                             { nullptr };
    pSetPreamp m_setPreamp                    { nullptr };
    pSetFrequency m_setFrequency              { nullptr };
};

#endif // LIBLOADER_H
