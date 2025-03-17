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
    
private:
    // Shaders
    ofShader mixerShader;      // Main effect mixer shader
    ofShader sharpenShader;    // Image sharpening shader
    
    // Helper methods
    bool loadShaderPair(ofShader& shader, const std::string& name);
};
