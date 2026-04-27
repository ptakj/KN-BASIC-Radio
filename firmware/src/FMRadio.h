#pragma once
#include <RDA5807.h>
#include <stdint.h>

/// Wrapper around the RDA5807 FM tuner chip.
/// Manages frequency, volume, and RDS data retrieval.
class FMRadio {
public:
    static constexpr uint8_t  VOLUME_MIN = 0;
    static constexpr uint8_t  VOLUME_MAX = 15;

    /// Initialise the chip and tune to startFreq with given volume.
    void     begin(uint16_t startFreq, uint8_t startVolume = 8);

    /// Tune to a frequency (MHz × 100, clamped to valid FM band).
    void     setFrequency(uint16_t freq);
    uint16_t getFrequency() const;

    /// Set absolute volume (clamped to VOLUME_MIN…VOLUME_MAX).
    void    setVolume(uint8_t vol);
    /// Step volume up (+1) or down (-1), clamped at limits.
    void    volumeStep(int8_t direction);
    uint8_t getVolume() const;

    int8_t getRSSI();

    /// Copy the RDS Program Service name (8 chars) into buffer.
    /// Returns true when valid data was available.
    bool getRDSStationName(char* buffer, uint8_t bufSize);

    /// Copy the RDS Radio Text (up to 64 chars) into buffer.
    /// Returns true when valid data was available.
    bool getRDSProgramInfo(char* buffer, uint8_t bufSize);

private:
    RDA5807  _radio;
    uint16_t _frequency = 0;
    uint8_t  _volume    = 8;

    static constexpr uint16_t FREQ_MIN = 8700;   // 87.0 MHz
    static constexpr uint16_t FREQ_MAX = 10800;  // 108.0 MHz
};
