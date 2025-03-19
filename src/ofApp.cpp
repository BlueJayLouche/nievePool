#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {
    // Check for platform-specific settings
    #if defined(TARGET_LINUX) && (defined(__arm__) || defined(__aarch64__))
        // Enable performance mode by default on Raspberry Pi
        bool performanceMode = true;
        int targetFramerate = 24;
        bool platformIsRaspberryPi = true;
        ofLogNotice("ofApp") << "Detected Raspberry Pi: enabling performance mode";
    #else
        bool performanceMode = false;
        int targetFramerate = 30;
        bool platformIsRaspberryPi = false;
    #endif
    
    // Set framerate based on platform
    ofSetFrameRate(targetFramerate);
    
    // Continue with normal setup...
    ofSetVerticalSync(true);
    ofBackground(0);
    ofHideCursor();
    
    // Initialize parameter manager first
    paramManager = std::make_unique<ParameterManager>();
    paramManager->setup();
    
    // Set performance mode based on platform
    if (platformIsRaspberryPi) {
        paramManager->setPerformanceModeEnabled(true);
        paramManager->setPerformanceScale(30);  // Lower resolution in performance mode
        paramManager->setHighQualityEnabled(false);
        ofSetFrameRate(24);  // Lower framerate for better performance
    } else {
        ofSetFrameRate(30);
    }
    
    // Check if settings file exists and validate structure
    ofFile settingsFile(ofToDataPath("settings.xml"));
    bool resetNeeded = false;
    
    if (settingsFile.exists()) {
        // Load file to check structure
        ofxXmlSettings testXml;
        if (testXml.loadFile(ofToDataPath("settings.xml"))) {
            // Check for nested paramManager tags
            if (testXml.pushTag("paramManager")) {
                if (testXml.tagExists("paramManager")) {
                    // Found nested paramManager, need reset
                    resetNeeded = true;
                    ofLogWarning("ofApp") << "Nested paramManager tags detected, resetting settings file";
                }
                testXml.popTag();
            }
        } else {
            // Couldn't load file, reset it
            resetNeeded = true;
            ofLogWarning("ofApp") << "Could not parse settings.xml, resetting file";
        }
    }
    
    if (resetNeeded || !settingsFile.exists()) {
        resetSettingsFile();
    }
    
    // Load app-level settings first (outside the paramManager tag)
    ofxXmlSettings xml;
    if (xml.loadFile(ofToDataPath("settings.xml"))) {
        // First try to load app settings from root level
        if (xml.tagExists("app")) {
            xml.pushTag("app");
            
            // Load application settings
            debugEnabled = xml.getValue("debugEnabled", false);
            configWidth = xml.getValue("width", 1024);
            configHeight = xml.getValue("height", 768);
            configFrameRate = xml.getValue("frameRate", 30);
            
            xml.popTag(); // pop app tag
            
            // Set framerate and window size based on config
            ofSetFrameRate(configFrameRate);
            ofSetWindowShape(configWidth, configHeight);
        }
    }
    
    // Continue with the standard setup
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
    videoManager->setup(configWidth, configHeight);  // Use config dimensions
    
    // Initialize MIDI manager with reference to parameter manager
    midiManager = std::make_unique<MidiManager>(paramManager.get());
    midiManager->setup();
    
    // Initialize Audio Reactivity manager with reference to parameter manager
    audioManager = std::make_unique<AudioReactivityManager>(paramManager.get());
    audioManager->setup();
    
    // Try to load settings from XML with proper structure
    if (xml.loadFile(ofToDataPath("settings.xml"))) {
        ofLogNotice("ofApp") << "Loading settings from settings.xml";
        
        try {
            // Load parameter manager settings first
            paramManager->loadFromXml(xml);
            
            // For other managers, we need to navigate to the paramManager tag first
            if (xml.pushTag("paramManager")) {
                // Load audio settings if present
                if (xml.tagExists("audioReactivity")) {
                    audioManager->loadFromXml(xml);
                } else {
                    ofLogWarning("ofApp") << "No audioReactivity section found in settings";
                }
                
                // Load video settings if present
                if (xml.tagExists("videoFeedback")) {
                    videoManager->loadFromXml(xml);
                } else {
                    ofLogWarning("ofApp") << "No videoFeedback section found in settings";
                }
                
                // Load MIDI settings if present
                if (xml.tagExists("midi")) {
                    midiManager->loadSettings(xml);
                } else {
                    ofLogWarning("ofApp") << "No midi section found in settings";
                }
                
                xml.popTag(); // Pop paramManager tag
            } else {
                ofLogWarning("ofApp") << "No paramManager tag found in settings.xml";
            }
        } catch (const std::exception& e) {
            ofLogError("ofApp") << "Exception loading settings: " << e.what();
        }
    } else {
        ofLogNotice("ofApp") << "No settings.xml found or couldn't load it, using defaults";
        
        // Add default audio mappings if none exist
        if (audioManager->getMappings().empty()) {
            setupDefaultAudioMappings();
        }
    }
    
    // Initialize performance monitoring
    for (int i = 0; i < 60; i++) {
        frameRateHistory[i] = 0.0f;
    }
}


// Add a helper method for default audio mappings
void ofApp::setupDefaultAudioMappings() {
    ofLogNotice("ofApp") << "Setting up default audio mappings";
    
    // Sub bass affects z_displace (create depth pulsing)
    AudioReactivityManager::BandMapping bassMapping;
    bassMapping.band = 0;  // Sub bass
    bassMapping.paramId = "z_displace";
    bassMapping.scale = 0.5f;
    bassMapping.min = -0.2f;
    bassMapping.max = 0.2f;
    bassMapping.additive = false;
    audioManager->addMapping(bassMapping);
    
    // Add more default mappings...
    
    // Low mids affect x_displace
    AudioReactivityManager::BandMapping lowMidsMapping;
    lowMidsMapping.band = 2;  // Low mids
    lowMidsMapping.paramId = "x_displace";
    lowMidsMapping.scale = 0.05f;
    lowMidsMapping.min = -0.1f;
    lowMidsMapping.max = 0.1f;
    lowMidsMapping.additive = false;
    audioManager->addMapping(lowMidsMapping);
    
    // Mids affect y_displace
    AudioReactivityManager::BandMapping midsMapping;
    midsMapping.band = 3;  // Mids
    midsMapping.paramId = "y_displace";
    midsMapping.scale = 0.5f;
    midsMapping.min = -0.1f;
    midsMapping.max = 0.1f;
    midsMapping.additive = false;
    audioManager->addMapping(midsMapping);
    
    // High mids affect hue
    AudioReactivityManager::BandMapping highMidsMapping;
    highMidsMapping.band = 4;  // High mids
    highMidsMapping.paramId = "hue";
    highMidsMapping.scale = 0.01f;
    highMidsMapping.min = 0.8f;
    highMidsMapping.max = 1.2f;
    highMidsMapping.additive = false;  // Direct setting works better for hue
    audioManager->addMapping(highMidsMapping);
}

void ofApp::resetSettingsFile() {
    ofxXmlSettings xml;
    
    // Create a clean XML structure
    xml.addTag("app");
    xml.pushTag("app");
    xml.setValue("version", "1.0.0");
    xml.setValue("lastSaved", ofGetTimestampString());
    xml.setValue("debugEnabled", debugEnabled ? 1 : 0);
    xml.setValue("width", configWidth);
    xml.setValue("height", configHeight);
    xml.setValue("frameRate", configFrameRate);
    xml.popTag(); // pop app
    
    // Add empty paramManager tag
    xml.addTag("paramManager");
    
    // Save with error handling
    try {
        bool saved = xml.saveFile(ofToDataPath("settings.xml"));
        ofLogNotice("ofApp") << "Settings file reset " << (saved ? "successfully" : "unsuccessfully");
    } catch (const std::exception& e) {
        ofLogError("ofApp") << "Exception resetting settings: " << e.what();
    }
}

//--------------------------------------------------------------
void ofApp::update() {
    float startTime = ofGetElapsedTimef();
    
    // Update managers
    paramManager->update();
    midiManager->update();
    
    // Debug camera status occasionally
    static int debugCounter = 0;
    if (debugEnabled && ++debugCounter % 300 == 0) {
        if (videoManager->cameraInitialized && videoManager->camera.isInitialized()) {
            ofLogNotice("ofApp") << "Camera is initialized, dimensions: "
                               << videoManager->camera.getWidth() << "x"
                               << videoManager->camera.getHeight();
            ofLogNotice("ofApp") << "New frame available: "
                               << (videoManager->camera.isFrameNew() ? "Yes" : "No");
        } else {
            ofLogWarning("ofApp") << "Camera NOT initialized";
        }
    }
    
    // Only process video at display framerate
    if (frameCounter % 1 == 0) { // Adjust divisor for lower processing rate
        videoManager->update();
    }
    
    audioManager->update();
    
    // Calculate frame time
    float endTime = ofGetElapsedTimef();
    lastFrameTime = (endTime - startTime) * 1000.0f; // Convert to ms
    
    // Update frame time history
    frameTimeHistory.push_back(lastFrameTime);
    if (frameTimeHistory.size() > 150) {
        frameTimeHistory.pop_front();
    }
    
    // Calculate average (last 150 frames)
    averageFrameTime = 0;
    for (float time : frameTimeHistory) {
        averageFrameTime += time;
    }
    averageFrameTime /= frameTimeHistory.size();
    
    frameCounter++;
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
    
    // Create a new XML document
    ofxXmlSettings xml;
    
    // Check if we can load the existing file to preserve settings
    bool existingFile = xml.loadFile(ofToDataPath("settings.xml"));
    
    // Update app settings (preserving existing values if file loaded)
    if (!xml.tagExists("app")) {
        xml.addTag("app");
    }
    xml.pushTag("app");
    
    // Only update these values if they don't exist or if file didn't load
    if (!existingFile || !xml.tagExists("version")) {
        xml.setValue("version", "1.0.0");
    }
    
    // Always update lastSaved timestamp
    xml.setValue("lastSaved", ofGetTimestampString());
    
    // Update current runtime values
    xml.setValue("debugEnabled", debugEnabled ? 1 : 0);
    xml.setValue("width", ofGetWidth());      // Save current window dimensions
    xml.setValue("height", ofGetHeight());
    xml.setValue("frameRate", (int)ofGetTargetFrameRate());
    
    xml.popTag(); // pop app
    
    // Ensure paramManager tag exists
    if (!xml.tagExists("paramManager")) {
        xml.addTag("paramManager");
    }
    
    // Let each manager save its settings
    paramManager->saveToXml(xml);
    
    // Other managers should now save inside the paramManager tag
    if (xml.pushTag("paramManager")) {
        audioManager->saveToXml(xml);
        videoManager->saveToXml(xml);
        midiManager->saveSettings(xml);
        xml.popTag(); // pop paramManager
    }
    
    // Save entire file with error handling
    try {
        bool saved = xml.saveFile(ofToDataPath("settings.xml"));
        ofLogNotice("ofApp") << "Settings saved " << (saved ? "successfully" : "unsuccessfully") << " to settings.xml";
    } catch (const std::exception& e) {
        ofLogError("ofApp") << "Exception saving settings: " << e.what();
    }
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
            
        // Video device controls
        case '<':
            if (shiftPressed) {
                int current = videoManager->getCurrentVideoDeviceIndex();
                if (current > 0) {
                    videoManager->selectVideoDevice(current - 1);
                }
            }
            break;
            
        case '>':
            if (shiftPressed) {
                int current = videoManager->getCurrentVideoDeviceIndex();
                videoManager->selectVideoDevice(current + 1);
            }
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

//        // Previous audio device - use '<' instead of '['
//        case '<':
//            if (shiftPressed) {
//                int current = audioManager->getCurrentDeviceIndex();
//                if (current > 0) {
//                    audioManager->selectAudioDevice(current - 1);
//                }
//            }
//            break;

        // Next audio device
        case 'D':
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
            
            // Toggle performance mode
//             case 'P':
//                 if (shiftPressed) {
//                     performanceMode = !performanceMode;
//                     ofLogNotice("ofApp") << "Performance mode " << (performanceMode ? "enabled" : "disabled");
//
//                     // Reinitialize components with performance settings
//                     audioManager->setup(performanceMode);
//                     // Adjust other managers for performance if needed
//                 }
//                 break;
                 
             // Fullscreen toggle
             case 'F':
                 if (shiftPressed) {
                     ofToggleFullscreen();
                 }
                 break;
                 
             // Save settings
             case 'S':
                 if (shiftPressed) {
                     ofxXmlSettings xml;
                     paramManager->saveToXml(xml);
                     audioManager->saveToXml(xml);
                     xml.saveFile(ofToDataPath("settings.xml"));
                     ofLogNotice("ofApp") << "Settings saved to settings.xml";
                 }
                 break;
                 
             // Load settings
             case 'L':
                 if (shiftPressed) {
                     ofxXmlSettings xml;
                     if (xml.loadFile(ofToDataPath("settings.xml"))) {
                         paramManager->loadFromXml(xml);
                         audioManager->loadFromXml(xml);
                         ofLogNotice("ofApp") << "Settings loaded from settings.xml";
                     }
                 }
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
    
    // Layout settings
    int margin = 10;
    int lineHeight = 15;
    int columnWidth = ofGetWidth() / 3 - margin * 2;
    
    // Draw system info in top-left
    drawSystemInfo(margin, margin, lineHeight);
    
    // Draw performance info below system info
    drawPerformanceInfo(margin, margin + lineHeight * 5, lineHeight);
    
    // Draw parameter info in the middle column
    drawParameterInfo(ofGetWidth() / 3 + margin, margin, lineHeight);
    
    // Draw audio debug info in the right column
    drawAudioDebugInfo(ofGetWidth() * 2 / 3 + margin, margin, lineHeight);
    
    // Draw video info at the bottom
    drawVideoInfo(margin, ofGetHeight() - lineHeight * 6, lineHeight);
    
    // Draw help text at bottom
    ofSetColor(180, 180, 255);
    ofDrawBitmapString("Press ` to toggle debug display", margin, ofGetHeight() - lineHeight * 3);
    ofDrawBitmapString("Press A to toggle audio reactivity", margin, ofGetHeight() - lineHeight * 2);
    ofDrawBitmapString("Press N to toggle audio normalization", margin, ofGetHeight() - lineHeight);
    
    ofPopStyle();
    
    ofPushMatrix();
    ofTranslate(ofGetWidth() - 220, ofGetHeight() - 180);
    ofSetColor(255);
    ofDrawBitmapString("Camera Input:", 0, -15);
    if (videoManager->getAspectRatioFbo().isAllocated()) {
        videoManager->getAspectRatioFbo().draw(0, 0, 200, 150);
    } else {
        ofDrawBitmapString("No camera FBO", 50, 75);
    }
    ofPopMatrix();
}

void ofApp::drawSystemInfo(int x, int y, int lineHeight) {
    ofSetColor(255, 255, 0);
    
    ofDrawBitmapString("SYSTEM INFO", x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Resolution: " + ofToString(ofGetWidth()) + "x" + ofToString(ofGetHeight()), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("OpenGL: " + ofToString(ofGetGLRenderer()), x, y);
    y += lineHeight;
    
    // Get app runtime
    float runTime = ofGetElapsedTimef();
    int hours = floor(runTime / 3600);
    int minutes = floor((runTime - hours * 3600) / 60);
    int seconds = floor(runTime - hours * 3600 - minutes * 60);
    string timeStr = ofToString(hours, 2, '0') + ":" +
                     ofToString(minutes, 2, '0') + ":" +
                     ofToString(seconds, 2, '0');
    
    ofDrawBitmapString("Runtime: " + timeStr, x, y);
    y += lineHeight;
}

void ofApp::drawPerformanceInfo(int x, int y, int lineHeight) {
    ofSetColor(255, 255, 0);
    
    ofDrawBitmapString("PERFORMANCE", x, y);
    y += lineHeight;
    
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
    
    // Draw target FPS line (30 FPS)
    ofSetColor(255, 255, 0, 100);
    float y30fps = y + graphHeight - ofMap(30.0f, 0, 60.0f, 0, graphHeight);
    ofDrawLine(x, y30fps, x + graphWidth, y30fps);
}

void ofApp::drawVideoInfo(int x, int y, int lineHeight) {
    ofSetColor(255, 255, 0);
    
    ofDrawBitmapString("VIDEO INFO", x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Device: " + videoManager->getCurrentVideoDeviceName(), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Feedback buffer: " + ofToString(videoManager->getFrameBufferLength()) + " frames", x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Delay: " + ofToString(paramManager->getDelayAmount()) + " frames", x, y);
    y += lineHeight;
    
    ofDrawBitmapString("HDMI Aspect: " + ofToString(videoManager->isHdmiAspectRatioEnabled()), x, y);
    y += lineHeight;
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

// Implementation for all the debug drawing methods in ofApp.cpp

void ofApp::drawParameterInfo(int x, int y, int lineHeight) {
    ofSetColor(255, 255, 0);
    ofDrawBitmapString("--- Parameters ---", x, y);
    y += lineHeight;
    
    // Main effect parameters
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
    
    ofDrawBitmapString("Temp. Mix: " + ofToString(paramManager->getTemporalFilterMix(), 3), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Temp. Res: " + ofToString(paramManager->getTemporalFilterResonance(), 3), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Sharpen: " + ofToString(paramManager->getSharpenAmount(), 3), x, y);
    y += lineHeight;
    
    // Displacement parameters
    y += lineHeight;
    ofDrawBitmapString("--- Displacement ---", x, y);
    y += lineHeight;
    
    ofDrawBitmapString("X Displace: " + ofToString(paramManager->getXDisplace(), 3), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Y Displace: " + ofToString(paramManager->getYDisplace(), 3), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Z Displace: " + ofToString(paramManager->getZDisplace(), 3), x, y);
    y += lineHeight;
    
    ofDrawBitmapString("Rotate: " + ofToString(paramManager->getRotate(), 3), x, y);
    y += lineHeight;
    
    // Toggles
    y += lineHeight;
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
}
