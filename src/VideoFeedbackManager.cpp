#include "VideoFeedbackManager.h"




VideoFeedbackManager::VideoFeedbackManager(ParameterManager* paramManager, ShaderManager* shaderManager)
    : paramManager(paramManager), shaderManager(shaderManager) {
    // Allocate pastFrames array
    pastFrames = new ofFbo[DEFAULT_FRAME_BUFFER_LENGTH];
}

void VideoFeedbackManager::setup(int width, int height) {
    this->width = width;
    this->height = height;
    
    // Setup camera and allocate FBOs
    setupCamera(width, height);
    allocateFbos(width, height);
    clearFbos();
}

void VideoFeedbackManager::update() {
    // Get performance mode setting
    bool performanceMode = paramManager->isPerformanceModeEnabled();
    
    // Update camera input
    updateCamera();
    
    // Only process on certain frames in performance mode
    if (!performanceMode || frameCount % 2 == 0) {
        // Process main video feedback pipeline
        processMainPipeline();
    }
    
    // Increment frame index for circular buffer
    incrementFrameIndex();
}

void VideoFeedbackManager::draw() {
    // Draw the final output to screen
    sharpenFbo.draw(0, 0, ofGetWidth(), ofGetHeight());
}

void VideoFeedbackManager::allocateFbos(int width, int height) {
    // Allocate main processing FBO
    mainFbo.allocate(width, height);
    mainFbo.begin();
    ofClear(0, 0, 0, 255);
    mainFbo.end();
    
    // Allocate aspect ratio correction FBO
    aspectRatioFbo.allocate(width, height);
    aspectRatioFbo.begin();
    ofClear(0, 0, 0, 255);
    aspectRatioFbo.end();
    
    // Allocate dry mode frame buffer
    dryFrameBuffer.allocate(width, height);
    dryFrameBuffer.begin();
    ofClear(0, 0, 0, 255);
    dryFrameBuffer.end();
    
    // Allocate sharpen effect buffer
    sharpenFbo.allocate(width, height);
    sharpenFbo.begin();
    ofClear(0, 0, 0, 255);
    sharpenFbo.end();
    
    // Allocate past frames for delay effect
    for (int i = 0; i < frameBufferLength; i++) {
        pastFrames[i].allocate(width, height);
        pastFrames[i].begin();
        ofClear(0, 0, 0, 255);
        pastFrames[i].end();
    }
}

void VideoFeedbackManager::clearFbos() {
    // Clear main FBO
    mainFbo.begin();
    ofClear(0, 0, 0, 255);
    mainFbo.end();
    
    // Clear aspect ratio FBO
    aspectRatioFbo.begin();
    ofClear(0, 0, 0, 255);
    aspectRatioFbo.end();
    
    // Clear dry buffer
    dryFrameBuffer.begin();
    ofClear(0, 0, 0, 255);
    dryFrameBuffer.end();
    
    // Clear sharpen buffer
    sharpenFbo.begin();
    ofClear(0, 0, 0, 255);
    sharpenFbo.end();
    
    // Clear all past frames
    for (int i = 0; i < frameBufferLength; i++) {
        pastFrames[i].begin();
        ofClear(0, 0, 0, 255);
        pastFrames[i].end();
    }
}

void VideoFeedbackManager::listVideoDevices() {
    // Use the camera instance to list devices
    videoDevices = camera.listDevices();
    
    ofLogNotice("VideoFeedbackManager") << "Available video input devices:";
    for (int i = 0; i < videoDevices.size(); i++) {
        const auto& device = videoDevices[i];
        ofLogNotice("VideoFeedbackManager") << i << ": " << device.deviceName
                                           << " (id:" << device.id << ")";
    }
}

std::vector<std::string> VideoFeedbackManager::getVideoDeviceList() const {
    std::vector<std::string> deviceNames;
    for (const auto& device : videoDevices) {
        deviceNames.push_back(device.deviceName);
    }
    return deviceNames;
}

int VideoFeedbackManager::getCurrentVideoDeviceIndex() const {
    return currentVideoDeviceIndex;
}

std::string VideoFeedbackManager::getCurrentVideoDeviceName() const {
    if (currentVideoDeviceIndex >= 0 && currentVideoDeviceIndex < videoDevices.size()) {
        return videoDevices[currentVideoDeviceIndex].deviceName;
    }
    return "No device selected";
}

bool VideoFeedbackManager::selectVideoDevice(int deviceIndex) {
    if (deviceIndex < 0 || deviceIndex >= videoDevices.size()) {
        ofLogError("VideoFeedbackManager") << "Invalid device index: " << deviceIndex;
        return false;
    }
    
    // Close current camera
    if (cameraInitialized) {
        camera.close();
        cameraInitialized = false;
    }
    
    // Set new device index
    currentVideoDeviceIndex = deviceIndex;
    
    // Setup camera with new device
    camera.setDeviceID(videoDevices[deviceIndex].id);
    camera.setDesiredFrameRate(30);
    camera.initGrabber(width, height);
    cameraInitialized = camera.isInitialized();
    
    ofLogNotice("VideoFeedbackManager") << "Selected video device: " << getCurrentVideoDeviceName();
    return cameraInitialized;
}

bool VideoFeedbackManager::selectVideoDevice(const std::string& deviceName) {
    for (int i = 0; i < videoDevices.size(); i++) {
        if (videoDevices[i].deviceName == deviceName) {
            return selectVideoDevice(i);
        }
    }
    
    ofLogError("VideoFeedbackManager") << "Video device not found: " << deviceName;
    return false;
}

void VideoFeedbackManager::setupCamera(int width, int height) {
    this->width = width;
    this->height = height;
    
    // Initialize camera first
    camera.setDeviceID(0); // Default to first device initially
    camera.setDesiredFrameRate(30);
    camera.initGrabber(width, height);
    cameraInitialized = camera.isInitialized();
    
    #ifdef TARGET_LINUX
        // Common issue: on Raspberry Pi, we need to ensure the format is set correctly
        if (cameraInitialized) {
            std::string devicePath = "/dev/video0"; // Default camera path
            
            // Try to use v4l2-ctl to force format (more reliable than ioctl calls)
            std::string cmd = "v4l2-ctl -d " + devicePath +
                              " --set-fmt-video=width=" + ofToString(width) +
                              ",height=" + ofToString(height) +
                              ",pixelformat=YUYV";
            
            system(cmd.c_str());
            ofLogNotice("VideoFeedbackManager") << "Running: " << cmd;
        }
    #endif
    
    // Then list available devices
    listVideoDevices();
    
    // If initialization failed, log error
    if (!cameraInitialized) {
        ofLogError("VideoFeedbackManager") << "Failed to initialize camera!";
    }
}

void VideoFeedbackManager::updateCamera() {
    if (cameraInitialized) {
        camera.update();
        
        // If there's a new frame and HD aspect ratio correction is enabled
        if (camera.isFrameNew() && hdmiAspectRatioEnabled) {
            aspectRatioFbo.begin();
            // Corner crop and stretch to preserve HD aspect ratio
            camera.draw(0, 0, 853, 480);
            aspectRatioFbo.end();
        }
    }
}

void VideoFeedbackManager::incrementFrameIndex() {
    frameCount++;
    currentFrameIndex = frameCount % frameBufferLength;
}

void VideoFeedbackManager::processMainPipeline() {
    // Safety checks before processing
    if (!paramManager || !shaderManager) {
        ofLogError("VideoFeedbackManager") << "Null manager pointers! Cannot process pipeline.";
        return;
    }
    
    if (!pastFrames) {
        ofLogError("VideoFeedbackManager") << "Null pastFrames array! Cannot process pipeline.";
        return;
    }
    
    // Get parameters with P-lock applied
    float lumakeyValue = paramManager->getLumakeyValue();
    float mix = paramManager->getMix();
    float hue = paramManager->getHue();
    float saturation = paramManager->getSaturation();
    float brightness = paramManager->getBrightness();
    float temporalFilterMix = paramManager->getTemporalFilterMix();
    float temporalFilterResonance = paramManager->getTemporalFilterResonance();
    float sharpenAmount = paramManager->getSharpenAmount();
    float xDisplace = paramManager->getXDisplace();
    float yDisplace = paramManager->getYDisplace();
    float zDisplace = paramManager->getZDisplace();
    
    // Get frequency parameters
    float zFrequency = paramManager->getZFrequency();
    float xFrequency = paramManager->getXFrequency();
    float yFrequency = paramManager->getYFrequency();
    
    float rotate = paramManager->getRotate();
    float hueModulation = paramManager->getHueModulation();
    float hueOffset = paramManager->getHueOffset();
    float hueLFO = paramManager->getHueLFO();
    
    // Get LFO values
    float xLfoAmp = paramManager->getXLfoAmp();
    float xLfoRate = paramManager->getXLfoRate();
    float yLfoAmp = paramManager->getYLfoAmp();
    float yLfoRate = paramManager->getYLfoRate();
    float zLfoAmp = paramManager->getZLfoAmp();
    float zLfoRate = paramManager->getZLfoRate();
    float rotateLfoAmp = paramManager->getRotateLfoAmp();
    float rotateLfoRate = paramManager->getRotateLfoRate();
    
    // Calculate delay index with safety bounds
    int delayAmount = ofClamp(paramManager->getDelayAmount(), 0, frameBufferLength - 1);
    
    // Safe index calculation with bounds checking
    int delayIndex = 0;
    if (frameBufferLength > 0) {
        delayIndex = ((frameBufferLength + currentFrameIndex - delayAmount) % frameBufferLength);
    }
    
    // Check if index is valid
    if (delayIndex < 0 || delayIndex >= frameBufferLength) {
        ofLogWarning("VideoFeedbackManager") << "Invalid delay index: " << delayIndex
                                           << " (frameBufferLength: " << frameBufferLength
                                           << ", currentFrameIndex: " << currentFrameIndex
                                           << ", delayAmount: " << delayAmount << ")";
        delayIndex = 0; // Use safe default
    }
    
    // Apply LFO modulation
    xDisplace += 0.01f * xLfoAmp * sin(ofGetElapsedTimef() * xLfoRate);
    yDisplace += 0.01f * yLfoAmp * sin(ofGetElapsedTimef() * yLfoRate);
    zDisplace *= (1.0f + 0.05f * zLfoAmp * sin(ofGetElapsedTimef() * zLfoRate));
    rotate += 0.314159265f * rotateLfoAmp * sin(ofGetElapsedTimef() * rotateLfoRate);
    
    try {
        // Main processing FBO
        mainFbo.begin();
        
        // Get mixer shader with safety check
        ofShader& mixerShader = shaderManager->getMixerShader();
        if (!mixerShader.isLoaded()) {
            ofLogError("VideoFeedbackManager") << "Mixer shader not loaded!";
            mainFbo.end();
            return;
        }
        
        mixerShader.begin();
        
        // Draw camera input (or aspect corrected version) with camera check
        if (cameraInitialized && camera.isInitialized()) {
            if (hdmiAspectRatioEnabled) {
                aspectRatioFbo.draw(0, 0, width, height);
            } else {
                camera.draw(0, 0, width, height);
            }
        } else {
            // Draw fallback if camera not available
            ofSetColor(50, 0, 0);
            ofDrawRectangle(0, 0, width, height);
            ofSetColor(255, 0, 0);
            ofDrawBitmapString("Camera not initialized", width/2 - 70, height/2);
            ofSetColor(255);
        }
        
        // Send the textures to shader with safety checks
        if (pastFrames && delayIndex >= 0 && delayIndex < frameBufferLength) {
            mixerShader.setUniformTexture("fb", pastFrames[delayIndex].getTexture(), 1);
        }
        
        // Choose between wet (feedback) or dry (direct camera) mode for temporal filter
        int temporalIndex = ((frameBufferLength + currentFrameIndex - 1) % frameBufferLength);
        if (temporalIndex >= 0 && temporalIndex < frameBufferLength) {
            if (paramManager->isWetModeEnabled()) {
                mixerShader.setUniformTexture("temporalFilter",
                    pastFrames[temporalIndex].getTexture(), 2);
            } else {
                mixerShader.setUniformTexture("temporalFilter", dryFrameBuffer.getTexture(), 2);
            }
        }
        
        // Send continuous parameters
        mixerShader.setUniform1f("lumakey", lumakeyValue);
        mixerShader.setUniform1f("fbMix", mix);
        mixerShader.setUniform1f("fbHue", hue);
        mixerShader.setUniform1f("fbSaturation", saturation);
        mixerShader.setUniform1f("fbBright", brightness);
        mixerShader.setUniform1f("temporalFilterMix", temporalFilterMix);
        mixerShader.setUniform1f("temporalFilterResonance", temporalFilterResonance);
        mixerShader.setUniform1f("fbXDisplace", xDisplace);
        mixerShader.setUniform1f("fbYDisplace", yDisplace);
        mixerShader.setUniform1f("fbZDisplace", zDisplace);
        
        // Send frequency values to the shader
        mixerShader.setUniform1f("fbZFrequency", zFrequency);
        mixerShader.setUniform1f("fbXFrequency", xFrequency);
        mixerShader.setUniform1f("fbYFrequency", yFrequency);
        
        mixerShader.setUniform1f("fbRotate", rotate);
        mixerShader.setUniform1f("fbHuexMod", hueModulation);
        mixerShader.setUniform1f("fbHuexOff", hueOffset);
        mixerShader.setUniform1f("fbHuexLfo", hueLFO);
        
        // Send toggle switches
        mixerShader.setUniform1i("toroidSwitch", paramManager->isToroidEnabled() ? 1 : 0);
        mixerShader.setUniform1i("mirrorSwitch", paramManager->isMirrorModeEnabled() ? 1 : 0);
        mixerShader.setUniform1i("brightInvert", paramManager->isBrightnessInverted() ? 1 : 0);
        mixerShader.setUniform1i("hueInvert", paramManager->isHueInverted() ? 1 : 0);
        mixerShader.setUniform1i("saturationInvert", paramManager->isSaturationInverted() ? 1 : 0);
        mixerShader.setUniform1i("horizontalMirror", paramManager->isHorizontalMirrorEnabled() ? 1 : 0);
        mixerShader.setUniform1i("verticalMirror", paramManager->isVerticalMirrorEnabled() ? 1 : 0);
        mixerShader.setUniform1i("lumakeyInvertSwitch", paramManager->isLumakeyInverted() ? 1 : 0);
        
        // Send video reactivity parameters
        mixerShader.setUniform1f("vLumakey", paramManager->getVLumakeyValue());
        mixerShader.setUniform1f("vMix", paramManager->getVMix());
        mixerShader.setUniform1f("vHue", paramManager->getVHue());
        mixerShader.setUniform1f("vSat", paramManager->getVSaturation());
        mixerShader.setUniform1f("vBright", paramManager->getVBrightness());
        mixerShader.setUniform1f("vtemporalFilterMix", paramManager->getVTemporalFilterMix());
        mixerShader.setUniform1f("vFb1X", paramManager->getVTemporalFilterResonance());
        mixerShader.setUniform1f("vX", paramManager->getVXDisplace());
        mixerShader.setUniform1f("vY", paramManager->getVYDisplace());
        mixerShader.setUniform1f("vZ", paramManager->getVZDisplace());
        mixerShader.setUniform1f("vRotate", paramManager->getVRotate());
        mixerShader.setUniform1f("vHuexMod", paramManager->getVHueModulation());
        mixerShader.setUniform1f("vHuexOff", paramManager->getVHueOffset());
        mixerShader.setUniform1f("vHuexLfo", paramManager->getVHueLFO());
        
        mixerShader.end();
        mainFbo.end();
        
        // Sharpen processing
        sharpenFbo.begin();
        ofShader& sharpenShader = shaderManager->getSharpenShader();
        
        if (!sharpenShader.isLoaded()) {
            ofLogError("VideoFeedbackManager") << "Sharpen shader not loaded!";
            ofSetColor(255);
            mainFbo.draw(0, 0);
            sharpenFbo.end();
            return;
        }
        
        sharpenShader.begin();
        mainFbo.draw(0, 0);
        sharpenShader.setUniform1f("sharpenAmount", sharpenAmount);
        sharpenShader.setUniform1f("vSharpenAmount", paramManager->getVSharpenAmount());
        sharpenShader.end();
        sharpenFbo.end();
        
        // Store frame in circular buffer with safety checks
        int storeIndex = ((frameBufferLength + currentFrameIndex - 1) % frameBufferLength);
        if (storeIndex >= 0 && storeIndex < frameBufferLength && pastFrames) {
            pastFrames[storeIndex].begin();
            
            // In dry mode, store camera input directly
            if (!paramManager->isWetModeEnabled()) {
                if (cameraInitialized && camera.isInitialized()) {
                    if (hdmiAspectRatioEnabled) {
                        aspectRatioFbo.draw(0, 0);
                    } else {
                        camera.draw(0, 0);
                    }
                } else {
                    // Draw black if no camera
                    ofClear(0, 0, 0, 255);
                }
                
                // Update the dry frame buffer for temporal filtering
                dryFrameBuffer.begin();
                sharpenFbo.draw(0, 0);
                dryFrameBuffer.end();
            } else {
                // In wet mode, store the processed output
                sharpenFbo.draw(0, 0);
            }
            
            pastFrames[storeIndex].end();
        }
    }
    catch (const std::exception& e) {
        ofLogError("VideoFeedbackManager") << "Exception in processMainPipeline: " << e.what();
    }
    catch (...) {
        ofLogError("VideoFeedbackManager") << "Unknown exception in processMainPipeline";
    }
}

int VideoFeedbackManager::getFrameBufferLength() const {
    return frameBufferLength;
}

void VideoFeedbackManager::setFrameBufferLength(int length) {
    // Allocate new framebuffer array
    ofFbo* newFrames = new ofFbo[length];
    
    // Copy and initialize frames
    for (int i = 0; i < length; i++) {
        if (i < frameBufferLength) {
            // Copy existing frame
            newFrames[i] = pastFrames[i];
        } else {
            // Initialize new frame
            newFrames[i].allocate(width, height);
            newFrames[i].begin();
            ofClear(0, 0, 0, 255);
            newFrames[i].end();
        }
    }
    
    // Cleanup old array and update
    delete[] pastFrames;
    pastFrames = newFrames;
    frameBufferLength = length;
}

bool VideoFeedbackManager::isHdmiAspectRatioEnabled() const {
    return hdmiAspectRatioEnabled;
}

void VideoFeedbackManager::setHdmiAspectRatioEnabled(bool enabled) {
    hdmiAspectRatioEnabled = enabled;
}

void VideoFeedbackManager::saveToXml(ofxXmlSettings& xml) const {
    // Create a video section if it doesn't exist
    if (!xml.tagExists("videoFeedback")) {
        xml.addTag("videoFeedback");
    }
    xml.pushTag("videoFeedback");
    
    // Save current device settings
    xml.setValue("deviceIndex", currentVideoDeviceIndex);
    xml.setValue("deviceName", getCurrentVideoDeviceName());
    
    // Save framebuffer settings
    xml.setValue("frameBufferLength", frameBufferLength);
    xml.setValue("hdmiAspectRatioEnabled", hdmiAspectRatioEnabled ? 1 : 0);
    
    // Add any other video-specific settings here
    
    xml.popTag(); // pop videoFeedback
}

void VideoFeedbackManager::loadFromXml(ofxXmlSettings& xml) {
    // We're already inside the paramManager tag from ofApp::setup
    if (xml.tagExists("videoFeedback")) {
        xml.pushTag("videoFeedback");
        
        ofLogNotice("VideoFeedbackManager") << "Loading video settings";
        
        // Load device settings
        int savedDeviceIndex = xml.getValue("deviceIndex", 0);
        std::string savedDeviceName = xml.getValue("deviceName", "");
        
        // Make sure device list is populated
        if (videoDevices.empty()) {
            listVideoDevices();
        }
        
        // Try to find and select the saved device
        bool deviceFound = false;
        
        // First try by name (more reliable across reboots)
        if (!savedDeviceName.empty() && savedDeviceName != "No device selected") {
            deviceFound = selectVideoDevice(savedDeviceName);
            ofLogNotice("VideoFeedbackManager") << "Selecting video device by name: " << savedDeviceName
                                               << (deviceFound ? " (success)" : " (failed)");
        }
        
        // If name lookup failed, try by index
        if (!deviceFound && savedDeviceIndex >= 0 && savedDeviceIndex < videoDevices.size()) {
            deviceFound = selectVideoDevice(savedDeviceIndex);
            ofLogNotice("VideoFeedbackManager") << "Selecting video device by index: " << savedDeviceIndex
                                               << (deviceFound ? " (success)" : " (failed)");
        }
        
        // Load framebuffer settings
        int savedBufferLength = xml.getValue("frameBufferLength", DEFAULT_FRAME_BUFFER_LENGTH);
        if (savedBufferLength != frameBufferLength && savedBufferLength > 0) {
            setFrameBufferLength(savedBufferLength);
            ofLogNotice("VideoFeedbackManager") << "Set frame buffer length to " << frameBufferLength;
        }
        
        // Load aspect ratio setting
        bool aspectEnabled = xml.getValue("hdmiAspectRatioEnabled", 0) != 0;
        setHdmiAspectRatioEnabled(aspectEnabled);
        ofLogNotice("VideoFeedbackManager") << "HDMI aspect ratio " << (aspectEnabled ? "enabled" : "disabled");
        
        xml.popTag(); // pop videoFeedback
    } else {
        ofLogWarning("VideoFeedbackManager") << "No videoFeedback tag found in settings";
    }
}
