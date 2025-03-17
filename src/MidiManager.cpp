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
    // Save preferred device name
    xml.setValue("midi:preferredDevice", getCurrentDeviceName());
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
    
    // Handle continuous controls
    bool videoReactiveMode = paramManager->isVideoReactiveEnabled();
    bool lfoAmpMode = paramManager->isLfoAmpModeEnabled();
    bool lfoRateMode = paramManager->isLfoRateModeEnabled();
    
    // Process based on control number
    switch (message.control) {
        // Lumakey value (CC 16)
        case 16:
            if (videoReactiveMode) {
                float normalizedValue = message.value / 127.0f;
                paramManager->setVLumakeyValue(normalizedValue);
            } else {
                float normalizedValue = message.value / 127.0f;
                paramManager->setLumakeyValue(normalizedValue);
            }
            break;
            
        // Mix (CC 17)
        case 17:
            if (videoReactiveMode) {
                float normalizedValue = normalizeValue(message.value, true);
                paramManager->setVMix(normalizedValue);
            } else {
                float normalizedValue = normalizeValue(message.value, true);
                paramManager->setMix(normalizedValue);
            }
            break;
            
        // Hue (CC 18)
        case 18:
            if (videoReactiveMode) {
                float normalizedValue = normalizeValue(message.value, true);
                paramManager->setVHue(normalizedValue);
            } else {
                float normalizedValue = normalizeValue(message.value, true);
                paramManager->setHue(normalizedValue);
            }
            break;
            
        // Saturation (CC 19)
        case 19:
            if (videoReactiveMode) {
                float normalizedValue = normalizeValue(message.value, true);
                paramManager->setVSaturation(normalizedValue);
            } else {
                float normalizedValue = normalizeValue(message.value, true);
                paramManager->setSaturation(normalizedValue);
            }
            break;
            
        // Brightness (CC 20)
        case 20:
            if (videoReactiveMode) {
                float normalizedValue = normalizeValue(message.value, true);
                paramManager->setVBrightness(normalizedValue);
            } else {
                float normalizedValue = normalizeValue(message.value, true);
                paramManager->setBrightness(normalizedValue);
            }
            break;
            
        // Temporal filter mix (CC 21)
        case 21:
            if (videoReactiveMode) {
                float normalizedValue = normalizeValue(message.value, true);
                paramManager->setVTemporalFilterMix(normalizedValue);
            } else {
                float normalizedValue = normalizeValue(message.value, true);
                paramManager->setTemporalFilterMix(normalizedValue);
            }
            break;
            
        // Temporal filter resonance (CC 22)
        case 22:
            if (videoReactiveMode) {
                float normalizedValue = message.value / 127.0f;
                paramManager->setVTemporalFilterResonance(normalizedValue);
            } else {
                float normalizedValue = message.value / 127.0f;
                paramManager->setTemporalFilterResonance(normalizedValue);
            }
            break;
            
        // Sharpen amount (CC 23)
        case 23:
            if (videoReactiveMode) {
                float normalizedValue = message.value / 127.0f;
                paramManager->setVSharpenAmount(normalizedValue);
            } else {
                float normalizedValue = message.value / 127.0f;
                paramManager->setSharpenAmount(normalizedValue);
            }
            break;
            
        // X displacement (CC 120)
        case 120:
            {
                float scale = xScaling.getScale();
                
                if (lfoAmpMode) {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setXLfoAmp(normalizedValue);
                } else if (lfoRateMode) {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setXLfoRate(normalizedValue);
                } else if (videoReactiveMode) {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setVXDisplace(normalizedValue);
                } else {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setXDisplace(normalizedValue);
                }
            }
            break;
            
        // Y displacement (CC 121)
        case 121:
            {
                float scale = yScaling.getScale();
                
                if (lfoAmpMode) {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setYLfoAmp(normalizedValue);
                } else if (lfoRateMode) {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setYLfoRate(normalizedValue);
                } else if (videoReactiveMode) {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setVYDisplace(normalizedValue);
                } else {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setYDisplace(normalizedValue);
                }
            }
            break;
            
        // Z displacement (CC 122)
        case 122:
            {
                float scale = zScaling.getScale();
                
                if (lfoAmpMode) {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setZLfoAmp(normalizedValue);
                } else if (lfoRateMode) {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setZLfoRate(normalizedValue);
                } else if (videoReactiveMode) {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setVZDisplace(normalizedValue);
                } else {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setZDisplace(normalizedValue);
                }
            }
            break;
            
        // Rotation (CC 123)
        case 123:
            {
                float scale = rotateScaling.getScale();
                
                if (lfoAmpMode) {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setRotateLfoAmp(normalizedValue);
                } else if (lfoRateMode) {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setRotateLfoRate(normalizedValue);
                } else if (videoReactiveMode) {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setVRotate(normalizedValue);
                } else {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setRotate(normalizedValue);
                }
            }
            break;
            
        // Hue modulation (CC 124)
        case 124:
            {
                float scale = hueModScaling.getScale();
                
                if (videoReactiveMode) {
                    float normalizedValue = message.value / (32.0f * scale);
                    paramManager->setVHueModulation(normalizedValue);
                } else {
                    float normalizedValue = message.value / (32.0f * scale);
                    paramManager->setHueModulation(normalizedValue);
                }
            }
            break;
            
        // Hue offset (CC 125)
        case 125:
            {
                float scale = hueOffsetScaling.getScale();
                
                if (videoReactiveMode) {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setVHueOffset(normalizedValue);
                } else {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setHueOffset(normalizedValue);
                }
            }
            break;
            
        // Hue LFO (CC 126)
        case 126:
            {
                float scale = hueLfoScaling.getScale();
                
                if (videoReactiveMode) {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setVHueLFO(normalizedValue);
                } else {
                    float normalizedValue = normalizeValue(message.value, true) * scale;
                    paramManager->setHueLFO(normalizedValue);
                }
            }
            break;
            
        // Delay amount (CC 127)
        case 127:
            {
                if (!videoReactiveMode) {
                    float normalizedValue = message.value / 127.0f;
                    paramManager->setDelayAmount(normalizedValue * 100); // Scale to reasonable range
                }
            }
            break;
    }
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
