#include "ofMain.h"
#include "ofApp.h"

//========================================================================
int main() {
    ofLogNotice("main") << "Starting application on " << ofGetTargetPlatform();
    
    ofGLWindowSettings settings;
    
    #if defined(TARGET_OSX)
        #if defined(__arm64__) || defined(__aarch64__)
            // Apple Silicon - use OpenGL 3.2
            settings.setGLVersion(3, 2);
            settings.setSize(1024, 768);
            ofLogNotice("main") << "Using OpenGL 3.2 renderer for Apple Silicon";
        #else
            // Intel Mac - use OpenGL 3.2
            settings.setGLVersion(3, 2);
            settings.setSize(1024, 768);
            ofLogNotice("main") << "Using OpenGL 3.2 renderer for Intel Mac";
        #endif
    #elif defined(TARGET_WIN32)
        // Windows - use OpenGL 3.2
        settings.setGLVersion(3, 2);
        settings.setSize(1024, 768);
        ofLogNotice("main") << "Using OpenGL 3.2 renderer for Windows";
    #elif defined(TARGET_LINUX)
        #if defined(__arm__) || defined(__aarch64__)
            // Linux ARM (e.g. Raspberry Pi)
            ofGLESWindowSettings glesSettings;
            glesSettings.glesVersion = 2;
            glesSettings.setSize(640, 480);
            ofLogNotice("main") << "Using OpenGL ES2 renderer for Linux ARM";
            auto window = ofCreateWindow(glesSettings);
            ofRunApp(window, make_shared<ofApp>());
            ofRunMainLoop();
            return 0;
        #else
            // Linux x86/x64
            settings.setGLVersion(3, 2);
            settings.setSize(1024, 768);
            ofLogNotice("main") << "Using OpenGL 3.2 renderer for Linux";
        #endif
    #else
        // Default fallback
        settings.setGLVersion(2, 1);  // More compatible default
        settings.setSize(1024, 768);
        ofLogNotice("main") << "Using OpenGL 2.1 renderer (default)";
    #endif
    
    // Window title using the correct API for OF v0.12.0
//    settings.setTitle("Video Feedback Studio");
    
    // Window position
    settings.setPosition(glm::vec2(100, 100));
    
    // Create window with appropriate settings
    auto window = ofCreateWindow(settings);
    
    // Make the window resizable after creation
    ofSetWindowShape(1024, 768);
    ofSetWindowPosition(100, 100);
    ofSetWindowTitle("Video Feedback Studio");
    
    // Run the app
    ofRunApp(window, make_shared<ofApp>());
    ofRunMainLoop();
    return 0;
}
