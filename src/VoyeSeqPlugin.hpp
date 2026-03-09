#pragma once
#include "DistrhoPluginInfo.h"
#include "DistrhoPlugin.hpp"
#include "SequencerEngine.hpp"
#include <algorithm>

START_NAMESPACE_DISTRHO

class SequencerEngine;

class VoyeSeqPlugin : public Plugin {
public:
    VoyeSeqPlugin();

protected:
    const char* getLabel() const override { return "VoyeSeq"; }
    const char* getMaker() const override { return "Yellius"; }
    const char* getLicense() const override { return "GPL"; }
    uint32_t getVersion() const override { return d_version(1, 0, 0); }

    void initParameter(uint32_t index, Parameter& parameter) override;
    float getParameterValue(uint32_t index) const override;
    void setParameterValue(uint32_t index, float value) override;
    
    void    initState(uint32_t index, State& state) override;
    void    setState(const char* key, const char* value) override;
    String  getState(const char* key) const override;

    void run(const float**, float**, uint32_t, const MidiEvent*, uint32_t) override;

private:
    SequencerEngine fEngine;
    TransportState  fTransport;
    float           fLastNote;
    uint8_t         fTriggerNote;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoyeSeqPlugin)
};

END_NAMESPACE_DISTRHO
