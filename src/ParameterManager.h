#pragma once

#include "ofMain.h"
#include "ofxXmlSettings.h"

/**
 * @class ParameterManager
 * @brief Manages all effect parameters with P-Lock automation
 *
 * This class centralizes all parameter handling for the video effects,
 * including parameter lock (P-Lock) recording and playback, LFO modulation,
 * and audio/video reactivity.
 */
class ParameterManager {
public:
    ParameterManager();

    // Performance settings
    bool performanceModeEnabled = false;
    int performanceScale = 50;     // Reduced resolution when in performance mode
    int noiseUpdateInterval = 4;   // Update noise every N frames
    bool highQualityEnabled = true;

    bool isPerformanceModeEnabled() const { return performanceModeEnabled; }
    void setPerformanceModeEnabled(bool enabled) { performanceModeEnabled = enabled; }

    int getPerformanceScale() const { return performanceScale; }
    void setPerformanceScale(int scale) { performanceScale = scale; }

    int getNoiseUpdateInterval() const { return noiseUpdateInterval; }
    void setNoiseUpdateInterval(int interval) { noiseUpdateInterval = interval; }

    bool isHighQualityEnabled() const { return highQualityEnabled; }
    void setHighQualityEnabled(bool enabled) { highQualityEnabled = enabled; }

    // Core methods
    void setup();
    void update();

    // P-Lock system
    void startRecording();
    void stopRecording();
    void clearAllLocks();
    void updatePLocks();

    // Settings management
    bool loadSettings();
    bool saveSettings();
    void resetToDefaults();

    // Get current P-Lock values (for smooth transitions)
    float getPLockValue(int paramIndex) const;

    // Toggle state getters/setters
    bool isHueInverted() const;
    void setHueInverted(bool enabled);

    bool isSaturationInverted() const;
    void setSaturationInverted(bool enabled);

    bool isBrightnessInverted() const;
    void setBrightnessInverted(bool enabled);

    bool isHorizontalMirrorEnabled() const;
    void setHorizontalMirrorEnabled(bool enabled);

    bool isVerticalMirrorEnabled() const;
    void setVerticalMirrorEnabled(bool enabled);

    bool isLumakeyInverted() const;
    void setLumakeyInverted(bool enabled);

    bool isToroidEnabled() const;
    void setToroidEnabled(bool enabled);

    bool isMirrorModeEnabled() const;
    void setMirrorModeEnabled(bool enabled);

    bool isWetModeEnabled() const;
    void setWetModeEnabled(bool enabled);

    // Video device settings getters/setters
    std::string getVideoDevicePath() const { return videoDevicePath; }
    void setVideoDevicePath(const std::string& path) { videoDevicePath = path; }

    int getVideoDeviceID() const { return videoDeviceID; }
    void setVideoDeviceID(int id) { videoDeviceID = id; }

    std::string getVideoFormat() const { return videoFormat; }
    void setVideoFormat(const std::string& format) { videoFormat = format; }

    int getVideoWidth() const { return videoWidth; }
    void setVideoWidth(int width) { videoWidth = width; }

    int getVideoHeight() const { return videoHeight; }
    void setVideoHeight(int height) { videoHeight = height; }

    int getVideoFrameRate() const { return videoFrameRate; }
    void setVideoFrameRate(int fps) { videoFrameRate = fps; }

    // OSC settings getters/setters
    int getOscPort() const { return oscPort; }
    void setOscPort(int port) { oscPort = port; }

    // --- Audio Reactivity Offset Setters ---
    // These methods allow AudioReactivityManager to set the calculated offset
    void setAudioLumakeyValueOffset(float offset);
    void setAudioMixOffset(float offset);
    void setAudioHueOffset(float offset);
    void setAudioSaturationOffset(float offset);
    void setAudioBrightnessOffset(float offset);
    void setAudioTemporalFilterMixOffset(float offset);
    void setAudioTemporalFilterResonanceOffset(float offset);
    void setAudioSharpenAmountOffset(float offset);
    void setAudioXDisplaceOffset(float offset);
    void setAudioYDisplaceOffset(float offset);
    void setAudioZDisplaceOffset(float offset);
    void setAudioRotateOffset(float offset);
    void setAudioHueModulationOffset(float offset);
    void setAudioHueOffsetOffset(float offset); // Renamed to avoid clash with base param name
    void setAudioHueLFOOffset(float offset);
    void setAudioDelayAmountOffset(int offset);
    // Frequency offsets (if needed in future, currently not mapped in default settings)
    void setAudioZFrequencyOffset(float offset);
    void setAudioXFrequencyOffset(float offset);
    void setAudioYFrequencyOffset(float offset);

    // Parameter getters (These will combine base + audio offset + P-Lock)
    float getLumakeyValue() const;
    float getMix() const;
    float getHue() const;
    float getSaturation() const;
    float getBrightness() const;
    float getTemporalFilterMix() const;
    float getTemporalFilterResonance() const;
    float getSharpenAmount() const;
    float getXDisplace() const;
    float getYDisplace() const;
    float getZDisplace() const;
    float getZFrequency() const;
    float getXFrequency() const;
    float getYFrequency() const;
    float getRotate() const;
    float getHueModulation() const;
    float getHueOffset() const;
    float getHueLFO() const;
    int getDelayAmount() const;

    // Base Parameter setters (Only set the base value)
    void setLumakeyValue(float value, bool recordable = true);
    void setMix(float value, bool recordable = true);
    void setHue(float value, bool recordable = true);
    void setSaturation(float value, bool recordable = true);
    void setBrightness(float value, bool recordable = true);
    void setTemporalFilterMix(float value, bool recordable = true);
    void setTemporalFilterResonance(float value, bool recordable = true);
    void setSharpenAmount(float value, bool recordable = true);
    void setXDisplace(float value, bool recordable = true);
    void setYDisplace(float value, bool recordable = true);
    void setZDisplace(float value, bool recordable = true);
    void setZFrequency(float value, bool recordable = true);
    void setXFrequency(float value, bool recordable = true);
    void setYFrequency(float value, bool recordable = true);
    void setRotate(float value, bool recordable = true);
    void setHueModulation(float value, bool recordable = true);
    void setHueOffset(float value, bool recordable = true);
    void setHueLFO(float value, bool recordable = true);
    void setDelayAmount(int value, bool recordable = true);

    // LFO getters/setters (These remain separate)
    float getXLfoAmp() const;
    void setXLfoAmp(float value);
    float getXLfoRate() const;
    void setXLfoRate(float value);
    float getYLfoAmp() const;
    void setYLfoAmp(float value);
    float getYLfoRate() const;
    void setYLfoRate(float value);
    float getZLfoAmp() const;
    void setZLfoAmp(float value);
    float getZLfoRate() const;
    void setZLfoRate(float value);
    float getRotateLfoAmp() const;
    void setRotateLfoAmp(float value);
    float getRotateLfoRate() const;
    void setRotateLfoRate(float value);

    // Video reactivity getters/setters (Keep for now, maybe remove later?)
    float getVLumakeyValue() const;
    void setVLumakeyValue(float value);
    float getVMix() const;
    void setVMix(float value);
    float getVHue() const;
    void setVHue(float value);
    float getVSaturation() const;
    void setVSaturation(float value);
    float getVBrightness() const;
    void setVBrightness(float value);
    float getVTemporalFilterMix() const;
    void setVTemporalFilterMix(float value);
    float getVTemporalFilterResonance() const;
    void setVTemporalFilterResonance(float value);
    float getVSharpenAmount() const;
    void setVSharpenAmount(float value);
    float getVXDisplace() const;
    void setVXDisplace(float value);
    float getVYDisplace() const;
    void setVYDisplace(float value);
    float getVZDisplace() const;
    void setVZDisplace(float value);
    float getVRotate() const;
    void setVRotate(float value);
    float getVHueModulation() const;
    void setVHueModulation(float value);
    float getVHueOffset() const;
    void setVHueOffset(float value);
    float getVHueLFO() const;
    void setVHueLFO(float value);

    // Mode getters/setters
    bool isVideoReactiveEnabled() const;
    void setVideoReactiveEnabled(bool enabled);
    bool isRecordingEnabled() const;
    void setRecordingEnabled(bool enabled);
    bool isLfoAmpModeEnabled() const;
    void setLfoAmpModeEnabled(bool enabled);
    bool isLfoRateModeEnabled() const;
    void setLfoRateModeEnabled(bool enabled);

    // XML settings
    void loadFromXml(ofxXmlSettings& xml);
    void saveToXml(ofxXmlSettings& xml) const;

    // Mapping Getters
    int getMidiChannel(const std::string& paramId) const;
    int getMidiControl(const std::string& paramId) const;
    std::string getOscAddress(const std::string& paramId) const;
    const std::vector<std::string>& getAllParameterIds() const;

private:
    // Helper Functions
    void initializeParameterMaps(); // Declaration added

    // Enum for P-Lock indices for clarity and safety
    enum PLockIndex {
        LUMAKEY_VALUE = 0, MIX = 1, HUE = 2, SATURATION = 3, BRIGHTNESS = 4,
        TEMPORAL_FILTER_MIX = 5, TEMPORAL_FILTER_RESONANCE = 6, SHARPEN_AMOUNT = 7,
        X_DISPLACE = 8, Y_DISPLACE = 9, Z_DISPLACE = 10, ROTATE = 11,
        HUE_MODULATION = 12, HUE_OFFSET = 13, HUE_LFO = 14, DELAY_AMOUNT = 15,
        UNUSED_16 = 16
    };

    // Parameter Info & Mappings
    std::vector<std::string> parameterIds;
    std::map<std::string, int> midiChannels;
    std::map<std::string, int> midiControls;
    std::map<std::string, std::string> oscAddresses;

    // P-Lock system constants
    static inline const int P_LOCK_SIZE = 240;
    static inline const int P_LOCK_NUMBER = 17;

    // P-Lock system variables
    bool recordingEnabled = false;
    int currentStep = 0;
    float pLockValues[P_LOCK_NUMBER][P_LOCK_SIZE];
    float pLockSmoothedValues[P_LOCK_NUMBER];
    float pLockSmoothFactor = 0.5f;

    // Record parameter value to P-Lock helper
    void recordParameter(int paramIndex, float value);

    // XML settings
    ofxXmlSettings XML;
    std::string settingsFile = "settings.xml";

    // Video device settings
    std::string videoDevicePath = "/dev/video0";
    int videoDeviceID = 0;
    std::string videoFormat = "YUYV";
    int videoWidth = 640;
    int videoHeight = 480;
    int videoFrameRate = 30;

    // Toggle states
    bool hueInvert = false;
    bool saturationInvert = false;
    bool brightnessInvert = false;
    bool horizontalMirror = false;
    bool verticalMirror = false;
    bool lumakeyInvert = false;
    bool toroidEnabled = false;
    bool mirrorModeEnabled = false;
    bool wetModeEnabled = true;

    // Effect parameters (Base values set by MIDI/OSC/Keyboard/Defaults)
    float lumakeyValue = 0.0f;
    float mix = 0.0f;
    float hue = 1.0f;
    float saturation = 1.0f;
    float brightness = 1.0f;
    float temporalFilterMix = 0.0f;
    float temporalFilterResonance = 0.0f;
    float sharpenAmount = 0.0f;
    float xDisplace = 0.0f;
    float yDisplace = 0.0f;
    float zDisplace = 1.0f;
    float rotate = 0.0f;
    float hueModulation = 1.0f;
    float hueOffset = 0.0f; // Base value for hue offset
    float hueLFO = 0.0f;
    int delayAmount = 0;
    float zFrequency = 0.03f;
    float xFrequency = 0.015f;
    float yFrequency = 0.02f;

    // LFO parameters
    float xLfoAmp = 0.0f;
    float xLfoRate = 0.0f;
    float yLfoAmp = 0.0f;
    float yLfoRate = 0.0f;
    float zLfoAmp = 0.0f;
    float zLfoRate = 0.0f;
    float rotateLfoAmp = 0.0f;
    float rotateLfoRate = 0.0f;

    // Video reactivity parameters (Potentially unused)
    float vLumakeyValue = 0.0f;
    float vMix = 0.0f;
    float vHue = 0.0f;
    float vSaturation = 0.0f;
    float vBrightness = 0.0f;
    float vTemporalFilterMix = 0.0f;
    float vTemporalFilterResonance = 0.0f;
    float vSharpenAmount = 0.0f;
    float vXDisplace = 0.0f;
    float vYDisplace = 0.0f;
    float vZDisplace = 0.0f;
    float vRotate = 0.0f;
    float vHueModulation = 0.0f;
    float vHueOffset = 0.0f;
    float vHueLFO = 0.0f;

    // --- Audio Reactivity Offset Storage ---
    // Stores the offset calculated by AudioReactivityManager
    float audioLumakeyValueOffset = 0.0f;
    float audioMixOffset = 0.0f;
    float audioHueOffset = 0.0f;
    float audioSaturationOffset = 0.0f;
    float audioBrightnessOffset = 0.0f;
    float audioTemporalFilterMixOffset = 0.0f;
    float audioTemporalFilterResonanceOffset = 0.0f;
    float audioSharpenAmountOffset = 0.0f;
    float audioXDisplaceOffset = 0.0f;
    float audioYDisplaceOffset = 0.0f;
    float audioZDisplaceOffset = 0.0f;
    float audioRotateOffset = 0.0f;
    float audioHueModulationOffset = 0.0f;
    float audioHueOffsetOffset = 0.0f; // Offset for the hueOffset parameter
    float audioHueLFOOffset = 0.0f;
    int   audioDelayAmountOffset = 0;
    float audioZFrequencyOffset = 0.0f;
    float audioXFrequencyOffset = 0.0f;
    float audioYFrequencyOffset = 0.0f;

    // Mode flags
    bool videoReactiveMode = false;
    bool lfoAmpMode = false;
    bool lfoRateMode = false;

    // Helper state tracking (May need review if v* params are removed)
    bool midiActiveState[P_LOCK_NUMBER];
    bool vMidiActiveState[P_LOCK_NUMBER];
    bool lfoAmpActiveState[P_LOCK_NUMBER];
    bool lfoRateActiveState[P_LOCK_NUMBER];

    // OSC settings
    int oscPort = 9000; // Default OSC listening port
};
