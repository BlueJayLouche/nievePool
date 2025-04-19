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
    // Initialize audio offsets to zero
    resetToDefaults(); // Call reset to ensure audio offsets are zeroed initially
}

// Helper function to initialize parameter maps
void ParameterManager::initializeParameterMaps() {
    parameterIds = {
        // Toggles
        "hueInvert", "saturationInvert", "brightnessInvert", "horizontalMirror",
        "verticalMirror", "lumakeyInvert", "toroidEnabled", "mirrorModeEnabled",
        "wetModeEnabled",
        // Effect Parameters (Float)
        "lumakeyValue", "mix", "hue", "saturation", "brightness",
        "temporalFilterMix", "temporalFilterResonance", "sharpenAmount",
        "xDisplace", "yDisplace", "zDisplace", "rotate", "hueModulation",
        "hueOffset", "hueLFO", "zFrequency", "xFrequency", "yFrequency",
        // Effect Parameters (Int)
        "delayAmount",
        // LFO Parameters
        "xLfoAmp", "xLfoRate", "yLfoAmp", "yLfoRate", "zLfoAmp", "zLfoRate",
        "rotateLfoAmp", "rotateLfoRate",
        // Video Reactivity Parameters (assuming they might be controllable too)
        "vLumakeyValue", "vMix", "vHue", "vSaturation", "vBrightness",
        "vTemporalFilterMix", "vTemporalFilterResonance", "vSharpenAmount",
        "vXDisplace", "vYDisplace", "vZDisplace", "vRotate", "vHueModulation",
        "vHueOffset", "vHueLFO",
        // Mode Flags (Toggles)
        "videoReactiveMode", "lfoAmpMode", "lfoRateMode"
        // Note: Video device settings are not included as controllable parameters here
    };

    // Initialize maps with defaults
    for (const auto& id : parameterIds) {
        midiChannels[id] = -1;
        midiControls[id] = -1;
        oscAddresses[id] = "";
    }
    // Reset OSC port to default
    oscPort = 9000;
}


void ParameterManager::setup() {
    initializeParameterMaps(); // Initialize IDs and default mappings first
    // Try to load settings from XML, this will overwrite defaults if mappings exist
    if (!loadSettings()) {
        // If loading fails (e.g., file not found), reset ensures defaults are set
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
    if (XML.load(ofToDataPath(settingsFile))) { // Use load() instead of deprecated loadFile()
        loadFromXml(XML);
        return true;
    }
    return false;
}

bool ParameterManager::saveSettings() {
    saveToXml(XML);
    return XML.save(ofToDataPath(settingsFile)); // Use save() instead of deprecated saveFile()
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
    zFrequency = 0.03f;
    xFrequency = 0.015f;
    yFrequency = 0.02f;

    // LFO parameters
    xLfoAmp = 0.0f;
    xLfoRate = 0.0f;
    yLfoAmp = 0.0f;
    yLfoRate = 0.0f;
    zLfoAmp = 0.0f;
    zLfoRate = 0.0f;
    rotateLfoAmp = 0.0f;
    rotateLfoRate = 0.0f;

    // Video reactivity parameters (Keep for now)
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

    // Reset Audio Offsets
    audioLumakeyValueOffset = 0.0f;
    audioMixOffset = 0.0f;
    audioHueOffset = 0.0f;
    audioSaturationOffset = 0.0f;
    audioBrightnessOffset = 0.0f;
    audioTemporalFilterMixOffset = 0.0f;
    audioTemporalFilterResonanceOffset = 0.0f;
    audioSharpenAmountOffset = 0.0f;
    audioXDisplaceOffset = 0.0f;
    audioYDisplaceOffset = 0.0f;
    audioZDisplaceOffset = 0.0f;
    audioRotateOffset = 0.0f;
    audioHueModulationOffset = 0.0f;
    audioHueOffsetOffset = 0.0f;
    audioHueLFOOffset = 0.0f;
    audioDelayAmountOffset = 0;
    audioZFrequencyOffset = 0.0f;
    audioXFrequencyOffset = 0.0f;
    audioYFrequencyOffset = 0.0f;

    // Mode flags
    videoReactiveMode = false;
    lfoAmpMode = false;
    lfoRateMode = false;
    clearAllLocks();

    // Reset mappings to defaults
    for (const auto& id : parameterIds) {
        midiChannels[id] = -1;
        midiControls[id] = -1;
        oscAddresses[id] = "";
    }
    // Reset OSC port to default
    oscPort = 9000;
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
    // Only proceed if paramManager tag exists
    if (!xml.tagExists("paramManager")) {
        ofLogWarning("ParameterManager") << "No paramManager tag found in settings";
        return;
    }

    // Push into the paramManager tag
    xml.pushTag("paramManager");

    // Load OSC Port
    oscPort = xml.getValue("osc:port", oscPort); // Load OSC port, keep default if not found

    // Load video settings
    videoDevicePath = xml.getValue("video:devicePath", videoDevicePath);
    videoDeviceID = xml.getValue("video:deviceID", videoDeviceID);
    videoFormat = xml.getValue("video:format", videoFormat);
    videoWidth = xml.getValue("video:width", videoWidth);
    videoHeight = xml.getValue("video:height", videoHeight);
    videoFrameRate = xml.getValue("video:frameRate", videoFrameRate);

    // Load P-Lock data if available
    if (xml.tagExists("plocks")) {
        xml.pushTag("plocks");
        pLockSmoothFactor = xml.getValue("smoothFactor", pLockSmoothFactor);
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

    // --- Load Parameters and Mappings ---
    int numParamTags = xml.getNumTags("param");
    ofLogNotice("ParameterManager::loadFromXml") << "Loading " << numParamTags << " parameters from XML.";
    for (int i = 0; i < numParamTags; ++i) {
        // Get attributes using the correct tag name ("param") and index (i)
        std::string id = xml.getAttribute("param", "id", std::string("unknown"), i);
        std::string valueStr = xml.getAttribute("param", "value", std::string(""), i);

        // Load mappings first (providing defaults with correct types if attributes are missing)
        // Ensure the ID is valid before trying to insert into maps
        if (id != "unknown" && std::find(parameterIds.begin(), parameterIds.end(), id) != parameterIds.end()) {
            midiChannels[id] = xml.getAttribute("param", "midiChannel", -1, i);
            midiControls[id] = xml.getAttribute("param", "midiControl", -1, i);
            oscAddresses[id] = xml.getAttribute("param", "oscAddr", std::string(""), i);
        } else if (id != "unknown") {
             ofLogWarning("ParameterManager::loadFromXml") << "Skipping unknown parameter ID found in XML: " << id;
             continue; // Skip to next param tag if ID is not recognized
        } else {
             ofLogWarning("ParameterManager::loadFromXml") << "Found param tag with missing ID attribute at index " << i;
             continue; // Skip if ID is missing
        }


        // Load value using appropriate setter based on ID
        // This requires knowing the type associated with each ID
        if (!valueStr.empty()) {
            try {
                // Floats
                if (id == "lumakeyValue") setLumakeyValue(ofToFloat(valueStr), false); // Don't record during load
                else if (id == "mix") setMix(ofToFloat(valueStr), false);
                else if (id == "hue") setHue(ofToFloat(valueStr), false);
                else if (id == "saturation") setSaturation(ofToFloat(valueStr), false);
                else if (id == "brightness") setBrightness(ofToFloat(valueStr), false);
                else if (id == "temporalFilterMix") setTemporalFilterMix(ofToFloat(valueStr), false);
                else if (id == "temporalFilterResonance") setTemporalFilterResonance(ofToFloat(valueStr), false);
                else if (id == "sharpenAmount") setSharpenAmount(ofToFloat(valueStr), false);
                else if (id == "xDisplace") setXDisplace(ofToFloat(valueStr), false);
                else if (id == "yDisplace") setYDisplace(ofToFloat(valueStr), false);
                else if (id == "zDisplace") setZDisplace(ofToFloat(valueStr), false);
                else if (id == "rotate") setRotate(ofToFloat(valueStr), false);
                else if (id == "hueModulation") setHueModulation(ofToFloat(valueStr), false);
                else if (id == "hueOffset") setHueOffset(ofToFloat(valueStr), false);
                else if (id == "hueLFO") setHueLFO(ofToFloat(valueStr), false);
                else if (id == "zFrequency") setZFrequency(ofToFloat(valueStr), false);
                else if (id == "xFrequency") setXFrequency(ofToFloat(valueStr), false);
                else if (id == "yFrequency") setYFrequency(ofToFloat(valueStr), false);
                // LFOs
                else if (id == "xLfoAmp") setXLfoAmp(ofToFloat(valueStr));
                else if (id == "xLfoRate") setXLfoRate(ofToFloat(valueStr));
                else if (id == "yLfoAmp") setYLfoAmp(ofToFloat(valueStr));
                else if (id == "yLfoRate") setYLfoRate(ofToFloat(valueStr));
                else if (id == "zLfoAmp") setZLfoAmp(ofToFloat(valueStr));
                else if (id == "zLfoRate") setZLfoRate(ofToFloat(valueStr));
                else if (id == "rotateLfoAmp") setRotateLfoAmp(ofToFloat(valueStr));
                else if (id == "rotateLfoRate") setRotateLfoRate(ofToFloat(valueStr));
                // Video Reactive
                else if (id == "vLumakeyValue") setVLumakeyValue(ofToFloat(valueStr));
                else if (id == "vMix") setVMix(ofToFloat(valueStr));
                else if (id == "vHue") setVHue(ofToFloat(valueStr));
                else if (id == "vSaturation") setVSaturation(ofToFloat(valueStr));
                else if (id == "vBrightness") setVBrightness(ofToFloat(valueStr));
                else if (id == "vTemporalFilterMix") setVTemporalFilterMix(ofToFloat(valueStr));
                else if (id == "vTemporalFilterResonance") setVTemporalFilterResonance(ofToFloat(valueStr));
                else if (id == "vSharpenAmount") setVSharpenAmount(ofToFloat(valueStr));
                else if (id == "vXDisplace") setVXDisplace(ofToFloat(valueStr));
                else if (id == "vYDisplace") setVYDisplace(ofToFloat(valueStr));
                else if (id == "vZDisplace") setVZDisplace(ofToFloat(valueStr));
                else if (id == "vRotate") setVRotate(ofToFloat(valueStr));
                else if (id == "vHueModulation") setVHueModulation(ofToFloat(valueStr));
                else if (id == "vHueOffset") setVHueOffset(ofToFloat(valueStr));
                else if (id == "vHueLFO") setVHueLFO(ofToFloat(valueStr));
                // Ints
                else if (id == "delayAmount") setDelayAmount(ofToInt(valueStr), false);
                // Bools (Toggles & Modes)
                else if (id == "hueInvert") setHueInverted(ofToBool(valueStr));
                else if (id == "saturationInvert") setSaturationInverted(ofToBool(valueStr));
                else if (id == "brightnessInvert") setBrightnessInverted(ofToBool(valueStr));
                else if (id == "horizontalMirror") setHorizontalMirrorEnabled(ofToBool(valueStr));
                else if (id == "verticalMirror") setVerticalMirrorEnabled(ofToBool(valueStr));
                else if (id == "lumakeyInvert") setLumakeyInverted(ofToBool(valueStr));
                else if (id == "toroidEnabled") setToroidEnabled(ofToBool(valueStr));
                else if (id == "mirrorModeEnabled") setMirrorModeEnabled(ofToBool(valueStr));
                else if (id == "wetModeEnabled") setWetModeEnabled(ofToBool(valueStr));
                else if (id == "videoReactiveMode") setVideoReactiveEnabled(ofToBool(valueStr));
                else if (id == "lfoAmpMode") setLfoAmpModeEnabled(ofToBool(valueStr));
                else if (id == "lfoRateMode") setLfoRateModeEnabled(ofToBool(valueStr));
                else {
                    ofLogWarning("ParameterManager::loadFromXml") << "Unknown parameter ID during load: " << id;
                }
            } catch (const std::invalid_argument& e) {
                 ofLogError("ParameterManager::loadFromXml") << "Conversion error for param '" << id << "' value '" << valueStr << "': " << e.what();
            } catch (const std::out_of_range& e) {
                 ofLogError("ParameterManager::loadFromXml") << "Out of range error for param '" << id << "' value '" << valueStr << "': " << e.what();
            }
        } else {
             ofLogWarning("ParameterManager::loadFromXml") << "Empty value attribute for param ID: " << id;
        }
        // Note: No popTag needed here as getAttribute doesn't change the current tag level
    }

    xml.popTag(); // pop paramManager
}


void ParameterManager::saveToXml(ofxXmlSettings& xml) const {
    // Ensure paramManager tag exists
    if (!xml.tagExists("paramManager")) {
        xml.addTag("paramManager");
    }
    xml.pushTag("paramManager");

    // Save OSC Port
    xml.setValue("osc:port", oscPort);

    // Save video settings
    xml.setValue("video:devicePath", videoDevicePath);
    xml.setValue("video:deviceID", videoDeviceID);
    xml.setValue("video:format", videoFormat);
    xml.setValue("video:width", videoWidth);
    xml.setValue("video:height", videoHeight);
    xml.setValue("video:frameRate", videoFrameRate);

    // Remove old <param> tags before saving new ones
    while(xml.getNumTags("param") > 0) {
        xml.removeTag("param", 0);
    }

    // Iterate through known parameter IDs and save each one
    for (const auto& id : parameterIds) {
        int tagIndex = xml.addTag("param"); // Add tag and get its index

        // Add attributes to the newly created tag using its index
        xml.addAttribute("param", "id", id, tagIndex);

        // Save value based on type using getters
        // Note: Using base values (e.g., `hue`) not the P-Locked ones (e.g., `getHue()`) for saving
        if (id == "hueInvert") xml.addAttribute("param", "value", isHueInverted() ? "1" : "0", tagIndex);
        else if (id == "saturationInvert") xml.addAttribute("param", "value", isSaturationInverted() ? "1" : "0", tagIndex);
        else if (id == "brightnessInvert") xml.addAttribute("param", "value", isBrightnessInverted() ? "1" : "0", tagIndex);
        else if (id == "horizontalMirror") xml.addAttribute("param", "value", isHorizontalMirrorEnabled() ? "1" : "0", tagIndex);
        else if (id == "verticalMirror") xml.addAttribute("param", "value", isVerticalMirrorEnabled() ? "1" : "0", tagIndex);
        else if (id == "lumakeyInvert") xml.addAttribute("param", "value", isLumakeyInverted() ? "1" : "0", tagIndex);
        else if (id == "toroidEnabled") xml.addAttribute("param", "value", isToroidEnabled() ? "1" : "0", tagIndex);
        else if (id == "mirrorModeEnabled") xml.addAttribute("param", "value", isMirrorModeEnabled() ? "1" : "0", tagIndex);
        else if (id == "wetModeEnabled") xml.addAttribute("param", "value", isWetModeEnabled() ? "1" : "0", tagIndex);
        else if (id == "lumakeyValue") xml.addAttribute("param", "value", lumakeyValue, tagIndex);
        else if (id == "mix") xml.addAttribute("param", "value", mix, tagIndex);
        else if (id == "hue") xml.addAttribute("param", "value", hue, tagIndex);
        else if (id == "saturation") xml.addAttribute("param", "value", saturation, tagIndex);
        else if (id == "brightness") xml.addAttribute("param", "value", brightness, tagIndex);
        else if (id == "temporalFilterMix") xml.addAttribute("param", "value", temporalFilterMix, tagIndex);
        else if (id == "temporalFilterResonance") xml.addAttribute("param", "value", temporalFilterResonance, tagIndex);
        else if (id == "sharpenAmount") xml.addAttribute("param", "value", sharpenAmount, tagIndex);
        else if (id == "xDisplace") xml.addAttribute("param", "value", xDisplace, tagIndex);
        else if (id == "yDisplace") xml.addAttribute("param", "value", yDisplace, tagIndex);
        else if (id == "zDisplace") xml.addAttribute("param", "value", zDisplace, tagIndex);
        else if (id == "rotate") xml.addAttribute("param", "value", rotate, tagIndex);
        else if (id == "hueModulation") xml.addAttribute("param", "value", hueModulation, tagIndex);
        else if (id == "hueOffset") xml.addAttribute("param", "value", hueOffset, tagIndex);
        else if (id == "hueLFO") xml.addAttribute("param", "value", hueLFO, tagIndex);
        else if (id == "delayAmount") xml.addAttribute("param", "value", delayAmount, tagIndex);
        else if (id == "zFrequency") xml.addAttribute("param", "value", zFrequency, tagIndex);
        else if (id == "xFrequency") xml.addAttribute("param", "value", xFrequency, tagIndex);
        else if (id == "yFrequency") xml.addAttribute("param", "value", yFrequency, tagIndex);
        else if (id == "xLfoAmp") xml.addAttribute("param", "value", xLfoAmp, tagIndex);
        else if (id == "xLfoRate") xml.addAttribute("param", "value", xLfoRate, tagIndex);
        else if (id == "yLfoAmp") xml.addAttribute("param", "value", yLfoAmp, tagIndex);
        else if (id == "yLfoRate") xml.addAttribute("param", "value", yLfoRate, tagIndex);
        else if (id == "zLfoAmp") xml.addAttribute("param", "value", zLfoAmp, tagIndex);
        else if (id == "zLfoRate") xml.addAttribute("param", "value", zLfoRate, tagIndex);
        else if (id == "rotateLfoAmp") xml.addAttribute("param", "value", rotateLfoAmp, tagIndex);
        else if (id == "rotateLfoRate") xml.addAttribute("param", "value", rotateLfoRate, tagIndex);
        else if (id == "vLumakeyValue") xml.addAttribute("param", "value", vLumakeyValue, tagIndex);
        else if (id == "vMix") xml.addAttribute("param", "value", vMix, tagIndex);
        else if (id == "vHue") xml.addAttribute("param", "value", vHue, tagIndex);
        else if (id == "vSaturation") xml.addAttribute("param", "value", vSaturation, tagIndex);
        else if (id == "vBrightness") xml.addAttribute("param", "value", vBrightness, tagIndex);
        else if (id == "vTemporalFilterMix") xml.addAttribute("param", "value", vTemporalFilterMix, tagIndex);
        else if (id == "vTemporalFilterResonance") xml.addAttribute("param", "value", vTemporalFilterResonance, tagIndex);
        else if (id == "vSharpenAmount") xml.addAttribute("param", "value", vSharpenAmount, tagIndex);
        else if (id == "vXDisplace") xml.addAttribute("param", "value", vXDisplace, tagIndex);
        else if (id == "vYDisplace") xml.addAttribute("param", "value", vYDisplace, tagIndex);
        else if (id == "vZDisplace") xml.addAttribute("param", "value", vZDisplace, tagIndex);
        else if (id == "vRotate") xml.addAttribute("param", "value", vRotate, tagIndex);
        else if (id == "vHueModulation") xml.addAttribute("param", "value", vHueModulation, tagIndex);
        else if (id == "vHueOffset") xml.addAttribute("param", "value", vHueOffset, tagIndex);
        else if (id == "vHueLFO") xml.addAttribute("param", "value", vHueLFO, tagIndex);
        else if (id == "videoReactiveMode") xml.addAttribute("param", "value", videoReactiveMode ? "1" : "0", tagIndex);
        else if (id == "lfoAmpMode") xml.addAttribute("param", "value", lfoAmpMode ? "1" : "0", tagIndex);
        else if (id == "lfoRateMode") xml.addAttribute("param", "value", lfoRateMode ? "1" : "0", tagIndex);
        else {
             ofLogWarning("ParameterManager::saveToXml") << "Unknown parameter ID during save: " << id;
             xml.addAttribute("param", "value", "", tagIndex); // Save empty if type unknown
        }

        // Save mappings (handle potential missing keys gracefully)
        xml.addAttribute("param", "midiChannel", midiChannels.count(id) ? midiChannels.at(id) : -1, tagIndex);
        xml.addAttribute("param", "midiControl", midiControls.count(id) ? midiControls.at(id) : -1, tagIndex);
        xml.addAttribute("param", "oscAddr", oscAddresses.count(id) ? oscAddresses.at(id) : "", tagIndex);
    }

    // Save P-Lock data
    // Ensure plocks tag exists and is clean before saving
    if (xml.tagExists("plocks")) {
         // If it exists, remove it first to avoid duplicate data or merging issues
         xml.removeTag("plocks");
    }
    // Add a fresh plocks tag
    xml.addTag("plocks");
    xml.pushTag("plocks"); // Push into the newly added plocks tag

    xml.setValue("smoothFactor", pLockSmoothFactor); // Save smooth factor inside plocks

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
        xml.popTag(); // pop lockTag
    }

    xml.popTag(); // pop locks
    xml.popTag(); // pop plocks

    xml.popTag(); // pop paramManager
}


// --- Audio Reactivity Offset Setters Implementation ---
void ParameterManager::setAudioLumakeyValueOffset(float offset) { audioLumakeyValueOffset = offset; }
void ParameterManager::setAudioMixOffset(float offset) { audioMixOffset = offset; }
void ParameterManager::setAudioHueOffset(float offset) { audioHueOffset = offset; }
void ParameterManager::setAudioSaturationOffset(float offset) { audioSaturationOffset = offset; }
void ParameterManager::setAudioBrightnessOffset(float offset) { audioBrightnessOffset = offset; }
void ParameterManager::setAudioTemporalFilterMixOffset(float offset) { audioTemporalFilterMixOffset = offset; }
void ParameterManager::setAudioTemporalFilterResonanceOffset(float offset) { audioTemporalFilterResonanceOffset = offset; }
void ParameterManager::setAudioSharpenAmountOffset(float offset) { audioSharpenAmountOffset = offset; }
void ParameterManager::setAudioXDisplaceOffset(float offset) { audioXDisplaceOffset = offset; }
void ParameterManager::setAudioYDisplaceOffset(float offset) { audioYDisplaceOffset = offset; }
void ParameterManager::setAudioZDisplaceOffset(float offset) { audioZDisplaceOffset = offset; }
void ParameterManager::setAudioRotateOffset(float offset) { audioRotateOffset = offset; }
void ParameterManager::setAudioHueModulationOffset(float offset) { audioHueModulationOffset = offset; }
void ParameterManager::setAudioHueOffsetOffset(float offset) { audioHueOffsetOffset = offset; }
void ParameterManager::setAudioHueLFOOffset(float offset) { audioHueLFOOffset = offset; }
void ParameterManager::setAudioDelayAmountOffset(int offset) { audioDelayAmountOffset = offset; }
void ParameterManager::setAudioZFrequencyOffset(float offset) { audioZFrequencyOffset = offset; }
void ParameterManager::setAudioXFrequencyOffset(float offset) { audioXFrequencyOffset = offset; }
void ParameterManager::setAudioYFrequencyOffset(float offset) { audioYFrequencyOffset = offset; }


// --- Mapping Getters Implementation ---
int ParameterManager::getMidiChannel(const std::string& paramId) const {
    if (midiChannels.count(paramId)) {
        return midiChannels.at(paramId);
    }
    return -1; // Default if not found
}

int ParameterManager::getMidiControl(const std::string& paramId) const {
    if (midiControls.count(paramId)) {
        return midiControls.at(paramId);
    }
    return -1; // Default if not found
}

std::string ParameterManager::getOscAddress(const std::string& paramId) const {
    if (oscAddresses.count(paramId)) {
        return oscAddresses.at(paramId);
    }
    return ""; // Default if not found
}

const std::vector<std::string>& ParameterManager::getAllParameterIds() const {
    return parameterIds;
}


// --- Toggle state getters/setters (Implementation remains the same) ---
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

// Parameter getters/setters (Updated to include audio offsets)
float ParameterManager::getLumakeyValue() const { return lumakeyValue + audioLumakeyValueOffset + getPLockValue(PLockIndex::LUMAKEY_VALUE); }
void ParameterManager::setLumakeyValue(float value, bool recordable) {
    lumakeyValue = value; // Set the base value
    if (recordable) {
        recordParameter(PLockIndex::LUMAKEY_VALUE, value);
    }
}

float ParameterManager::getMix() const { return mix + audioMixOffset + getPLockValue(PLockIndex::MIX); }
void ParameterManager::setMix(float value, bool recordable) {
    mix = value; // Set the base value
    if (recordable) {
        recordParameter(PLockIndex::MIX, value);
    }
}

// Note: P-Lock for Hue, Sat, Bright, ZDisplace, HueMod is multiplicative in original code.
// Keeping that logic for P-Lock, but adding audio offset additively.
float ParameterManager::getHue() const { return (hue + audioHueOffset) * (1.0f + getPLockValue(PLockIndex::HUE)); }
void ParameterManager::setHue(float value, bool recordable) {
    hue = value; // Set the base value
    if (recordable) {
        recordParameter(PLockIndex::HUE, value);
    }
}

float ParameterManager::getSaturation() const { return (saturation + audioSaturationOffset) * (1.0f + getPLockValue(PLockIndex::SATURATION)); }
void ParameterManager::setSaturation(float value, bool recordable) {
    saturation = value; // Set the base value
    if (recordable) {
        recordParameter(PLockIndex::SATURATION, value);
    }
}

float ParameterManager::getBrightness() const { return (brightness + audioBrightnessOffset) * (1.0f + getPLockValue(PLockIndex::BRIGHTNESS)); }
void ParameterManager::setBrightness(float value, bool recordable) {
    brightness = value; // Set the base value
    if (recordable) {
        recordParameter(PLockIndex::BRIGHTNESS, value);
    }
}

float ParameterManager::getTemporalFilterMix() const { return temporalFilterMix + audioTemporalFilterMixOffset + getPLockValue(PLockIndex::TEMPORAL_FILTER_MIX); }
void ParameterManager::setTemporalFilterMix(float value, bool recordable) {
    temporalFilterMix = value; // Set the base value
    if (recordable) {
        recordParameter(PLockIndex::TEMPORAL_FILTER_MIX, value);
    }
}

float ParameterManager::getTemporalFilterResonance() const { return temporalFilterResonance + audioTemporalFilterResonanceOffset + getPLockValue(PLockIndex::TEMPORAL_FILTER_RESONANCE); }
void ParameterManager::setTemporalFilterResonance(float value, bool recordable) {
    temporalFilterResonance = value; // Set the base value
    if (recordable) {
        recordParameter(PLockIndex::TEMPORAL_FILTER_RESONANCE, value);
    }
}

float ParameterManager::getSharpenAmount() const { return sharpenAmount + audioSharpenAmountOffset + getPLockValue(PLockIndex::SHARPEN_AMOUNT); }
void ParameterManager::setSharpenAmount(float value, bool recordable) {
    sharpenAmount = value; // Set the base value
    if (recordable) {
        recordParameter(PLockIndex::SHARPEN_AMOUNT, value);
    }
}

float ParameterManager::getXDisplace() const { return xDisplace + audioXDisplaceOffset + getPLockValue(PLockIndex::X_DISPLACE); }
void ParameterManager::setXDisplace(float value, bool recordable) {
    xDisplace = value; // Set the base value
    if (recordable) {
        recordParameter(PLockIndex::X_DISPLACE, value);
    }
}

float ParameterManager::getYDisplace() const { return yDisplace + audioYDisplaceOffset + getPLockValue(PLockIndex::Y_DISPLACE); }
void ParameterManager::setYDisplace(float value, bool recordable) {
    yDisplace = value; // Set the base value
    if (recordable) {
        recordParameter(PLockIndex::Y_DISPLACE, value);
    }
}

float ParameterManager::getZDisplace() const { return (zDisplace + audioZDisplaceOffset) * (1.0f + getPLockValue(PLockIndex::Z_DISPLACE)); }
void ParameterManager::setZDisplace(float value, bool recordable) {
    zDisplace = value; // Set the base value
    if (recordable) {
        recordParameter(PLockIndex::Z_DISPLACE, value);
    }
}

// Frequency getters/setters - Additive audio offset, no P-Lock
float ParameterManager::getZFrequency() const {
    return zFrequency + audioZFrequencyOffset;
}

void ParameterManager::setZFrequency(float value, bool recordable) {
    zFrequency = value;
    // No P-Lock recording for frequency
}

float ParameterManager::getXFrequency() const {
    return xFrequency + audioXFrequencyOffset;
}

void ParameterManager::setXFrequency(float value, bool recordable) {
    xFrequency = value;
    // No P-Lock recording for frequency
}

float ParameterManager::getYFrequency() const {
    return yFrequency + audioYFrequencyOffset;
}

void ParameterManager::setYFrequency(float value, bool recordable) {
    yFrequency = value;
    // No P-Lock recording for frequency
}

float ParameterManager::getRotate() const { return rotate + audioRotateOffset + getPLockValue(PLockIndex::ROTATE); }
void ParameterManager::setRotate(float value, bool recordable) {
    rotate = value; // Set the base value
    if (recordable) {
        recordParameter(PLockIndex::ROTATE, value);
    }
}

float ParameterManager::getHueModulation() const { return (hueModulation + audioHueModulationOffset) * (1.0f - getPLockValue(PLockIndex::HUE_MODULATION)); }
void ParameterManager::setHueModulation(float value, bool recordable) {
    hueModulation = value; // Set the base value
    if (recordable) {
        recordParameter(PLockIndex::HUE_MODULATION, value);
    }
}

float ParameterManager::getHueOffset() const { return hueOffset + audioHueOffsetOffset + getPLockValue(PLockIndex::HUE_OFFSET); }
void ParameterManager::setHueOffset(float value, bool recordable) {
    hueOffset = value; // Set the base value
    if (recordable) {
        recordParameter(PLockIndex::HUE_OFFSET, value);
    }
}

float ParameterManager::getHueLFO() const { return hueLFO + audioHueLFOOffset + getPLockValue(PLockIndex::HUE_LFO); }
void ParameterManager::setHueLFO(float value, bool recordable) {
    hueLFO = value; // Set the base value
    if (recordable) {
        recordParameter(PLockIndex::HUE_LFO, value);
    }
}

int ParameterManager::getDelayAmount() const { return delayAmount + audioDelayAmountOffset + (int)(getPLockValue(PLockIndex::DELAY_AMOUNT) * (P_LOCK_SIZE - 1.0f)); }
void ParameterManager::setDelayAmount(int value, bool recordable) {
    delayAmount = value; // Set the base value
    if (recordable) {
        recordParameter(PLockIndex::DELAY_AMOUNT, static_cast<float>(value) / (P_LOCK_SIZE - 1.0f));
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
