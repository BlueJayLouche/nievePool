#pragma once

#include "ofMain.h"
#include "ofxMidi.h"
#include "ofxNDI.h" // Main NDI header likely includes finder and receiver
#include "ofxOsc.h" // Using user-suggested main OSC header
#include "ofVideoGrabber.h" // Need this for camera
#include "ofVideoPlayer.h" // Need this for video file playback
#include "ParameterManager.h"
#include "VideoFeedbackManager.h"
#include "ShaderManager.h"
#include "MidiManager.h"
#include "AudioReactivityManager.h" // Added the new header

/**
 * @class ofApp
 * @brief Main application class integrating all components
 *
 * This class ties together all the manager components and handles
 * user input, initialization, and the main application loop.
 */
class ofApp : public ofBaseApp {
    
public:
    // Core application methods
    void setup();
    void update();
    void draw();
    void exit();
    
    // Input handling
    void keyPressed(int key);
    void keyReleased(int key);
    
    // Debug visualization
    void drawDebugInfo();
    void drawAudioDebugInfo(int x, int y, int lineHeight); // Added method for audio debugging
    void drawMidiInfo(int x, int y, int lineHeight); // Added method for MIDI debugging
    
private:
    // Input Source Management
    enum InputSource { CAMERA, NDI, VIDEO_FILE };
    InputSource currentInputSource = CAMERA; // Default to camera
    std::string videoFilePath = "input.mov"; // Default video file path

    // Input Objects
    ofVideoGrabber camera;
    bool cameraInitialized = false;
    ofVideoPlayer videoPlayer;
    // Note: ndiReceiver is already declared below

    // App settings from XML
    int configWidth = 1024;
    int configHeight = 768;
    int configFrameRate = 30;
    
    // void setupDefaultAudioMappings(); // Removed - Logic moved to AudioReactivityManager
    void resetSettingsFile();
    
    // Application managers
    std::unique_ptr<ParameterManager> paramManager;
    std::unique_ptr<ShaderManager> shaderManager;
    std::unique_ptr<VideoFeedbackManager> videoManager;
    std::unique_ptr<MidiManager> midiManager;
    std::unique_ptr<AudioReactivityManager> audioManager; // Added audio manager

    // NDI Input
    ofxNDIreceiver ndiReceiver; // Using user-suggested type name (lowercase 'r')
    // ofxNdiFinder ndiFinder; // Reverted - Finder might not be needed if receiver handles it
    // std::vector<ofxNdiSource> ndiSources; // Reverted
    ofTexture ndiTexture; // Texture to hold the received NDI frame (used as input)
    int currentNdiSourceIndex = 0; // Reverted - Index of the currently selected NDI source (start at 0)
    // Removed discoveredNdiSources vector - list managed internally by receiver

    // Texture to hold the currently selected input before processing
    ofTexture currentInputTexture; 

    // OSC Control
    ofxOscReceiver oscReceiver;
    // Note: OSC Port is now managed by ParameterManager

    // Configuration
    int width = 640;
    int height = 480;
    bool debugEnabled = false;
    
    // Performance monitoring
    float frameRateHistory[60];
    int frameRateIndex = 0;
    
    // Debug UI sections
    void drawSystemInfo(int x, int y, int lineHeight);
    void drawPerformanceInfo(int x, int y, int lineHeight);
    void drawParameterInfo(int x, int y, int lineHeight);
    void drawVideoInfo(int x, int y, int lineHeight);
    
    // Performance monitoring
    std::deque<float> frameTimeHistory;
    float lastFrameTime;
    float averageFrameTime;
    int frameCounter;
};
