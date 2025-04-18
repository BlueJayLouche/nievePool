#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {
    // Check for platform-specific settings
    #if defined(TARGET_LINUX) && (defined(__arm__) || defined(__aarch64__))
        bool performanceMode = true;
        int targetFramerate = 24;
        bool platformIsRaspberryPi = true;
        // Platform flags for later use
        bool performanceMode = true; 
        bool platformIsRaspberryPi = true;
        ofLogNotice("ofApp") << "Detected Raspberry Pi: enabling performance mode";
    #else
        // Platform flags for later use
        bool performanceMode = false;
        bool platformIsRaspberryPi = false;
    #endif
    
    // Framerate will be set after loading settings
    
    // Continue with normal setup...
    ofSetVerticalSync(true); // Keep VSync setting early
    ofBackground(0);
    ofHideCursor();
    
    // Initialize parameter manager first
    paramManager = std::make_unique<ParameterManager>();
    paramManager->setup(); // This now loads settings including mappings
    
    // Set performance mode based on platform
    if (platformIsRaspberryPi) {
        paramManager->setPerformanceModeEnabled(true);
        paramManager->setPerformanceScale(30); 
    paramManager->setHighQualityEnabled(false);
        // Don't set framerate here anymore
    } else {
        // Don't set framerate here anymore
    }
    
    // Check if settings file exists and validate structure (Keep this?)
    ofFile settingsFile(ofToDataPath("settings.xml"));
    bool resetNeeded = false;
    if (settingsFile.exists()) {
        ofxXmlSettings testXml;
        if (testXml.loadFile(ofToDataPath("settings.xml"))) {
            if (testXml.pushTag("paramManager")) {
                if (testXml.tagExists("paramManager")) {
                    resetNeeded = true;
                    ofLogWarning("ofApp") << "Nested paramManager tags detected, resetting settings file";
                }
                testXml.popTag();
            }
        } else {
            resetNeeded = true;
            ofLogWarning("ofApp") << "Could not parse settings.xml, resetting file";
        }
    }
    if (resetNeeded || !settingsFile.exists()) {
        resetSettingsFile();
        paramManager->loadSettings(); // Reload after reset
    }
    
    // Load app-level settings first
    ofxXmlSettings xml;
    if (xml.loadFile(ofToDataPath("settings.xml"))) {
        if (xml.tagExists("app")) {
            xml.pushTag("app");
            debugEnabled = xml.getValue("debugEnabled", false);
            configWidth = xml.getValue("width", 1024);
            configHeight = xml.getValue("height", 768);
            // Load framerate from XML, default to 30 if missing/invalid later
            configFrameRate = xml.getValue("frameRate", 30); 
            
            // Load video input settings
            std::string sourceStr = xml.getValue("videoInputSource", "CAMERA");
            if (sourceStr == "NDI") {
                currentInputSource = NDI;
            } else if (sourceStr == "VIDEO_FILE") {
                currentInputSource = VIDEO_FILE;
            } else {
                currentInputSource = CAMERA; // Default
            }
            videoFilePath = xml.getValue("videoFilePath", videoFilePath);
            currentNdiSourceIndex = xml.getValue("ndiSourceIndex", 0); // Load NDI source index, default to 0
            ofLogNotice("ofApp::setup") << "Initial video input source: " << sourceStr;
            ofLogNotice("ofApp::setup") << "Video file path: " << videoFilePath;
            ofLogNotice("ofApp::setup") << "Loaded NDI source index: " << currentNdiSourceIndex;

            xml.popTag(); // pop app
        } else {
             ofLogWarning("ofApp::setup") << "No <app> tag found in settings.xml, using default app settings.";
        }
    } else {
         ofLogWarning("ofApp::setup") << "Could not load settings.xml, using default app settings.";
         // Ensure configFrameRate has a default if XML load failed
         configFrameRate = 30; 
    }

    // --- Apply Final Framerate ---
    // Validate loaded/default framerate, apply platform defaults only if invalid
    if (configFrameRate <= 0) {
        ofLogWarning("ofApp::setup") << "Invalid frameRate (" << configFrameRate << ") loaded or defaulted. Applying platform default.";
        #if defined(TARGET_LINUX) && (defined(__arm__) || defined(__aarch64__))
            configFrameRate = 24; // RPi default
        #else
            configFrameRate = 30; // Non-RPi default
        #endif
    }
    ofLogNotice("ofApp::setup") << "Setting final frame rate to: " << configFrameRate;
    ofSetFrameRate(configFrameRate); 

    // Set window shape 
    ofSetWindowShape(configWidth, configHeight);
    
    // Continue with the standard setup
    // ofSetVerticalSync(true); // Moved earlier
    ofBackground(0);
    ofHideCursor();
    ofDisableArbTex();
    
    // Initialize shader manager
    shaderManager = std::make_unique<ShaderManager>();
    shaderManager->setup();
    
    // Initialize video feedback manager (FBOs etc.)
    videoManager = std::make_unique<VideoFeedbackManager>(paramManager.get(), shaderManager.get());
    videoManager->setup(configWidth, configHeight);  

    // --- Setup Input Sources ---
    
    // 1. Camera Setup (Now handled mostly within VideoFeedbackManager)
    // cameraInitialized = videoManager->isCameraInitialized(); // Get status from videoManager

    // 2. NDI Setup 
    // NOTE: Removing explicit CreateFinder/FindSenders here. 
    // GetSenderCount() in keyPressed should handle finding sources implicitly.
    // We still need to attempt an initial connection if NDI is the starting source.
    if (currentInputSource == NDI) {
        // Attempt to connect to the source index loaded from settings
        // Note: currentNdiSourceIndex is already loaded from XML earlier in setup()
        ofLogNotice("ofApp::setup") << "Attempting initial connection to NDI source index loaded from settings: " << currentNdiSourceIndex;
        if (!ndiReceiver.CreateReceiver(currentNdiSourceIndex)) {
             ofLogWarning("ofApp::setup") << "Failed to create initial NDI receiver for source index " << currentNdiSourceIndex << ". Check if source is available.";
        } else {
             ofLogNotice("ofApp::setup") << "Successfully created initial NDI receiver for source index " << currentNdiSourceIndex;
        }
    }
    // Allocate texture regardless of initial connection success
    ndiTexture.allocate(configWidth, configHeight, GL_RGBA); 

    // 3. Video Player Setup
    ofLogNotice("ofApp::setup") << "Setting up video player with file: " << videoFilePath;
    videoPlayer.load(videoFilePath);
    videoPlayer.setLoopState(OF_LOOP_NORMAL);
    if (currentInputSource == VIDEO_FILE) {
        videoPlayer.play(); 
    }

    // Allocate the texture that holds the currently selected input for debug preview
    currentInputTexture.allocate(videoManager->getMainFbo().getWidth(), videoManager->getMainFbo().getHeight(), GL_RGBA);
    ofLogNotice("ofApp::setup") << "Allocated currentInputTexture: " << currentInputTexture.getWidth() << "x" << currentInputTexture.getHeight();

    // Initialize MIDI manager 
    midiManager = std::make_unique<MidiManager>(paramManager.get());
    midiManager->setup(); 
    
    // Initialize Audio Reactivity manager
    audioManager = std::make_unique<AudioReactivityManager>(paramManager.get());
    // Pass paramManager and performanceMode to setup as required by the updated signature
    audioManager->setup(paramManager.get(), performanceMode); 
    
    // Load remaining manager settings (VideoFeedbackManager) from XML
    if (xml.loadFile(ofToDataPath("settings.xml"))) {
         if (xml.pushTag("paramManager")) {
             // Load VideoFeedbackManager settings if tag exists
             if (xml.tagExists("videoFeedback")) {
                 videoManager->loadFromXml(xml);
             }
             // Load AudioReactivityManager settings if tag exists
             if (xml.tagExists("audioReactivity")) {
                 audioManager->loadFromXml(xml);
             }
             // Load MidiManager settings if tag exists
             if (xml.tagExists("midi")) {
                 midiManager->loadSettings(xml);
             }
             xml.popTag(); // Pop paramManager
         }
    }

    // Initialize performance monitoring
    for (int i = 0; i < 60; i++) {
        frameRateHistory[i] = 0.0f;
    }

    // --- OSC Setup --- 
    int currentOscPort = paramManager->getOscPort(); // Get port from ParameterManager
    ofLogNotice("ofApp::setup") << "Listening for OSC messages on port " << currentOscPort;
    oscReceiver.setup(currentOscPort);
}

// Removed ofApp::setupDefaultAudioMappings() - Logic moved to AudioReactivityManager::addDefaultMappings()

void ofApp::resetSettingsFile() {
    ofxXmlSettings xml;
    
    xml.addTag("app");
    xml.pushTag("app");
    xml.setValue("version", "1.0.0");
    xml.setValue("lastSaved", ofGetTimestampString());
    xml.setValue("debugEnabled", debugEnabled ? 1 : 0);
    xml.setValue("width", configWidth);
    xml.setValue("height", configHeight);
    xml.setValue("frameRate", configFrameRate);
    xml.setValue("videoInputSource", "CAMERA"); 
    xml.setValue("videoFilePath", "input.mov");
    xml.setValue("ndiSourceIndex", 0); // Add default NDI source index on reset
    xml.popTag(); 
    
    xml.addTag("paramManager");
    // ParameterManager::saveToXml will populate this on next save
    
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
    
    paramManager->update();
    midiManager->update();
    audioManager->update(); // Update audio manager

    // --- Update Input Source and Process Video ---
    if (currentInputSource == CAMERA) {
        videoManager->updateCamera(); // Updates internal camera and draws to aspectRatioFbo
        // Check if the camera inside videoManager is ready and its FBO has content
        if (videoManager->isCameraInitialized() && videoManager->getAspectRatioFbo().isAllocated()) { // Use public getter
            const auto& camTex = videoManager->getAspectRatioFbo().getTexture();
            if (camTex.isAllocated()) {
                videoManager->processMainPipeline(camTex); // Use renamed public method

                // Update debug preview texture
                ofPixels pixels;
                videoManager->getAspectRatioFbo().readToPixels(pixels);
                if (pixels.isAllocated()) {
                    currentInputTexture.loadData(pixels);
                }
            }
        }
    } else if (currentInputSource == NDI) {
        if (ndiReceiver.ReceiveImage(ndiTexture)) { // Check for new NDI frame
             if (ndiTexture.isAllocated()) {
                 videoManager->processMainPipeline(ndiTexture); // Use renamed public method

                 // Update debug preview texture
                 ofPixels ndiPixels;
                 ndiTexture.readToPixels(ndiPixels);
                 if(ndiPixels.isAllocated()) {
                    currentInputTexture.loadData(ndiPixels);
                 }
             }
        }
    } else if (currentInputSource == VIDEO_FILE) {
        videoPlayer.update();
        if (videoPlayer.isFrameNew() && videoPlayer.isLoaded() && videoPlayer.getTexture().isAllocated()) {
             const auto& vidTex = videoPlayer.getTexture();
             videoManager->processMainPipeline(vidTex); // Use renamed public method

             // Update debug preview texture
             currentInputTexture.loadData(videoPlayer.getPixels());
         }
    }

    // Increment video manager's frame index for feedback loop timing
    videoManager->incrementFrameIndex();

    // --- OSC Update ---
    while (oscReceiver.hasWaitingMessages()) {
        ofxOscMessage m;
        oscReceiver.getNextMessage(m);
        string incomingAddr = m.getAddress();
        bool handled = false;

        for (const auto& paramId : paramManager->getAllParameterIds()) {
            std::string configuredAddr = paramManager->getOscAddress(paramId);
            if (!configuredAddr.empty() && configuredAddr == incomingAddr) {
                if (m.getNumArgs() == 1) {
                    try {
                        if (m.getArgType(0) == OFXOSC_TYPE_FLOAT) {
                            float value = m.getArgAsFloat(0);
                            if (paramId == "lumakeyValue") paramManager->setLumakeyValue(value);
                            else if (paramId == "mix") paramManager->setMix(value);
                            else if (paramId == "hue") paramManager->setHue(value);
                            else if (paramId == "saturation") paramManager->setSaturation(value);
                            else if (paramId == "brightness") paramManager->setBrightness(value);
                            else if (paramId == "temporalFilterMix") paramManager->setTemporalFilterMix(value);
                            else if (paramId == "temporalFilterResonance") paramManager->setTemporalFilterResonance(value);
                            else if (paramId == "sharpenAmount") paramManager->setSharpenAmount(value);
                            else if (paramId == "xDisplace") paramManager->setXDisplace(value);
                            else if (paramId == "yDisplace") paramManager->setYDisplace(value);
                            else if (paramId == "zDisplace") paramManager->setZDisplace(value);
                            else if (paramId == "zFrequency") paramManager->setZFrequency(value);
                            else if (paramId == "xFrequency") paramManager->setXFrequency(value);
                            else if (paramId == "yFrequency") paramManager->setYFrequency(value);
                            else if (paramId == "rotate") paramManager->setRotate(value);
                            else if (paramId == "hueModulation") paramManager->setHueModulation(value);
                            else if (paramId == "hueOffset") paramManager->setHueOffset(value);
                            else if (paramId == "hueLFO") paramManager->setHueLFO(value);
                            else if (paramId == "xLfoAmp") paramManager->setXLfoAmp(value);
                            else if (paramId == "xLfoRate") paramManager->setXLfoRate(value);
                            else if (paramId == "yLfoAmp") paramManager->setYLfoAmp(value);
                            else if (paramId == "yLfoRate") paramManager->setYLfoRate(value);
                            else if (paramId == "zLfoAmp") paramManager->setZLfoAmp(value);
                            else if (paramId == "zLfoRate") paramManager->setZLfoRate(value);
                            else if (paramId == "rotateLfoAmp") paramManager->setRotateLfoAmp(value);
                            else if (paramId == "rotateLfoRate") paramManager->setRotateLfoRate(value);
                            else { ofLogWarning("ofApp::update") << "OSC: No float setter found for matched address: " << incomingAddr << " (paramId: " << paramId << ")"; }
                            handled = true;
                        } else if (m.getArgType(0) == OFXOSC_TYPE_INT32 || m.getArgType(0) == OFXOSC_TYPE_INT64) {
                            int value = m.getArgAsInt(0);
                            if (paramId == "delayAmount") paramManager->setDelayAmount(value);
                            else { ofLogWarning("ofApp::update") << "OSC: No int setter found for matched address: " << incomingAddr << " (paramId: " << paramId << ")"; }
                             handled = true;
                        } else if (m.getArgType(0) == OFXOSC_TYPE_TRUE || m.getArgType(0) == OFXOSC_TYPE_FALSE) {
                            bool value = m.getArgAsBool(0);
                            if (paramId == "hueInvert") paramManager->setHueInverted(value);
                            else if (paramId == "saturationInvert") paramManager->setSaturationInverted(value);
                            else if (paramId == "brightnessInvert") paramManager->setBrightnessInverted(value);
                            else if (paramId == "horizontalMirror") paramManager->setHorizontalMirrorEnabled(value);
                            else if (paramId == "verticalMirror") paramManager->setVerticalMirrorEnabled(value);
                            else if (paramId == "lumakeyInvert") paramManager->setLumakeyInverted(value);
                            else if (paramId == "toroidEnabled") paramManager->setToroidEnabled(value);
                            else if (paramId == "mirrorModeEnabled") paramManager->setMirrorModeEnabled(value);
                            else if (paramId == "wetModeEnabled") paramManager->setWetModeEnabled(value);
                            else if (paramId == "videoReactiveMode") paramManager->setVideoReactiveEnabled(value);
                            else if (paramId == "lfoAmpMode") paramManager->setLfoAmpModeEnabled(value);
                            else if (paramId == "lfoRateMode") paramManager->setLfoRateModeEnabled(value);
                            else { ofLogWarning("ofApp::update") << "OSC: No bool setter found for matched address: " << incomingAddr << " (paramId: " << paramId << ")"; }
                            handled = true;
                        }
                    } catch (const std::exception& e) {
                         ofLogError("ofApp::update") << "OSC Error processing message " << incomingAddr << ": " << e.what();
                         handled = true; 
                    }
                } else {
                     ofLogWarning("ofApp::update") << "OSC: Received message with != 1 arguments for address: " << incomingAddr;
                     handled = true; 
                }
                break; 
            }
        }
        if (!handled) {
             ofLogVerbose("ofApp::update") << "OSC: Received unhandled message: " << incomingAddr;
        }
    }
    
    // Calculate frame time
    float endTime = ofGetElapsedTimef();
    lastFrameTime = (endTime - startTime) * 1000.0f; 
    
    frameTimeHistory.push_back(lastFrameTime);
    if (frameTimeHistory.size() > 150) {
        frameTimeHistory.pop_front();
    }
    
    averageFrameTime = 0;
    for (float time : frameTimeHistory) {
        averageFrameTime += time;
    }
    if (!frameTimeHistory.empty()) {
       averageFrameTime /= frameTimeHistory.size();
    }
    
    frameCounter++;
}

//--------------------------------------------------------------
void ofApp::draw() {
    // On Raspberry Pi, we want to show debug info even if rendering fails
    #if defined(TARGET_LINUX) && (defined(__arm__) || defined(__aarch64__))
    bool safeDrawMode = true;
    #else
    bool safeDrawMode = false;
    #endif
    
    if (safeDrawMode) {
        try {
            videoManager->draw();
        } catch (const std::exception& e) {
            ofLogError("ofApp") << "Exception in videoManager->draw(): " << e.what();
            ofBackground(0); ofSetColor(255, 0, 0);
            ofDrawBitmapString("Render error: " + std::string(e.what()), 20, 20);
        } catch (...) {
            ofLogError("ofApp") << "Unknown exception in videoManager->draw()";
            ofBackground(0); ofSetColor(255, 0, 0);
            ofDrawBitmapString("Unknown render error", 20, 20);
        }
        drawDebugInfo();
    } else {
        videoManager->draw();
        if (debugEnabled) {
            drawDebugInfo();
        }
    }
}

void ofApp::drawDebugInfo() {
    ofPushStyle();
    ofEnableAlphaBlending();
    
    ofSetColor(255, 255, 0);
    ofDrawBitmapString("DEBUG MODE", 10, 15);
    
    bool isSmallScreen = (ofGetWidth() < 800 || ofGetHeight() < 600);
    int margin = isSmallScreen ? 5 : 10;
    int lineHeight = isSmallScreen ? 12 : 15;
    int columnWidth = isSmallScreen ? (ofGetWidth() / 3 - margin) : (ofGetWidth() / 3 - margin * 2);
    
    ofSetColor(0, 0, 0, 180);
    int sysInfoHeight = lineHeight * 6;
    ofDrawRectangle(margin, margin, columnWidth, sysInfoHeight);
    int perfInfoHeight = lineHeight * 6 + 40; 
    ofDrawRectangle(margin, margin + sysInfoHeight + 5, columnWidth, perfInfoHeight);
    ofDrawRectangle(ofGetWidth() / 3 + margin, margin, columnWidth, ofGetHeight() - margin * 2 - lineHeight * 5);
    ofDrawRectangle(ofGetWidth() * 2 / 3 + margin, margin, columnWidth, ofGetHeight() - margin * 2 - lineHeight * 5);
    ofDrawRectangle(margin, ofGetHeight() - lineHeight * 6 - margin, ofGetWidth() - margin * 2, lineHeight * 6);
    
    drawSystemInfo(margin + 5, margin + 15, lineHeight);
    drawPerformanceInfo(margin + 5, margin + sysInfoHeight + 20, lineHeight);
    drawParameterInfo(ofGetWidth() / 3 + margin + 5, margin + 15, lineHeight);
    drawAudioDebugInfo(ofGetWidth() * 2 / 3 + margin + 5, margin + 15, lineHeight);
    drawVideoInfo(margin + 5, ofGetHeight() - lineHeight * 5 - margin, lineHeight);
    
    ofSetColor(0, 0, 0, 200);
    ofDrawRectangle(margin, ofGetHeight() - lineHeight * 3, ofGetWidth() - margin * 2, lineHeight * 3);
    ofSetColor(180, 180, 255);
    ofDrawBitmapString("Press ` to toggle debug display", margin + 5, ofGetHeight() - lineHeight * 3 + 12);
    ofDrawBitmapString("Press A to toggle audio reactivity", margin + 5, ofGetHeight() - lineHeight * 2 + 12);
    ofDrawBitmapString("Press N to toggle audio normalization", margin + 5, ofGetHeight() - lineHeight + 12);
    
    ofPopStyle();

    // --- Draw NDI Preview (in debug mode) ---
    if (debugEnabled && currentInputSource == NDI && ndiTexture.isAllocated()) {
        ofPushMatrix(); ofPushStyle();
        int ndiPreviewWidth = 160; int ndiPreviewHeight = 120;
        int ndiPreviewX = ofGetWidth() - ndiPreviewWidth - 20;
        int ndiPreviewY = ofGetHeight() - ndiPreviewHeight - 20 - 150 - 35 - 10; 
        ofSetColor(0, 0, 0, 200);
        ofDrawRectangle(ndiPreviewX - 10, ndiPreviewY - 25, ndiPreviewWidth + 20, ndiPreviewHeight + 35);
        ofSetColor(255);
        ofDrawBitmapString("NDI Input:", ndiPreviewX, ndiPreviewY - 10);
        ndiTexture.draw(ndiPreviewX, ndiPreviewY, ndiPreviewWidth, ndiPreviewHeight);
        ofPopStyle(); ofPopMatrix();
    }
    
    // Draw Input Preview (Camera, NDI, or Video File)
    ofPushMatrix(); ofPushStyle();
    int previewWidth = 200; int previewHeight = 150;
    int previewX = ofGetWidth() - previewWidth - 20;
    int previewY = ofGetHeight() - previewHeight - 20;
    ofSetColor(0, 0, 0, 200);
    ofDrawRectangle(previewX - 10, previewY - 25, previewWidth + 20, previewHeight + 35);
    ofSetColor(255);
    ofDrawBitmapString("Input Preview:", previewX, previewY - 10); 
    ofTranslate(previewX, previewY);
    if (currentInputTexture.isAllocated()) {
         ofSetColor(255);
         currentInputTexture.draw(0, 0, previewWidth, previewHeight);
         std::string sourceLabel = "Input: ";
         if (currentInputSource == CAMERA) sourceLabel += "Camera";
         else if (currentInputSource == NDI) sourceLabel += "NDI";
         else if (currentInputSource == VIDEO_FILE) sourceLabel += "File";
         ofDrawBitmapStringHighlight(sourceLabel, 5, 15, ofColor(0,0,0,150), ofColor(255,255,0));
    } else {
         ofSetColor(255, 0, 0);
         ofDrawBitmapString("Input Texture Not Allocated", 10, previewHeight/2);
    }
    ofPopStyle(); ofPopMatrix();
}

//--------------------------------------------------------------
void ofApp::exit() {
    // Clean shutdown of audio
    audioManager->exit();

    // Release NDI resources
    ndiReceiver.ReleaseReceiver(); 
    // No need to release finder if we didn't explicitly create it persistently
    
    // Save settings before exiting
    ofxXmlSettings xml;
    if (xml.loadFile(ofToDataPath("settings.xml"))) {
         // Preserve existing non-app settings if possible
    } else {
        xml.addTag("app"); // Ensure app tag exists if file was missing
    }

    xml.pushTag("app");
    xml.setValue("version", "1.0.0"); // Update version or keep existing
    xml.setValue("lastSaved", ofGetTimestampString());
    xml.setValue("debugEnabled", debugEnabled ? 1 : 0);
    xml.setValue("width", ofGetWidth());      
    xml.setValue("height", ofGetHeight());
    xml.setValue("frameRate", (int)ofGetTargetFrameRate());
    
    // Save current input source
    std::string sourceStr = "CAMERA";
    if (currentInputSource == NDI) sourceStr = "NDI";
    else if (currentInputSource == VIDEO_FILE) sourceStr = "VIDEO_FILE";
    xml.setValue("videoInputSource", sourceStr);
    xml.setValue("videoFilePath", videoFilePath); 
    xml.setValue("ndiSourceIndex", currentNdiSourceIndex); // Save NDI source index

    xml.popTag(); // pop app
    
    // Save manager settings
    paramManager->saveToXml(xml); 
    if (xml.pushTag("paramManager")) { // Push into the tag created/found by paramManager
        audioManager->saveToXml(xml);
        videoManager->saveToXml(xml);
        midiManager->saveSettings(xml);
        xml.popTag(); 
    }
    
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
    // Use videoManager methods for device control
    int currentDeviceIndex = videoManager->getCurrentVideoDeviceIndex();
    auto deviceList = videoManager->getVideoDeviceList(); // Get names
    int newDeviceIndex = -1; // Declare outside

    switch (key) {
        // Debug toggle
        case '`':
            debugEnabled = !debugEnabled;
            break;
            
         // Video device controls (using videoManager now) / NDI Source Switching
         case '<': // ASCII value 60
              if (currentInputSource == NDI && !shiftPressed) {
                  // NDI Source Switching (Previous)
                  int nSources = ndiReceiver.GetSenderCount(); 
                  if (nSources > 0) {
                      int newNdiIndex = (currentNdiSourceIndex - 1 + nSources) % nSources; 
                      ofLogNotice("ofApp::keyPressed") << "Attempting to switch NDI source from " << currentNdiSourceIndex << " to " << newNdiIndex;
                      ndiReceiver.ReleaseReceiver(); // Release current connection
                      // Attempt to create receiver for the new index
                      if (ndiReceiver.CreateReceiver(newNdiIndex)) { 
                          currentNdiSourceIndex = newNdiIndex; // Update index if successful
                          ofLogNotice("ofApp::keyPressed") << "Successfully switched NDI source to index: " << currentNdiSourceIndex << " (" << ndiReceiver.GetSenderName() << ")";
                      } else {
                          ofLogError("ofApp::keyPressed") << "Failed to create NDI receiver for source index: " << newNdiIndex;
                          // Optional: Attempt to reconnect to the previous source?
                          // if(!ndiReceiver.CreateReceiver(currentNdiSourceIndex)) {
                          //     ofLogError("ofApp::keyPressed") << "Failed to reconnect to original NDI source index: " << currentNdiSourceIndex;
                          // }
                      }
                  }
              } else if (currentInputSource == CAMERA && shiftPressed) {
                  // Camera Device Switching (Previous)
                  if (currentDeviceIndex > 0) { // Ensure we don't go below index 0
                      newDeviceIndex = currentDeviceIndex - 1;
                      if (videoManager->selectVideoDevice(newDeviceIndex)) {
                         ofLogNotice("ofApp::keyPressed") << "Switched camera to device index: " << newDeviceIndex;
                     } else {
                         ofLogError("ofApp::keyPressed") << "Failed to switch camera to device index: " << newDeviceIndex;
                     }
                 }
            }
             break;
             
         case '>': // ASCII value 62
              if (currentInputSource == NDI && !shiftPressed) {
                  // NDI Source Switching (Next)
                   int nSources = ndiReceiver.GetSenderCount(); 
                  if (nSources > 0) {
                      int newNdiIndex = (currentNdiSourceIndex + 1) % nSources; 
                      ofLogNotice("ofApp::keyPressed") << "Attempting to switch NDI source from " << currentNdiSourceIndex << " to " << newNdiIndex;
                      ndiReceiver.ReleaseReceiver(); // Release current connection
                      // Attempt to create receiver for the new index
                      if (ndiReceiver.CreateReceiver(newNdiIndex)) {
                          currentNdiSourceIndex = newNdiIndex; // Update index if successful
                          ofLogNotice("ofApp::keyPressed") << "Successfully switched NDI source to index: " << currentNdiSourceIndex << " (" << ndiReceiver.GetSenderName() << ")";
                      } else {
                          ofLogError("ofApp::keyPressed") << "Failed to create NDI receiver for source index: " << newNdiIndex;
                          // Optional: Attempt to reconnect to the previous source?
                          // if(!ndiReceiver.CreateReceiver(currentNdiSourceIndex)) {
                          //     ofLogError("ofApp::keyPressed") << "Failed to reconnect to original NDI source index: " << currentNdiSourceIndex;
                          // }
                      }
                  }
              } else if (currentInputSource == CAMERA && shiftPressed) {
                  // Camera Device Switching (Next)
                  if (currentDeviceIndex != -1 && currentDeviceIndex < deviceList.size() - 1) { // Ensure we don't go past the end
                      newDeviceIndex = currentDeviceIndex + 1;
                      if (videoManager->selectVideoDevice(newDeviceIndex)) {
                         ofLogNotice("ofApp::keyPressed") << "Switched camera to device index: " << newDeviceIndex;
                     } else {
                         ofLogError("ofApp::keyPressed") << "Failed to switch camera to device index: " << newDeviceIndex;
                     }
                 }
            }
            break;

        // Input Source Cycling (Example: using 'I' key)
        // case 'i': // Keep lowercase 'i' separate from parameter control - This conflicts with LFO control
        case 'I': // Use uppercase 'I' only
             if (currentInputSource == CAMERA) {
                 currentInputSource = NDI;
                 if (videoPlayer.isPlaying()) videoPlayer.stop();
                 ofLogNotice("ofApp") << "Switched input source to NDI";
             } else if (currentInputSource == NDI) {
                 currentInputSource = VIDEO_FILE;
                 videoPlayer.play(); // Start video when switching to it
                 ofLogNotice("ofApp") << "Switched input source to VIDEO_FILE";
             } else { // VIDEO_FILE
                 currentInputSource = CAMERA;
                 if (videoPlayer.isPlaying()) videoPlayer.stop();
                 ofLogNotice("ofApp") << "Switched input source to CAMERA";
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

        // Next audio device (Keep 'D' for audio, avoid conflict with '<'/'<')
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
    if (!frameTimeHistory.empty()) { // Avoid division by zero
        for (float time : frameTimeHistory) {
            avgFps += (1000.0f / time); // Calculate FPS from frame time
        }
         avgFps /= frameTimeHistory.size();
    }
    
    string avgFpsStr = "Avg FPS: " + ofToString(avgFps, 1);
    ofDrawBitmapString(avgFpsStr, x, y);
    y += lineHeight;
    
    // Draw FPS graph
    int graphWidth = 120;
    int graphHeight = 40;
    
    ofSetColor(50);
    ofDrawRectangle(x, y, graphWidth, graphHeight);
    
    ofSetColor(0, 255, 0);
    int historySize = frameTimeHistory.size();
    for (int i = 0; i < historySize; i++) {
        float frameTime = frameTimeHistory[i];
        float currentFps = (frameTime > 0) ? (1000.0f / frameTime) : 0; // Avoid division by zero
        float barHeight = ofMap(currentFps, 0, 60, 0, graphHeight, true); // Clamp values
        ofDrawLine(x + i * (graphWidth / (float)historySize), y + graphHeight, 
                   x + i * (graphWidth / (float)historySize), y + graphHeight - barHeight);
    }
    
    // Draw target FPS line (30 FPS)
    ofSetColor(255, 255, 0, 100);
    float y30fps = y + graphHeight - ofMap(30.0f, 0, 60.0f, 0, graphHeight, true);
    ofDrawLine(x, y30fps, x + graphWidth, y30fps);
}

void ofApp::drawVideoInfo(int x, int y, int lineHeight) {
    ofSetColor(255, 255, 0);
    
    ofDrawBitmapString("VIDEO INFO", x, y);
    y += lineHeight;

    // Display current input source
    std::string sourceStr = "UNKNOWN";
    if(currentInputSource == CAMERA) sourceStr = "CAMERA";
    else if(currentInputSource == NDI) sourceStr = "NDI";
    else if(currentInputSource == VIDEO_FILE) sourceStr = "VIDEO_FILE";
    ofDrawBitmapString("Input Source: " + sourceStr + " (Press 'I' to cycle)", x, y);
    y += lineHeight;

    // Display Camera device info if Camera is the source
    if (currentInputSource == CAMERA) {
        std::string deviceName = videoManager->getCurrentVideoDeviceName(); // Use videoManager method
        if (!videoManager->isCameraInitialized()) { // Use videoManager method
            deviceName += " (Error)";
        }
        ofDrawBitmapString("Camera Device: " + deviceName + " (Shift+ </> to change)", x, y);
         y += lineHeight;
     } else if (currentInputSource == NDI) {
          std::string ndiStatus = "NDI Source [" + ofToString(currentNdiSourceIndex) + "]: ";
          if (ndiReceiver.ReceiverConnected()) {
              ndiStatus += ndiReceiver.GetSenderName();
          } else {
              ndiStatus += "Connecting...";
          }
          ndiStatus += " (< / > to change)";
          ofDrawBitmapString(ndiStatus, x, y);
          y += lineHeight;
     } else if (currentInputSource == VIDEO_FILE) {
         ofDrawBitmapString("Video File: " + videoFilePath, x, y);
         y += lineHeight;
         ofDrawBitmapString("Video Pos: " + ofToString(videoPlayer.getPosition() * 100.0f, 1) + "%", x, y);
         y += lineHeight;
    }

    // Display info managed by VideoFeedbackManager
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
    
    ofDrawBitmapString("Vert Mirror: " + ofToString(paramManager->isVerticalMirrorEnabled()), x, y);
    y += lineHeight;
}
