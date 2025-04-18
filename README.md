# nievePool

NievePool is an experimental video feedback and effects application built with openFrameworks. It allows for real-time manipulation of video input (from Camera, NDI sources, or video files) using various parameters, modulation sources (LFOs, audio reactivity), and MIDI/OSC control.

## Features

*   **Video Feedback:** Core feedback loop with displacement and filtering.
*   **Multiple Input Sources:** Supports standard Cameras, NDI network video sources, and local Video Files.
*   **Effects:** Hue, saturation, brightness, temporal filtering, sharpening, displacement (X, Y, Z), rotation, hue modulation, mirroring, toroid effect.
*   **Parameter Control:**
    *   **MIDI Control:** Configurable mapping of MIDI CC messages to parameters via `settings.xml`.
    *   **OSC Control:** Configurable mapping of OSC messages via `settings.xml`.
    *   **LFO Modulation:** Low-Frequency Oscillators for X, Y, Z displacement and rotation.
    *   **Audio Reactivity:** Modulate parameters based on FFT analysis of audio input. Configurable frequency bands and mappings via `settings.xml`.
*   **Configurable Settings:** Load and save parameters, mappings, input sources, and device preferences via `settings.xml`.
*   **Debug Overlay:** Toggleable display showing performance, parameter values, input status, and audio analysis.

## Acknowledgements

This project is heavily based on the fantastic work of Andrei Jay on the original [waaaave_pool](https://github.com/ex-zee-ex/waaaave_pool). Many thanks for the inspiration and foundational code!

## Setup & Running

1.  **Install openFrameworks:** Follow the setup guide for your operating system on the [openFrameworks website](https://openframeworks.cc/setup/).
2.  **Install Addons:** Ensure you have the required addons installed (usually placed in `openFrameworks/addons/`):
    *   `ofxFft`
    *   `ofxMidi`
    *   `ofxNDI`
    *   `ofxOsc`
    *   `ofxXmlSettings` (usually included with openFrameworks)
3.  **Place Project:** Put the `nievePool` project folder inside your `openFrameworks/apps/myApps/` directory.
4.  **Compile:** Use the openFrameworks Project Generator or your IDE (like Visual Studio Code with the C++ extension and Make/CMake tools) to compile the project.
5.  **Run:** Execute the compiled application found in the `bin/` directory.

## Configuration (`settings.xml`)

The application's behavior, parameters, and mappings are configured through the `bin/data/settings.xml` file. If this file doesn't exist or is invalid, it will be created with default values on first run.

```xml
<!-- Main Application Settings -->
<app>
    <version>1.0.0</version>
    <lastSaved>...</lastSaved>
    <debugEnabled>0</debugEnabled> <!-- 0 or 1 -->
    <width>1024</width>
    <height>768</height>
    <frameRate>60</frameRate> <!-- Target framerate -->
    <videoInputSource>CAMERA</videoInputSource> <!-- CAMERA, NDI, or VIDEO_FILE -->
    <videoFilePath>input.mov</videoFilePath> <!-- Path relative to bin/data/ -->
    <ndiSourceIndex>0</ndiSourceIndex> <!-- Index of NDI source to connect to initially -->
</app>

<!-- Parameter Manager Settings -->
<paramManager>
    <!-- Video Feedback Settings -->
    <videoFeedback>
        <frameBufferLength>60</frameBufferLength>
        <hdmiAspectRatioEnabled>0</hdmiAspectRatioEnabled> <!-- 0 or 1 -->
    </videoFeedback>

    <!-- Audio Reactivity Settings -->
    <audioReactivity>
        <enabled>0</enabled> <!-- 0 or 1 -->
        <normalizationEnabled>1</normalizationEnabled> <!-- 0 or 1 -->
        <sensitivity>1.0</sensitivity>
        <smoothing>0.85</smoothing>
        <numBands>8</numBands>
        <deviceName>Default Device Name</deviceName> <!-- Saves last used audio input device -->
        <deviceIndex>0</deviceIndex>
        <bandRanges> <!-- Optional: Define custom FFT bin ranges -->
            <range> <minBin>1</minBin> <maxBin>2</maxBin> </range>
            <!-- ... more ranges ... -->
        </bandRanges>
        <mappings> <!-- Define how audio bands affect parameters -->
            <mapping>
                <band>0</band> <!-- Index of the frequency band (0-7 by default) -->
                <paramId>z_displace</paramId> <!-- Target parameter ID -->
                <scale>0.5</scale> <!-- Scaling factor -->
                <min>-0.2</min> <!-- Minimum output value -->
                <max>0.2</max> <!-- Maximum output value -->
                <additive>0</additive> <!-- 0=Set parameter, 1=Add to parameter -->
            </mapping>
            <!-- ... more mappings ... -->
        </mappings>
    </audioReactivity>

    <!-- MIDI Device Preference -->
    <midi>
        <preferredDevice>Your MIDI Device Name</preferredDevice> <!-- Saves last connected device -->
    </midi>

    <!-- Parameter Definitions and Mappings -->
    <!-- Each parameter has an entry like this: -->
    <param id="parameterName" value="initialValue" midiChannel="channel" midiControl="ccNumber" oscAddr="/osc/address"></param>

    <!-- Example: Map MIDI CC 16 on Channel 1 to Lumakey -->
    <param id="lumakeyValue" value="0.0" midiChannel="1" midiControl="16" oscAddr="/lumakey"></param>

    <!-- Example: Map MIDI CC 17 on Channel 1 to Mix -->
    <param id="mix" value="0.0" midiChannel="1" midiControl="17" oscAddr="/mix"></param>

    <!-- ... many more param tags for all controllable parameters ... -->

</paramManager>
```

**Key Sections:**

*   **`<app>`:** General application settings.
    *   `debugEnabled`: Show/hide the debug overlay.
    *   `width`, `height`: Window dimensions.
    *   `frameRate`: Target application framerate.
    *   `videoInputSource`: Initial video source (`CAMERA`, `NDI`, `VIDEO_FILE`).
    *   `videoFilePath`: Path to the video file (relative to `bin/data/`) if `videoInputSource` is `VIDEO_FILE`.
    *   `ndiSourceIndex`: The index of the NDI source to connect to if `videoInputSource` is `NDI`.
*   **`<paramManager>`:** Contains settings for various managers.
    *   **`<videoFeedback>`:** Settings for the feedback buffer length and aspect ratio correction.
    *   **`<audioReactivity>`:** Settings for audio analysis.
        *   `enabled`: Turn audio reactivity on/off.
        *   `normalizationEnabled`: Normalize FFT band levels.
        *   `sensitivity`, `smoothing`: Control audio analysis response.
        *   `deviceName`: Preferred audio input device.
        *   `<mappings>`: Define how specific frequency bands (`<band>`) modulate parameters (`<paramId>`) with scaling, min/max limits, and additive/set mode. Default mappings are created if this section is empty.
    *   **`<midi>`:** Stores the name of the preferred MIDI input device.
    *   **`<param>`:** Defines each controllable parameter.
        *   `id`: The internal name of the parameter (do not change).
        *   `value`: The initial value loaded when the app starts.
        *   `midiChannel`: The MIDI channel (1-16) this parameter listens to. Set to `-1` to disable MIDI control.
        *   `midiControl`: The MIDI Control Change (CC) number this parameter listens to. Set to `-1` to disable MIDI control.
        *   `oscAddr`: The OSC address this parameter listens to (e.g., `/nieve/hue`). Leave empty to disable OSC control.

## Input Source Selection

*   **Initial Source:** Set the desired starting source (`CAMERA`, `NDI`, `VIDEO_FILE`) in `settings.xml` under `<app><videoInputSource>`.
*   **NDI Index:** If using NDI, set the desired source index in `<app><ndiSourceIndex>`. The application will attempt to connect to this source on startup.
*   **Video File:** If using VIDEO_FILE, set the path in `<app><videoFilePath>`.
*   **Cycling Sources:** Press 'I' (uppercase i) during runtime to cycle through CAMERA -> NDI -> VIDEO_FILE -> CAMERA.
*   **Switching NDI Sources (Runtime):** Press '<' or '>' (without Shift) while NDI is active to attempt switching to the previous/next available NDI source. *Note: This runtime switching might be unreliable depending on the NDI network and addon behavior.*
*   **Switching Camera Devices (Runtime):** Press Shift + '<' or Shift + '>' while CAMERA is active to switch between connected camera devices.

## Audio Reactivity Control

*   **Enable/Disable:** Press 'A' to toggle audio reactivity on/off.
*   **Normalization:** Press 'N' to toggle FFT band normalization.
*   **Sensitivity:** Press Shift + '+' / Shift + '-' to adjust sensitivity.
*   **Audio Device:** Press Shift + 'D' to cycle through available audio input devices.
*   **Mappings:** Configure band-to-parameter mappings in the `<audioReactivity><mappings>` section of `settings.xml`.

## MIDI Control

You can control most parameters using a MIDI controller.

1.  **Connect MIDI Device:** Ensure your MIDI device is connected before starting the application. The application will attempt to connect to the first available device or the `preferredDevice` saved in `settings.xml`.
2.  **Configure Mappings:** Edit the `bin/data/settings.xml` file. For each parameter you want to control:
    *   Find the corresponding `<param>` tag using the `id` attribute.
    *   Set the `midiChannel` attribute to the desired MIDI channel (1-16).
    *   Set the `midiControl` attribute to the desired MIDI CC number (0-127).
    *   Save the `settings.xml` file.
3.  **Restart Application:** Restart nievePool for the changes to take effect.

**Example Mapping:**

To control the `hue` parameter with CC 74 on MIDI channel 10:

```xml
<param id="hue" value="1.0" midiChannel="10" midiControl="74" oscAddr=""></param>
```

## OSC Control

OSC (Open Sound Control) is also supported for controlling parameters.

1.  **Configure Port:** The OSC listening port is set in `ParameterManager.cpp` (default is often 12345, check the code if needed). *This is not currently configurable via `settings.xml`.*
2.  **Configure Addresses:** Edit `bin/data/settings.xml`. For each parameter:
    *   Find the corresponding `<param>` tag.
    *   Set the `oscAddr` attribute to the desired OSC address (e.g., `/nieve/saturation`).
    *   Save the file.
3.  **Restart Application:** Restart nievePool.
4.  **Send OSC Messages:** Send OSC messages to the application's listening port. The application expects a single float or int argument matching the parameter type.

**Example:** To control saturation via OSC address `/nieve/saturation`:

```xml
<param id="saturation" value="1.0" midiChannel="-1" midiControl="-1" oscAddr="/nieve/saturation"></param>
```
You would then send an OSC message like `/nieve/saturation 0.5` to set saturation to 0.5.

## Keyboard Controls (Partial List)

*   **`** : Toggle Debug Overlay
*   **I** : Cycle Input Source (Camera -> NDI -> Video File)
*   **< / >** : (NDI Mode) Attempt to switch NDI source index (Under Development)
*   **Shift + < / >** : (Camera Mode) Switch camera device
*   **A** : Toggle Audio Reactivity
*   **N** : Toggle Audio Normalization
*   **Shift + + / -** : Adjust Audio Sensitivity
*   **Shift + D** : Cycle Audio Input Device
*   **[ / ]** : Adjust Feedback Delay Amount
*   **!** : Reset Parameters to Default
*   **Shift + F** : Toggle Fullscreen
*   **Shift + S** : Save current settings to `settings.xml`
*   **Shift + L** : Load settings from `settings.xml`
*   **(Various Letter Keys)**: Adjust specific effect parameters (see `ofApp::keyPressed` for details).
