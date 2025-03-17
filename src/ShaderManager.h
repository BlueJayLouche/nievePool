#pragma once

#include "ofMain.h"

/**
 * @class ShaderManager
 * @brief Manages shader loading and provides cross-platform compatibility
 *
 * This class handles loading the appropriate shaders based on OpenGL version
 * and platform, providing seamless compatibility across different systems.
 */
class ShaderManager {
public:
    ShaderManager();
    
    // Core methods
    void setup();
    
    // Shader access
    ofShader& getMixerShader();
    ofShader& getSharpenShader();
    
    // Load shaders for different GL versions
    bool loadShadersForCurrentRenderer();
    
    // Utility functions
    std::string getShaderDirectory() const;
    
    std::string getCompatibilityHeader() const {
        if (ofIsGLProgrammableRenderer()) {
            return "#version 150\n#define SAMPLER_FN texture";
        } else {
            #ifdef TARGET_OPENGLES
            return "#version 100\nprecision highp float;\n#define SAMPLER_FN texture2D";
            #else
            return "#version 120\n#define SAMPLER_FN texture2D";
            #endif
        }
    }
    
private:
    // Shaders
    ofShader mixerShader;      // Main effect mixer shader
    ofShader sharpenShader;    // Image sharpening shader
    
    // Helper methods
    bool loadShaderPair(ofShader& shader, const std::string& name);
};
