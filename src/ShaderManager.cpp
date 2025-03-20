// Replace or modify ShaderManager.cpp

#include "ShaderManager.h"
#include "TextureHelper.h"

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
    
    // Log the shader directory and OpenGL details
    ofLogNotice("ShaderManager") << "Loading shaders from: " << shaderDir;
    ofLogNotice("ShaderManager") << "OpenGL renderer: " << ofGetGLRenderer();
    ofLogNotice("ShaderManager") << "Using programmable renderer: " << (ofIsGLProgrammableRenderer() ? "Yes" : "No");
    
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
    
    // First try to load the shader from files
    bool loadSuccess = shader.load(vertPath, fragPath);
    
    // If loading from files failed, try to load from strings with TextureHelper compatibility
    if (!loadSuccess) {
        ofLogWarning("ShaderManager") << "Failed to load shader from files, trying string-based loading with compatibility";
        
        // Load shader files into strings
        ofFile vertFile(vertPath);
        ofFile fragFile(fragPath);
        
        if (vertFile.exists() && fragFile.exists()) {
            std::string vertSource = ofBufferFromFile(vertPath).getText();
            std::string fragSource = ofBufferFromFile(fragPath).getText();
            
            // Apply texture compatibility fix
            fragSource = TextureHelper::fixTextureFunction(fragSource);
            
            // Add appropriate version string and compatibility headers
            std::string vertHeader = TextureHelper::getVersionString();
            std::string fragHeader = TextureHelper::getVersionString() + TextureHelper::getFragmentPrecision();
            
            // Load shader from strings
            loadSuccess = shader.setupShaderFromSource(GL_VERTEX_SHADER, vertHeader + vertSource);
            loadSuccess &= shader.setupShaderFromSource(GL_FRAGMENT_SHADER, fragHeader + fragSource);
            loadSuccess &= shader.linkProgram();
        }
    }
    
    if (loadSuccess) {
        ofLogNotice("ShaderManager") << "Successfully loaded shader: " << name;
    } else {
        ofLogError("ShaderManager") << "Failed to load shader: " << name;
    }
    
    return loadSuccess;
}
