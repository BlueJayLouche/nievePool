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
    void setup(int width, int height); // Setup FBOs and initial state
    // void update(const ofTexture& inputTexture); // Removed - Use processInputTexture instead
    void draw(); // Draw the final output
    
    // FBO management
    void allocateFbos(int width, int height);
    void clearFbos();
    
    // Get/set for frame buffer size (Keep these)
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

    // Public getter for the final output texture
    const ofTexture& getOutputTexture() const;

    // Camera device info accessors (needed by ofApp)
    std::vector<std::string> getVideoDeviceList() const;
    std::string getCurrentVideoDeviceName() const;
    int getCurrentVideoDeviceIndex() const;
    bool selectVideoDevice(int deviceIndex);
    bool selectVideoDevice(const std::string& deviceName);
    void updateCamera(); // Add back declaration
    
    // Make processing and frame index public
    void processMainPipeline(const ofTexture& inputTexture); // Renamed from processInputTexture for consistency
    void incrementFrameIndex();

    // Add getter for camera status
    bool isCameraInitialized() const { return cameraInitialized; }

    // Lazy allocation for frame buffers (Keep this internal detail)
    void allocatePastFrameIfNeeded(int index) {
        if (index >= 0 && index < frameBufferLength && !pastFramesAllocated[index]) {
            pastFrames[index].allocate(fboSettings);
            pastFrames[index].begin();
            ofClear(0, 0, 0, 255);
            pastFrames[index].end();
            pastFramesAllocated[index] = true;
        }
    }
    
    // XML settings (Keep for buffer length, aspect ratio, etc.)
    void saveToXml(ofxXmlSettings& xml) const; 
    void loadFromXml(ofxXmlSettings& xml);
    
    // Add back getter for aspectRatioFbo
    ofFbo& getAspectRatioFbo() { return aspectRatioFbo; }
    
    // Accessors for internal FBOs (might still be useful for debugging or advanced effects)
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
    void listVideoDevices(); // Add back declaration
    void setupCamera(int width, int height); // Add back declaration
    // void incrementFrameIndex(); // Moved to public
    // void processMainPipeline(const ofTexture& inputTexture); // Moved to public
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

    // Add back camera-related members
    ofVideoGrabber camera;
    bool cameraInitialized = false;
    std::vector<ofVideoDevice> videoDevices;
    int currentVideoDeviceIndex = -1;
};
