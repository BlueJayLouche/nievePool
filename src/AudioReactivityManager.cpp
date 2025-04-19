#include "AudioReactivityManager.h"

AudioReactivityManager::AudioReactivityManager(ParameterManager* paramManager)
    : numBands(8),
      sensitivity(1.0f),
      smoothing(0.85f),
      enabled(false),
      paramManager(paramManager),
      currentDeviceIndex(-1),
      audioInputInitialized(false),
      audioInputLevel(0.0f),
      bufferSize(1024),
      normalizationEnabled(true) {

    // Ensure vectors are properly initialized
    fftSpectrum.resize(kNumFFTBins / 2, 0.0f);
    fftSmoothed.resize(kNumFFTBins / 2, 0.0f);

    bands.resize(numBands, 0.0f);
    smoothedBands.resize(numBands, 0.0f);
    audioBuffer.resize(bufferSize, 0.0f);

    // Additional initialization
    setupDefaultBandRanges();
    listAudioDevices();
}

AudioReactivityManager::~AudioReactivityManager() {
    closeAudioInput();
}

// Updated signature to match header
void AudioReactivityManager::setup(ParameterManager* paramManager, bool performanceMode) {
    this->paramManager = paramManager; // Assign paramManager

    // Start with default band ranges if not configured
    if (bandRanges.empty()) {
        setupDefaultBandRanges();
    }

    // Adjust buffer size for performance if needed
    if (performanceMode) {
        bufferSize = 512; // Smaller FFT size for performance
    }

    // Create FFT object with power-of-two size and Hamming window (good for audio)
    fft = std::shared_ptr<ofxFft>(ofxFft::create(bufferSize, OF_FFT_WINDOW_HAMMING));

    // Resize audio buffer for the new size
    audioBuffer.resize(bufferSize, 0.0f);

    // Setup audio input if enabled
    if (enabled) {
        setupAudioInput();
    }
}

// Clean shutdown of audio system
void AudioReactivityManager::exit() {
    closeAudioInput();

    // Wait for audio threads to complete
    ofSleepMillis(100);

    // Release FFT resources
    std::lock_guard<std::mutex> lock(audioMutex);
    fft.reset();
}

void AudioReactivityManager::setupDefaultBandRanges() {
    bandRanges.clear();

    // Logarithmic distribution of frequency bands (more detail in low frequencies)
    // These bands roughly correspond to:
    // 0: Sub bass (20-60Hz)
    // 1: Bass (60-250Hz)
    // 2: Low mids (250-500Hz)
    // 3: Mids (500-2000Hz)
    // 4: High mids (2-4kHz)
    // 5: Presence (4-6kHz)
    // 6: Brilliance (6-12kHz)
    // 7: Air (12-20kHz)

    // Calculate frequency range based on sample rate (typically 44100Hz)
    // Each bin represents (sampleRate/2) / (kNumFFTBins/2) Hz
    // For 44100Hz sample rate and 1024 FFT size, each bin is about 43Hz

    if (numBands == 8) {
        // These bin ranges are adjusted for kNumFFTBins = 1024 and 44100Hz sample rate
        bandRanges = {
            {1, 2},       // Sub bass (20-60Hz)
            {3, 5},       // Bass (60-250Hz)
            {6, 11},      // Low mids (250-500Hz)
            {12, 46},     // Mids (500-2000Hz)
            {47, 92},     // High mids (2-4kHz)
            {93, 139},    // Presence (4-6kHz)
            {140, 278},   // Brilliance (6-12kHz)
            {279, 511}    // Air (12-20kHz)
        };
    } else {
        // Create evenly distributed ranges if numBands is not 8
        // Note: We use only the first half of FFT bins (kNumFFTBins/2)
        // as the second half are mirrored for real signals
        int usableBins = kNumFFTBins / 2;
        int binsPerBand = usableBins / numBands;

        for (int i = 0; i < numBands; i++) {
            int minBin = i * binsPerBand;
            int maxBin = ((i+1) * binsPerBand) - 1;
            if (i == numBands - 1) {
                maxBin = usableBins - 1; // Ensure the last bin is included
            }
            bandRanges.push_back({minBin, maxBin});
        }
    }
}

void AudioReactivityManager::update() {
    if (!enabled || !paramManager || !fft) return;

    // Use mutex to protect FFT data during analysis
    {
        std::lock_guard<std::mutex> lock(audioMutex);

        // Analyze audio input
        analyzeAudio();

        // Group FFT bins into bands
        groupBands();
    }

    // Apply mappings to parameters
    applyMappings();
}

// Replaced with sputnikMesh version
void AudioReactivityManager::analyzeAudio() {
    // Get amplitude spectrum from FFT
    const auto& fftResult = fft->getAmplitudeVector();

    // Check that we have FFT data to process
    if (fftResult.empty()) {
        ofLogWarning("AudioReactivityManager") << "FFT result is empty!";
        return;
    }

    // Process only the first half of the spectrum (real signals mirror)
    int spectrumSize = std::min<int>(fftResult.size(), kNumFFTBins / 2);

    // Ensure our storage vectors are the right size
    if (fftSpectrum.size() != spectrumSize) {
        fftSpectrum.resize(spectrumSize, 0.0f);
        fftSmoothed.resize(spectrumSize, 0.0f);
    }

    // More aggressive processing from sputnikMesh
    float maxVal = 0.0f;
    for (int i = 0; i < spectrumSize; i++) {
        // Apply high sensitivity and exponential scaling
        float value = powf(fftResult[i] * sensitivity * 10.0f, 2.0f);

        // Track maximum value for normalization
        if (normalizationEnabled) {
            maxVal = std::max(maxVal, value);
        }

        // Store raw spectrum
        fftSpectrum[i] = value;
    }

    // If normalization is disabled, use a fixed value instead
    if (!normalizationEnabled) {
        maxVal = 1.0f;  // Use raw values without normalization
    }

    // Normalize (if enabled) and smooth (sputnikMesh version)
    for (int i = 0; i < spectrumSize; i++) {
        // Normalize by max value or use direct value if normalization is disabled
        float processedValue = (maxVal > 0) ? fftSpectrum[i] / maxVal : 0.0f;

        // More aggressive smoothing with peak detection
        fftSmoothed[i] = std::max(
            fftSmoothed[i] * smoothing,  // Decay
            processedValue * (1.0f - smoothing)  // New value
        );
    }
}

void AudioReactivityManager::setEnabled(bool enabled) {
    this->enabled = enabled;

    // Setup or close audio input based on enabled state
    if (enabled) {
        setupAudioInput();
    } else {
        closeAudioInput();
    }
}

void AudioReactivityManager::groupBands() {
    // Calculate average energy for each band range
    for (int i = 0; i < numBands; i++) {
        if (i >= bandRanges.size()) continue;

        const BandRange& range = bandRanges[i];
        float sum = 0.0f;
        int count = 0;

        // Sum the FFT bins in this band's range
        for (int bin = range.minBin; bin <= range.maxBin; bin++) {
            if (bin < fftSmoothed.size()) {
                sum += fftSmoothed[bin];
                count++;
            }
        }

        // Calculate average (avoid division by zero)
        float average = (count > 0) ? (sum / count) : 0.0f;

        // Apply smoothing between frames
        smoothedBands[i] = average * (1.0f - smoothing) + smoothedBands[i] * smoothing;

        // Store raw band energy
        bands[i] = average;
    }
}

// Replaced with sputnikMesh version
void AudioReactivityManager::applyMappings() {
    for (const auto& mapping : mappings) {
        if (mapping.band >= 0 && mapping.band < numBands) {
            // Get the audio band value
            float value = smoothedBands[mapping.band];

            // Scale and constrain to min/max range (sputnikMesh logic)
            value = mapping.min + value * mapping.scale * (mapping.max - mapping.min);
            value = ofClamp(value, mapping.min, mapping.max);

            // Apply to parameter using the existing applyParameterValue method
            // which handles the parameter ID matching for this project
            applyParameterValue(mapping.paramId, value, mapping.additive);
        }
    }
}


void AudioReactivityManager::applyParameterValue(const std::string& paramId, float value, bool additive) {
    // Note: The 'additive' flag from settings is now ignored here.
    // We always call the dedicated offset setters in ParameterManager.
    if (!paramManager) return;

    // Match parameter IDs and call the corresponding *offset* setter
    if (paramId == "lumakey_value" || paramId == "lumakeyValue") {
        paramManager->setAudioLumakeyValueOffset(value);
    }
    else if (paramId == "mix") {
        paramManager->setAudioMixOffset(value);
    }
    else if (paramId == "hue") {
        paramManager->setAudioHueOffset(value);
    }
    else if (paramId == "saturation") {
        paramManager->setAudioSaturationOffset(value);
    }
    else if (paramId == "brightness") {
        paramManager->setAudioBrightnessOffset(value);
    }
    else if (paramId == "temporal_filter_mix" || paramId == "temporalFilterMix") {
        paramManager->setAudioTemporalFilterMixOffset(value);
    }
    else if (paramId == "temporal_filter_resonance" || paramId == "temporalFilterResonance") {
        paramManager->setAudioTemporalFilterResonanceOffset(value);
    }
    else if (paramId == "sharpen_amount" || paramId == "sharpenAmount") {
        paramManager->setAudioSharpenAmountOffset(value);
    }
    else if (paramId == "x_displace" || paramId == "xDisplace") {
        paramManager->setAudioXDisplaceOffset(value);
    }
    else if (paramId == "y_displace" || paramId == "yDisplace") {
        paramManager->setAudioYDisplaceOffset(value);
    }
    else if (paramId == "z_displace" || paramId == "zDisplace") {
        paramManager->setAudioZDisplaceOffset(value);
    }
    else if (paramId == "z_frequency" || paramId == "zFrequency") {
        paramManager->setAudioZFrequencyOffset(value);
    }
    else if (paramId == "x_frequency" || paramId == "xFrequency") {
        paramManager->setAudioXFrequencyOffset(value);
    }
    else if (paramId == "y_frequency" || paramId == "yFrequency") {
        paramManager->setAudioYFrequencyOffset(value);
    }
    else if (paramId == "rotate") {
        paramManager->setAudioRotateOffset(value);
    }
    else if (paramId == "hue_modulation" || paramId == "hueModulation") {
        paramManager->setAudioHueModulationOffset(value);
    }
    else if (paramId == "hue_offset" || paramId == "hueOffset") {
        // Use the renamed offset setter for hueOffset
        paramManager->setAudioHueOffsetOffset(value);
    }
    else if (paramId == "hue_lfo" || paramId == "hueLFO") {
        paramManager->setAudioHueLFOOffset(value);
    }
    else if (paramId == "delay_amount" || paramId == "delayAmount") {
        paramManager->setAudioDelayAmountOffset(static_cast<int>(value));
    }
    // Note: If new parameters are added for audio reactivity,
    // ensure corresponding offset variables and setters/getters exist in ParameterManager
    // and update this function to call the correct offset setter.
}

void AudioReactivityManager::listAudioDevices() {
    // Get list of audio devices
    deviceList = ofSoundStreamListDevices();

    // Print devices to console
    ofLogNotice("AudioReactivityManager") << "Available audio input devices:";
    for (int i = 0; i < deviceList.size(); i++) {
        const auto& device = deviceList[i];
        if (device.inputChannels > 0) {
            ofLogNotice("AudioReactivityManager") << i << ": " << device.name
                                                 << " (in:" << device.inputChannels
                                                 << ", out:" << device.outputChannels << ")";
        }
    }
}

std::vector<std::string> AudioReactivityManager::getAudioDeviceList() const {
    std::vector<std::string> deviceNames;
    for (const auto& device : deviceList) {
        if (device.inputChannels > 0) {
            deviceNames.push_back(device.name);
        }
    }
    return deviceNames;
}

int AudioReactivityManager::getCurrentDeviceIndex() const {
    return currentDeviceIndex;
}

std::string AudioReactivityManager::getCurrentDeviceName() const {
    if (currentDeviceIndex >= 0 && currentDeviceIndex < deviceList.size()) {
        return deviceList[currentDeviceIndex].name;
    }
    return "No device selected";
}

bool AudioReactivityManager::selectAudioDevice(int deviceIndex) {
    // Check if index is valid
    if (deviceIndex < 0 || deviceIndex >= deviceList.size() ||
        deviceList[deviceIndex].inputChannels <= 0) {
        ofLogError("AudioReactivityManager") << "Invalid device index: " << deviceIndex;
        return false;
    }

    // Close current audio input if it's open
    if (audioInputInitialized) {
        closeAudioInput();
    }

    // Set new device index
    currentDeviceIndex = deviceIndex;

    // Setup audio input with new device
    if (enabled) {
        setupAudioInput();
    }

    ofLogNotice("AudioReactivityManager") << "Selected audio device: " << getCurrentDeviceName();
    return true;
}

bool AudioReactivityManager::selectAudioDevice(const std::string& deviceName) {
    // Find device by name
    for (int i = 0; i < deviceList.size(); i++) {
        if (deviceList[i].name == deviceName && deviceList[i].inputChannels > 0) {
            return selectAudioDevice(i);
        }
    }

    ofLogError("AudioReactivityManager") << "Audio device not found: " << deviceName;
    return false;
}

void AudioReactivityManager::setupAudioInput() {
    // Close any existing audio input
    closeAudioInput();

    // Setup audio input stream
    ofSoundStreamSettings settings;

    // If we have a selected device, use it
    if (currentDeviceIndex >= 0 && currentDeviceIndex < deviceList.size()) {
        settings.setInDevice(deviceList[currentDeviceIndex]);
    } else {
        ofLogWarning("AudioReactivityManager") << "No device selected, using default";
    }

    // Configure stream
    settings.numInputChannels = 1;
    settings.numOutputChannels = 0;
    settings.sampleRate = 44100;
    settings.bufferSize = bufferSize;
    settings.numBuffers = 4;
    settings.setInListener(this);

    try {
        // Setup sound stream
        soundStream.setup(settings);
        audioInputInitialized = true;
        ofLogNotice("AudioReactivityManager") << "Audio input initialized with device: " << getCurrentDeviceName();
    } catch (std::exception& e) {
        ofLogError("AudioReactivityManager") << "Failed to initialize audio input: " << e.what();
        audioInputInitialized = false;
    }
}

void AudioReactivityManager::closeAudioInput() {
    if (audioInputInitialized) {
        soundStream.close();
        audioInputInitialized = false;
        ofLogNotice("AudioReactivityManager") << "Audio input closed";
    }
}

// Replaced with sputnikMesh version (including NaN/Inf check and explicit FFT computation trigger)
void AudioReactivityManager::audioIn(ofSoundBuffer &input) {
    // Use mutex to protect shared data
    std::lock_guard<std::mutex> lock(audioMutex);

    // Check if input is valid
    if (input.getNumFrames() == 0) {
        ofLogWarning("AudioReactivityManager") << "Received empty audio buffer";
        return;
    }

    // Resize audioBuffer if needed
    if (audioBuffer.size() != bufferSize) {
        audioBuffer.resize(bufferSize, 0.0f);
    }

    // Copy input to audioBuffer with NaN/Inf checking
    int numSamples = std::min(static_cast<int>(input.getNumFrames()), bufferSize);
    const float* inputBuffer = input.getBuffer().data();

    // Calculate RMS for input level with NaN/Inf protection
    float sumSquared = 0.0f;
    for (int i = 0; i < numSamples; i++) {
        float sample = inputBuffer[i];
        if (std::isnan(sample) || std::isinf(sample)) {
            sample = 0.0f; // Replace NaN/Inf with 0
        }
        audioBuffer[i] = sample;
        sumSquared += sample * sample;
    }

    // Update input level (avoid division by zero)
    audioInputLevel = (numSamples > 0) ? sqrt(sumSquared / numSamples) : 0.0f;

    // Compute FFT using pointer-based API to ensure computation
    if (fft) {
        // Explicitly set signal to trigger FFT computation
        fft->setSignal(audioBuffer.data());

        // Use getAmplitude() to ensure FFT is computed and get pointer (optional check)
        float* amplitudePtr = fft->getAmplitude();
        // if (!amplitudePtr) {
        //     ofLogWarning("AudioReactivityManager") << "Failed to get FFT amplitude pointer after setSignal";
        // }
    }
}


// Getter and setter methods

float AudioReactivityManager::getBand(int band) const {
    if (band >= 0 && band < numBands) {
        return smoothedBands[band];
    }
    return 0.0f;
}

int AudioReactivityManager::getNumBands() const {
    return numBands;
}

std::vector<float> AudioReactivityManager::getAllBands() const {
    return smoothedBands;
}

void AudioReactivityManager::setNormalizationEnabled(bool enabled) {
    normalizationEnabled = enabled;
}

bool AudioReactivityManager::isNormalizationEnabled() const {
    return normalizationEnabled;
}

float AudioReactivityManager::getAudioInputLevel() const {
    return audioInputLevel;
}

bool AudioReactivityManager::isEnabled() const {
    return enabled;
}

void AudioReactivityManager::setSensitivity(float sensitivity) {
    this->sensitivity = sensitivity;
}

float AudioReactivityManager::getSensitivity() const {
    return sensitivity;
}

void AudioReactivityManager::setSmoothing(float smoothing) {
    this->smoothing = ofClamp(smoothing, 0.0f, 0.99f);
}

float AudioReactivityManager::getSmoothing() const {
    return smoothing;
}

void AudioReactivityManager::addMapping(const BandMapping& mapping) {
    mappings.push_back(mapping);
}

void AudioReactivityManager::removeMapping(int index) {
    if (index >= 0 && index < mappings.size()) {
        mappings.erase(mappings.begin() + index);
    }
}

void AudioReactivityManager::clearMappings() {
    mappings.clear();
}

std::vector<AudioReactivityManager::BandMapping> AudioReactivityManager::getMappings() const {
    return mappings;
}

// Helper function to add default mappings
void AudioReactivityManager::addDefaultMappings() {
    ofLogNotice("AudioReactivityManager") << "Adding default audio mappings";

    BandMapping bassMapping;
    bassMapping.band = 0; bassMapping.paramId = "z_displace"; bassMapping.scale = 0.5f;
    bassMapping.min = -0.2f; bassMapping.max = 0.2f; bassMapping.additive = false;
    addMapping(bassMapping);

    BandMapping lowMidsMapping;
    lowMidsMapping.band = 2; lowMidsMapping.paramId = "x_displace"; lowMidsMapping.scale = 0.05f;
    lowMidsMapping.min = -0.1f; lowMidsMapping.max = 0.1f; lowMidsMapping.additive = false;
    addMapping(lowMidsMapping);

    BandMapping midsMapping;
    midsMapping.band = 3; midsMapping.paramId = "y_displace"; midsMapping.scale = 0.5f;
    midsMapping.min = -0.1f; midsMapping.max = 0.1f; midsMapping.additive = false;
    addMapping(midsMapping);

    BandMapping highMidsMapping;
    highMidsMapping.band = 4; highMidsMapping.paramId = "hue"; highMidsMapping.scale = 0.01f;
    highMidsMapping.min = 0.8f; highMidsMapping.max = 1.2f; highMidsMapping.additive = false;
    addMapping(highMidsMapping);

    BandMapping presenceMapping;
    presenceMapping.band = 5; presenceMapping.paramId = "rotate"; presenceMapping.scale = 0.05f;
    presenceMapping.min = -0.05f; presenceMapping.max = 0.05f; presenceMapping.additive = true; // Additive rotation
    addMapping(presenceMapping);

    BandMapping brillianceMapping;
    brillianceMapping.band = 6; brillianceMapping.paramId = "saturation"; brillianceMapping.scale = 0.5f;
    brillianceMapping.min = 0.5f; brillianceMapping.max = 1.5f; brillianceMapping.additive = false; // Direct set
    addMapping(brillianceMapping);

    BandMapping airMapping;
    airMapping.band = 7; airMapping.paramId = "brightness"; airMapping.scale = 0.5f;
    airMapping.min = 0.5f; airMapping.max = 1.5f; airMapping.additive = false; // Direct set
    addMapping(airMapping);

    BandMapping sharpenMapping;
    sharpenMapping.band = 3; sharpenMapping.paramId = "sharpenAmount"; sharpenMapping.scale = 0.2f;
    sharpenMapping.min = 0.0f; sharpenMapping.max = 0.2f; sharpenMapping.additive = false;
    addMapping(sharpenMapping);
}

// XML Configuration methods

void AudioReactivityManager::loadFromXml(ofxXmlSettings& xml) {
    try {
        // First check if the audioReactivity tag exists
        if (!xml.tagExists("audioReactivity")) {
            ofLogNotice("AudioReactivityManager") << "No audio reactivity settings found";
            return;
        }

        // Push into audioReactivity tag safely
        if (!xml.pushTag("audioReactivity")) {
            ofLogError("AudioReactivityManager") << "Failed to push into audioReactivity tag";
            return;
        }

        // Load basic settings with safe defaults
        enabled = xml.getValue("enabled", false);
        normalizationEnabled = xml.getValue("normalizationEnabled", true);
        sensitivity = xml.getValue("sensitivity", 1.0f);
        smoothing = xml.getValue("smoothing", 0.85f);
        numBands = xml.getValue("numBands", 8);

        // Device settings
        std::string deviceName = xml.getValue("deviceName", "");
        if (!deviceName.empty()) {
            selectAudioDevice(deviceName);
        }

        // Resize bands arrays
        bands.resize(numBands, 0.0f);
        smoothedBands.resize(numBands, 0.0f);

        // Setup default band ranges
        setupDefaultBandRanges();

        // Safely check and load custom band ranges
        if (xml.tagExists("bandRanges") && xml.pushTag("bandRanges")) {
            int numRanges = xml.getNumTags("range");
            if (numRanges > 0) {
                bandRanges.clear();

                for (int i = 0; i < numRanges; i++) {
                    if (xml.pushTag("range", i)) {
                        BandRange range;
                        range.minBin = xml.getValue("minBin", 0);
                        range.maxBin = xml.getValue("maxBin", 0);
                        bandRanges.push_back(range);
                        xml.popTag(); // pop range
                    }
                }
            }
            xml.popTag(); // pop bandRanges
        }

        // Clear any existing mappings
        clearMappings();

        // Safely check and load mappings
        if (xml.tagExists("mappings") && xml.pushTag("mappings")) {
            int numMappings = xml.getNumTags("mapping");
            for (int i = 0; i < numMappings; i++) {
                if (xml.pushTag("mapping", i)) {
                    BandMapping mapping;
                    mapping.band = xml.getValue("band", 0);
                    mapping.paramId = xml.getValue("paramId", "");
                    mapping.scale = xml.getValue("scale", 1.0f);
                    mapping.min = xml.getValue("min", 0.0f);
                    mapping.max = xml.getValue("max", 1.0f);
                    mapping.additive = xml.getValue("additive", true);

                    addMapping(mapping);
                    xml.popTag(); // pop mapping
                }
            }
            xml.popTag(); // pop mappings
        }

        // If no mappings were loaded from XML, add the defaults
        if (mappings.empty()) {
            addDefaultMappings();
        }

        // Make sure to pop the audioReactivity tag
        xml.popTag(); // pop audioReactivity

        ofLogNotice("AudioReactivityManager") << "Loaded audio reactivity settings with "
                                             << mappings.size() << " mappings";

        // Setup audio if enabled
        if (enabled) {
            setupAudioInput();
        }
    } catch (const std::exception& e) {
        ofLogError("AudioReactivityManager") << "Error loading audio settings: " << e.what();
    } catch (...) {
        ofLogError("AudioReactivityManager") << "Unknown error loading audio settings";
    }
}

void AudioReactivityManager::saveToXml(ofxXmlSettings& xml) const {
    // Check if the tag already exists, if so remove it to avoid duplicates
    if (xml.tagExists("audioReactivity")) {
        xml.removeTag("audioReactivity");
    }

    // Add the main tag
    xml.addTag("audioReactivity");
    if (xml.pushTag("audioReactivity")) {
        // Save basic settings
        xml.setValue("enabled", enabled);
        xml.setValue("normalizationEnabled", normalizationEnabled);
        xml.setValue("sensitivity", sensitivity);
        xml.setValue("smoothing", smoothing);
        xml.setValue("numBands", numBands);

        if (currentDeviceIndex >= 0 && currentDeviceIndex < deviceList.size()) {
            xml.setValue("deviceName", deviceList[currentDeviceIndex].name);
            xml.setValue("deviceIndex", currentDeviceIndex);
        }

        // Save band ranges
        xml.addTag("bandRanges");
        if (xml.pushTag("bandRanges")) {
            for (size_t i = 0; i < bandRanges.size(); i++) {
                xml.addTag("range");
                if (xml.pushTag("range", i)) {
                    xml.setValue("minBin", bandRanges[i].minBin);
                    xml.setValue("maxBin", bandRanges[i].maxBin);
                    xml.popTag(); // pop range
                }
            }
            xml.popTag(); // pop bandRanges
        }

        // Save mappings
        xml.addTag("mappings");
        if (xml.pushTag("mappings")) {
            for (size_t i = 0; i < mappings.size(); i++) {
                xml.addTag("mapping");
                if (xml.pushTag("mapping", i)) {
                    xml.setValue("band", mappings[i].band);
                    xml.setValue("paramId", mappings[i].paramId);
                    xml.setValue("scale", mappings[i].scale);
                    xml.setValue("min", mappings[i].min);
                    xml.setValue("max", mappings[i].max);
                    xml.setValue("additive", mappings[i].additive);
                    xml.popTag(); // pop mapping
                }
            }
            xml.popTag(); // pop mappings
        }

        xml.popTag(); // pop audioReactivity
    }

    ofLogNotice("AudioReactivityManager") << "Saved audio reactivity settings with "
                                         << mappings.size() << " mappings";
}
