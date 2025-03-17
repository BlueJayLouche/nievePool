#pragma once

#include "ofMain.h"
#include "ofxMidi.h"
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
    
private:
    // App settings from XML
    int configWidth = 1024;
    int configHeight = 768;
    int configFrameRate = 30;
    
    void setupDefaultAudioMappings();
    void resetSettingsFile();
    
    // Application managers
    std::unique_ptr<ParameterManager> paramManager;
    std::unique_ptr<ShaderManager> shaderManager;
    std::unique_ptr<VideoFeedbackManager> videoManager;
    std::unique_ptr<MidiManager> midiManager;
    std::unique_ptr<AudioReactivityManager> audioManager; // Added audio manager
    
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
