#pragma once

#include "ofMain.h"
#include "ofxXmlSettings.h"
#include "ofxFft.h"
#include "ParameterManager.h"
#include <mutex>
#include <memory>

/**
 * @class AudioReactivityManager
 * @brief Handles audio input analysis and parameter modulation based on frequency bands
 *
 * This class performs real-time audio analysis using FFT, divides the frequency spectrum
 * into configurable bands, and maps these bands to effect parameters. This creates
 * audio-reactive visuals where the video effects respond to the audio input.
 *
 * Features:
 * - Multiple audio input device support with hot-swapping
 * - Configurable frequency bands (defaults to 8 bands)
 * - Customizable mapping of frequency bands to effect parameters
 * - Sensitivity and smoothing controls
 * - Normalization to handle varying input levels
 * - Thread-safe design for audio processing
 * - XML configuration for saving and loading settings
 *
 * Usage example:
 * @code
 * // Initialize with parameter manager
 * audioManager = std::make_unique<AudioReactivityManager>(paramManager.get());
 * audioManager->setup();
 *
 * // Add a mapping from bass band to z-displacement
 * AudioReactivityManager::BandMapping bassMapping;
 * bassMapping.band = 0;  // Sub bass
 * bassMapping.paramId = "z_displace";
 * bassMapping.scale = 0.02f;
 * bassMapping.min = -0.02f;
 * bassMapping.max = 0.02f;
 * bassMapping.additive = true;
 * audioManager->addMapping(bassMapping);
 * @endcode
 */

class AudioReactivityManager : public ofBaseSoundInput {
public:
    AudioReactivityManager(ParameterManager* paramManager);
    ~AudioReactivityManager();
    
    // Setup and lifecycle
    void setup(ParameterManager* paramManager, bool performanceMode = false); // Added paramManager argument
    void update();
    void exit();
    
    // Audio input callback (must be public since we inherit from ofBaseSoundInput)
    void audioIn(ofSoundBuffer &input) override;
    
    // Audio analysis info
    float getBand(int band) const;
    int getNumBands() const;
    std::vector<float> getAllBands() const;
    float getAudioInputLevel() const;
    
    // Toggle controls
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    void setNormalizationEnabled(bool enabled);
    bool isNormalizationEnabled() const;
    
    // Settings
    void setSensitivity(float sensitivity);
    float getSensitivity() const;
    
    void setSmoothing(float smoothing);
    float getSmoothing() const;
    
    // Audio device management
    void listAudioDevices();
    std::vector<std::string> getAudioDeviceList() const;
    int getCurrentDeviceIndex() const;
    std::string getCurrentDeviceName() const;
    bool selectAudioDevice(int deviceIndex);
    bool selectAudioDevice(const std::string& deviceName);
    void setupAudioInput();
    void closeAudioInput();
    
    // Band mapping control
    struct BandMapping {
        int band;               // Which frequency band
        std::string paramId;    // Which parameter to affect
        float scale;            // Scaling factor
        float min;              // Minimum value
        float max;              // Maximum value
        bool additive;          // Add to (true) or replace (false) parameter value
    };
    
    void addMapping(const BandMapping& mapping);
    void removeMapping(int index);
    void clearMappings();
    
    std::vector<BandMapping> getMappings() const;
    
    // XML configuration
    void loadFromXml(ofxXmlSettings& xml);
    void saveToXml(ofxXmlSettings& xml) const;
    
private:
    // Audio analysis
    void analyzeAudio();
    void groupBands();
    void applyMappings();
    
    
    // Helper methods
    float getParameterValue(const std::string& paramId) const;
    void applyParameterValue(const std::string& paramId, float value, bool additive);
    void setupDefaultBandRanges();
    void addDefaultMappings(); // Added declaration for default mappings helper
    
    // Audio settings
    bool enabled;
    bool normalizationEnabled;
    float sensitivity;
    float smoothing;
    int numBands;
    int bufferSize;
    
    // FFT and band data
    static constexpr size_t kNumFFTBins = 1024; // Power of 2 for optimal FFT performance
    
    // ofxFft specific members
    std::shared_ptr<ofxFft> fft;
    std::vector<float> audioBuffer;
    
    // FFT data storage
    std::vector<float> fftSpectrum;
    std::vector<float> fftSmoothed;
    
    // Band data
    std::vector<float> bands;
    std::vector<float> smoothedBands;
    
    // Audio input handling
    ofSoundStream soundStream;
    int currentDeviceIndex;
    std::vector<ofSoundDevice> deviceList;
    bool audioInputInitialized;
    float audioInputLevel;
    
    // Frequency bands configuration
    struct BandRange {
        int minBin;
        int maxBin;
    };
    std::vector<BandRange> bandRanges;
    
    // Parameter mappings
    std::vector<BandMapping> mappings;
    
    // Reference to parameter manager
    ParameterManager* paramManager;
    
    // Thread safety
    std::mutex audioMutex;
};
