#pragma once
#include "PowerFSM.h"
#include "concurrency/Lock.h"
#include "concurrency/LockGuard.h"
#include "concurrency/OSThread.h"
#include "configuration.h"
#include "main.h"
#include "sleep.h"
#include <memory>

#ifdef HAS_I2S
#include <AudioFileSourcePROGMEM.h>
#include <AudioGeneratorRTTTL.h>
#include <AudioOutputI2S.h>
#include <ESP8266SAM.h>

#ifdef USE_XL9555
#include "ExtensionIOXL9555.hpp"
extern ExtensionIOXL9555 io;
#endif

#define AUDIO_THREAD_INTERVAL_MS 100

class AudioThread : public concurrency::OSThread
{
  public:
    enum class RtttlOwner : uint8_t { NONE, SYSTEM, EXTERNAL_NOTIFICATION };

    AudioThread() : OSThread("Audio") { initOutput(); }

    void beginRttl(const void *data, uint32_t len, RtttlOwner owner = RtttlOwner::SYSTEM)
    {
        concurrency::LockGuard guard(&rtttlLock);
        beginRttlUnlocked(data, len, owner);
    }

    bool beginRttlIfIdle(const void *data, uint32_t len, RtttlOwner owner = RtttlOwner::SYSTEM)
    {
        concurrency::LockGuard guard(&rtttlLock);
        if (isPlayingUnlocked()) {
            return false;
        }
        beginRttlUnlocked(data, len, owner);
        return true;
    }

    // Also handles actually playing the RTTTL, needs to be called in loop
    bool isPlaying()
    {
        concurrency::LockGuard guard(&rtttlLock);
        return isPlayingUnlocked();
    }

    bool isPlaying(RtttlOwner owner)
    {
        concurrency::LockGuard guard(&rtttlLock);
        const bool playing = isPlayingUnlocked();
        return playing && rtttlOwner == owner;
    }

    bool stopRtttlIfOwnedBy(RtttlOwner owner)
    {
        concurrency::LockGuard guard(&rtttlLock);
        if (rtttlOwner != owner) {
            return false;
        }
        stopUnlocked();
        return true;
    }

    void stop()
    {
        concurrency::LockGuard guard(&rtttlLock);
        stopUnlocked();
    }

    void readAloud(const char *text)
    {
        concurrency::LockGuard guard(&rtttlLock);
        if (i2sRtttl != nullptr) {
            i2sRtttl->stop();
            i2sRtttl = nullptr;
        }
        rtttlFile = nullptr;
        rtttlOwner = RtttlOwner::NONE;

#ifdef T_LORA_PAGER
        io.digitalWrite(EXPANDS_AMP_EN, HIGH);
#endif
        auto sam = std::unique_ptr<ESP8266SAM>(new ESP8266SAM);
        sam->Say(audioOut.get(), text);
        setCPUFast(false);
#ifdef T_LORA_PAGER
        io.digitalWrite(EXPANDS_AMP_EN, LOW);
#endif
    }

  protected:
    int32_t runOnce() override
    {
        canSleep = true; // Assume we should not keep the board awake

        // if (i2sRtttl != nullptr && i2sRtttl->isRunning()) {
        //     i2sRtttl->loop();
        // }
        return AUDIO_THREAD_INTERVAL_MS;
    }

  private:
    void beginRttlUnlocked(const void *data, uint32_t len, RtttlOwner owner)
    {
        if (i2sRtttl != nullptr) {
            i2sRtttl->stop();
        }
#ifdef T_LORA_PAGER
        io.digitalWrite(EXPANDS_AMP_EN, HIGH);
#endif
        setCPUFast(true);
        rtttlFile = std::unique_ptr<AudioFileSourcePROGMEM>(new AudioFileSourcePROGMEM(data, len));
        i2sRtttl = std::unique_ptr<AudioGeneratorRTTTL>(new AudioGeneratorRTTTL());
        rtttlOwner = owner;
        i2sRtttl->begin(rtttlFile.get(), audioOut.get());
    }

    bool isPlayingUnlocked()
    {
        if (i2sRtttl == nullptr) {
            return false;
        }
        const bool playing = i2sRtttl->isRunning() && i2sRtttl->loop();
        if (!playing) {
            stopUnlocked();
        }
        return playing;
    }

    void stopUnlocked()
    {
        if (i2sRtttl != nullptr) {
            i2sRtttl->stop();
            i2sRtttl = nullptr;
        }
        rtttlFile = nullptr;
        rtttlOwner = RtttlOwner::NONE;
        setCPUFast(false);
#ifdef T_LORA_PAGER
        io.digitalWrite(EXPANDS_AMP_EN, LOW);
#endif
    }

    void initOutput()
    {
        audioOut = std::unique_ptr<AudioOutputI2S>(new AudioOutputI2S(1, AudioOutputI2S::EXTERNAL_I2S));
        audioOut->SetPinout(DAC_I2S_BCK, DAC_I2S_WS, DAC_I2S_DOUT, DAC_I2S_MCLK);
        audioOut->SetGain(0.2);
    };

    std::unique_ptr<AudioGeneratorRTTTL> i2sRtttl = nullptr;
    std::unique_ptr<AudioOutputI2S> audioOut = nullptr;

    std::unique_ptr<AudioFileSourcePROGMEM> rtttlFile = nullptr;
    RtttlOwner rtttlOwner = RtttlOwner::NONE;
    concurrency::Lock rtttlLock;
};

#endif
