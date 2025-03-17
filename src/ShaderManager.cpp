#include "ShaderManager.h"

ShaderManager::ShaderManager() {
    // Initialize shaders
}

void ShaderManager::setup() {
    // Load appropriate shaders based on OpenGL capabilities
    if (!loadShadersForCurrentRenderer()) {
        ofLogError("ShaderManager") << "Failed to load shaders!";
    }
}

ofShader& ShaderManager::getMixerShader() {
    return mixerShader;
}

ofShader& ShaderManager::getSharpenShader() {
    return sharpenShader;
}

bool ShaderManager::loadShadersForCurrentRenderer() {
    std::string shaderDir = getShaderDirectory();
    
    // Load the shader pairs
    bool mixerLoaded = loadShaderPair(mixerShader, "shader_mixer");
    bool sharpenLoaded = loadShaderPair(sharpenShader, "shaderSharpen");
    
    // Success only if both shaders loaded
    return mixerLoaded && sharpenLoaded;
}

std::string ShaderManager::getShaderDirectory() const {
    // Determine the shader directory based on the renderer
    if (ofIsGLProgrammableRenderer()) {
        return "shadersGL3/";
    } else {
        #ifdef TARGET_OPENGLES
            return "shadersES2/";
        #else
            return "shadersGL2/";
        #endif
    }
}

bool ShaderManager::loadShaderPair(ofShader& shader, const std::string& name) {
    std::string shaderDir = getShaderDirectory();
    std::string vertPath = shaderDir + name + ".vert";
    std::string fragPath = shaderDir + name + ".frag";
    
    ofLogNotice("ShaderManager") << "Loading shader: " << name 
                               << " from " << vertPath << " and " << fragPath;
    
    return shader.load(vertPath, fragPath);
}
