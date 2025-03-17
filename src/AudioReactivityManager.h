#pragma once

#include "ofMain.h"
#include "ofxXmlSettings.h"
#include "ofxFft.h"
#include "ParameterManager.h"
#include <mutex>
#include <memory>

/**
 * @class AudioReactivityManager
 * @brief Handles audio analysis and parameter modulation based on sound input
 *
 * This class analyzes audio input using ofxFft, groups frequency bands,
 * and applies audio-reactive modulation to parameters based on XML configuration.
 */
class AudioReactivityManager : public ofBaseSoundInput {
public:
    AudioReactivityManager(ParameterManager* paramManager);
    ~AudioReactivityManager();
    
    // Core methods
    void setup(bool performanceMode = false);
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
    void applyParameterValue(const std::string& paramId, float value, bool additive);
    void setupDefaultBandRanges();
    
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
