#pragma once

#include "ofMain.h"
#include "ParameterManager.h"
#include "ShaderManager.h"

/**
 * @class VideoFeedbackManager
 * @brief Manages the video feedback processing pipeline and framebuffers
 *
 * This class handles the FBO creation, management of the circular framebuffer
 * array for delay effects, and processes the video through shaders.
 */
class VideoFeedbackManager {
public:
    VideoFeedbackManager(ParameterManager* paramManager, ShaderManager* shaderManager);
    ~VideoFeedbackManager(); // Explicitly declare the destructor
    
    // Core methods
    void setup(int width, int height);
    void update();
    void draw();
    
    // FBO management
    void allocateFbos(int width, int height);
    void clearFbos();
    
    // Camera handling
    void setupCamera(int width, int height);
    void updateCamera();
    void updateCameraTexture(); // New method for threaded camera updates
    
    // Camera
    ofVideoGrabber camera;
    bool cameraInitialized = false;
    
    // Get/set for frame buffer size
    int getFrameBufferLength() const;
    void setFrameBufferLength(int length);
    
    // Get frame skip factor based on performance settings
    int getFrameSkipFactor() const {
        bool performanceMode = paramManager ? paramManager->isPerformanceModeEnabled() : false;
        #if defined(TARGET_LINUX) && (defined(__arm__) || defined(__aarch64__))
            return performanceMode ? 3 : 2;
        #else
            return performanceMode ? 2 : 1;
        #endif
    }
    
    // Toggle HD aspect ratio correction
    bool isHdmiAspectRatioEnabled() const;
    void setHdmiAspectRatioEnabled(bool enabled);
    
    // Lazy allocation for frame buffers
    void allocatePastFrameIfNeeded(int index) {
        if (index >= 0 && index < frameBufferLength && !pastFramesAllocated[index]) {
            pastFrames[index].allocate(fboSettings);
            pastFrames[index].begin();
            ofClear(0, 0, 0, 255);
            pastFrames[index].end();
            pastFramesAllocated[index] = true;
        }
    }
    
    // Video device management methods
    void listVideoDevices();
    std::vector<std::string> getVideoDeviceList() const;
    int getCurrentVideoDeviceIndex() const;
    std::string getCurrentVideoDeviceName() const;
    bool selectVideoDevice(int deviceIndex);
    bool selectVideoDevice(const std::string& deviceName);
    
    // XML settings
    void saveToXml(ofxXmlSettings& xml) const;
    void loadFromXml(ofxXmlSettings& xml);
    
    ofFbo& getAspectRatioFbo() { return aspectRatioFbo; }
    
    // Additional helpful accessor methods
    ofFbo& getMainFbo() { return mainFbo; }
    ofFbo& getSharpenFbo() { return sharpenFbo; }
    ofFbo& getDryFrameBuffer() { return dryFrameBuffer; }
    ofFbo& getPastFrame(int index) {
        if (index >= 0 && index < frameBufferLength) {
            return pastFrames[index];
        }
        return dryFrameBuffer; // Fallback to avoid crashes
    }
        
private:
    // Constants
    static const int DEFAULT_FRAME_BUFFER_LENGTH = 60;
    
    // Helper methods
    void incrementFrameIndex();
    void processMainPipeline();
    void checkGLError(const std::string& operation);
    
    // Determine optimal frame buffer length based on platform
    int determineOptimalFrameBufferLength() {
        #if defined(TARGET_LINUX) && (defined(__arm__) || defined(__aarch64__))
            // Raspberry Pi/ARM - use smaller buffer
            return 30;
        #elif defined(TARGET_OSX) || defined(TARGET_WIN32)
            // Modern desktops - can handle larger buffer
            return 60;
        #else
            // Default fallback
            return 45;
        #endif
    }
    
    // Reference to managers
    ParameterManager* paramManager;
    ShaderManager* shaderManager;
    
    // Resolution
    int width = 640;
    int height = 480;
    bool hdmiAspectRatioEnabled = false;
    
    // Framebuffers
    ofFbo mainFbo;              // Main processing buffer
    ofFbo sharpenFbo;           // Buffer for sharpen effect
    ofFbo dryFrameBuffer;       // Buffer for dry mode
    ofFbo aspectRatioFbo;       // Buffer for aspect ratio correction
    
    // FBO settings storage for reuse
    ofFboSettings fboSettings;  // Store settings for reuse in lazy allocation
    
    // Circular buffer for delay effect
    int frameBufferLength = DEFAULT_FRAME_BUFFER_LENGTH;
    ofFbo* pastFrames = nullptr;
    bool* pastFramesAllocated = nullptr; // Track which frames are allocated
    
    // Thread synchronization
    std::mutex fboMutex;
    bool newFrameReady = false;
    
    // Frame counting and delay management
    unsigned int frameCount = 0;
    int currentFrameIndex = 0;
    
    // Video device management
    std::vector<ofVideoDevice> videoDevices;
    int currentVideoDeviceIndex = 0;
};
