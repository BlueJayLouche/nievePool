#include "ParameterManager.h"

ParameterManager::ParameterManager() {
    // Initialize P-Lock arrays with zeros
    for (int i = 0; i < P_LOCK_NUMBER; i++) {
        pLockSmoothedValues[i] = 0.0f;
        for (int j = 0; j < P_LOCK_SIZE; j++) {
            pLockValues[i][j] = 0.0f;
        }
        
        // Initialize state tracking arrays
        midiActiveState[i] = false;
        vMidiActiveState[i] = false;
        lfoAmpActiveState[i] = false;
        lfoRateActiveState[i] = false;
    }
}

void ParameterManager::setup() {
    // Try to load settings from XML, fall back to defaults if not found
    if (!loadSettings()) {
        resetToDefaults();
    }
}

void ParameterManager::update() {
    // Update P-Lock system (automated parameter changes)
    updatePLocks();
}

void ParameterManager::startRecording() {
    recordingEnabled = true;
    
    // Copy current P-Lock values to all steps
    for (int i = 0; i < P_LOCK_NUMBER; i++) {
        float currentValue = pLockValues[i][currentStep];
        for (int j = 0; j < P_LOCK_SIZE; j++) {
            pLockValues[i][j] = currentValue;
        }
    }
}

void ParameterManager::stopRecording() {
    recordingEnabled = false;
}

void ParameterManager::clearAllLocks() {
    for (int i = 0; i < P_LOCK_NUMBER; i++) {
        pLockSmoothedValues[i] = 0.0f;
        for (int j = 0; j < P_LOCK_SIZE; j++) {
            pLockValues[i][j] = 0.0f;
        }
    }
}

void ParameterManager::updatePLocks() {
    // Apply smoothing to all parameters
    for (int i = 0; i < P_LOCK_NUMBER; i++) {
        // Apply smoothing formula: smoothed = current * (1-factor) + previous * factor
        pLockSmoothedValues[i] = pLockValues[i][currentStep] * (1.0f - pLockSmoothFactor)
                              + pLockSmoothedValues[i] * pLockSmoothFactor;
        
        // Eliminate very small values to prevent jitter
        if (abs(pLockSmoothedValues[i]) < 0.01) {
            pLockSmoothedValues[i] = 0.0f;
        }
    }
    
    // Increment step if recording is enabled
    if (recordingEnabled) {
        currentStep = (currentStep + 1) % P_LOCK_SIZE;
    }
}

bool ParameterManager::loadSettings() {
    if (XML.loadFile(ofToDataPath(settingsFile))) {
        loadFromXml(XML);
        return true;
    }
    return false;
}

bool ParameterManager::saveSettings() {
    saveToXml(XML);
    return XML.saveFile(ofToDataPath(settingsFile));
}

void ParameterManager::resetToDefaults() {
    // Reset all parameters to defaults
    
    // Toggle states
    hueInvert = false;
    saturationInvert = false;
    brightnessInvert = false;
    horizontalMirror = false;
    verticalMirror = false;
    lumakeyInvert = false;
    toroidEnabled = false;
    mirrorModeEnabled = false;
    wetModeEnabled = true;
    
    // Effect parameters
    lumakeyValue = 0.0f;
    mix = 0.0f;
    hue = 1.0f;
    saturation = 1.0f;
    brightness = 1.0f;
    temporalFilterMix = 0.0f;
    temporalFilterResonance = 0.0f;
    sharpenAmount = 0.0f;
    xDisplace = 0.0f;
    yDisplace = 0.0f;
    zDisplace = 1.0f;
    rotate = 0.0f;
    hueModulation = 1.0f;
    hueOffset = 0.0f;
    hueLFO = 0.0f;
    delayAmount = 0;
    
    // LFO parameters
    xLfoAmp = 0.0f;
    xLfoRate = 0.0f;
    yLfoAmp = 0.0f;
    yLfoRate = 0.0f;
    zLfoAmp = 0.0f;
    zLfoRate = 0.0f;
    rotateLfoAmp = 0.0f;
    rotateLfoRate = 0.0f;
    
    // Video reactivity parameters
    vLumakeyValue = 0.0f;
    vMix = 0.0f;
    vHue = 0.0f;
    vSaturation = 0.0f;
    vBrightness = 0.0f;
    vTemporalFilterMix = 0.0f;
    vTemporalFilterResonance = 0.0f;
    vSharpenAmount = 0.0f;
    vXDisplace = 0.0f;
    vYDisplace = 0.0f;
    vZDisplace = 0.0f;
    vRotate = 0.0f;
    vHueModulation = 0.0f;
    vHueOffset = 0.0f;
    vHueLFO = 0.0f;
    
    // Mode flags
    videoReactiveMode = false;
    lfoAmpMode = false;
    lfoRateMode = false;
    
    clearAllLocks();
}

void ParameterManager::recordParameter(int paramIndex, float value) {
    if (recordingEnabled && paramIndex >= 0 && paramIndex < P_LOCK_NUMBER) {
        pLockValues[paramIndex][currentStep] = value;
    }
}

float ParameterManager::getPLockValue(int paramIndex) const {
    if (paramIndex >= 0 && paramIndex < P_LOCK_NUMBER) {
        return pLockSmoothedValues[paramIndex];
    }
    return 0.0f;
}

void ParameterManager::loadFromXml(ofxXmlSettings& xml) {
    // Load toggle states
    hueInvert = xml.getValue("toggles:hueInvert", false);
    saturationInvert = xml.getValue("toggles:saturationInvert", false);
    brightnessInvert = xml.getValue("toggles:brightnessInvert", false);
    horizontalMirror = xml.getValue("toggles:horizontalMirror", false);
    verticalMirror = xml.getValue("toggles:verticalMirror", false);
    lumakeyInvert = xml.getValue("toggles:lumakeyInvert", false);
    toroidEnabled = xml.getValue("toggles:toroidEnabled", false);
    mirrorModeEnabled = xml.getValue("toggles:mirrorModeEnabled", false);
    wetModeEnabled = xml.getValue("toggles:wetModeEnabled", true);
    
    // Load effect parameters
    lumakeyValue = xml.getValue("parameters:lumakeyValue", 0.0f);
    mix = xml.getValue("parameters:mix", 0.0f);
    hue = xml.getValue("parameters:hue", 1.0f);
    saturation = xml.getValue("parameters:saturation", 1.0f);
    brightness = xml.getValue("parameters:brightness", 1.0f);
    temporalFilterMix = xml.getValue("parameters:temporalFilterMix", 0.0f);
    temporalFilterResonance = xml.getValue("parameters:temporalFilterResonance", 0.0f);
    sharpenAmount = xml.getValue("parameters:sharpenAmount", 0.0f);
    xDisplace = xml.getValue("parameters:xDisplace", 0.0f);
    yDisplace = xml.getValue("parameters:yDisplace", 0.0f);
    zDisplace = xml.getValue("parameters:zDisplace", 1.0f);
    rotate = xml.getValue("parameters:rotate", 0.0f);
    hueModulation = xml.getValue("parameters:hueModulation", 1.0f);
    hueOffset = xml.getValue("parameters:hueOffset", 0.0f);
    hueLFO = xml.getValue("parameters:hueLFO", 0.0f);
    delayAmount = xml.getValue("parameters:delayAmount", 0);
    
    // Load LFO parameters
    xLfoAmp = xml.getValue("lfo:xLfoAmp", 0.0f);
    xLfoRate = xml.getValue("lfo:xLfoRate", 0.0f);
    yLfoAmp = xml.getValue("lfo:yLfoAmp", 0.0f);
    yLfoRate = xml.getValue("lfo:yLfoRate", 0.0f);
    zLfoAmp = xml.getValue("lfo:zLfoAmp", 0.0f);
    zLfoRate = xml.getValue("lfo:zLfoRate", 0.0f);
    rotateLfoAmp = xml.getValue("lfo:rotateLfoAmp", 0.0f);
    rotateLfoRate = xml.getValue("lfo:rotateLfoRate", 0.0f);
    
    // Load video reactivity parameters
    vLumakeyValue = xml.getValue("videoReactive:vLumakeyValue", 0.0f);
    vMix = xml.getValue("videoReactive:vMix", 0.0f);
    vHue = xml.getValue("videoReactive:vHue", 0.0f);
    vSaturation = xml.getValue("videoReactive:vSaturation", 0.0f);
    vBrightness = xml.getValue("videoReactive:vBrightness", 0.0f);
    vTemporalFilterMix = xml.getValue("videoReactive:vTemporalFilterMix", 0.0f);
    vTemporalFilterResonance = xml.getValue("videoReactive:vTemporalFilterResonance", 0.0f);
    vSharpenAmount = xml.getValue("videoReactive:vSharpenAmount", 0.0f);
    vXDisplace = xml.getValue("videoReactive:vXDisplace", 0.0f);
    vYDisplace = xml.getValue("videoReactive:vYDisplace", 0.0f);
    vZDisplace = xml.getValue("videoReactive:vZDisplace", 0.0f);
    vRotate = xml.getValue("videoReactive:vRotate", 0.0f);
    vHueModulation = xml.getValue("videoReactive:vHueModulation", 0.0f);
    vHueOffset = xml.getValue("videoReactive:vHueOffset", 0.0f);
    vHueLFO = xml.getValue("videoReactive:vHueLFO", 0.0f);
    
    // Load mode flags
    videoReactiveMode = xml.getValue("modes:videoReactiveMode", false);
    lfoAmpMode = xml.getValue("modes:lfoAmpMode", false);
    lfoRateMode = xml.getValue("modes:lfoRateMode", false);
    
    // Load P-Lock data if available
    if (xml.tagExists("plocks")) {
        xml.pushTag("plocks");
        
        // Load smooth factor
        pLockSmoothFactor = xml.getValue("smoothFactor", 0.5f);
        
        if (xml.tagExists("locks")) {
            xml.pushTag("locks");
            
            for (int i = 0; i < P_LOCK_NUMBER; i++) {
                std::string lockTag = "lock" + ofToString(i);
                
                if (xml.tagExists(lockTag)) {
                    xml.pushTag(lockTag);
                    
                    std::string valuesStr = xml.getValue("values", "");
                    if (!valuesStr.empty()) {
                        std::vector<std::string> values = ofSplitString(valuesStr, ",");
                        
                        for (int j = 0; j < std::min((int)values.size(), P_LOCK_SIZE); j++) {
                            pLockValues[i][j] = ofToFloat(values[j]);
                        }
                    }
                    
                    xml.popTag(); // pop lockTag
                }
            }
            
            xml.popTag(); // pop locks
        }
        
        xml.popTag(); // pop plocks
    }
}

void ParameterManager::saveToXml(ofxXmlSettings& xml) const {
    // Save toggle states
    xml.setValue("toggles:hueInvert", hueInvert);
    xml.setValue("toggles:saturationInvert", saturationInvert);
    xml.setValue("toggles:brightnessInvert", brightnessInvert);
    xml.setValue("toggles:horizontalMirror", horizontalMirror);
    xml.setValue("toggles:verticalMirror", verticalMirror);
    xml.setValue("toggles:lumakeyInvert", lumakeyInvert);
    xml.setValue("toggles:toroidEnabled", toroidEnabled);
    xml.setValue("toggles:mirrorModeEnabled", mirrorModeEnabled);
    xml.setValue("toggles:wetModeEnabled", wetModeEnabled);
    
    // Save effect parameters
    xml.setValue("parameters:lumakeyValue", lumakeyValue);
    xml.setValue("parameters:mix", mix);
    xml.setValue("parameters:hue", hue);
    xml.setValue("parameters:saturation", saturation);
    xml.setValue("parameters:brightness", brightness);
    xml.setValue("parameters:temporalFilterMix", temporalFilterMix);
    xml.setValue("parameters:temporalFilterResonance", temporalFilterResonance);
    xml.setValue("parameters:sharpenAmount", sharpenAmount);
    xml.setValue("parameters:xDisplace", xDisplace);
    xml.setValue("parameters:yDisplace", yDisplace);
    xml.setValue("parameters:zDisplace", zDisplace);
    xml.setValue("parameters:rotate", rotate);
    xml.setValue("parameters:hueModulation", hueModulation);
    xml.setValue("parameters:hueOffset", hueOffset);
    xml.setValue("parameters:hueLFO", hueLFO);
    xml.setValue("parameters:delayAmount", delayAmount);
    
    // Save LFO parameters
    xml.setValue("lfo:xLfoAmp", xLfoAmp);
    xml.setValue("lfo:xLfoRate", xLfoRate);
    xml.setValue("lfo:yLfoAmp", yLfoAmp);
    xml.setValue("lfo:yLfoRate", yLfoRate);
    xml.setValue("lfo:zLfoAmp", zLfoAmp);
    xml.setValue("lfo:zLfoRate", zLfoRate);
    xml.setValue("lfo:rotateLfoAmp", rotateLfoAmp);
    xml.setValue("lfo:rotateLfoRate", rotateLfoRate);
    
    // Save video reactivity parameters
    xml.setValue("videoReactive:vLumakeyValue", vLumakeyValue);
    xml.setValue("videoReactive:vMix", vMix);
    xml.setValue("videoReactive:vHue", vHue);
    xml.setValue("videoReactive:vSaturation", vSaturation);
    xml.setValue("videoReactive:vBrightness", vBrightness);
    xml.setValue("videoReactive:vTemporalFilterMix", vTemporalFilterMix);
    xml.setValue("videoReactive:vTemporalFilterResonance", vTemporalFilterResonance);
    xml.setValue("videoReactive:vSharpenAmount", vSharpenAmount);
    xml.setValue("videoReactive:vXDisplace", vXDisplace);
    xml.setValue("videoReactive:vYDisplace", vYDisplace);
    xml.setValue("videoReactive:vZDisplace", vZDisplace);
    xml.setValue("videoReactive:vRotate", vRotate);
    xml.setValue("videoReactive:vHueModulation", vHueModulation);
    xml.setValue("videoReactive:vHueOffset", vHueOffset);
    xml.setValue("videoReactive:vHueLFO", vHueLFO);
    
    // Save mode flags
    xml.setValue("modes:videoReactiveMode", videoReactiveMode);
    xml.setValue("modes:lfoAmpMode", lfoAmpMode);
    xml.setValue("modes:lfoRateMode", lfoRateMode);
    
    // Save P-Lock data
    xml.setValue("plocks:smoothFactor", pLockSmoothFactor);
    
    if (xml.tagExists("plocks")) {
        xml.removeTag("plocks");
    }
    xml.addTag("plocks");
    xml.pushTag("plocks");
    
    xml.addTag("locks");
    xml.pushTag("locks");
    
    for (int i = 0; i < P_LOCK_NUMBER; i++) {
        std::string lockTag = "lock" + ofToString(i);
        xml.addTag(lockTag);
        xml.pushTag(lockTag);
        
        std::string valuesStr = "";
        for (int j = 0; j < P_LOCK_SIZE; j++) {
            valuesStr += ofToString(pLockValues[i][j]);
            if (j < P_LOCK_SIZE - 1) {
                valuesStr += ",";
            }
        }
        
        xml.setValue("values", valuesStr);
        xml.popTag();
    }
    
    xml.popTag(); // pop locks
    xml.popTag(); // pop plocks
}

// Get/Set Methods Implementation
// Toggle state getters/setters
bool ParameterManager::isHueInverted() const { return hueInvert; }
void ParameterManager::setHueInverted(bool enabled) { hueInvert = enabled; }

bool ParameterManager::isSaturationInverted() const { return saturationInvert; }
void ParameterManager::setSaturationInverted(bool enabled) { saturationInvert = enabled; }

bool ParameterManager::isBrightnessInverted() const { return brightnessInvert; }
void ParameterManager::setBrightnessInverted(bool enabled) { brightnessInvert = enabled; }

bool ParameterManager::isHorizontalMirrorEnabled() const { return horizontalMirror; }
void ParameterManager::setHorizontalMirrorEnabled(bool enabled) { horizontalMirror = enabled; }

bool ParameterManager::isVerticalMirrorEnabled() const { return verticalMirror; }
void ParameterManager::setVerticalMirrorEnabled(bool enabled) { verticalMirror = enabled; }

bool ParameterManager::isLumakeyInverted() const { return lumakeyInvert; }
void ParameterManager::setLumakeyInverted(bool enabled) { lumakeyInvert = enabled; }

bool ParameterManager::isToroidEnabled() const { return toroidEnabled; }
void ParameterManager::setToroidEnabled(bool enabled) { toroidEnabled = enabled; }

bool ParameterManager::isMirrorModeEnabled() const { return mirrorModeEnabled; }
void ParameterManager::setMirrorModeEnabled(bool enabled) { mirrorModeEnabled = enabled; }

bool ParameterManager::isWetModeEnabled() const { return wetModeEnabled; }
void ParameterManager::setWetModeEnabled(bool enabled) { wetModeEnabled = enabled; }

// Parameter getters/setters
float ParameterManager::getLumakeyValue() const { return lumakeyValue + getPLockValue(0); }
void ParameterManager::setLumakeyValue(float value, bool recordable) {
    lumakeyValue = value;
    if (recordable) {
        recordParameter(0, value);
    }
}

float ParameterManager::getMix() const { return mix + getPLockValue(1); }
void ParameterManager::setMix(float value, bool recordable) {
    mix = value;
    if (recordable) {
        recordParameter(1, value);
    }
}

float ParameterManager::getHue() const { return hue * (1.0f + getPLockValue(2)); }
void ParameterManager::setHue(float value, bool recordable) {
    hue = value;
    if (recordable) {
        recordParameter(2, value);
    }
}

float ParameterManager::getSaturation() const { return saturation * (1.0f + getPLockValue(3)); }
void ParameterManager::setSaturation(float value, bool recordable) {
    saturation = value;
    if (recordable) {
        recordParameter(3, value);
    }
}

float ParameterManager::getBrightness() const { return brightness * (1.0f + getPLockValue(4)); }
void ParameterManager::setBrightness(float value, bool recordable) {
    brightness = value;
    if (recordable) {
        recordParameter(4, value);
    }
}

float ParameterManager::getTemporalFilterMix() const { return temporalFilterMix + getPLockValue(5); }
void ParameterManager::setTemporalFilterMix(float value, bool recordable) {
    temporalFilterMix = value;
    if (recordable) {
        recordParameter(5, value);
    }
}

float ParameterManager::getTemporalFilterResonance() const { return temporalFilterResonance + getPLockValue(6); }
void ParameterManager::setTemporalFilterResonance(float value, bool recordable) {
    temporalFilterResonance = value;
    if (recordable) {
        recordParameter(6, value);
    }
}

float ParameterManager::getSharpenAmount() const { return sharpenAmount + getPLockValue(7); }
void ParameterManager::setSharpenAmount(float value, bool recordable) {
    sharpenAmount = value;
    if (recordable) {
        recordParameter(7, value);
    }
}

float ParameterManager::getXDisplace() const { return xDisplace + getPLockValue(8); }
void ParameterManager::setXDisplace(float value, bool recordable) {
    xDisplace = value;
    if (recordable) {
        recordParameter(8, value);
    }
}

float ParameterManager::getYDisplace() const { return yDisplace + getPLockValue(9); }
void ParameterManager::setYDisplace(float value, bool recordable) {
    yDisplace = value;
    if (recordable) {
        recordParameter(9, value);
    }
}

float ParameterManager::getZDisplace() const { return zDisplace * (1.0f + getPLockValue(10)); }
void ParameterManager::setZDisplace(float value, bool recordable) {
    zDisplace = value;
    if (recordable) {
        recordParameter(10, value);
    }
}

// Frequency getters/setters
float ParameterManager::getZFrequency() const {
    // Using parameter index 3 for zFrequency based on the original code's organization
    return zFrequency + getPLockValue(3);
}

void ParameterManager::setZFrequency(float value, bool recordable) {
    zFrequency = value;
    if (recordable) {
        recordParameter(3, value);
    }
}

float ParameterManager::getXFrequency() const {
    // Using parameter index 4 for xFrequency
    return xFrequency + getPLockValue(4);
}

void ParameterManager::setXFrequency(float value, bool recordable) {
    xFrequency = value;
    if (recordable) {
        recordParameter(4, value);
    }
}

float ParameterManager::getYFrequency() const {
    // Using parameter index 5 for yFrequency
    return yFrequency + getPLockValue(5);
}

void ParameterManager::setYFrequency(float value, bool recordable) {
    yFrequency = value;
    if (recordable) {
        recordParameter(5, value);
    }
}

float ParameterManager::getRotate() const { return rotate + getPLockValue(11); }
void ParameterManager::setRotate(float value, bool recordable) {
    rotate = value;
    if (recordable) {
        recordParameter(11, value);
    }
}

float ParameterManager::getHueModulation() const { return hueModulation * (1.0f - getPLockValue(12)); }
void ParameterManager::setHueModulation(float value, bool recordable) {
    hueModulation = value;
    if (recordable) {
        recordParameter(12, value);
    }
}

float ParameterManager::getHueOffset() const { return hueOffset + getPLockValue(13); }
void ParameterManager::setHueOffset(float value, bool recordable) {
    hueOffset = value;
    if (recordable) {
        recordParameter(13, value);
    }
}

float ParameterManager::getHueLFO() const { return hueLFO + getPLockValue(14); }
void ParameterManager::setHueLFO(float value, bool recordable) {
    hueLFO = value;
    if (recordable) {
        recordParameter(14, value);
    }
}

int ParameterManager::getDelayAmount() const { return delayAmount + (int)(getPLockValue(15) * (P_LOCK_SIZE - 1.0f)); }
void ParameterManager::setDelayAmount(int value, bool recordable) {
    delayAmount = value;
    if (recordable) {
        recordParameter(15, static_cast<float>(value) / (P_LOCK_SIZE - 1.0f));
    }
}

// LFO getters/setters
float ParameterManager::getXLfoAmp() const { return xLfoAmp; }
void ParameterManager::setXLfoAmp(float value) { xLfoAmp = value; }

float ParameterManager::getXLfoRate() const { return xLfoRate; }
void ParameterManager::setXLfoRate(float value) { xLfoRate = value; }

float ParameterManager::getYLfoAmp() const { return yLfoAmp; }
void ParameterManager::setYLfoAmp(float value) { yLfoAmp = value; }

float ParameterManager::getYLfoRate() const { return yLfoRate; }
void ParameterManager::setYLfoRate(float value) { yLfoRate = value; }

float ParameterManager::getZLfoAmp() const { return zLfoAmp; }
void ParameterManager::setZLfoAmp(float value) { zLfoAmp = value; }

float ParameterManager::getZLfoRate() const { return zLfoRate; }
void ParameterManager::setZLfoRate(float value) { zLfoRate = value; }

float ParameterManager::getRotateLfoAmp() const { return rotateLfoAmp; }
void ParameterManager::setRotateLfoAmp(float value) { rotateLfoAmp = value; }

float ParameterManager::getRotateLfoRate() const { return rotateLfoRate; }
void ParameterManager::setRotateLfoRate(float value) { rotateLfoRate = value; }

// Video reactivity getters/setters
float ParameterManager::getVLumakeyValue() const { return vLumakeyValue; }
void ParameterManager::setVLumakeyValue(float value) { vLumakeyValue = value; }

float ParameterManager::getVMix() const { return vMix; }
void ParameterManager::setVMix(float value) { vMix = value; }

float ParameterManager::getVHue() const { return vHue; }
void ParameterManager::setVHue(float value) { vHue = value; }

float ParameterManager::getVSaturation() const { return vSaturation; }
void ParameterManager::setVSaturation(float value) { vSaturation = value; }

float ParameterManager::getVBrightness() const { return vBrightness; }
void ParameterManager::setVBrightness(float value) { vBrightness = value; }

float ParameterManager::getVTemporalFilterMix() const { return vTemporalFilterMix; }
void ParameterManager::setVTemporalFilterMix(float value) { vTemporalFilterMix = value; }

float ParameterManager::getVTemporalFilterResonance() const { return vTemporalFilterResonance; }
void ParameterManager::setVTemporalFilterResonance(float value) { vTemporalFilterResonance = value; }

float ParameterManager::getVSharpenAmount() const { return vSharpenAmount; }
void ParameterManager::setVSharpenAmount(float value) { vSharpenAmount = value; }

float ParameterManager::getVXDisplace() const { return vXDisplace; }
void ParameterManager::setVXDisplace(float value) { vXDisplace = value; }

float ParameterManager::getVYDisplace() const { return vYDisplace; }
void ParameterManager::setVYDisplace(float value) { vYDisplace = value; }

float ParameterManager::getVZDisplace() const { return vZDisplace; }
void ParameterManager::setVZDisplace(float value) { vZDisplace = value; }

float ParameterManager::getVRotate() const { return vRotate; }
void ParameterManager::setVRotate(float value) { vRotate = value; }

float ParameterManager::getVHueModulation() const { return vHueModulation; }
void ParameterManager::setVHueModulation(float value) { vHueModulation = value; }

float ParameterManager::getVHueOffset() const { return vHueOffset; }
void ParameterManager::setVHueOffset(float value) { vHueOffset = value; }

float ParameterManager::getVHueLFO() const { return vHueLFO; }
void ParameterManager::setVHueLFO(float value) { vHueLFO = value; }

// Mode getters/setters
bool ParameterManager::isVideoReactiveEnabled() const { return videoReactiveMode; }
void ParameterManager::setVideoReactiveEnabled(bool enabled) { videoReactiveMode = enabled; }

bool ParameterManager::isRecordingEnabled() const { return recordingEnabled; }
void ParameterManager::setRecordingEnabled(bool enabled) { recordingEnabled = enabled; }

bool ParameterManager::isLfoAmpModeEnabled() const { return lfoAmpMode; }
void ParameterManager::setLfoAmpModeEnabled(bool enabled) { lfoAmpMode = enabled; }

bool ParameterManager::isLfoRateModeEnabled() const { return lfoRateMode; }
void ParameterManager::setLfoRateModeEnabled(bool enabled) { lfoRateMode = enabled; }
