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
    
    // Get/set for frame buffer size
    int getFrameBufferLength() const;
    void setFrameBufferLength(int length);
    
    // Toggle HD aspect ratio correction
    bool isHdmiAspectRatioEnabled() const;
    void setHdmiAspectRatioEnabled(bool enabled);
    
    // Video device management methods - these need to be PUBLIC
    void listVideoDevices();
    std::vector<std::string> getVideoDeviceList() const;
    int getCurrentVideoDeviceIndex() const;
    std::string getCurrentVideoDeviceName() const;
    bool selectVideoDevice(int deviceIndex);
    bool selectVideoDevice(const std::string& deviceName);
    
    // XML settings
    void saveToXml(ofxXmlSettings& xml) const;
    void loadFromXml(ofxXmlSettings& xml);
    
private:
    // Constants
    static const int DEFAULT_FRAME_BUFFER_LENGTH = 60;
    
    // Helper methods
    void incrementFrameIndex();
    void processMainPipeline();
    
    // Reference to managers
    ParameterManager* paramManager;
    ShaderManager* shaderManager;
    
    // Camera
    ofVideoGrabber camera;
    bool cameraInitialized = false;
    
    // Resolution
    int width = 640;
    int height = 480;
    bool hdmiAspectRatioEnabled = false;
    
    // Framebuffers
    ofFbo mainFbo;              // Main processing buffer
    ofFbo sharpenFbo;           // Buffer for sharpen effect
    ofFbo dryFrameBuffer;       // Buffer for dry mode
    ofFbo aspectRatioFbo;       // Buffer for aspect ratio correction
    
    // Circular buffer for delay effect
    int frameBufferLength = DEFAULT_FRAME_BUFFER_LENGTH;
    ofFbo* pastFrames = nullptr;
    
    // Frame counting and delay management
    unsigned int frameCount = 0;
    int currentFrameIndex = 0;
    
    // Video device management
    std::vector<ofVideoDevice> videoDevices;
    int currentVideoDeviceIndex = 0;
};
