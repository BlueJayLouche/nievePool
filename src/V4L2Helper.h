#pragma once

#include "ofMain.h"

#ifdef TARGET_LINUX
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#endif

class V4L2Helper {
public:
    struct VideoFormat {
        uint32_t pixelFormat;
        std::string name;
        std::string fourcc;
    };
    
    struct VideoDevice {
        std::string path;
        std::string name;
        int id;
    };
    
    struct Resolution {
        int width;
        int height;
    };
    
    static std::vector<VideoDevice> listDevices();
    static std::vector<VideoFormat> listFormats(const std::string& devicePath);
    static std::vector<Resolution> listResolutions(const std::string& devicePath, uint32_t format);
    static bool setFormat(const std::string& devicePath, uint32_t format, int width, int height);
    static VideoFormat getCurrentFormat(const std::string& devicePath);
    static uint32_t formatNameToCode(const std::string& formatName);
    static std::string formatCodeToName(uint32_t pixelFormat);
    static std::string formatCodeToFourCC(uint32_t pixelFormat);
};
