#pragma once

#include "ofMain.h"
#include "ofxMidi.h"
#include "ParameterManager.h"
#include "ofxXmlSettings.h"

/**
 * @class MidiManager
 * @brief Handles MIDI input and maps controls to parameters
 *
 * This class manages MIDI device connections, message processing,
 * and mapping of MIDI controls to effect parameters.
 */
class MidiManager : public ofxMidiListener {
public:
    MidiManager(ParameterManager* paramManager);
    ~MidiManager();
    
    // Core methods
    void setup();
    void update();
    
    // MIDI device handling
    void scanForDevices();
    bool connectToDevice(int deviceIndex);
    bool connectToDevice(const std::string& deviceName);
    void disconnectCurrentDevice();
    
    // MIDI message handling
    void newMidiMessage(ofxMidiMessage& message) override;
    
    // Message queue access
    std::vector<ofxMidiMessage> getRecentMessages() const;
    
    // Device information
    std::vector<std::string> getAvailableDevices() const;
    std::string getCurrentDeviceName() const;
    int getCurrentDeviceIndex() const;
    std::string getPreferredDeviceName() const; // Added getter for preferred name
    
    // Settings
    void loadSettings(ofxXmlSettings& xml);
    void saveSettings(ofxXmlSettings& xml) const;
    
private:
    // Constants
    static constexpr float MIDI_MAGIC = 63.50f;
    static constexpr float CONTROL_THRESHOLD = 0.04f;
    
    // Message processing methods
    void processControlChange(const ofxMidiMessage& message);
    float normalizeValue(int value, bool centered = false) const;
    
    // MIDI input
    ofxMidiIn midiIn;
    std::vector<ofxMidiMessage> midiMessages;
    size_t maxMessages = 10;
    
    // Device management
    std::vector<std::string> availableDevices;
    int currentDeviceIndex = -1;
    std::string preferredDeviceName;
    
    // Connection state
    bool isDeviceConnected = false;
    float lastDeviceScanTime = 0;
    const float DEVICE_SCAN_INTERVAL = 2.0f; // seconds
    
    // Reference to parameter manager
    ParameterManager* paramManager;
    
    // Scaling helpers for MIDI controls
    struct ScalingHelper {
        bool times2 = false;
        bool times5 = false;
        bool times10 = false;
        
        float getScale() const {
            if (times10) return 10.0f;
            if (times5) return 5.0f;
            if (times2) return 2.0f;
            return 1.0f;
        }
    };
    
    ScalingHelper xScaling;
    ScalingHelper yScaling;
    ScalingHelper zScaling;
    ScalingHelper rotateScaling;
    ScalingHelper hueModScaling;
    ScalingHelper hueOffsetScaling;
    ScalingHelper hueLfoScaling;
};
