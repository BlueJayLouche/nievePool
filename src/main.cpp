#include "ofMain.h"
#include "ofApp.h"

//========================================================================
int main() {
    // Log platform information to help with debugging
    ofLogNotice("main") << "Starting application on " << ofGetTargetPlatform();
    ofLogNotice("main") << "OpenFrameworks version: " << OF_VERSION_MAJOR << "."
                         << OF_VERSION_MINOR << "." << OF_VERSION_PATCH;
    
    // Check if running as root - not recommended but often necessary for direct hardware access
#ifdef TARGET_LINUX
    if (geteuid() == 0) {
        ofLogWarning("main") << "Application is running as root. This can be a security risk.";
    }
#endif
    
    bool useGLES = false;
    bool platformIsRaspberryPi = false;
    
    // Detect Raspberry Pi or ARM Linux specifically
#if defined(TARGET_LINUX)
    // Check if we're running on ARM architecture
    #if defined(__arm__) || defined(__aarch64__)
        ofLogNotice("main") << "Detected ARM Linux platform";
        platformIsRaspberryPi = true;
        
        // On ARM platforms, default to GLES but check capabilities first
        std::string glxinfo = "glxinfo | grep 'OpenGL version'";
        FILE* pipe = popen(glxinfo.c_str(), "r");
        if (pipe) {
            char buffer[512];
            std::string result = "";
            while (!feof(pipe)) {
                if (fgets(buffer, sizeof(buffer), pipe) != NULL)
                    result += buffer;
            }
            pclose(pipe);
            
            // Check if we have real OpenGL available vs GLES
            if (result.find("OpenGL ES") != std::string::npos) {
                useGLES = true;
                ofLogNotice("main") << "Detected OpenGL ES support";
            } else if (result.find("OpenGL version") != std::string::npos) {
                // We have desktop GL, see what version
                ofLogNotice("main") << "Detected desktop OpenGL: " << result;
                
                // If we have at least OpenGL 2.1, use it instead of GLES
                if (result.find("2.") != std::string::npos ||
                    result.find("3.") != std::string::npos ||
                    result.find("4.") != std::string::npos) {
                    useGLES = false;
                } else {
                    // Unknown or too old GL version, use GLES
                    useGLES = true;
                }
            } else {
                // No glxinfo output, conservatively use GLES
                useGLES = true;
            }
        } else {
            // Can't run glxinfo, assume we need GLES
            useGLES = true;
        }
    #endif
#endif

    // GLES initialization for Raspberry Pi and other ARM Linux platforms
    if (useGLES) {
        try {
            ofLogNotice("main") << "Using OpenGL ES2 renderer for ARM platform";
            
            // Use GLES2 settings
            ofGLESWindowSettings glesSettings;
            glesSettings.glesVersion = 2;
            glesSettings.setSize(640, 480);
            glesSettings.windowMode = OF_WINDOW;
            glesSettings.title = "Video Feedback Studio";
            
            // Create window with GLES settings
            auto window = ofCreateWindow(glesSettings);
            
            // Run the app
            ofRunApp(window, make_shared<ofApp>());
            ofRunMainLoop();
            return 0;
        } catch (const std::exception& e) {
            ofLogError("main") << "Error with OpenGL ES: " << e.what() << " - falling back to legacy GL";
            // Fall through to legacy OpenGL setup
        } catch (...) {
            ofLogError("main") << "Unknown error with OpenGL ES - falling back to legacy GL";
            // Fall through to legacy OpenGL setup
        }
    }
    
    // Desktop OpenGL initialization path
    try {
        // Configure appropriate GL version
        ofGLWindowSettings settings;
        
    #if defined(TARGET_OSX)
        #if defined(__arm64__) || defined(__aarch64__)
            // Apple Silicon - use OpenGL 3.2 core profile
            settings.setGLVersion(3, 2);
            settings.setSize(1024, 768);
            ofLogNotice("main") << "Using OpenGL 3.2 renderer for Apple Silicon";
        #else
            // Intel Mac - use OpenGL 3.2 core profile
            settings.setGLVersion(3, 2);
            settings.setSize(1024, 768);
            ofLogNotice("main") << "Using OpenGL 3.2 renderer for Intel Mac";
        #endif
    #elif defined(TARGET_WIN32)
        // Windows - use OpenGL 3.2 if available, fall back to 2.1 if not
        settings.setGLVersion(3, 2);
        settings.setSize(1024, 768);
        ofLogNotice("main") << "Using OpenGL 3.2 renderer for Windows";
    #else
        // For other platforms (including Linux x86/x64), use OpenGL 2.1 for maximum compatibility
        settings.setGLVersion(2, 1);  // More compatible default
        settings.setSize(1024, 768);
        ofLogNotice("main") << "Using OpenGL 2.1 renderer (default)";
    #endif
    
        // Window position - use fixed position instead of trying to center
        // This avoids calling ofGetScreenWidth() before GL context is created
        settings.setPosition(glm::vec2(100, 100)); // Safe default position
        settings.windowMode = OF_WINDOW;
        settings.title = "Video Feedback Studio";
        
        // Create window with appropriate settings
        auto window = ofCreateWindow(settings);
        
        // Run the app with proper platform flag set
        auto app = make_shared<ofApp>();
        ofRunApp(window, app);
        ofRunMainLoop();
        return 0;
    } catch (const std::exception& e) {
        ofLogError("main") << "Error creating window: " << e.what();
        
        // Last resort - try with bare minimum OpenGL settings
        ofGLWindowSettings minSettings;
        minSettings.setGLVersion(2, 1);
        minSettings.setSize(640, 480);
        
        auto window = ofCreateWindow(minSettings);
        ofRunApp(window, make_shared<ofApp>());
        ofRunMainLoop();
        return 0;
    }
}
