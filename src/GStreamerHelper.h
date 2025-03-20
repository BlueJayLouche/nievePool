#pragma once

#include "ofMain.h"

/**
 * @class GStreamerHelper
 * @brief Helper class for creating custom GStreamer pipelines for problematic devices
 */
class GStreamerHelper {
public:
    /**
     * @brief Create a custom GStreamer pipeline string for the EM2860 device
     * This helps work around format conversion issues with this specific chipset
     */
    static std::string createEM2860Pipeline(const std::string& devicePath, int width, int height) {
        std::stringstream ss;
        
        // Basic pipeline structure for EM2860 devices
        ss << "v4l2src device=" << devicePath;
        
        // EM2860 commonly outputs Bayer format at 720x576 (PAL) or 720x480 (NTSC)
        if (height == 576) {
            // PAL resolution
            ss << " ! video/x-bayer,width=720,height=576,framerate=25/1";
        } else if (height == 480) {
            // NTSC resolution
            ss << " ! video/x-bayer,width=720,height=480,framerate=30/1";
        } else {
            // Fall back to a common format
            ss << " ! video/x-bayer,width=" << width << ",height=" << height << ",framerate=30/1";
        }
        
        // Convert Bayer to RGB, then use videoconvert instead of videoscale to preserve format
        ss << " ! bayer2rgb ! videoconvert";
        
        // Add caps filter to ensure proper format compatibility with appsink
        ss << " ! video/x-raw,format=RGB,width=" << width << ",height=" << height;
        
        // Finish with appsink for OpenFrameworks
        ss << " ! appsink name=ofappsink enable-last-sample=0";
        
        return ss.str();
    }
    
    /**
     * @brief Set a custom GStreamer pipeline for ofVideoGrabber
     * Requires accessing the internal GStreamer implementation of ofVideoGrabber
     */
    static bool setCustomPipeline(ofVideoGrabber& grabber, const std::string& pipeline) {
        // This would require access to internals of ofVideoGrabber
        // For Raspberry Pi, you might need to modify ofGstVideoGrabber.cpp directly
        
        // For now, just log the pipeline that should be used
        ofLogNotice("GStreamerHelper") << "Custom pipeline that should be used: " << pipeline;
        ofLogNotice("GStreamerHelper") << "NOTE: To use this pipeline, you need to modify ofVideoGrabber's implementation";
        
        return false;
    }
    
    /**
     * @brief Detect if the device is an EM2860 capture card
     */
    static bool isEM2860Device(const std::string& devicePath) {
        bool isEM2860 = false;
        
#ifdef TARGET_LINUX
        int fd = open(devicePath.c_str(), O_RDWR);
        if (fd >= 0) {
            struct v4l2_capability cap;
            if (ioctl(fd, VIDIOC_QUERYCAP, &cap) >= 0) {
                std::string cardName = reinterpret_cast<const char*>(cap.card);
                if (cardName.find("EM2860") != std::string::npos ||
                    cardName.find("SAA711X") != std::string::npos) {
                    isEM2860 = true;
                }
            }
            close(fd);
        }
#endif
        
        return isEM2860;
    }
};
