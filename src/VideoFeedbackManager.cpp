#include "VideoFeedbackManager.h"
#include "V4L2Helper.h" 
#ifdef TARGET_LINUX
#include <sys/sysinfo.h>
#endif

VideoFeedbackManager::VideoFeedbackManager(ParameterManager* paramManager, ShaderManager* shaderManager)
    : paramManager(paramManager), shaderManager(shaderManager), cameraInitialized(false), currentVideoDeviceIndex(0) { // Initialize members
    frameBufferLength = determineOptimalFrameBufferLength();
    pastFrames = new ofFbo[frameBufferLength];
    pastFramesAllocated = new bool[frameBufferLength];
    for (int i = 0; i < frameBufferLength; i++) {
        pastFramesAllocated[i] = false;
    }
    // List devices early so the list is available for loading settings
    listVideoDevices(); 
}

VideoFeedbackManager::~VideoFeedbackManager() {
    if (pastFrames) {
        delete[] pastFrames;
        pastFrames = nullptr;
    }
    if (pastFramesAllocated) {
        delete[] pastFramesAllocated;
        pastFramesAllocated = nullptr;
    }
    if (cameraInitialized) {
        camera.close();
    }
}

void VideoFeedbackManager::setup(int width, int height) {
    this->width = width;
    this->height = height;
    // Load settings first, which might dictate the device to set up
    // Note: loadFromXml is now called from ofApp after paramManager is loaded
    // setupCamera will use the device ID potentially loaded from XML via paramManager
    setupCamera(width, height); 
    allocateFbos(width, height);
    for (int i = 0; i < std::min(5, frameBufferLength); i++) {
        allocatePastFrameIfNeeded(i);
    }
    clearFbos();
}

void VideoFeedbackManager::allocateFbos(int width, int height) {
    bool performanceMode = paramManager ? paramManager->isPerformanceModeEnabled() : false;
    int fboWidth = width;
    int fboHeight = height;
    if (performanceMode && paramManager) { // Check paramManager exists
        float scalePercent = paramManager->getPerformanceScale();
        fboWidth = width * (scalePercent / 100.0f);
        fboHeight = height * (scalePercent / 100.0f);
         // Ensure minimum dimensions
        fboWidth = std::max(fboWidth, 160); 
        fboHeight = std::max(fboHeight, 120);
        ofLogNotice("VideoFeedbackManager") << "Performance Mode (" << scalePercent << "%): Reduced FBO resolution to "
                                           << fboWidth << "x" << fboHeight;
    }
    
    ofFboSettings settings;
    settings.width = fboWidth;
    settings.height = fboHeight;
    settings.numColorbuffers = 1;
    settings.useDepth = false;
    settings.useStencil = false;
    settings.numSamples = 0; 
    settings.textureTarget = GL_TEXTURE_2D; // Ensure standard texture target

    bool isArmLinux = false;
    #if defined(TARGET_LINUX) && (defined(__arm__) || defined(__aarch64__))
        isArmLinux = true;
    #endif

    if (isArmLinux) {
        settings.internalformat = GL_RGBA8; 
    } else {
        #ifdef TARGET_LINUX
             // Defaulting to RGBA on Linux too for broader compatibility unless specific issues arise
            settings.internalformat = GL_RGBA8; 
        #else
            settings.internalformat = GL_RGBA8; 
        #endif
    }
    fboSettings = settings;

    ofLogNotice("VideoFeedbackManager") << "Allocating FBOs with format: "
                                       << (settings.internalformat == GL_RGBA8 ? "GL_RGBA8" : "GL_RGB"); // Adjust log if GL_RGB is used
    
    try {
        mainFbo.allocate(settings);
        mainFbo.begin(); ofClear(0, 0, 0, 255); mainFbo.end();
        aspectRatioFbo.allocate(settings); // Still needed for camera aspect correction
        aspectRatioFbo.begin(); ofClear(0, 0, 0, 255); aspectRatioFbo.end();
        dryFrameBuffer.allocate(settings);
        dryFrameBuffer.begin(); ofClear(0, 0, 0, 255); dryFrameBuffer.end();
        sharpenFbo.allocate(settings);
        sharpenFbo.begin(); ofClear(0, 0, 0, 255); sharpenFbo.end();
        
        // Re-initialize allocation tracking array if needed
        if (!pastFramesAllocated) pastFramesAllocated = new bool[frameBufferLength];
        for (int i = 0; i < frameBufferLength; i++) pastFramesAllocated[i] = false;
        
        // Re-allocate pastFrames array if needed
        if (!pastFrames) pastFrames = new ofFbo[frameBufferLength];
        
        int preAllocateCount = std::min(5, frameBufferLength);
        for (int i = 0; i < preAllocateCount; i++) allocatePastFrameIfNeeded(i);
        
        ofLogNotice("VideoFeedbackManager") << "FBOs allocated with "
                                           << fboWidth << "x" << fboHeight << " resolution";
    } catch (std::exception& e) {
        ofLogError("VideoFeedbackManager") << "Error allocating FBOs: " << e.what();
    }
}

// Removed parameterless update() method. ofApp::update() now calls processMainPipeline directly.

// Renamed function to match header declaration
void VideoFeedbackManager::processMainPipeline(const ofTexture& inputTexture) {
    // Pre-allocate frames we'll need for this frame
    int delayAmount = ofClamp(paramManager->getDelayAmount(), 0, frameBufferLength - 1);
    int delayIndex = ((frameBufferLength + currentFrameIndex - delayAmount) % frameBufferLength);
    int temporalIndex = ((frameBufferLength + currentFrameIndex - 1) % frameBufferLength);
    int storeIndex = ((frameBufferLength + currentFrameIndex - 1) % frameBufferLength);
    
    // Ensure needed frames are allocated
    allocatePastFrameIfNeeded(delayIndex);
    allocatePastFrameIfNeeded(temporalIndex);
    allocatePastFrameIfNeeded(storeIndex);
    
    // Safety checks before processing
    if (!paramManager || !shaderManager) {
        ofLogError("VideoFeedbackManager") << "Missing manager pointers! Cannot process pipeline.";
        return;
    }
    if (!inputTexture.isAllocated()) {
        ofLogError("VideoFeedbackManager") << "Input texture not allocated! Cannot process pipeline.";
        if(mainFbo.isAllocated()) { mainFbo.begin(); ofClear(255,0,0,255); mainFbo.end(); }
        if(sharpenFbo.isAllocated()) { sharpenFbo.begin(); ofClear(255,0,0,255); sharpenFbo.end(); }
        return;
    }
    
    // Get shader with validation
    ofShader& mixerShader = shaderManager->getMixerShader();
    if (!mixerShader.isLoaded()) {
        ofLogError("VideoFeedbackManager") << "Mixer shader not loaded!";
        return;
    }
    
    // Main processing FBO
    mainFbo.begin();
    ofClear(0, 0, 0, 255);
    mixerShader.begin();
    
    // Draw the provided input texture
    // Use the dimensions of the mainFbo for drawing
    inputTexture.draw(0, 0, mainFbo.getWidth(), mainFbo.getHeight());
    
    // Get parameters
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
    float zFrequency = paramManager->getZFrequency();
    float xFrequency = paramManager->getXFrequency();
    float yFrequency = paramManager->getYFrequency();
    float rotate = paramManager->getRotate();
    float hueModulation = paramManager->getHueModulation();
    float hueOffset = paramManager->getHueOffset();
    float hueLFO = paramManager->getHueLFO();
    float xLfoAmp = paramManager->getXLfoAmp();
    float xLfoRate = paramManager->getXLfoRate();
    float yLfoAmp = paramManager->getYLfoAmp();
    float yLfoRate = paramManager->getYLfoRate();
    float zLfoAmp = paramManager->getZLfoAmp();
    float zLfoRate = paramManager->getZLfoRate();
    float rotateLfoAmp = paramManager->getRotateLfoAmp();
    float rotateLfoRate = paramManager->getRotateLfoRate();

    if (frameBufferLength <= 0) delayIndex = 0;
    if (delayIndex < 0 || delayIndex >= frameBufferLength) delayIndex = 0;

    // Apply LFO modulation
    xDisplace += 0.01f * xLfoAmp * sin(ofGetElapsedTimef() * xLfoRate);
    yDisplace += 0.01f * yLfoAmp * sin(ofGetElapsedTimef() * yLfoRate);
    zDisplace *= (1.0f + 0.05f * zLfoAmp * sin(ofGetElapsedTimef() * zLfoRate));
    rotate += 0.314159265f * rotateLfoAmp * sin(ofGetElapsedTimef() * rotateLfoRate);

    try {
        // Send textures
        if (pastFrames && delayIndex >= 0 && delayIndex < frameBufferLength && pastFrames[delayIndex].isAllocated()) {
            mixerShader.setUniformTexture("fb", pastFrames[delayIndex].getTexture(), 1);
        } else {
             // Maybe bind a black texture or just don't bind if feedback frame isn't ready?
             // ofLogWarning("VideoFeedbackManager") << "Feedback texture at index " << delayIndex << " not ready.";
        }
        int tempIdx = ((frameBufferLength + currentFrameIndex - 1) % frameBufferLength);
        if (tempIdx >= 0 && tempIdx < frameBufferLength && pastFrames[tempIdx].isAllocated()) {
            if (paramManager->isWetModeEnabled()) {
                mixerShader.setUniformTexture("temporalFilter", pastFrames[tempIdx].getTexture(), 2);
            } else {
                 if(dryFrameBuffer.isAllocated()) {
                    mixerShader.setUniformTexture("temporalFilter", dryFrameBuffer.getTexture(), 2);
                 }
            }
        } else {
            // ofLogWarning("VideoFeedbackManager") << "Temporal filter texture at index " << tempIdx << " not ready.";
        }

        // Send uniforms
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
        mixerShader.setUniform1f("fbZFrequency", zFrequency);
        mixerShader.setUniform1f("fbXFrequency", xFrequency);
        mixerShader.setUniform1f("fbYFrequency", yFrequency);
        mixerShader.setUniform1f("fbRotate", rotate);
        mixerShader.setUniform1f("fbHuexMod", hueModulation);
        mixerShader.setUniform1f("fbHuexOff", hueOffset);
        mixerShader.setUniform1f("fbHuexLfo", hueLFO);
        mixerShader.setUniform1i("toroidSwitch", paramManager->isToroidEnabled() ? 1 : 0);
        mixerShader.setUniform1i("mirrorSwitch", paramManager->isMirrorModeEnabled() ? 1 : 0);
        mixerShader.setUniform1i("brightInvert", paramManager->isBrightnessInverted() ? 1 : 0);
        mixerShader.setUniform1i("hueInvert", paramManager->isHueInverted() ? 1 : 0);
        mixerShader.setUniform1i("saturationInvert", paramManager->isSaturationInverted() ? 1 : 0);
        mixerShader.setUniform1i("horizontalMirror", paramManager->isHorizontalMirrorEnabled() ? 1 : 0);
        mixerShader.setUniform1i("verticalMirror", paramManager->isVerticalMirrorEnabled() ? 1 : 0);
        mixerShader.setUniform1i("lumakeyInvertSwitch", paramManager->isLumakeyInverted() ? 1 : 0);
        mixerShader.setUniform1f("vLumakey", paramManager->getVLumakeyValue());
        mixerShader.setUniform1f("vMix", paramManager->getVMix());
        mixerShader.setUniform1f("vHue", paramManager->getVHue());
        mixerShader.setUniform1f("vSat", paramManager->getVSaturation());
        mixerShader.setUniform1f("vBright", paramManager->getVBrightness());
        mixerShader.setUniform1f("vtemporalFilterMix", paramManager->getVTemporalFilterMix());
        mixerShader.setUniform1f("vFb1X", paramManager->getVTemporalFilterResonance()); // Mismatch? vFb1X vs vTemporalFilterResonance
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
            ofSetColor(255); mainFbo.draw(0, 0); sharpenFbo.end(); return;
        }
        sharpenShader.begin();
        mainFbo.draw(0, 0);
        sharpenShader.setUniform1f("sharpenAmount", sharpenAmount);
        sharpenShader.setUniform1f("vSharpenAmount", paramManager->getVSharpenAmount());
        sharpenShader.end();
        sharpenFbo.end();
        
        // Store frame in circular buffer
        int storeIdx = ((frameBufferLength + currentFrameIndex - 1) % frameBufferLength);
         if (storeIdx >= 0 && storeIdx < frameBufferLength && pastFrames && pastFrames[storeIdx].isAllocated()) {
            pastFrames[storeIdx].begin();
            if (!paramManager->isWetModeEnabled()) {
                 // In dry mode, store the *input* texture directly (before processing)
                 if(inputTexture.isAllocated()) { inputTexture.draw(0, 0, pastFrames[storeIdx].getWidth(), pastFrames[storeIdx].getHeight()); } 
                 else { ofClear(0,0,0,255); }
                // Update the dry frame buffer for temporal filtering (store processed frame here)
                if(dryFrameBuffer.isAllocated()) {
                    dryFrameBuffer.begin(); sharpenFbo.draw(0, 0); dryFrameBuffer.end();
                }
            } else {
                // In wet mode, store the processed output (from sharpenFbo)
                if(sharpenFbo.isAllocated()) { sharpenFbo.draw(0, 0); } 
                else { ofClear(0,0,0,255); }
            }
            pastFrames[storeIdx].end();
        }
    }
    catch (const std::exception& e) {
        ofLogError("VideoFeedbackManager") << "Exception in processMainPipeline: " << e.what();
    }
    catch (...) {
        ofLogError("VideoFeedbackManager") << "Unknown exception in processMainPipeline";
    }
    // } // End of frameSkip check (optional)
}


void VideoFeedbackManager::draw() {
    if (sharpenFbo.isAllocated()) {
        sharpenFbo.draw(0, 0, ofGetWidth(), ofGetHeight());
    } else {
        ofSetColor(255,0,0); ofDrawRectangle(0,0,ofGetWidth(), ofGetHeight());
        ofSetColor(255); ofDrawBitmapString("Output FBO not allocated", 20, 20);
    }
}

void VideoFeedbackManager::clearFbos() {
    if(mainFbo.isAllocated()) { mainFbo.begin(); ofClear(0, 0, 0, 255); mainFbo.end(); }
    if(aspectRatioFbo.isAllocated()) { aspectRatioFbo.begin(); ofClear(0, 0, 0, 255); aspectRatioFbo.end(); }
    if(dryFrameBuffer.isAllocated()) { dryFrameBuffer.begin(); ofClear(0, 0, 0, 255); dryFrameBuffer.end(); }
    if(sharpenFbo.isAllocated()) { sharpenFbo.begin(); ofClear(0, 0, 0, 255); sharpenFbo.end(); }
    for (int i = 0; i < frameBufferLength; i++) {
         if (pastFramesAllocated && pastFramesAllocated[i] && pastFrames[i].isAllocated()) {
            pastFrames[i].begin(); ofClear(0, 0, 0, 255); pastFrames[i].end();
        }
    }
}

// --- Camera related methods remain ---
void VideoFeedbackManager::listVideoDevices() {
    videoDevices = camera.listDevices();
    ofLogNotice("VideoFeedbackManager") << "Available video input devices:";
    for (int i = 0; i < videoDevices.size(); i++) {
        const auto& device = videoDevices[i];
        ofLogNotice("VideoFeedbackManager") << i << ": " << device.deviceName << " (id:" << device.id << ")";
    }
}

std::vector<std::string> VideoFeedbackManager::getVideoDeviceList() const {
    std::vector<std::string> deviceNames;
    for (const auto& device : videoDevices) { deviceNames.push_back(device.deviceName); }
    return deviceNames;
}

int VideoFeedbackManager::getCurrentVideoDeviceIndex() const { return currentVideoDeviceIndex; }

std::string VideoFeedbackManager::getCurrentVideoDeviceName() const {
    if (videoDevices.empty()) {
        const_cast<VideoFeedbackManager*>(this)->listVideoDevices(); 
    }
    if (currentVideoDeviceIndex >= 0 && currentVideoDeviceIndex < videoDevices.size()) {
        return videoDevices[currentVideoDeviceIndex].deviceName;
    }
    return "No device selected";
}

bool VideoFeedbackManager::selectVideoDevice(int deviceIndex) {
    if (videoDevices.empty()) listVideoDevices(); 
    if (deviceIndex < 0 || deviceIndex >= videoDevices.size()) {
        ofLogError("VideoFeedbackManager") << "Invalid device index: " << deviceIndex; return false;
    }
    if (cameraInitialized) { camera.close(); cameraInitialized = false; }
    currentVideoDeviceIndex = deviceIndex;
    ofLogNotice("VideoFeedbackManager") << "Selecting video device: " << videoDevices[deviceIndex].deviceName;
    if (paramManager) { paramManager->setVideoDeviceID(videoDevices[deviceIndex].id); } 
    camera.setDeviceID(videoDevices[deviceIndex].id);
    int vidWidth = width; int vidHeight = height;
    if (paramManager) { vidWidth = paramManager->getVideoWidth(); vidHeight = paramManager->getVideoHeight(); }
    cameraInitialized = camera.setup(vidWidth, vidHeight); 
    if (cameraInitialized) {
        ofLogNotice("VideoFeedbackManager") << "Camera initialized successfully: " << camera.getWidth() << "x" << camera.getHeight();
        if (paramManager) { paramManager->setVideoWidth(camera.getWidth()); paramManager->setVideoHeight(camera.getHeight()); }
    } else {
        ofLogError("VideoFeedbackManager") << "Failed to initialize camera with ID: " << videoDevices[deviceIndex].id;
    }
    return cameraInitialized;
}

bool VideoFeedbackManager::selectVideoDevice(const std::string& deviceName) {
     if (videoDevices.empty()) listVideoDevices();
    for (int i = 0; i < videoDevices.size(); i++) {
        if (videoDevices[i].deviceName == deviceName) { return selectVideoDevice(i); }
    }
    ofLogError("VideoFeedbackManager") << "Video device not found: " << deviceName; return false;
}

void VideoFeedbackManager::setupCamera(int width, int height) {
    this->width = width; this->height = height;
    ofLogNotice("VideoFeedbackManager") << "Setting up camera with dimensions: " << width << "x" << height;
    #ifdef TARGET_LINUX
    setenv("OF_VIDEO_CAPTURE_BACKEND", "v4l2", 1);
    setenv("GST_DEBUG", "0", 1);
    std::string devicePath = "/dev/video0";
    if (paramManager) { devicePath = paramManager->getVideoDevicePath(); }
    ofLogNotice("VideoFeedbackManager") << "Using device path: " << devicePath;
    auto devices_v4l2 = V4L2Helper::listDevices(); 
    ofLogNotice("VideoFeedbackManager") << "Found " << devices_v4l2.size() << " video devices (V4L2):";
    for (const auto& device : devices_v4l2) { ofLogNotice("VideoFeedbackManager") << "  " << device.id << ": " << device.name << " (" << device.path << ")"; }
    for (const auto& device : devices_v4l2) {
        if (device.path == devicePath) {
            auto formats = V4L2Helper::listFormats(devicePath);
            ofLogNotice("VideoFeedbackManager") << "Available formats for " << device.name << ":";
            for (const auto& format : formats) { ofLogNotice("VideoFeedbackManager") << "  " << format.name << " (" << format.fourcc << ")"; }
            uint32_t selectedFormat = 0; std::string preferredFormats[] = {"YUYV", "YUY2", "MJPG", "RGB3"};
            for (const auto& preferred : preferredFormats) {
                for (const auto& format : formats) { if (format.fourcc == preferred) { selectedFormat = format.pixelFormat; ofLogNotice("VideoFeedbackManager") << "Selected format: " << format.name << " (" << format.fourcc << ")"; break; } }
                if (selectedFormat != 0) break;
            }
            if (selectedFormat == 0 && !formats.empty()) { selectedFormat = formats[0].pixelFormat; ofLogNotice("VideoFeedbackManager") << "Using first available format: " << formats[0].name << " (" << formats[0].fourcc << ")"; }
            if (selectedFormat != 0) {
                auto resolutions = V4L2Helper::listResolutions(devicePath, selectedFormat);
                ofLogNotice("VideoFeedbackManager") << "Available resolutions:";
                for (const auto& res : resolutions) { ofLogNotice("VideoFeedbackManager") << "  " << res.width << "x" << res.height; }
                int bestWidth = 0, bestHeight = 0;
                for (const auto& res : resolutions) { if (res.width <= width && res.height <= height && (res.width > bestWidth || res.height > bestHeight)) { bestWidth = res.width; bestHeight = res.height; } }
                if (bestWidth == 0 || bestHeight == 0) { bestWidth = 10000; bestHeight = 10000; for (const auto& res : resolutions) { if (res.width < bestWidth && res.height < bestHeight) { bestWidth = res.width; bestHeight = res.height; } } }
                if (bestWidth > 0 && bestWidth < 10000 && bestHeight > 0 && bestHeight < 10000) { ofLogNotice("VideoFeedbackManager") << "Setting format to " << bestWidth << "x" << bestHeight; V4L2Helper::setFormat(devicePath, selectedFormat, bestWidth, bestHeight); width = bestWidth; height = bestHeight; }
            }
            break;
        }
    }
    #endif
    videoDevices = camera.listDevices(); 
    camera.setDesiredFrameRate(30);
    int deviceId = 0; if (paramManager) { deviceId = paramManager->getVideoDeviceID(); }
    
    bool validDevice = false;
    if (!videoDevices.empty()) {
        for(const auto& dev : videoDevices) { if(dev.id == deviceId) { validDevice = true; break; } }
        if (!validDevice) { 
            deviceId = videoDevices[0].id; 
            if(paramManager) paramManager->setVideoDeviceID(deviceId); 
        }
    } else {
        deviceId = -1; // No devices found
         if(paramManager) paramManager->setVideoDeviceID(deviceId);
    }

    if (deviceId != -1) {
        camera.setDeviceID(deviceId);
        ofLogNotice("VideoFeedbackManager") << "Setting camera device ID to: " << deviceId;
        camera.setUseTexture(true);
        cameraInitialized = camera.setup(width, height); 
    } else {
         cameraInitialized = false;
    }

    if (!cameraInitialized) {
        ofLogWarning("VideoFeedbackManager") << "Camera initialization failed. Creating fallback pattern.";
        ofImage fallbackImg; fallbackImg.allocate(width, height, OF_IMAGE_COLOR);
        int squareSize = 40; ofPixels& pixels = fallbackImg.getPixels();
        for (int y = 0; y < height; y++) { for (int x = 0; x < width; x++) { bool isEvenRow = ((y / squareSize) % 2) == 0; bool isEvenCol = ((x / squareSize) % 2) == 0; if (isEvenRow == isEvenCol) { pixels.setColor(x, y, ofColor(80, 10, 100)); } else { pixels.setColor(x, y, ofColor(10, 80, 100)); } if ((x > width/2 - 2 && x < width/2 + 2) || (y > height/2 - 2 && y < height/2 + 2)) { pixels.setColor(x, y, ofColor(255, 0, 0)); } } }
        fallbackImg.update();
        if(aspectRatioFbo.isAllocated()) { aspectRatioFbo.begin(); ofClear(0, 0, 0, 255); fallbackImg.draw(0, 0, aspectRatioFbo.getWidth(), aspectRatioFbo.getHeight()); aspectRatioFbo.end(); }
    } else {
        if (paramManager) { paramManager->setVideoWidth(camera.getWidth()); paramManager->setVideoHeight(camera.getHeight()); }
        // Store the index corresponding to the successfully initialized device ID
        for(int i=0; i<videoDevices.size(); ++i) {
            if(videoDevices[i].id == deviceId) {
                currentVideoDeviceIndex = i;
                break;
            }
        }
    }
}

// Removed updateCamera() method. Camera updates are handled in ofApp. // Re-adding updateCamera
void VideoFeedbackManager::updateCamera() {
    if (cameraInitialized) {
        try {
            camera.update();
            if (camera.isFrameNew()) {
                if(aspectRatioFbo.isAllocated()) {
                    aspectRatioFbo.begin();
                    ofClear(0, 0, 0, 255);
                    int camWidth = camera.getWidth(); int camHeight = camera.getHeight();
                    if (camWidth > 0 && camHeight > 0) {
                        if (hdmiAspectRatioEnabled) { 
                             float targetAspect = 16.0f / 9.0f;
                             float fboAspect = (float)aspectRatioFbo.getWidth() / aspectRatioFbo.getHeight();
                             float drawWidth, drawHeight, xOffset, yOffset;
                             if (fboAspect > targetAspect) { 
                                 drawHeight = aspectRatioFbo.getHeight(); drawWidth = drawHeight * targetAspect;
                                 xOffset = (aspectRatioFbo.getWidth() - drawWidth) / 2.0f; yOffset = 0;
                             } else { 
                                 drawWidth = aspectRatioFbo.getWidth(); drawHeight = drawWidth / targetAspect;
                                 xOffset = 0; yOffset = (aspectRatioFbo.getHeight() - drawHeight) / 2.0f;
                             }
                             camera.draw(xOffset, yOffset, drawWidth, drawHeight);
                        } else {
                            camera.draw(0, 0, aspectRatioFbo.getWidth(), aspectRatioFbo.getHeight());
                        }
                    }
                    aspectRatioFbo.end();
                }
            }
        } catch (std::exception& e) {
            ofLogError("VideoFeedbackManager") << "Error updating camera: " << e.what();
        }
    }
}

// updateCameraTexture is removed 

void VideoFeedbackManager::incrementFrameIndex() {
    frameCount++;
    currentFrameIndex = frameCount % frameBufferLength;
}

// processMainPipeline implementation moved earlier and modified

int VideoFeedbackManager::getFrameBufferLength() const { return frameBufferLength; }

void VideoFeedbackManager::setFrameBufferLength(int length) {
    if (length <= 0) {
        ofLogWarning("VideoFeedbackManager") << "Invalid frame buffer length requested: " << length;
        return;
    }
    if (pastFrames) delete[] pastFrames;
    if (pastFramesAllocated) delete[] pastFramesAllocated;

    frameBufferLength = length;
    pastFrames = new ofFbo[frameBufferLength];
    pastFramesAllocated = new bool[frameBufferLength];
    for (int i = 0; i < frameBufferLength; i++) pastFramesAllocated[i] = false;
    
    // Re-allocate essential FBOs if dimensions might have changed implicitly
    // allocateFbos(width, height); // Consider if needed
    
    for (int i = 0; i < std::min(5, frameBufferLength); i++) {
        allocatePastFrameIfNeeded(i);
    }
    currentFrameIndex = 0; 
    frameCount = 0;
    ofLogNotice("VideoFeedbackManager") << "Frame buffer length set to: " << frameBufferLength;
}

bool VideoFeedbackManager::isHdmiAspectRatioEnabled() const { return hdmiAspectRatioEnabled; }

void VideoFeedbackManager::setHdmiAspectRatioEnabled(bool enabled) { hdmiAspectRatioEnabled = enabled; }

const ofTexture& VideoFeedbackManager::getOutputTexture() const {
    if (sharpenFbo.isAllocated()) {
        return sharpenFbo.getTexture();
    } else {
        // Return a reference to an empty texture or handle error
        static ofTexture dummy; 
        if (!dummy.isAllocated()) dummy.allocate(1, 1, GL_RGBA); 
        ofLogError("VideoFeedbackManager::getOutputTexture") << "Sharpen FBO not allocated, returning dummy texture.";
        return dummy; 
    }
}

void VideoFeedbackManager::checkGLError(const std::string& operation) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::string errorString;
        switch (err) {
            case GL_INVALID_ENUM: errorString = "GL_INVALID_ENUM"; break;
            case GL_INVALID_VALUE: errorString = "GL_INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: errorString = "GL_INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY: errorString = "GL_OUT_OF_MEMORY"; break;
            default: errorString = "Unknown GL error code " + ofToString(err); break;
        }
        ofLogError("VideoFeedbackManager") << "OpenGL error after " << operation << ": " << errorString;
    }
}

void VideoFeedbackManager::saveToXml(ofxXmlSettings& xml) const {
    // Assumes xml is already pushed into paramManager tag
    if (!xml.tagExists("videoFeedback")) {
        xml.addTag("videoFeedback");
    }
    xml.pushTag("videoFeedback");
    
    // Save framebuffer settings (Device settings removed from here, saved in ParamManager)
    xml.setValue("frameBufferLength", frameBufferLength);
    xml.setValue("hdmiAspectRatioEnabled", hdmiAspectRatioEnabled ? 1 : 0);
    
    xml.popTag(); // pop videoFeedback
}

void VideoFeedbackManager::loadFromXml(ofxXmlSettings& xml) {
    // Assumes xml is already pushed into paramManager tag
    if (xml.tagExists("videoFeedback")) {
        xml.pushTag("videoFeedback");
        
        ofLogNotice("VideoFeedbackManager") << "Loading video feedback settings";
        
        // Load framebuffer settings (Device settings removed from here, loaded via ParamManager in ofApp::setup)
        int savedBufferLength = xml.getValue("frameBufferLength", DEFAULT_FRAME_BUFFER_LENGTH);
        if (savedBufferLength != frameBufferLength && savedBufferLength > 0) {
             setFrameBufferLength(savedBufferLength); // Use the setter to handle reallocation
        }
        
        bool aspectEnabled = xml.getValue("hdmiAspectRatioEnabled", 0) != 0;
        setHdmiAspectRatioEnabled(aspectEnabled);
        ofLogNotice("VideoFeedbackManager") << "HDMI aspect ratio " << (aspectEnabled ? "enabled" : "disabled");
        
        xml.popTag(); // pop videoFeedback
    } else {
        ofLogWarning("VideoFeedbackManager") << "No videoFeedback tag found in settings";
    }
}
