#include "VideoFeedbackManager.h"
#include "V4L2Helper.h"
#ifdef TARGET_LINUX
#include <sys/sysinfo.h>
#endif

VideoFeedbackManager::VideoFeedbackManager(ParameterManager* paramManager, ShaderManager* shaderManager)
    : paramManager(paramManager), shaderManager(shaderManager) {
    // Determine optimal frame buffer length based on platform
    frameBufferLength = determineOptimalFrameBufferLength();
    
    // Allocate pastFrames array
    pastFrames = new ofFbo[frameBufferLength];
    
    // Allocate tracking array
    pastFramesAllocated = new bool[frameBufferLength];
    for (int i = 0; i < frameBufferLength; i++) {
        pastFramesAllocated[i] = false;
    }
}

void VideoFeedbackManager::setup(int width, int height) {
    this->width = width;
    this->height = height;
    
    // Setup camera and allocate FBOs
    setupCamera(width, height);
    allocateFbos(width, height);
    
    // Only pre-allocate the first few frames that are frequently used
    for (int i = 0; i < std::min(5, frameBufferLength); i++) {
        allocatePastFrameIfNeeded(i);
    }
    
    clearFbos();
}

void VideoFeedbackManager::allocateFbos(int width, int height) {
    // Check if we're in performance mode
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
    
    // IMPORTANT FIX: Always use RGBA8 on ARM Linux
    bool isArmLinux = false;
    #if defined(TARGET_LINUX)
        #if defined(__arm__) || defined(__aarch64__)
            isArmLinux = true;
        #else
            // Fallback detection method using /proc/cpuinfo
            FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
            if (cpuinfo) {
                char line[256];
                while (fgets(line, sizeof(line), cpuinfo)) {
                    if (strstr(line, "Raspberry Pi") || strstr(line, "BCM27") ||
                        strstr(line, "BCM28") || strstr(line, "ARM")) {
                        isArmLinux = true;
                        break;
                    }
                }
                fclose(cpuinfo);
            }
        #endif
    #endif

    if (isArmLinux) {
        ofLogNotice("VideoFeedbackManager") << "ARM Linux detected, using GL_RGBA8 for better compatibility";
        settings.internalformat = GL_RGBA8;  // Always use RGBA8 on ARM Linux
    } else {
        #ifdef TARGET_LINUX
            settings.internalformat = GL_RGB;
        #else
            settings.internalformat = GL_RGBA8;  // Default to RGBA8 on non-Linux platforms
        #endif
    }

    // Store the settings for later use
    fboSettings = settings;

    ofLogNotice("VideoFeedbackManager") << "Allocating FBOs with format: "
                                       << (settings.internalformat == GL_RGBA8 ? "GL_RGBA8" : "GL_RGB");
    
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
        
        // Initialize allocation tracking (if implemented)
        if (pastFramesAllocated) {
            for (int i = 0; i < frameBufferLength; i++) {
                pastFramesAllocated[i] = false;
            }
            
            // Pre-allocate just a few frames to start
            int preAllocateCount = std::min(5, frameBufferLength);
            for (int i = 0; i < preAllocateCount; i++) {
                allocatePastFrameIfNeeded(i);
            }
        } else {
            // Traditional allocation if lazy loading not implemented
            for (int i = 0; i < frameBufferLength; i++) {
                pastFrames[i].allocate(settings);
                pastFrames[i].begin();
                ofClear(0, 0, 0, 255);
                pastFrames[i].end();
            }
        }
        
        ofLogNotice("VideoFeedbackManager") << "FBOs allocated with "
                                           << fboWidth << "x" << fboHeight << " resolution";
    } catch (std::exception& e) {
        ofLogError("VideoFeedbackManager") << "Error allocating FBOs: " << e.what();
    }
}

void VideoFeedbackManager::update() {
    // Get performance mode setting and determine appropriate frame rate divisor
    int frameSkip = getFrameSkipFactor();
    
    // Always update camera input
    updateCamera();
    
    // Process main pipeline only on certain frames to improve performance
    if (frameCount % frameSkip == 0) {
        // Pre-allocate frames we'll need for this frame
        int delayAmount = ofClamp(paramManager->getDelayAmount(), 0, frameBufferLength - 1);
        int delayIndex = ((frameBufferLength + currentFrameIndex - delayAmount) % frameBufferLength);
        int temporalIndex = ((frameBufferLength + currentFrameIndex - 1) % frameBufferLength);
        int storeIndex = ((frameBufferLength + currentFrameIndex - 1) % frameBufferLength);
        
        // Ensure needed frames are allocated
        allocatePastFrameIfNeeded(delayIndex);
        allocatePastFrameIfNeeded(temporalIndex);
        allocatePastFrameIfNeeded(storeIndex);
        
        // Process the main video feedback pipeline
        processMainPipeline();
    }
    
    // Increment frame index for circular buffer
    incrementFrameIndex();
}

void VideoFeedbackManager::draw() {
    // Draw the final output to screen
    sharpenFbo.draw(0, 0, ofGetWidth(), ofGetHeight());
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

// Add this method to VideoFeedbackManager.cpp

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
    
    // Get list of devices using V4L2Helper
    auto devices = V4L2Helper::listDevices();
    ofLogNotice("VideoFeedbackManager") << "Found " << devices.size() << " video devices:";
    for (const auto& device : devices) {
        ofLogNotice("VideoFeedbackManager") << "  " << device.id << ": " << device.name << " (" << device.path << ")";
    }
    
    // Find the device that matches our path
    for (const auto& device : devices) {
        if (device.path == devicePath) {
            // Get available formats for this device
            auto formats = V4L2Helper::listFormats(devicePath);
            ofLogNotice("VideoFeedbackManager") << "Available formats for " << device.name << ":";
            
            for (const auto& format : formats) {
                ofLogNotice("VideoFeedbackManager") << "  " << format.name << " (" << format.fourcc << ")";
            }
            
            // Select a preferred format (prioritize YUYV for compatibility)
            uint32_t selectedFormat = 0;
            std::string preferredFormats[] = {"YUYV", "YUY2", "MJPG", "RGB3"};
            
            for (const auto& preferred : preferredFormats) {
                for (const auto& format : formats) {
                    if (format.fourcc == preferred) {
                        selectedFormat = format.pixelFormat;
                        ofLogNotice("VideoFeedbackManager") << "Selected format: " << format.name << " (" << format.fourcc << ")";
                        break;
                    }
                }
                if (selectedFormat != 0) break;
            }
            
            // If no preferred format found, use the first available
            if (selectedFormat == 0 && !formats.empty()) {
                selectedFormat = formats[0].pixelFormat;
                ofLogNotice("VideoFeedbackManager") << "Using first available format: "
                                                  << formats[0].name << " (" << formats[0].fourcc << ")";
            }
            
            // Get available resolutions for this format
            if (selectedFormat != 0) {
                auto resolutions = V4L2Helper::listResolutions(devicePath, selectedFormat);
                ofLogNotice("VideoFeedbackManager") << "Available resolutions:";
                
                for (const auto& res : resolutions) {
                    ofLogNotice("VideoFeedbackManager") << "  " << res.width << "x" << res.height;
                }
                
                // Find the closest resolution that's not larger than requested
                int bestWidth = 0, bestHeight = 0;
                for (const auto& res : resolutions) {
                    if (res.width <= width && res.height <= height &&
                        (res.width > bestWidth || res.height > bestHeight)) {
                        bestWidth = res.width;
                        bestHeight = res.height;
                    }
                }
                
                // If no suitable resolution found, find the smallest one
                if (bestWidth == 0 || bestHeight == 0) {
                    bestWidth = 10000;
                    bestHeight = 10000;
                    for (const auto& res : resolutions) {
                        if (res.width < bestWidth && res.height < bestHeight) {
                            bestWidth = res.width;
                            bestHeight = res.height;
                        }
                    }
                }
                
                // Make sure we have valid dimensions
                if (bestWidth > 0 && bestWidth < 10000 && bestHeight > 0 && bestHeight < 10000) {
                    // Set the format using V4L2Helper
                    ofLogNotice("VideoFeedbackManager") << "Setting format to " << bestWidth << "x" << bestHeight;
                    V4L2Helper::setFormat(devicePath, selectedFormat, bestWidth, bestHeight);
                    
                    // Update width and height to match what we set
                    width = bestWidth;
                    height = bestHeight;
                }
            }
            
            break;
        }
    }
#endif

    // List available devices before attempting to initialize
    camera.listDevices();
    videoDevices = camera.listDevices();
    
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
    
    try {
        if (camera.isInitialized()) camera.close();
        
        // Set the texture format to be compatible across platforms
        camera.setUseTexture(true);
        
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

void VideoFeedbackManager::updateCameraTexture() {
    // This would be called from a camera thread in a threaded implementation
    if (cameraInitialized && camera.isFrameNew()) {
        std::lock_guard<std::mutex> lock(fboMutex);
        
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
        }
        
        aspectRatioFbo.end();
        newFrameReady = true;
    }
}

void VideoFeedbackManager::incrementFrameIndex() {
    frameCount++;
    currentFrameIndex = frameCount % frameBufferLength;
}

void VideoFeedbackManager::processMainPipeline() {
    // Pre-allocate only the frames we'll need this frame
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
      
    // Safe index calculation with bounds checking
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

VideoFeedbackManager::~VideoFeedbackManager() {
    if (pastFrames) {
        delete[] pastFrames;
        pastFrames = nullptr;
    }
    
    if (pastFramesAllocated) {
        delete[] pastFramesAllocated;
        pastFramesAllocated = nullptr;
    }
}
