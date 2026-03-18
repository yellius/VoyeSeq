#include "Constants.hpp"
#include "DistrhoPluginInfo.h"
#include "DistrhoPlugin.hpp"
#include "VoyeSeqPlugin.hpp"
#include <atomic>
#include <cstring>

START_NAMESPACE_DISTRHO

VoyeSeqPlugin::VoyeSeqPlugin() 
    : Plugin(Voye::PARAM_COUNT, 0, 2),
      fLastNote(0.0f), fUIPattern(64)
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
        case Voye::PARAM_BEAT:      parameter.name = "Beat"; break;
        case Voye::PARAM_TICK:      parameter.name = "Tick"; break;
        case Voye::PARAM_TICKSPERBEAT: parameter.name = "TicksPerBeat"; break;
        case Voye::PARAM_SIG_NUM:   parameter.name = "SigNum"; break;
        case Voye::PARAM_SIG_DEN:   parameter.name = "SigDen"; break;
        case Voye::PARAM_UI_PATTERN:
            parameter.name = "UIPattern";
            parameter.hints = kParameterIsAutomatable | kParameterIsInteger;
            parameter.ranges.def = 64.0f; // Default pattern
            parameter.ranges.min = 0.0f;  // Min MIDI note
            parameter.ranges.max = 127.0f; // Max MIDI note
            break;
        case Voye::PARAM_AUDITION:
            parameter.name = "Audition";
            parameter.hints = kParameterIsInteger | kParameterIsHidden;
            parameter.ranges.def = 0; 
            parameter.ranges.min = 0; 
            parameter.ranges.max = 0x7FFFFFFF; // Max 32-bit int
            break;
        case Voye::PARAM_CH_TRIGGER:
            parameter.name = "TrigCh";
            parameter.hints = kParameterIsInteger | kParameterIsAutomatable;
            parameter.ranges.def = 0; parameter.ranges.min = 0; parameter.ranges.max = 17; 
            break;
        case Voye::PARAM_CH_THRU:
            parameter.name = "ThruCh";
            parameter.hints = kParameterIsInteger | kParameterIsAutomatable;
            parameter.ranges.def = 0; parameter.ranges.min = 0; parameter.ranges.max = 17;
            break;
        case Voye::PARAM_CH_OUT:
            parameter.name = "OutCh";
            parameter.hints = kParameterIsInteger | kParameterIsAutomatable;
            parameter.ranges.def = 1; parameter.ranges.min = 1; parameter.ranges.max = 16;
            break;
    }

}

float VoyeSeqPlugin::getParameterValue(uint32_t index) const {
    switch (index) {
        case Voye::PARAM_NOTE:      return fLastNote;
        case Voye::PARAM_BPM:       return fTransport.bpm;
        case Voye::PARAM_TRANSPORT: return fTransport.playing ? 1.0f : 0.0f;
        case Voye::PARAM_BAR:       return (float)fTransport.bar;
        case Voye::PARAM_BEAT:      return (float)fTransport.beat;
        case Voye::PARAM_TICK:      return (float)fTransport.tick;
        case Voye::PARAM_TICKSPERBEAT: return (float)fTransport.tpb;
        case Voye::PARAM_SIG_NUM:   return (float)fTransport.sigNum;
        case Voye::PARAM_SIG_DEN:   return (float)fTransport.sigDen;
        case Voye::PARAM_UI_PATTERN: return fUIPattern;
        case Voye::PARAM_CH_TRIGGER: return fChTrigger;
        case Voye::PARAM_CH_THRU:    return fChThru;
        case Voye::PARAM_CH_OUT:     return fChOut;
        default: return 0.0f;
    }
}

void VoyeSeqPlugin::setParameterValue(uint32_t index, float value) {
    // In this plugin, most parameters are "Output-only" (Engine -> UI).
    // However, if we add an "Internal/External Clock" switch later, 
    // we would handle it here.
    switch (index) {
        //case Voye::PARAM_TRANSPORT: fIsPlaying = value; break;
        case Voye::PARAM_UI_PATTERN: fUIPattern = static_cast<uint8_t>(value); break;
        case Voye::PARAM_AUDITION:
            // IGNORE if the DAW is just loading the project!
            if (fHasRunOnce) {
                int packed = static_cast<int>(value);
                uint8_t pitch = packed & 0xFF;
                uint8_t vel = (packed >> 8) & 0xFF;
                fEngine.pushAudition(pitch, vel);
            }
            break;
        case Voye::PARAM_CH_TRIGGER: fChTrigger = static_cast<uint8_t>(value); break;
        case Voye::PARAM_CH_THRU:    fChThru = static_cast<uint8_t>(value); break;
        case Voye::PARAM_CH_OUT:     fChOut = static_cast<uint8_t>(value); break;
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
    if (std::strcmp(key, "pattern_data") == 0) {
        fNextBank.deserializeData(value);
        fHasNextBank.store(true, std::memory_order_release);
    }
    if (std::strcmp(key, "pattern_names") == 0) {
        fNextBank.deserializeNames(value);
        fHasNextBank.store(true, std::memory_order_release);
    }
}

void VoyeSeqPlugin::run(const float**, float**, uint32_t frames, const MidiEvent* events, uint32_t count) {
    const TimePosition& timePos(getTimePosition());

    fHasRunOnce = true;
    
    if (fHasNextBank.exchange(false, std::memory_order_acquire)) {
        std::swap(fEngine.getBank(), fNextBank); 
    }

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

    // 1. Array to hold all output events (Thru + Sequencer) safely on the stack
    MidiEvent outEvents[1024]; 
    size_t outCount = 0;

    for (uint32_t i = 0; i < count; ++i) {
        uint8_t status  = events[i].data[0] & 0xF0;
        uint8_t channel = (events[i].data[0] & 0x0F) + 1; 
        uint8_t note    = events[i].data[1];
        uint8_t vel     = events[i].data[2];

        bool isNoteOn  = (status == 0x90 && vel > 0);
        bool isNoteOff = (status == 0x80 || (status == 0x90 && vel == 0));

        // --- MIDI THRU ---
        if (fChThru != 17 && (fChThru == 0 || fChThru == channel)) { 
            if (outCount < 1024) {
                MidiEvent thruEv = events[i];
                thruEv.data[0] = status | (fChOut - 1); // Stamp output channel
                outEvents[outCount++] = thruEv;         // Store for sorting later
            }
        }

        // --- PATTERN TRIGGER ---
        if (fChTrigger != 17 && (fChTrigger == 0 || fChTrigger == channel)) {
            if (isNoteOn) {
                fTriggerNote = note;
                triggerReceived = true;
                triggerReleased = false;
            } else if (isNoteOff && note == fTriggerNote) {
                triggerReleased = true;
                triggerReceived = false;
            }
        }
    }
    
    // 2. Process Engine
    fEngine.process(timePos, triggerReceived, triggerReleased, fTriggerNote, frames, getSampleRate());

    // 3. Add Sequencer Notes to Output Array
    size_t engineEventCount = fEngine.getEventCount();
    for (size_t i = 0; i < engineEventCount; ++i) {
        const auto& pendingEv = fEngine.getEvent(i);
        
        if (outCount < 1024) {
            MidiEvent outEv;
            outEv.frame = pendingEv.frame; 
            outEv.size = 3;
            outEv.data[0] = (pendingEv.data[0] & 0xF0) | (fChOut - 1); 
            outEv.data[1] = pendingEv.data[1];
            outEv.data[2] = pendingEv.data[2];
            
            outEvents[outCount++] = outEv;
        }
    }

    // 4. Sort all events chronologically to satisfy DAW MIDI rules!
    std::sort(outEvents, outEvents + outCount, [](const MidiEvent& a, const MidiEvent& b) {
        return a.frame < b.frame;
    });

    // 5. Finally, write the strictly-ordered events to the host
    for (size_t i = 0; i < outCount; ++i) {
        writeMidiEvent(outEvents[i]);
    }
}

Plugin* createPlugin() { return new VoyeSeqPlugin(); }

END_NAMESPACE_DISTRHO
