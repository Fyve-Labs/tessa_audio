#ifndef TESSA_AUDIO_VERSION_H
#define TESSA_AUDIO_VERSION_H

namespace tessa_audio {
    // Version numbers
    constexpr const char* VERSION = "0.1.2";
    constexpr int VERSION_MAJOR = 0;
    constexpr int VERSION_MINOR = 1;
    constexpr int VERSION_PATCH = 2;
    
    // Build information
    constexpr const char* BUILD_DATE = __DATE__;
    constexpr const char* BUILD_TIME = __TIME__;
    
    // Returns full version string
    inline const char* getVersionString() {
        return VERSION;
    }
}

#endif // TESSA_AUDIO_VERSION_H 