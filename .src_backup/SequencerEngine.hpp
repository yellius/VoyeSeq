#pragma once
#include "DistrhoPlugin.hpp"
#include "PatternModel.hpp"
#include "Constants.hpp"
#include <vector>
#include <algorithm>
#include <atomic>

START_NAMESPACE_DISTRHO

class SequencerEngine {
public:
    struct PendingEvent {
        uint32_t frame;
        uint8_t data[3];
    };
    
    Pattern& getWritablePattern() { 
        return fBank.patterns[fActiveTriggerNote]; 
    }
    const Pattern& getPattern() const { 
        auto it = fBank.patterns.find(fActiveTriggerNote);
        if (it != fBank.patterns.end()) {
            return it->second;
        }
        static Pattern emptyPattern;
        return emptyPattern;
    }

    PatternBank& getBank() { return fBank; }
    const PatternBank& getBank() const { return fBank; }

    SequencerEngine() : fIsPlaying(false), fInternalTick(0.0),
                        fLastProcessedTick(-1), fActiveTriggerNote(64) {
        fBank.patterns[fActiveTriggerNote].lengthTicks = 768;
        fBank.patterns[fActiveTriggerNote].notes.clear();
    }

    void process(const TimePosition& timePos, bool triggerReceived, bool triggerReleased, uint8_t triggerNote, uint32_t frames, double sampleRate) {
        fPendingEvents.clear();

        if (fNeedsAudition.exchange(false)) { // Using atomic exchange
            
            if (fLastAuditionPitch != 255) {
                pushMidi(0, 0x80, fLastAuditionPitch, 0); 
            }
            pushMidi(0, 0x90, fAuditionPitch, fAuditionVel);
            fLastAuditionPitch = fAuditionPitch;
        }

        if (triggerReceived) {
            fIsPlaying = true;
            fInternalTick = 0.0;
            fLastProcessedTick = -1;
            fActiveTriggerNote = triggerNote;
        }

        if (triggerReleased) {
            killAllActiveNotes(0);
            fIsPlaying = false;
            return;
        }
        
        Pattern& fPattern = fBank.patterns[fActiveTriggerNote];

        if (!fIsPlaying) return;

        double beatsPerSecond = timePos.bbt.beatsPerMinute / 60.0;
        double ticksPerSecond = beatsPerSecond * Voye::PPQN;
        double ticksPerSample = ticksPerSecond / sampleRate;

        for (uint32_t f = 0; f < frames; ++f) {
            // 1. Determine current integer tick
            uint32_t currentTick = (uint32_t)fInternalTick;

            // 2. Only process if the tick has changed
            if ((int)currentTick != fLastProcessedTick) {
                
                // Loop through notes to find matches for this specific tick
                for (const auto& note : fPattern.notes) {
                    
                    // --- Note On ---
                    if (currentTick == note.startTick) {
                        pushMidi(f, 0x90, note.pitch, note.velocity);
                        fActiveNotes.push_back(note.pitch);
                    }
                    
                    // --- Note Off ---
                    // We check if the note should end at this tick
                    uint32_t offTick = (note.startTick + note.lengthTicks) % fPattern.lengthTicks;
                    if (currentTick == offTick) {
                        pushMidi(f, 0x80, note.pitch, note.off_velocity);
                        removeActiveNote(note.pitch);
                    }
                }
                fLastProcessedTick = (int)currentTick;
            }

            // 3. Increment and Wrap
            fInternalTick += ticksPerSample;
            if (fInternalTick >= (double)fPattern.lengthTicks) {
                fInternalTick -= (double)fPattern.lengthTicks;
                fLastProcessedTick = -1; // Reset to allow processing tick 0 again
            }
        }
    }
    
    std::vector<PendingEvent> getEvents() { return fPendingEvents; }
    uint32_t getCurrentTick() const { 
        // .at() is a const-safe way to access map elements
        return (uint32_t)fInternalTick % fBank.patterns.at(fActiveTriggerNote).lengthTicks; 
    }

    std::atomic<bool> fNeedsAudition{false};
    uint8_t fAuditionPitch = 0;
    uint8_t fAuditionVel = 0;

private:
    void pushMidi(uint32_t frame, uint8_t status, uint8_t d1, uint8_t d2) {
        fPendingEvents.push_back({frame, {status, d1, d2}});
    }

    void removeActiveNote(uint8_t pitch) {
        fActiveNotes.erase(std::remove(fActiveNotes.begin(), fActiveNotes.end(), pitch), fActiveNotes.end());
    }

    void killAllActiveNotes(uint32_t frame) {
        for (uint8_t pitch : fActiveNotes) {
            pushMidi(frame, 0x80, pitch, 64);
        }
        fActiveNotes.clear();
    }

    PatternBank   fBank;
    uint8_t       fActiveTriggerNote = 64;
    bool          fIsPlaying;
    double        fInternalTick;       // High precision local playhead
    int           fLastProcessedTick;
    uint8_t       fLastAuditionPitch = 255;
    std::vector<uint8_t> fActiveNotes;
    std::vector<PendingEvent> fPendingEvents;
};

END_NAMESPACE_DISTRHO
