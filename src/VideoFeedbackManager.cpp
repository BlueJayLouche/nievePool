#include "VideoFeedbackManager.h"

VideoFeedbackManager::VideoFeedbackManager(ParameterManager* paramManager, ShaderManager* shaderManager)
    : paramManager(paramManager), shaderManager(shaderManager) {
    // Allocate pastFrames array
    pastFrames = new ofFbo[DEFAULT_FRAME_BUFFER_LENGTH];
}

void VideoFeedbackManager::setup(int width, int height) {
    this->width = width;
    this->height = height;
    
    // Setup camera and allocate FBOs
    setupCamera(width, height);
    allocateFbos(width, height);
    clearFbos();
}

void VideoFeedbackManager::update() {
    // Update camera input
    updateCamera();
    
    // Process main video feedback pipeline
    processMainPipeline();
    
    // Increment frame index for circular buffer
    incrementFrameIndex();
}

void VideoFeedbackManager::draw() {
    // Draw the final output to screen
    sharpenFbo.draw(0, 0, ofGetWidth(), ofGetHeight());
}

void VideoFeedbackManager::allocateFbos(int width, int height) {
    // Allocate main processing FBO
    mainFbo.allocate(width, height);
    mainFbo.begin();
    ofClear(0, 0, 0, 255);
    mainFbo.end();
    
    // Allocate aspect ratio correction FBO
    aspectRatioFbo.allocate(width, height);
    aspectRatioFbo.begin();
    ofClear(0, 0, 0, 255);
    aspectRatioFbo.end();
    
    // Allocate dry mode frame buffer
    dryFrameBuffer.allocate(width, height);
    dryFrameBuffer.begin();
    ofClear(0, 0, 0, 255);
    dryFrameBuffer.end();
    
    // Allocate sharpen effect buffer
    sharpenFbo.allocate(width, height);
    sharpenFbo.begin();
    ofClear(0, 0, 0, 255);
    sharpenFbo.end();
    
    // Allocate past frames for delay effect
    for (int i = 0; i < frameBufferLength; i++) {
        pastFrames[i].allocate(width, height);
        pastFrames[i].begin();
        ofClear(0, 0, 0, 255);
        pastFrames[i].end();
    }
}

void VideoFeedbackManager::clearFbos() {
    // Clear main FBO
    mainFbo.begin();
    ofClear(0, 0, 0, 255);
    mainFbo.end();
    
    // Clear aspect ratio FBO
    aspectRatioFbo.begin();
    ofClear(0, 0, 0, 255);
    aspectRatioFbo.end();
    
    // Clear dry buffer
    dryFrameBuffer.begin();
    ofClear(0, 0, 0, 255);
    dryFrameBuffer.end();
    
    // Clear sharpen buffer
    sharpenFbo.begin();
    ofClear(0, 0, 0, 255);
    sharpenFbo.end();
    
    // Clear all past frames
    for (int i = 0; i < frameBufferLength; i++) {
        pastFrames[i].begin();
        ofClear(0, 0, 0, 255);
        pastFrames[i].end();
    }
}

void VideoFeedbackManager::setupCamera(int width, int height) {
    // Initialize the camera
    camera.setDeviceID(1);
    camera.setDesiredFrameRate(30);
    camera.initGrabber(width, height);
    cameraInitialized = camera.isInitialized();
    
    if (!cameraInitialized) {
        ofLogError("VideoFeedbackManager") << "Failed to initialize camera!";
    }
}

void VideoFeedbackManager::updateCamera() {
    if (cameraInitialized) {
        camera.update();
        
        // If there's a new frame and HD aspect ratio correction is enabled
        if (camera.isFrameNew() && hdmiAspectRatioEnabled) {
            aspectRatioFbo.begin();
            // Corner crop and stretch to preserve HD aspect ratio
            camera.draw(0, 0, 853, 480);
            aspectRatioFbo.end();
        }
    }
}

void VideoFeedbackManager::incrementFrameIndex() {
    frameCount++;
    currentFrameIndex = frameCount % frameBufferLength;
}

void VideoFeedbackManager::processMainPipeline() {
    // Get parameters with P-lock applied
    float lumakeyValue = paramManager->getLumakeyValue();
    float mix = paramManager->getMix();
    float hue = paramManager->getHue();
    float saturation = paramManager->getSaturation();
    float brightness = paramManager->getBrightness();
    float temporalFilterMix = paramManager->getTemporalFilterMix();
    float temporalFilterResonance = paramManager->getTemporalFilterResonance();
    float sharpenAmount = paramManager->getSharpenAmount();
    float xDisplace = paramManager->getXDisplace();
    float yDisplace = paramManager->getYDisplace();
    float zDisplace = paramManager->getZDisplace();
    
    // Get frequency parameters
    float zFrequency = paramManager->getZFrequency();
    float xFrequency = paramManager->getXFrequency();
    float yFrequency = paramManager->getYFrequency();
    
    float rotate = paramManager->getRotate();
    float hueModulation = paramManager->getHueModulation();
    float hueOffset = paramManager->getHueOffset();
    float hueLFO = paramManager->getHueLFO();
    
    // Get LFO values
    float xLfoAmp = paramManager->getXLfoAmp();
    float xLfoRate = paramManager->getXLfoRate();
    float yLfoAmp = paramManager->getYLfoAmp();
    float yLfoRate = paramManager->getYLfoRate();
    float zLfoAmp = paramManager->getZLfoAmp();
    float zLfoRate = paramManager->getZLfoRate();
    float rotateLfoAmp = paramManager->getRotateLfoAmp();
    float rotateLfoRate = paramManager->getRotateLfoRate();
    
    // Calculate delay index
    int delayAmount = paramManager->getDelayAmount();
    int delayIndex = (abs(frameBufferLength - currentFrameIndex - delayAmount) - 1) % frameBufferLength;
    
    // Apply LFO modulation
    xDisplace += 0.01f * xLfoAmp * sin(ofGetElapsedTimef() * xLfoRate);
    yDisplace += 0.01f * yLfoAmp * sin(ofGetElapsedTimef() * yLfoRate);
    zDisplace *= (1.0f + 0.05f * zLfoAmp * sin(ofGetElapsedTimef() * zLfoRate));
    rotate += 0.314159265f * rotateLfoAmp * sin(ofGetElapsedTimef() * rotateLfoRate);
    
    // Main processing FBO
    mainFbo.begin();
    
    // Begin mixer shader
    ofShader& mixerShader = shaderManager->getMixerShader();
    mixerShader.begin();
    
    // Draw camera input (or aspect corrected version)
    if (hdmiAspectRatioEnabled) {
        aspectRatioFbo.draw(0, 0, width, height);
    } else {
        camera.draw(0, 0, width, height);
    }
    
    // Send the textures to shader
    mixerShader.setUniformTexture("fb", pastFrames[delayIndex].getTexture(), 1);
    
    // Choose between wet (feedback) or dry (direct camera) mode for temporal filter
    if (paramManager->isWetModeEnabled()) {
        mixerShader.setUniformTexture("temporalFilter", 
            pastFrames[(abs(frameBufferLength - currentFrameIndex) - 1) % frameBufferLength].getTexture(), 2);
    } else {
        mixerShader.setUniformTexture("temporalFilter", dryFrameBuffer.getTexture(), 2);
    }
    
    // Send continuous parameters
    mixerShader.setUniform1f("lumakey", lumakeyValue);
    mixerShader.setUniform1f("fbMix", mix);
    mixerShader.setUniform1f("fbHue", hue);
    mixerShader.setUniform1f("fbSaturation", saturation);
    mixerShader.setUniform1f("fbBright", brightness);
    mixerShader.setUniform1f("temporalFilterMix", temporalFilterMix);
    mixerShader.setUniform1f("temporalFilterResonance", temporalFilterResonance);
    mixerShader.setUniform1f("fbXDisplace", xDisplace);
    mixerShader.setUniform1f("fbYDisplace", yDisplace);
    mixerShader.setUniform1f("fbZDisplace", zDisplace);
    
    // Send frequency values to the shader
    mixerShader.setUniform1f("fbZFrequency", zFrequency);
    mixerShader.setUniform1f("fbXFrequency", xFrequency);
    mixerShader.setUniform1f("fbYFrequency", yFrequency);
    
    mixerShader.setUniform1f("fbRotate", rotate);
    mixerShader.setUniform1f("fbHuexMod", hueModulation);
    mixerShader.setUniform1f("fbHuexOff", hueOffset);
    mixerShader.setUniform1f("fbHuexLfo", hueLFO);
    
    // Send toggle switches
    mixerShader.setUniform1i("toroidSwitch", paramManager->isToroidEnabled() ? 1 : 0);
    mixerShader.setUniform1i("mirrorSwitch", paramManager->isMirrorModeEnabled() ? 1 : 0);
    mixerShader.setUniform1i("brightInvert", paramManager->isBrightnessInverted() ? 1 : 0);
    mixerShader.setUniform1i("hueInvert", paramManager->isHueInverted() ? 1 : 0);
    mixerShader.setUniform1i("saturationInvert", paramManager->isSaturationInverted() ? 1 : 0);
    mixerShader.setUniform1i("horizontalMirror", paramManager->isHorizontalMirrorEnabled() ? 1 : 0);
    mixerShader.setUniform1i("verticalMirror", paramManager->isVerticalMirrorEnabled() ? 1 : 0);
    mixerShader.setUniform1i("lumakeyInvertSwitch", paramManager->isLumakeyInverted() ? 1 : 0);
    
    // Send video reactivity parameters
    mixerShader.setUniform1f("vLumakey", paramManager->getVLumakeyValue());
    mixerShader.setUniform1f("vMix", paramManager->getVMix());
    mixerShader.setUniform1f("vHue", paramManager->getVHue());
    mixerShader.setUniform1f("vSat", paramManager->getVSaturation());
    mixerShader.setUniform1f("vBright", paramManager->getVBrightness());
    mixerShader.setUniform1f("vtemporalFilterMix", paramManager->getVTemporalFilterMix());
    mixerShader.setUniform1f("vFb1X", paramManager->getVTemporalFilterResonance());
    mixerShader.setUniform1f("vX", paramManager->getVXDisplace());
    mixerShader.setUniform1f("vY", paramManager->getVYDisplace());
    mixerShader.setUniform1f("vZ", paramManager->getVZDisplace());
    mixerShader.setUniform1f("vRotate", paramManager->getVRotate());
    mixerShader.setUniform1f("vHuexMod", paramManager->getVHueModulation());
    mixerShader.setUniform1f("vHuexOff", paramManager->getVHueOffset());
    mixerShader.setUniform1f("vHuexLfo", paramManager->getVHueLFO());
    
    mixerShader.end();
    mainFbo.end();
    
    // Sharpen processing
    sharpenFbo.begin();
    ofShader& sharpenShader = shaderManager->getSharpenShader();
    sharpenShader.begin();
    
    mainFbo.draw(0, 0);
    
    sharpenShader.setUniform1f("sharpenAmount", sharpenAmount);
    sharpenShader.setUniform1f("vSharpenAmount", paramManager->getVSharpenAmount());
    
    sharpenShader.end();
    sharpenFbo.end();
    
    // Store frame in circular buffer
    int storeIndex = abs(frameBufferLength - currentFrameIndex) - 1;
    if (storeIndex < 0) storeIndex += frameBufferLength;
    
    pastFrames[storeIndex].begin();
    
    // In dry mode, store camera input directly
    if (!paramManager->isWetModeEnabled()) {
        if (hdmiAspectRatioEnabled) {
            aspectRatioFbo.draw(0, 0);
        } else {
            camera.draw(0, 0);
        }
        
        // Update the dry frame buffer for temporal filtering
        dryFrameBuffer.begin();
        sharpenFbo.draw(0, 0);
        dryFrameBuffer.end();
    } else {
        // In wet mode, store the processed output
        sharpenFbo.draw(0, 0);
    }
    
    pastFrames[storeIndex].end();
}

int VideoFeedbackManager::getFrameBufferLength() const {
    return frameBufferLength;
}

void VideoFeedbackManager::setFrameBufferLength(int length) {
    // Allocate new framebuffer array
    ofFbo* newFrames = new ofFbo[length];
    
    // Copy and initialize frames
    for (int i = 0; i < length; i++) {
        if (i < frameBufferLength) {
            // Copy existing frame
            newFrames[i] = pastFrames[i];
        } else {
            // Initialize new frame
            newFrames[i].allocate(width, height);
            newFrames[i].begin();
            ofClear(0, 0, 0, 255);
            newFrames[i].end();
        }
    }
    
    // Cleanup old array and update
    delete[] pastFrames;
    pastFrames = newFrames;
    frameBufferLength = length;
}

bool VideoFeedbackManager::isHdmiAspectRatioEnabled() const {
    return hdmiAspectRatioEnabled;
}

void VideoFeedbackManager::setHdmiAspectRatioEnabled(bool enabled) {
    hdmiAspectRatioEnabled = enabled;
}
