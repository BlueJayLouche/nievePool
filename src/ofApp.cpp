#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {
    // Set up basic OF settings
    ofSetFrameRate(30);
    ofSetVerticalSync(true);
    ofBackground(0);
    ofHideCursor();
    
    ofDisableArbTex();
    
    // Initialize parameter manager first
    paramManager = std::make_unique<ParameterManager>();
    paramManager->setup();
    
    // Initialize shader manager
    shaderManager = std::make_unique<ShaderManager>();
    shaderManager->setup();
    
    // Initialize video feedback manager with references to other managers
    videoManager = std::make_unique<VideoFeedbackManager>(paramManager.get(), shaderManager.get());
    videoManager->setup(width, height);
    
    // Initialize MIDI manager with reference to parameter manager
    midiManager = std::make_unique<MidiManager>(paramManager.get());
    midiManager->setup();
    
    // Initialize Audio Reactivity manager with reference to parameter manager
    audioManager = std::make_unique<AudioReactivityManager>(paramManager.get());
    audioManager->setup();
    
    // Try to load audio settings from XML
    ofxXmlSettings xml;
    if (xml.loadFile(ofToDataPath("settings.xml"))) {
        audioManager->loadFromXml(xml);
    }
    
    // Add some default mappings if none exist
    if (audioManager->getMappings().empty()) {
        ofLogNotice("ofApp") << "Setting up default audio mappings";
        
        // Sub bass affects z_displace (create depth pulsing)
        AudioReactivityManager::BandMapping bassMapping;
        bassMapping.band = 0;  // Sub bass
        bassMapping.paramId = "z_displace";
        bassMapping.scale = 0.25f;
        bassMapping.min = 0.0f;
        bassMapping.max = 1.0f;
        bassMapping.additive = true;
        audioManager->addMapping(bassMapping);
        
        // Low mids affect x_displace
        AudioReactivityManager::BandMapping lowMidsMapping;
        lowMidsMapping.band = 2;  // Low mids
        lowMidsMapping.paramId = "x_displace";
        lowMidsMapping.scale = 0.01f;
        lowMidsMapping.min = -0.1f;
        lowMidsMapping.max = 0.1f;
        lowMidsMapping.additive = true;
        audioManager->addMapping(lowMidsMapping);
        
        // Mids affect y_displace
        AudioReactivityManager::BandMapping midsMapping;
        midsMapping.band = 3;  // Mids
        midsMapping.paramId = "y_displace";
        midsMapping.scale = 0.01f;
        midsMapping.min = -0.1f;
        midsMapping.max = 0.1f;
        midsMapping.additive = true;
        audioManager->addMapping(midsMapping);
        
        // Presence affects saturation
        AudioReactivityManager::BandMapping presenceMapping;
        presenceMapping.band = 5;  // Presence (4-6kHz)
        presenceMapping.paramId = "saturation";
        presenceMapping.scale = 0.5f;
        presenceMapping.min = 0.8f;
        presenceMapping.max = 1.2f;
        presenceMapping.additive = false;  // Directly set the value
        audioManager->addMapping(presenceMapping);
        
        // Brilliance affects hue modulation
        AudioReactivityManager::BandMapping brillianceMapping;
        brillianceMapping.band = 6;  // Brilliance (6-12kHz)
        brillianceMapping.paramId = "hue_modulation";
        brillianceMapping.scale = 0.5f;
        brillianceMapping.min = 0.9f;
        brillianceMapping.max = 1.2f;
        brillianceMapping.additive = true;
        audioManager->addMapping(brillianceMapping);
    }
    
    // Initialize performance monitoring
    for (int i = 0; i < 60; i++) {
        frameRateHistory[i] = 0.0f;
    }
}

//--------------------------------------------------------------
void ofApp::update() {
    // Update managers
    paramManager->update();
    midiManager->update();
    videoManager->update();
    audioManager->update(); // Update audio manager
    
    // Update performance monitoring
    frameRateHistory[frameRateIndex] = ofGetFrameRate();
    frameRateIndex = (frameRateIndex + 1) % 60;
}

//--------------------------------------------------------------
void ofApp::draw() {
    // Draw video feedback
    videoManager->draw();
    
    // Draw debug info if enabled
    if (debugEnabled) {
        drawDebugInfo();
    }
}

//--------------------------------------------------------------
void ofApp::exit() {
    // Clean shutdown of audio
    audioManager->exit();
    
    // Save all settings
    ofxXmlSettings xml;
    
    // Load existing settings if available
    xml.loadFile(ofToDataPath("settings.xml"));
    
    // Save parameter settings
    paramManager->saveToXml(xml);
    
    // Save audio settings
    audioManager->saveToXml(xml);
    
    // Save entire file
    xml.saveFile(ofToDataPath("settings.xml"));
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
    // Check for shift key
    bool shiftPressed = ofGetKeyPressed(OF_KEY_SHIFT);
    
    // Process key controls
    switch (key) {
        // Debug toggle
        case '`':
            debugEnabled = !debugEnabled;
            break;
            
        // Audio enabling/disabling (capital A)
        case 'A':
            audioManager->setEnabled(!audioManager->isEnabled());
            break;

        // Toggle audio normalization (capital N)
        case 'N':
            audioManager->setNormalizationEnabled(!audioManager->isNormalizationEnabled());
            break;

        // Increase audio sensitivity
        case '+':
            if (shiftPressed) {
                audioManager->setSensitivity(audioManager->getSensitivity() + 0.1f);
            }
            break;

        // Decrease audio sensitivity
        case '-':
            if (shiftPressed) {
                audioManager->setSensitivity(audioManager->getSensitivity() - 0.1f);
            }
            break;

        // Previous audio device - use '<' instead of '['
        case '<':
            if (shiftPressed) {
                int current = audioManager->getCurrentDeviceIndex();
                if (current > 0) {
                    audioManager->selectAudioDevice(current - 1);
                }
            }
            break;

        // Next audio device - use '>' instead of ']'
        case '>':
            if (shiftPressed) {
                int current = audioManager->getCurrentDeviceIndex();
                audioManager->selectAudioDevice(current + 1);
            }
            break;
        
        // Parameter controls
        case 'a':
            paramManager->setLumakeyValue(paramManager->getLumakeyValue() + 0.01f);
            break;
        case 'z':
            paramManager->setLumakeyValue(paramManager->getLumakeyValue() - 0.01f);
            break;
            
        case 's':
            paramManager->setZFrequency(paramManager->getZFrequency() + 0.0001f);
            break;
        case 'x':
            paramManager->setZFrequency(paramManager->getZFrequency() - 0.0001f);
            break;
            
        case 'd':
            paramManager->setYDisplace(paramManager->getYDisplace() + 0.0001f);
            break;
        case 'c':
            paramManager->setYDisplace(paramManager->getYDisplace() - 0.0001f);
            break;
            
        case 'f':
            paramManager->setHue(paramManager->getHue() + 0.001f);
            break;
        case 'v':
            paramManager->setHue(paramManager->getHue() - 0.001f);
            break;
            
        case 'g':
            paramManager->setSaturation(paramManager->getSaturation() + 0.001f);
            break;
        case 'b':
            paramManager->setSaturation(paramManager->getSaturation() - 0.001f);
            break;
            
        case 'h':
            paramManager->setBrightness(paramManager->getBrightness() + 0.001f);
            break;
        case 'n':
            paramManager->setBrightness(paramManager->getBrightness() - 0.001f);
            break;
            
        case 'j':
            paramManager->setMix(paramManager->getMix() + 0.01f);
            break;
        case 'm':
            paramManager->setMix(paramManager->getMix() - 0.01f);
            break;
            
        case 'k':
            paramManager->setLumakeyValue(ofClamp(paramManager->getLumakeyValue() + 0.01f, 0.0f, 1.0f));
            break;
        case ',':
            paramManager->setLumakeyValue(ofClamp(paramManager->getLumakeyValue() - 0.01f, 0.0f, 1.0f));
            break;
            
        case 'l':
            paramManager->setSharpenAmount(paramManager->getSharpenAmount() + 0.01f);
            break;
        case '.':
            paramManager->setSharpenAmount(paramManager->getSharpenAmount() - 0.01f);
            break;
            
        case ';':
            paramManager->setTemporalFilterResonance(paramManager->getTemporalFilterResonance() + 0.01f);
            break;
        case '\'':
            paramManager->setTemporalFilterResonance(paramManager->getTemporalFilterResonance() - 0.01f);
            break;
            
        case 'q':
            paramManager->setRotate(paramManager->getRotate() + 0.0001f);
            break;
        case 'w':
            paramManager->setRotate(paramManager->getRotate() - 0.0001f);
            break;
            
        case 'e':
            paramManager->setHueModulation(paramManager->getHueModulation() + 0.001f);
            break;
        case 'r':
            paramManager->setHueModulation(paramManager->getHueModulation() - 0.001f);
            break;
            
        case 't':
            paramManager->setHueOffset(paramManager->getHueOffset() + 0.01f);
            break;
        case 'y':
            paramManager->setHueOffset(paramManager->getHueOffset() - 0.01f);
            break;
            
        case 'u':
            paramManager->setHueLFO(paramManager->getHueLFO() + 0.01f);
            break;
        case 'i':
            paramManager->setHueLFO(paramManager->getHueLFO() - 0.01f);
            break;
            
        case 'o':
            paramManager->setTemporalFilterMix(paramManager->getTemporalFilterMix() + 0.01f);
            break;
        case 'p':
            paramManager->setTemporalFilterMix(paramManager->getTemporalFilterMix() - 0.01f);
            break;
            
        // Frame buffer delay controls
        case '[':
            paramManager->setDelayAmount(paramManager->getDelayAmount() + 1);
            break;
        case ']':
            {
                int delay = paramManager->getDelayAmount() - 1;
                if (delay < 0) {
                    delay = videoManager->getFrameBufferLength() - delay;
                }
                paramManager->setDelayAmount(delay);
            }
            break;
            
        // Reset all parameters
        case '!':
            paramManager->resetToDefaults();
            break;
            
        // LFO mode controls
        case '1':
            if (shiftPressed) {
                paramManager->setLfoAmpModeEnabled(true);
                if (paramManager->isRecordingEnabled()) {
                    paramManager->setRecordingEnabled(false);
                }
                if (paramManager->isVideoReactiveEnabled()) {
                    paramManager->setVideoReactiveEnabled(false);
                }
            }
            break;
            
        case '2':
            if (shiftPressed) {
                paramManager->setLfoRateModeEnabled(true);
                if (paramManager->isRecordingEnabled()) {
                    paramManager->setRecordingEnabled(false);
                }
                if (paramManager->isVideoReactiveEnabled()) {
                    paramManager->setVideoReactiveEnabled(false);
                }
            }
            break;
            
        // Reset all LFOs
        case '0':
            paramManager->setXLfoAmp(0.0f);
            paramManager->setXLfoRate(0.0f);
            paramManager->setYLfoAmp(0.0f);
            paramManager->setYLfoRate(0.0f);
            paramManager->setZLfoAmp(0.0f);
            paramManager->setZLfoRate(0.0f);
            paramManager->setRotateLfoAmp(0.0f);
            paramManager->setRotateLfoRate(0.0f);
            break;
    }
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key) {
    bool shiftPressed = ofGetKeyPressed(OF_KEY_SHIFT);
    
    // Handle key release events
    switch (key) {
        case '1':
            if (shiftPressed) {
                paramManager->setLfoAmpModeEnabled(false);
                paramManager->setRecordingEnabled(true);
            }
            break;
            
        case '2':
            if (shiftPressed) {
                paramManager->setLfoRateModeEnabled(false);
                paramManager->setRecordingEnabled(true);
            }
            break;
    }
}

//--------------------------------------------------------------
void ofApp::drawDebugInfo() {
    ofPushStyle();
    ofSetColor(255, 255, 0); // Yellow text
    
    int x = 10;
    int y = 20;
    int lineHeight = 15;
    
    // FPS information
    string fpsStr = "FPS: " + ofToString(ofGetFrameRate(), 1);
    ofDrawBitmapString(fpsStr, x, y);
    y += lineHeight;
    
    // Calculate average FPS
    float avgFps = 0;
    for (int i = 0; i < 60; i++) {
        avgFps += frameRateHistory[i];
    }
    avgFps /= 60.0f;
    
    string avgFpsStr = "Avg FPS: " + ofToString(avgFps, 1);
    ofDrawBitmapString(avgFpsStr, x, y);
    y += lineHeight;
    
    // Draw FPS graph
    int graphWidth = 120;
    int graphHeight = 40;
    
    ofSetColor(50);
    ofDrawRectangle(x, y, graphWidth, graphHeight);
    
    ofSetColor(0, 255, 0);
    for (int i = 0; i < 60; i++) {
        float barHeight = ofMap(frameRateHistory[i], 0, 60, 0, graphHeight);
        ofDrawLine(x + i * 2, y + graphHeight, x + i * 2, y + graphHeight - barHeight);
    }
    
    y += graphHeight + lineHeight;
    
    // Draw key parameters
    ofSetColor(255, 255, 0);
    ofDrawBitmapString("--- Parameters ---", x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Lumakey: " + ofToString(paramManager->getLumakeyValue(), 3), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Mix: " + ofToString(paramManager->getMix(), 3), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Hue: " + ofToString(paramManager->getHue(), 3), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Saturation: " + ofToString(paramManager->getSaturation(), 3), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Brightness: " + ofToString(paramManager->getBrightness(), 3), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("X Displace: " + ofToString(paramManager->getXDisplace(), 3), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Y Displace: " + ofToString(paramManager->getYDisplace(), 3), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Z Displace: " + ofToString(paramManager->getZDisplace(), 3), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Rotate: " + ofToString(paramManager->getRotate(), 3), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Delay: " + ofToString(paramManager->getDelayAmount()), x, y);
    y += lineHeight;
    
    // Draw audio debug info in the middle column
    drawAudioDebugInfo(ofGetWidth() / 2 - 100, 20, lineHeight);
    
    // Toggles
    y += lineHeight;
    ofSetColor(255, 255, 0);
    ofDrawBitmapString("--- Toggles ---", x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Hue Invert: " + ofToString(paramManager->isHueInverted()), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Sat Invert: " + ofToString(paramManager->isSaturationInverted()), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Bright Invert: " + ofToString(paramManager->isBrightnessInverted()), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Horiz Mirror: " + ofToString(paramManager->isHorizontalMirrorEnabled()), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Vert Mirror: " + ofToString(paramManager->isVerticalMirrorEnabled()), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Toroid: " + ofToString(paramManager->isToroidEnabled()), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Wet Mode: " + ofToString(paramManager->isWetModeEnabled()), x, y);
    y += lineHeight;
    
    // Mode info
    y += lineHeight;
    ofDrawBitmapString("--- Modes ---", x, y);
    y += lineHeight;
    
    ofDrawBitmapString("P-Lock Recording: " + ofToString(paramManager->isRecordingEnabled()), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Video Reactive: " + ofToString(paramManager->isVideoReactiveEnabled()), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("LFO Amp Mode: " + ofToString(paramManager->isLfoAmpModeEnabled()), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("LFO Rate Mode: " + ofToString(paramManager->isLfoRateModeEnabled()), x, y);
    y += lineHeight;
    
    // MIDI info on the right side
    int rightX = ofGetWidth() - 250;
    y = 20;
    
    ofDrawBitmapString("--- MIDI Info ---", rightX, y);
    y += lineHeight;
    
    ofDrawBitmapString("Device: " + midiManager->getCurrentDeviceName(), rightX, y);
    y += lineHeight;
    
    // Recent MIDI messages
    ofDrawBitmapString("Recent messages:", rightX, y);
    y += lineHeight;
    
    auto midiMessages = midiManager->getRecentMessages();
    int maxMessages = std::min(static_cast<int>(midiMessages.size()), 5);
    
    if (midiMessages.size() > 0) {
        for (int i = midiMessages.size() - maxMessages; i < midiMessages.size(); i++) {
            const auto& msg = midiMessages[i];
            std::string msgType;
            
            switch (msg.status) {
                case MIDI_NOTE_ON: msgType = "Note On"; break;
                case MIDI_NOTE_OFF: msgType = "Note Off"; break;
                case MIDI_CONTROL_CHANGE: msgType = "CC"; break;
                case MIDI_PROGRAM_CHANGE: msgType = "Program"; break;
                case MIDI_PITCH_BEND: msgType = "Pitch Bend"; break;
                case MIDI_AFTERTOUCH: msgType = "Aftertouch"; break;
                case MIDI_POLY_AFTERTOUCH: msgType = "Poly AT"; break;
                default: msgType = "Other"; break;
            }
            
            std::string msgInfo = msgType + " Ch:" + ofToString(msg.channel) +
                                " Ctrl:" + ofToString(msg.control) +
                                " Val:" + ofToString(msg.value);
            
            ofDrawBitmapString(msgInfo, rightX, y);
            y += lineHeight;
        }
    } else {
        ofDrawBitmapString("No MIDI messages received", rightX, y);
        y += lineHeight;
    }
    
    // Help text at bottom
    ofSetColor(180, 180, 255);
    ofDrawBitmapString("Press ` to toggle debug display", x, ofGetHeight() - 45);
    ofDrawBitmapString("Press A to toggle audio reactivity", x, ofGetHeight() - 30);
    ofDrawBitmapString("Press N to toggle audio normalization", x, ofGetHeight() - 15);
    ofDrawBitmapString("Use Shift+< and Shift+> to change audio device", x + 300, ofGetHeight() - 30);
    ofDrawBitmapString("Use Shift++ and Shift+- to adjust audio sensitivity", x + 300, ofGetHeight() - 15);

    ofPopStyle();
}


//--------------------------------------------------------------
void ofApp::drawAudioDebugInfo(int x, int y, int lineHeight) {
    // Title
    ofSetColor(255, 255, 0);
    ofDrawBitmapString("--- Audio Reactivity ---", x, y);
    y += lineHeight;
    
    // Enabled state
    ofDrawBitmapString("Enabled: " + ofToString(audioManager->isEnabled()), x, y);
    y += lineHeight;
    
    // Audio device info
    ofDrawBitmapString("Device: " + audioManager->getCurrentDeviceName(), x, y);
    y += lineHeight;
    
    // Audio level
    float level = audioManager->getAudioInputLevel();
    ofDrawBitmapString("Level: " + ofToString(level, 3), x, y);
    y += lineHeight;
    
    // Audio settings
    ofDrawBitmapString("Sensitivity: " + ofToString(audioManager->getSensitivity(), 2), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Smoothing: " + ofToString(audioManager->getSmoothing(), 2), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Normalization: " + ofToString(audioManager->isNormalizationEnabled()), x, y);
    y += lineHeight;
    
    // Draw frequency bands
    y += lineHeight;
    ofDrawBitmapString("Frequency Bands:", x, y);
    y += lineHeight;
    
    // Draw band visualization
    int numBands = audioManager->getNumBands();
    std::vector<float> bands = audioManager->getAllBands();
    
    int bandWidth = 20;
    int bandHeight = 100;
    int bandSpacing = 5;
    
    ofSetColor(50);
    ofDrawRectangle(x, y, (bandWidth + bandSpacing) * numBands, bandHeight);
    
    for (int i = 0; i < numBands; i++) {
        // Choose color based on band index (rainbow effect)
        ofColor bandColor;
        float hue = (float)i / numBands * 255.0f;
        bandColor.setHsb(hue, 200, 255);
        
        ofSetColor(bandColor);
        
        float bandValue = bands[i];
        float barHeight = bandValue * bandHeight;
        
        int bandX = x + i * (bandWidth + bandSpacing);
        int bandY = y + bandHeight - barHeight;
        
        ofDrawRectangle(bandX, bandY, bandWidth, barHeight);
        
        // Draw band number
        ofSetColor(255);
        ofDrawBitmapString(ofToString(i), bandX + 5, y + bandHeight + 15);
    }
    
    // List active mappings
    y += bandHeight + 25;
    ofSetColor(255, 255, 0);
    ofDrawBitmapString("Active Mappings:", x, y);
    y += lineHeight;
    
    auto mappings = audioManager->getMappings();
    if (mappings.empty()) {
        ofDrawBitmapString("No mappings defined", x, y);
    } else {
        for (int i = 0; i < std::min(5, (int)mappings.size()); i++) {
            const auto& mapping = mappings[i];
            std::string modeStr = mapping.additive ? "Add" : "Set";
            
            ofDrawBitmapString("Band " + ofToString(mapping.band) + " -> " +
                              mapping.paramId + " (" + modeStr + ")", x, y);
            y += lineHeight;
        }
        
        if (mappings.size() > 5) {
            ofDrawBitmapString("... and " + ofToString(mappings.size() - 5) + " more", x, y);
        }
    }
}
