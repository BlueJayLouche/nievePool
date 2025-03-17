#pragma once

#include "ofMain.h"
#include "ofxXmlSettings.h"

/**
 * @class ParameterManager
 * @brief Manages all effect parameters with P-Lock automation
 *
 * This class centralizes all parameter handling for the video effects,
 * including parameter lock (P-Lock) recording and playback, LFO modulation,
 * and video reactivity.
 */
class ParameterManager {
public:
    ParameterManager();
    
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
    
    // Parameter getters/setters
    float getLumakeyValue() const;
    void setLumakeyValue(float value, bool recordable = true);
    
    float getMix() const;
    void setMix(float value, bool recordable = true);
    
    float getHue() const;
    void setHue(float value, bool recordable = true);
    
    float getSaturation() const;
    void setSaturation(float value, bool recordable = true);
    
    float getBrightness() const;
    void setBrightness(float value, bool recordable = true);
    
    float getTemporalFilterMix() const;
    void setTemporalFilterMix(float value, bool recordable = true);
    
    float getTemporalFilterResonance() const;
    void setTemporalFilterResonance(float value, bool recordable = true);
    
    float getSharpenAmount() const;
    void setSharpenAmount(float value, bool recordable = true);
    
    float getXDisplace() const;
    void setXDisplace(float value, bool recordable = true);
    
    float getYDisplace() const;
    void setYDisplace(float value, bool recordable = true);
    
    float getZDisplace() const;
    void setZDisplace(float value, bool recordable = true);
    
    float getZFrequency() const;
    void setZFrequency(float value, bool recordable = true);
    
    float getXFrequency() const;
    void setXFrequency(float value, bool recordable = true);
    
    float getYFrequency() const;
    void setYFrequency(float value, bool recordable = true);
    
    float getRotate() const;
    void setRotate(float value, bool recordable = true);
    
    float getHueModulation() const;
    void setHueModulation(float value, bool recordable = true);
    
    float getHueOffset() const;
    void setHueOffset(float value, bool recordable = true);
    
    float getHueLFO() const;
    void setHueLFO(float value, bool recordable = true);
    
    int getDelayAmount() const;
    void setDelayAmount(int value, bool recordable = true);
    
    // LFO getters/setters
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
    
    // Video reactivity getters/setters
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
    
private:
    // Maximum total size of the p_lock array
    static inline const int P_LOCK_SIZE = 240;

    // Maximum number of p_locks
    static inline const int P_LOCK_NUMBER = 17;
    
    // Record parameter value to P-Lock
    void recordParameter(int paramIndex, float value);
    
    // XML settings
    ofxXmlSettings XML;
    std::string settingsFile = "settings.xml";
    
    // P-Lock system
    bool recordingEnabled = false;
    int currentStep = 0;
    float pLockValues[P_LOCK_NUMBER][P_LOCK_SIZE];
    float pLockSmoothedValues[P_LOCK_NUMBER];
    float pLockSmoothFactor = 0.5f;
    
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
    
    // Effect parameters
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
    float hueOffset = 0.0f;
    float hueLFO = 0.0f;
    int delayAmount = 0;
    
    // Added the missing frequency parameters
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
    
    // Video reactivity parameters
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
    
    // Mode flags
    bool videoReactiveMode = false;
    bool lfoAmpMode = false;
    bool lfoRateMode = false;
    
    // Helper state tracking
    bool midiActiveState[P_LOCK_NUMBER];
    bool vMidiActiveState[P_LOCK_NUMBER];
    bool lfoAmpActiveState[P_LOCK_NUMBER];
    bool lfoRateActiveState[P_LOCK_NUMBER];
};
