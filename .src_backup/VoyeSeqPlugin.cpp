#include "DistrhoPluginInfo.h"
#include "DistrhoPlugin.hpp"
#include "VoyeSeqPlugin.hpp"
#include <cstring>

START_NAMESPACE_DISTRHO

VoyeSeqPlugin::VoyeSeqPlugin() 
    : Plugin(Voye::PARAM_COUNT, 0, 2),
      fLastNote(0.0f)
{
    fTransport.bpm = 120.0f;
    fTransport.playing = false;
    fTransport.bar = 1;
    fTransport.tick = 0;
    fTransport.tpb = 192.0;
    fTransport.sigNum = 4;
    fTransport.sigDen = 4;
}

void VoyeSeqPlugin::initParameter(uint32_t index, Parameter& parameter) {
    parameter.hints = kParameterIsAutomatable | kParameterIsOutput;

    switch (index) {
        case Voye::PARAM_NOTE:      parameter.name = "Note"; break;
        case Voye::PARAM_BPM:       parameter.name = "BPM"; break;
        case Voye::PARAM_TRANSPORT: parameter.name = "Transport"; break;
        case Voye::PARAM_BAR:       parameter.name = "Bar"; break;
        case Voye::PARAM_BEAT:       parameter.name = "Beat"; break;
        case Voye::PARAM_TICK:      parameter.name = "Tick"; break;
        case Voye::PARAM_TICKSPERBEAT: parameter.name = "TicksPerBeat"; break;
        case Voye::PARAM_SIG_NUM:   parameter.name = "SigNum"; break;
        case Voye::PARAM_SIG_DEN:   parameter.name = "SigDen"; break;
    }

}

float VoyeSeqPlugin::getParameterValue(uint32_t index) const {
    switch (index) {
        case Voye::PARAM_NOTE:      return fLastNote;
        case Voye::PARAM_BPM:       return fTransport.bpm;
        case Voye::PARAM_TRANSPORT: return fTransport.playing ? 1.0f : 0.0f;
        case Voye::PARAM_BAR:       return (float)fTransport.bar;
        case Voye::PARAM_BEAT:       return (float)fTransport.beat;
        case Voye::PARAM_TICK:      return (float)fTransport.tick;
        case Voye::PARAM_TICKSPERBEAT: return (float)fTransport.tpb;
        case Voye::PARAM_SIG_NUM:   return (float)fTransport.sigNum;
        case Voye::PARAM_SIG_DEN:   return (float)fTransport.sigDen;
        default: return 0.0f;
    }
}

void VoyeSeqPlugin::setParameterValue(uint32_t index, float value) {
    // In this plugin, most parameters are "Output-only" (Engine -> UI).
    // However, if we add an "Internal/External Clock" switch later, 
    // we would handle it here.
    switch (index) {
        //case Voye::PARAM_TRANSPORT: fIsPlaying = value; break;
        default: break;
    }
}

void VoyeSeqPlugin::initState(uint32_t index, State& state) {
    if (index == 0) {
        state.key = "pattern_names";
        state.defaultValue = "";
        state.hints = kStateIsHostWritable;
    }
    if (index == 1) {
        state.key = "pattern_data";
        state.defaultValue = "";
        state.hints = kStateIsHostWritable;
    }
}

String VoyeSeqPlugin::getState(const char* key) const {
    if (std::strcmp(key, "pattern_names") == 0) {
        return String(fEngine.getBank().serializeNames().c_str());
    }
    if (std::strcmp(key, "pattern_data") == 0) {
        return String(fEngine.getBank().serializeData().c_str());
    }
    return String();
}

void VoyeSeqPlugin::setState(const char* key, const char* value) {
/*    d_stdout("VoyeSeq [Plugin]: setState called | Key: %s | Value Length: %zu", 
             key, std::strlen(value));
    std::fflush(stdout);*/
    if (std::strcmp(key, "pattern_data") == 0) {
        fEngine.getBank().deserializeData(value);
    }
    if (std::strcmp(key, "pattern_names") == 0) {
        fEngine.getBank().deserializeNames(value);
    }
}

void VoyeSeqPlugin::run(const float**, float**, uint32_t frames, const MidiEvent* events, uint32_t count) {
    const TimePosition& timePos(getTimePosition());

    if (timePos.bbt.valid) {
        fTransport.bpm    = (float)timePos.bbt.beatsPerMinute;
        fTransport.bar    = (int)timePos.bbt.bar;
        fTransport.beat    = (int)timePos.bbt.beat;
        fTransport.tick   = (int)timePos.bbt.tick;
        fTransport.tpb    = (double)timePos.bbt.ticksPerBeat;
        fTransport.sigNum = (int)timePos.bbt.beatsPerBar;
        fTransport.sigDen = (int)timePos.bbt.beatType;
    }
    
    bool playingNow = timePos.playing;
    if (playingNow != fTransport.playing) {
        fTransport.playing = playingNow;
        // Notify DPF that the transport parameter changed
        setParameterValue(Voye::PARAM_TRANSPORT, fTransport.playing ? 1.0f : 0.0f);
    }

    bool triggerReceived = false;
    bool triggerReleased = false;
    
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t status  = events[i].data[0] & 0xF0;
        uint8_t channel = events[i].data[0] & 0x0F;
        uint8_t note    = events[i].data[1];
        uint8_t vel     = events[i].data[2];

        // --- UI AUDITION TRIGGER (Channel 16) ---
        if (channel == 15) {
            if (status == 0x90 && vel > 0) {
                fEngine.fAuditionPitch = note;
                fEngine.fAuditionVel = vel;
                fEngine.fNeedsAudition.store(true);
            }
            continue; // Skip the rest of the loop for UI messages
        }

        // --- MIDI PUSH TO HOST (All other channels) ---
        if (channel != 15) {
            // NOTE ON
            if (status == 0x90 && vel > 0) {
              fTriggerNote = note;
              triggerReceived = true;
            }
            // NOTE OFF
            else if (status == 0x80 || (status == 0x90 && vel == 0)) triggerReleased = true;
        }
    }
    
    fEngine.process(timePos, triggerReceived, triggerReleased, fTriggerNote,
                    frames, getSampleRate());
    
    // Output MIDI from engine
    for (const auto& ev : fEngine.getEvents()) {
        MidiEvent outEv;
        outEv.frame = ev.frame; outEv.size = 3;
        std::copy(ev.data, ev.data + 3, outEv.data);
        writeMidiEvent(outEv);
    }
}

Plugin* createPlugin() { return new VoyeSeqPlugin(); }

END_NAMESPACE_DISTRHO
