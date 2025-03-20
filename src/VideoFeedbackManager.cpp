#include "VideoFeedbackManager.h"
#include "V4L2Helper.h"

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
    // Check if we're in performance mode (regardless of platform)
    bool performanceMode = paramManager ? paramManager->isPerformanceModeEnabled() : false;
    int fboWidth = width;
    int fboHeight = height;
    
    // Reduce resolution in performance mode
    if (performanceMode) {
        float aspectRatio = (float)height / width;
        fboWidth = std::min(width, 640);
        fboHeight = round(fboWidth * aspectRatio);
        ofLogNotice("VideoFeedbackManager") << "Performance Mode: Reduced FBO resolution to "
                                           << fboWidth << "x" << fboHeight;
    }
    
    // Create platform-agnostic FBO settings
    ofFboSettings settings;
    settings.width = fboWidth;
    settings.height = fboHeight;
    settings.numColorbuffers = 1;
    settings.useDepth = false;
    settings.useStencil = false;
    settings.numSamples = 0;  // No MSAA for compatibility
    
    // Use explicit RGB format on Linux, RGBA elsewhere for better compatibility
#ifdef TARGET_LINUX
    settings.internalformat = GL_RGB;
#else
    settings.internalformat = GL_RGBA8;  // Explicit RGBA8 for macOS/Windows
#endif
    
    // Allocate FBOs with proper error handling
    try {
        // Main processing FBO
        mainFbo.allocate(settings);
        mainFbo.begin();
        ofClear(0, 0, 0, 255);
        mainFbo.end();
        
        // Aspect ratio correction FBO
        aspectRatioFbo.allocate(settings);
        aspectRatioFbo.begin();
        ofClear(0, 0, 0, 255);
        aspectRatioFbo.end();
        
        // Dry mode frame buffer
        dryFrameBuffer.allocate(settings);
        dryFrameBuffer.begin();
        ofClear(0, 0, 0, 255);
        dryFrameBuffer.end();
        
        // Sharpen effect buffer
        sharpenFbo.allocate(settings);
        sharpenFbo.begin();
        ofClear(0, 0, 0, 255);
        sharpenFbo.end();
        
        // Allocate past frames for delay effect
        for (int i = 0; i < frameBufferLength; i++) {
            pastFrames[i].allocate(settings);
            pastFrames[i].begin();
            ofClear(0, 0, 0, 255);
            pastFrames[i].end();
        }
        
        ofLogNotice("VideoFeedbackManager") << "FBOs allocated with "
                                           << fboWidth << "x" << fboHeight << " resolution";
    } catch (std::exception& e) {
        ofLogError("VideoFeedbackManager") << "Error allocating FBOs: " << e.what();
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
    
    // Close current camera if needed
    if (cameraInitialized) {
        camera.close();
        cameraInitialized = false;
    }
    
    // Update current device index
    currentVideoDeviceIndex = deviceIndex;
    
    // Log selection
    ofLogNotice("VideoFeedbackManager") << "Selecting video device: "
                                       << videoDevices[deviceIndex].deviceName;
    
    // Update parameter manager
    if (paramManager) {
        paramManager->setVideoDeviceID(deviceIndex);
    }
    
    // Set device ID and reopen camera
    camera.setDeviceID(videoDevices[deviceIndex].id);
    
    // Get dimensions from parameter manager if available
    int vidWidth = width;
    int vidHeight = height;
    if (paramManager) {
        vidWidth = paramManager->getVideoWidth();
        vidHeight = paramManager->getVideoHeight();
    }
    
    // Initialize camera
    camera.initGrabber(vidWidth, vidHeight);
    cameraInitialized = camera.isInitialized();
    
    // Log result
    if (cameraInitialized) {
        ofLogNotice("VideoFeedbackManager") << "Camera initialized successfully: "
                                           << camera.getWidth() << "x" << camera.getHeight();
        
        // Update actual dimensions in parameter manager
        if (paramManager) {
            paramManager->setVideoWidth(camera.getWidth());
            paramManager->setVideoHeight(camera.getHeight());
        }
    } else {
        ofLogError("VideoFeedbackManager") << "Failed to initialize camera";
    }
    
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
    
    // Log that we're setting up the camera
    ofLogNotice("VideoFeedbackManager") << "Setting up camera with dimensions: " << width << "x" << height;
    
#ifdef TARGET_LINUX
    // On Raspberry Pi / Linux, ensure we have the correct Video4Linux backends set
    setenv("OF_VIDEO_CAPTURE_BACKEND", "v4l2", 1);
    
    // Disable verbose GST logging that might impact performance
    setenv("GST_DEBUG", "0", 1);
    
    // Get device path from parameter manager or use default
    std::string devicePath = "/dev/video0";
    if (paramManager) {
        devicePath = paramManager->getVideoDevicePath();
    }
    
    ofLogNotice("VideoFeedbackManager") << "Using device path: " << devicePath;
    
    // Get or list available video formats to choose the most compatible one
    auto formats = V4L2Helper::listFormats(devicePath);
    ofLogNotice("VideoFeedbackManager") << "Available formats:";
    bool hasYUYV = false;
    for (const auto& format : formats) {
        ofLogNotice("VideoFeedbackManager") << "  " << format.name << " (" << format.fourcc << ")";
        if (format.fourcc == "YUYV") {
            hasYUYV = true;
        }
    }
    
    // Force YUYV format for the EM2860 devices - this format is most reliable
    if (hasYUYV) {
        ofLogNotice("VideoFeedbackManager") << "Pre-configuring for YUYV format";
        
        // Try multiple common resolutions for this device, starting with requested dimensions
        std::vector<std::pair<int, int>> resolutions = {
            {width, height},    // Requested dimensions
            {640, 480},         // Standard VGA
            {720, 576},         // PAL
            {720, 480},         // NTSC
            {352, 288},         // CIF
            {320, 240}          // QVGA (fallback)
        };
        
        bool formatSet = false;
        for (const auto& res : resolutions) {
            // Try to set the format via V4L2Helper
            if (V4L2Helper::setFormat(devicePath, V4L2Helper::formatNameToCode("YUYV"),
                                     res.first, res.second)) {
                ofLogNotice("VideoFeedbackManager") << "Successfully set camera format to YUYV "
                                                   << res.first << "x" << res.second;
                
                width = res.first;   // Update dimensions to what was actually set
                height = res.second;
                formatSet = true;
                break;
            }
        }
        
        if (!formatSet) {
            ofLogWarning("VideoFeedbackManager") << "Failed to set camera format, continuing anyway...";
        }
    }
#endif

    // List available devices before attempting to initialize
    listVideoDevices();
    
    // Set reasonable defaults for camera
    camera.setDesiredFrameRate(30);
    
    // Using the detected device ID
    int deviceId = 0;
    if (paramManager) {
        deviceId = paramManager->getVideoDeviceID();
    }
    
    camera.setDeviceID(deviceId);
    ofLogNotice("VideoFeedbackManager") << "Setting camera device ID to: " << deviceId;
    
    // Try initialize with requested dimensions
    bool initSuccess = false;
    
    // First attempt with clean settings
    try {
        if (camera.isInitialized()) camera.close();
        ofLogNotice("VideoFeedbackManager") << "Initializing camera at " << width << "x" << height;
        camera.initGrabber(width, height);
        if (camera.isInitialized()) {
            initSuccess = true;
            ofLogNotice("VideoFeedbackManager") << "Successfully initialized camera with dimensions: "
                                              << camera.getWidth() << "x" << camera.getHeight();
        }
    } catch (const std::exception& e) {
        ofLogError("VideoFeedbackManager") << "Exception initializing camera: " << e.what();
    }
    
    // If first attempt failed, try different resolutions
    if (!initSuccess) {
        std::vector<std::pair<int, int>> fallbackResolutions = {
            {640, 480},   // Standard VGA
            {720, 576},   // PAL
            {720, 480},   // NTSC
            {320, 240},   // QVGA
            {352, 288},   // CIF
            {160, 120}    // QQVGA (very low resolution fallback)
        };
        
        for (const auto& res : fallbackResolutions) {
            try {
                ofLogNotice("VideoFeedbackManager") << "Trying fallback resolution: " << res.first << "x" << res.second;
                if (camera.isInitialized()) camera.close();
                camera.initGrabber(res.first, res.second);
                
                if (camera.isInitialized()) {
                    initSuccess = true;
                    ofLogNotice("VideoFeedbackManager") << "Successfully initialized camera with fallback dimensions: "
                                                     << camera.getWidth() << "x" << camera.getHeight();
                    break;
                }
            } catch (const std::exception& e) {
                ofLogError("VideoFeedbackManager") << "Exception with fallback resolution: " << e.what();
            }
        }
    }
    
    // Last resort: try to use ANY available settings
    if (!initSuccess) {
        try {
            ofLogNotice("VideoFeedbackManager") << "Trying initialization with default settings";
            if (camera.isInitialized()) camera.close();
            camera.initGrabber(320, 240); // Use smallest common resolution
            
            if (camera.isInitialized()) {
                initSuccess = true;
                ofLogNotice("VideoFeedbackManager") << "Successfully initialized camera with minimal settings: "
                                                 << camera.getWidth() << "x" << camera.getHeight();
            } else {
                ofLogError("VideoFeedbackManager") << "All camera initialization attempts failed";
            }
        } catch (const std::exception& e) {
            ofLogError("VideoFeedbackManager") << "Last exception in camera init: " << e.what();
        }
    }
    
    cameraInitialized = initSuccess;
    
    // Create fallback image if camera failed
    if (!cameraInitialized) {
        ofLogWarning("VideoFeedbackManager") << "Creating fallback test pattern since camera initialization failed";
        
        ofImage fallbackImg;
        fallbackImg.allocate(width, height, OF_IMAGE_COLOR);
        
        // Create a test pattern (checkerboard)
        int squareSize = 40;
        ofPixels& pixels = fallbackImg.getPixels();
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Create a checkerboard pattern
                bool isEvenRow = ((y / squareSize) % 2) == 0;
                bool isEvenCol = ((x / squareSize) % 2) == 0;
                
                if (isEvenRow == isEvenCol) {
                    pixels.setColor(x, y, ofColor(80, 10, 100)); // Purple
                } else {
                    pixels.setColor(x, y, ofColor(10, 80, 100)); // Teal-ish
                }
                
                // Add cross in the middle to show center and orientation
                if ((x > width/2 - 2 && x < width/2 + 2) ||
                    (y > height/2 - 2 && y < height/2 + 2)) {
                    pixels.setColor(x, y, ofColor(255, 0, 0)); // Red cross
                }
            }
        }
        
        fallbackImg.update();
        
        // Copy to aspect ratio FBO as fallback
        aspectRatioFbo.begin();
        ofClear(0, 0, 0, 255);
        fallbackImg.draw(0, 0, width, height);
        aspectRatioFbo.end();
    } else {
        // Update actual dimensions in parameter manager if initialized successfully
        if (paramManager) {
            paramManager->setVideoWidth(camera.getWidth());
            paramManager->setVideoHeight(camera.getHeight());
        }
    }
}

void VideoFeedbackManager::updateCamera() {
    if (cameraInitialized) {
        try {
            // Update the camera with proper error handling
            camera.update();
            
            // If there's a new frame, copy it to our FBO with careful error handling
            if (camera.isFrameNew()) {
                ofLogVerbose("VideoFeedbackManager") << "New camera frame available";
                aspectRatioFbo.begin();
                ofClear(0, 0, 0, 255);
                
                // Make sure dimensions are valid
                int camWidth = camera.getWidth();
                int camHeight = camera.getHeight();
                
                if (camWidth > 0 && camHeight > 0) {
                    // Draw using different aspect ratio handling based on settings
                    if (hdmiAspectRatioEnabled) {
                        // Use 16:9 aspect ratio correction
                        camera.draw(0, 0, 853, 480);
                    } else {
                        // Use normal dimensions
                        camera.draw(0, 0, width, height);
                    }
                    
                    // Log successful camera draw (once during development)
                    static bool firstDraw = true;
                    if (firstDraw) {
                        ofLogNotice("VideoFeedbackManager") << "Successfully drew camera frame at "
                                                           << camWidth << "x" << camHeight;
                        firstDraw = false;
                    }
                } else {
                    ofLogError("VideoFeedbackManager") << "Invalid camera dimensions: "
                                                      << camWidth << "x" << camHeight;
                }
                aspectRatioFbo.end();
            }
        } catch (std::exception& e) {
            ofLogError("VideoFeedbackManager") << "Error updating camera: " << e.what();
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
        ofLogError("VideoFeedbackManager") << "Missing manager pointers! Cannot process pipeline.";
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
    
    // Clear background first to avoid artifacts
    ofClear(0, 0, 0, 255);
    
    // Begin shader
    mixerShader.begin();
    
    // Add debug info for video texture
    static bool textureDebugDone = false;
    if (!textureDebugDone) {
        ofLogNotice("VideoFeedbackManager") << "Camera texture available: "
                                           << (cameraInitialized && camera.isInitialized());
        ofLogNotice("VideoFeedbackManager") << "aspectRatioFbo allocated: " << aspectRatioFbo.isAllocated();
        textureDebugDone = true;
    }
    
    // Draw camera input (or aspect corrected version) with camera check
    if (cameraInitialized && camera.isInitialized() && aspectRatioFbo.isAllocated()) {
        // Use the aspect-corrected FBO we prepared in updateCamera()
        aspectRatioFbo.draw(0, 0, mainFbo.getWidth(), mainFbo.getHeight());
    } else {
        // Draw fallback if camera not available
        ofSetColor(50, 0, 0);
        ofDrawRectangle(0, 0, mainFbo.getWidth(), mainFbo.getHeight());
        ofSetColor(255, 0, 0);
        ofDrawBitmapString("Camera not available", mainFbo.getWidth()/2 - 70, mainFbo.getHeight()/2);
        ofSetColor(255);
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
        // Send the textures to shader with safety checks
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
