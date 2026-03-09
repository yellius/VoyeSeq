#include "DistrhoPlugin.hpp"
#include <algorithm>

START_NAMESPACE_DISTRHO

class VoyeSeqPlugin : public Plugin {
public:
    VoyeSeqPlugin() : Plugin(9, 0, 0), 
                      fLastNote(0.0f), fTranspose(0.0f), fBPM(120.0f), fIsPlaying(0.0f),
                      fBar(1.0f), fBeat(1.0f), fTick(0.0f), fSigNum(4.0f), fSigDen(4.0f) {}

protected:
    const char* getLabel()   const override { return "VoyeSeq"; }
    const char* getMaker()   const override { return "DISTRHO"; }
    const char* getLicense() const override { return "GPL"; }
    uint32_t    getVersion() const override { return d_version(1, 0, 0); }

    void initParameter(uint32_t index, Parameter& parameter) override {
        parameter.hints = kParameterIsAutomatable | kParameterIsOutput;
        if (index == 1) parameter.hints = kParameterIsAutomatable; // Transpose is Input

        switch (index) {
            case 0: parameter.name = "Note"; break;
            case 1: parameter.name = "Transpose"; break;
            case 2: parameter.name = "BPM"; break;
            case 3: parameter.name = "Transport"; break;
            case 4: parameter.name = "Bar"; break;
            case 5: parameter.name = "Beat"; break;
            case 6: parameter.name = "Tick"; break;
            case 7: parameter.name = "SigNum"; break;
            case 8: parameter.name = "SigDen"; break;
        }
    }

    float getParameterValue(uint32_t index) const override {
        switch (index) {
            case 0: return fLastNote;  case 1: return fTranspose;
            case 2: return fBPM;       case 3: return fIsPlaying;
            case 4: return fBar;       case 5: return fBeat;
            case 6: return fTick;      case 7: return fSigNum;
            case 8: return fSigDen;
            default: return 0.0f;
        }
    }

    void setParameterValue(uint32_t index, float value) override {
        if (index == 1) fTranspose = value;
    }

    void run(const float**, float**, uint32_t, const MidiEvent* events, uint32_t count) override {
        const TimePosition& timePos(getTimePosition());
        
        if (timePos.bbt.valid) {
            fBPM = (float)timePos.bbt.beatsPerMinute;
            fBar = (float)timePos.bbt.bar;
            fBeat = (float)timePos.bbt.beat;
            fTick = (float)timePos.bbt.tick;
            fSigNum = (float)timePos.bbt.beatsPerBar;
            fSigDen = (float)timePos.bbt.beatType;
        }

        fIsPlaying = timePos.playing ? 1.0f : 0.0f;

        for (uint32_t i = 0; i < count; ++i) {
            MidiEvent ev = events[i];
            uint8_t status = ev.data[0] & 0xF0;
            if (status == 0x90 || status == 0x80) {
                int shifted = (int)ev.data[1] + (int)fTranspose;
                ev.data[1] = (uint8_t)std::clamp(shifted, 0, 127);
                if (status == 0x90 && ev.data[2] > 0) {
                    fLastNote = (float)ev.data[1];
                }
            }
            writeMidiEvent(ev);
        }
    }

private:
    float fLastNote, fTranspose, fBPM, fIsPlaying, fBar, fBeat, fTick, fSigNum, fSigDen;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoyeSeqPlugin)
};

Plugin* createPlugin() { return new VoyeSeqPlugin(); }
END_NAMESPACE_DISTRHO
