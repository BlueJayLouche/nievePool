#include "MidiManager.h"

MidiManager::MidiManager(ParameterManager* paramManager)
    : paramManager(paramManager) {
}

MidiManager::~MidiManager() {
    disconnectCurrentDevice();
    midiIn.removeListener(this);
}

void MidiManager::setup() {
    // List available devices
    midiIn.listInPorts();
    
    // Register as MIDI listener
    midiIn.addListener(this);
    
    // Scan for devices
    scanForDevices();
    
    // Try to connect to first available device
    if (!availableDevices.empty()) {
        connectToDevice(0);
    }
}

void MidiManager::update() {
    // Check for device changes periodically
    float currentTime = ofGetElapsedTimef();
    if (currentTime - lastDeviceScanTime > DEVICE_SCAN_INTERVAL) {
        scanForDevices();
        lastDeviceScanTime = currentTime;
    }
}

void MidiManager::scanForDevices() {
    // Get list of available MIDI devices
    availableDevices = midiIn.getInPortList();
    
    // Log the found devices
    ofLogNotice("MidiManager") << "Found " << availableDevices.size() << " MIDI devices:";
    for (size_t i = 0; i < availableDevices.size(); i++) {
        ofLogNotice("MidiManager") << i << ": " << availableDevices[i];
    }
}

bool MidiManager::connectToDevice(int deviceIndex) {
    if (deviceIndex < 0 || deviceIndex >= availableDevices.size()) {
        ofLogError("MidiManager") << "Invalid device index: " << deviceIndex;
        return false;
    }
    
    // Disconnect from current device if connected
    disconnectCurrentDevice();
    
    // Try to open the device
    try {
        midiIn.openPort(deviceIndex);
        currentDeviceIndex = deviceIndex;
        isDeviceConnected = true;
        ofLogNotice("MidiManager") << "Connected to MIDI device: " << availableDevices[deviceIndex];
        
        // Don't ignore MIDI messages
        midiIn.ignoreTypes(false, false, false);
        
        return true;
    } catch (const std::exception& e) {
        ofLogError("MidiManager") << "Failed to connect to MIDI device: " << e.what();
        return false;
    }
}

bool MidiManager::connectToDevice(const std::string& deviceName) {
    for (size_t i = 0; i < availableDevices.size(); i++) {
        if (availableDevices[i] == deviceName) {
            return connectToDevice(i);
        }
    }
    
    ofLogError("MidiManager") << "Device not found: " << deviceName;
    return false;
}

void MidiManager::disconnectCurrentDevice() {
    if (isDeviceConnected) {
        midiIn.closePort();
        isDeviceConnected = false;
        currentDeviceIndex = -1;
        ofLogNotice("MidiManager") << "Disconnected from MIDI device";
    }
}

void MidiManager::newMidiMessage(ofxMidiMessage& message) {
    // Add to message queue
    midiMessages.push_back(message);
    
    // Remove old messages if queue is too long
    while (midiMessages.size() > maxMessages) {
        midiMessages.erase(midiMessages.begin());
    }
    
    // Process the message based on type
    if (message.status == MIDI_CONTROL_CHANGE) {
        processControlChange(message);
    }
}

std::vector<ofxMidiMessage> MidiManager::getRecentMessages() const {
    return midiMessages;
}

std::vector<std::string> MidiManager::getAvailableDevices() const {
    return availableDevices;
}

std::string MidiManager::getCurrentDeviceName() const {
    if (isDeviceConnected && currentDeviceIndex >= 0 && currentDeviceIndex < availableDevices.size()) {
        return availableDevices[currentDeviceIndex];
    }
    return "Not connected";
}

int MidiManager::getCurrentDeviceIndex() const {
    return currentDeviceIndex;
}

std::string MidiManager::getPreferredDeviceName() const {
    return preferredDeviceName;
}

void MidiManager::loadSettings(ofxXmlSettings& xml) {
    // We're already inside the paramManager tag from ofApp::setup
    if (xml.tagExists("midi")) {
        xml.pushTag("midi");
        
        // Get preferred device name
        preferredDeviceName = xml.getValue("preferredDevice", "");
        
        ofLogNotice("MidiManager") << "Loading MIDI settings, preferred device: " << preferredDeviceName;
        
        // Try to connect to the saved device if it exists
        if (!preferredDeviceName.empty() && preferredDeviceName != "Not connected") {
            for (size_t i = 0; i < availableDevices.size(); i++) {
                if (availableDevices[i] == preferredDeviceName) {
                    connectToDevice(i);
                    ofLogNotice("MidiManager") << "Connected to saved MIDI device: " << preferredDeviceName;
                    break;
                }
            }
        }
        
        xml.popTag(); // pop midi
    } else {
        ofLogWarning("MidiManager") << "No midi tag found in settings";
    }
}

void MidiManager::saveSettings(ofxXmlSettings& xml) const {
    // Save the preferred device name loaded from settings, not the currently connected one
    // This preserves the user's preference in settings.xml
    if (!preferredDeviceName.empty()) {
        xml.setValue("midi:preferredDevice", preferredDeviceName);
    } else {
        // If no preferred name was loaded, save the current one (or "Not connected")
        xml.setValue("midi:preferredDevice", getCurrentDeviceName());
    }
}

void MidiManager::processControlChange(const ofxMidiMessage& message) {
    // Handle toggles and mode switches
    switch (message.control) {
        // P-Lock record toggle
        case 55:
            if (message.value == 127) {
                paramManager->startRecording();
            } else if (message.value == 0) {
                paramManager->stopRecording();
            }
            return;
            
        // Video reactive toggle
        case 39:
            if (message.value == 127) {
                paramManager->setVideoReactiveEnabled(true);
                paramManager->setRecordingEnabled(false);
            } else if (message.value == 0) {
                paramManager->setVideoReactiveEnabled(false);
                paramManager->setRecordingEnabled(true);
            }
            return;
            
        // Clear buffers
        case 58:
            if (message.value == 127) {
                // TODO: Implement clear functionality in VideoFeedbackManager
            }
            return;
            
        // Reset all parameters
        case 59:
            if (message.value == 127) {
                paramManager->resetToDefaults();
            }
            return;
            
        // X scaling toggles
        case 32:
            xScaling.times2 = (message.value == 127);
            return;
        case 48:
            xScaling.times5 = (message.value == 127);
            return;
        case 64:
            xScaling.times10 = (message.value == 127);
            return;
            
        // Y scaling toggles
        case 33:
            yScaling.times2 = (message.value == 127);
            return;
        case 49:
            yScaling.times5 = (message.value == 127);
            return;
        case 65:
            yScaling.times10 = (message.value == 127);
            return;
            
        // Z scaling toggles
        case 34:
            zScaling.times2 = (message.value == 127);
            return;
        case 50:
            zScaling.times5 = (message.value == 127);
            return;
        case 66:
            zScaling.times10 = (message.value == 127);
            return;
            
        // Rotation scaling toggles
        case 35:
            rotateScaling.times2 = (message.value == 127);
            return;
        case 51:
            rotateScaling.times5 = (message.value == 127);
            return;
        case 67:
            rotateScaling.times10 = (message.value == 127);
            return;
            
        // Hue modulation scaling toggles
        case 36:
            hueModScaling.times2 = (message.value == 127);
            return;
        case 52:
            hueModScaling.times5 = (message.value == 127);
            return;
        case 68:
            hueModScaling.times10 = (message.value == 127);
            return;
            
        // Hue offset scaling toggles
        case 37:
            hueOffsetScaling.times2 = (message.value == 127);
            return;
        case 53:
            hueOffsetScaling.times5 = (message.value == 127);
            return;
        case 69:
            hueOffsetScaling.times10 = (message.value == 127);
            return;
            
        // Hue LFO scaling toggles
        case 38:
            hueLfoScaling.times2 = (message.value == 127);
            return;
        case 54:
            hueLfoScaling.times5 = (message.value == 127);
            return;
        case 70:
            hueLfoScaling.times10 = (message.value == 127);
            return;
            
        // Other effect toggles
        case 42:
            paramManager->setHueInverted(message.value == 127);
            return;
        case 43:
            paramManager->setBrightnessInverted(message.value == 127);
            return;
        case 44:
            paramManager->setSaturationInverted(message.value == 127);
            return;
        case 41:
            paramManager->setHorizontalMirrorEnabled(message.value == 127);
            return;
        case 45:
            paramManager->setVerticalMirrorEnabled(message.value == 127);
            return;
        case 46:
            paramManager->setToroidEnabled(message.value == 127);
            return;
        case 61:
            paramManager->setMirrorModeEnabled(message.value == 127);
            return;
        case 60:
            paramManager->setLumakeyInverted(message.value == 127);
            return;
        case 71:
            paramManager->setWetModeEnabled(message.value == 0);
            return;
    }

    // --- Dynamic MIDI Mapping for Continuous Controls ---
    bool videoReactiveMode = paramManager->isVideoReactiveEnabled();
    bool lfoAmpMode = paramManager->isLfoAmpModeEnabled();
    bool lfoRateMode = paramManager->isLfoRateModeEnabled();

    // Iterate through all known parameters to find a match
    const auto& paramIds = paramManager->getAllParameterIds();
    for (const auto& id : paramIds) {
        int mappedChannel = paramManager->getMidiChannel(id);
        int mappedControl = paramManager->getMidiControl(id);

        // Check if the incoming message matches the mapping for this parameter
        if (mappedChannel != -1 && mappedControl != -1 &&
            message.channel == mappedChannel && message.control == mappedControl)
        {
            // Found a match, process the value based on parameter ID and mode
            float normalizedValue = 0.0f;
            float scale = 1.0f; // Default scale

            // Determine normalization and scaling based on parameter ID
            // (This logic mirrors the original switch statement's behavior)
            if (id == "lumakeyValue" || id == "temporalFilterResonance" || id == "sharpenAmount" || id == "delayAmount") {
                normalizedValue = normalizeValue(message.value, false); // 0 to 1
            } else if (id == "mix" || id == "hue" || id == "saturation" || id == "brightness" ||
                       id == "temporalFilterMix" || id == "xDisplace" || id == "yDisplace" ||
                       id == "zDisplace" || id == "rotate" || id == "hueOffset" || id == "hueLFO" ||
                       id == "xLfoAmp" || id == "xLfoRate" || id == "yLfoAmp" || id == "yLfoRate" ||
                       id == "zLfoAmp" || id == "zLfoRate" || id == "rotateLfoAmp" || id == "rotateLfoRate" ||
                       id == "vMix" || id == "vHue" || id == "vSaturation" || id == "vBrightness" ||
                       id == "vTemporalFilterMix" || id == "vXDisplace" || id == "vYDisplace" ||
                       id == "vZDisplace" || id == "vRotate" || id == "vHueOffset" || id == "vHueLFO") {
                normalizedValue = normalizeValue(message.value, true); // -1 to 1
            } else if (id == "hueModulation" || id == "vHueModulation") {
                 normalizedValue = message.value / 32.0f; // Special scaling for hue modulation
            } else {
                // If it's not a known continuous float/int parameter, skip
                // (Toggles are handled by the switch above)
                continue;
            }

            // Apply scaling factors for specific parameters
            if (id == "xDisplace" || id == "vXDisplace" || id == "xLfoAmp" || id == "xLfoRate") scale = xScaling.getScale();
            else if (id == "yDisplace" || id == "vYDisplace" || id == "yLfoAmp" || id == "yLfoRate") scale = yScaling.getScale();
            else if (id == "zDisplace" || id == "vZDisplace" || id == "zLfoAmp" || id == "zLfoRate") scale = zScaling.getScale();
            else if (id == "rotate" || id == "vRotate" || id == "rotateLfoAmp" || id == "rotateLfoRate") scale = rotateScaling.getScale();
            else if (id == "hueModulation" || id == "vHueModulation") scale = hueModScaling.getScale();
            else if (id == "hueOffset" || id == "vHueOffset") scale = hueOffsetScaling.getScale();
            else if (id == "hueLFO" || id == "vHueLFO") scale = hueLfoScaling.getScale();

            normalizedValue *= scale;

            // Apply the value using the correct setter based on mode and ID
            if (videoReactiveMode) {
                if (id == "vLumakeyValue") paramManager->setVLumakeyValue(normalizedValue);
                else if (id == "vMix") paramManager->setVMix(normalizedValue);
                else if (id == "vHue") paramManager->setVHue(normalizedValue);
                else if (id == "vSaturation") paramManager->setVSaturation(normalizedValue);
                else if (id == "vBrightness") paramManager->setVBrightness(normalizedValue);
                else if (id == "vTemporalFilterMix") paramManager->setVTemporalFilterMix(normalizedValue);
                else if (id == "vTemporalFilterResonance") paramManager->setVTemporalFilterResonance(normalizedValue);
                else if (id == "vSharpenAmount") paramManager->setVSharpenAmount(normalizedValue);
                else if (id == "vXDisplace") paramManager->setVXDisplace(normalizedValue);
                else if (id == "vYDisplace") paramManager->setVYDisplace(normalizedValue);
                else if (id == "vZDisplace") paramManager->setVZDisplace(normalizedValue);
                else if (id == "vRotate") paramManager->setVRotate(normalizedValue);
                else if (id == "vHueModulation") paramManager->setVHueModulation(normalizedValue);
                else if (id == "vHueOffset") paramManager->setVHueOffset(normalizedValue);
                else if (id == "vHueLFO") paramManager->setVHueLFO(normalizedValue);
                // Note: DelayAmount, LFO Amp/Rate are not affected by videoReactiveMode in original code
            } else if (lfoAmpMode) {
                if (id == "xLfoAmp") paramManager->setXLfoAmp(normalizedValue);
                else if (id == "yLfoAmp") paramManager->setYLfoAmp(normalizedValue);
                else if (id == "zLfoAmp") paramManager->setZLfoAmp(normalizedValue);
                else if (id == "rotateLfoAmp") paramManager->setRotateLfoAmp(normalizedValue);
                // Note: Other params not affected by lfoAmpMode
            } else if (lfoRateMode) {
                if (id == "xLfoRate") paramManager->setXLfoRate(normalizedValue);
                else if (id == "yLfoRate") paramManager->setYLfoRate(normalizedValue);
                else if (id == "zLfoRate") paramManager->setZLfoRate(normalizedValue);
                else if (id == "rotateLfoRate") paramManager->setRotateLfoRate(normalizedValue);
                 // Note: Other params not affected by lfoRateMode
            } else {
                // Normal mode
                if (id == "lumakeyValue") paramManager->setLumakeyValue(normalizedValue);
                else if (id == "mix") paramManager->setMix(normalizedValue);
                else if (id == "hue") paramManager->setHue(normalizedValue);
                else if (id == "saturation") paramManager->setSaturation(normalizedValue);
                else if (id == "brightness") paramManager->setBrightness(normalizedValue);
                else if (id == "temporalFilterMix") paramManager->setTemporalFilterMix(normalizedValue);
                else if (id == "temporalFilterResonance") paramManager->setTemporalFilterResonance(normalizedValue);
                else if (id == "sharpenAmount") paramManager->setSharpenAmount(normalizedValue);
                else if (id == "xDisplace") paramManager->setXDisplace(normalizedValue);
                else if (id == "yDisplace") paramManager->setYDisplace(normalizedValue);
                else if (id == "zDisplace") paramManager->setZDisplace(normalizedValue);
                else if (id == "rotate") paramManager->setRotate(normalizedValue);
                else if (id == "hueModulation") paramManager->setHueModulation(normalizedValue);
                else if (id == "hueOffset") paramManager->setHueOffset(normalizedValue);
                else if (id == "hueLFO") paramManager->setHueLFO(normalizedValue);
                else if (id == "delayAmount") paramManager->setDelayAmount(static_cast<int>(normalizedValue * 100)); // Scale int delay
                // Note: LFO Amp/Rate setters are called directly in lfoAmp/Rate modes above
            }

            // Found and processed the mapping, no need to check further
            return;
        }
    }

    // If no mapping was found for this CC message, log it (optional)
    // ofLogVerbose("MidiManager") << "Unhandled MIDI CC: Ch=" << message.channel << " Ctrl=" << message.control << " Val=" << message.value;
}

float MidiManager::normalizeValue(int value, bool centered) const {
    if (centered) {
        // For values that should be centered around 0 (-1 to 1)
        return (value - MIDI_MAGIC) / MIDI_MAGIC;
    } else {
        // For values that range from 0 to 1
        return value / 127.0f;
    }
}
