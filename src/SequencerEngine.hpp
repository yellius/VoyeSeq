#pragma once
#include "DistrhoPlugin.hpp"
#include "PatternModel.hpp"
#include "Constants.hpp"
#include <array>
#include <atomic>

START_NAMESPACE_DISTRHO

class SequencerEngine {
public:
    struct PendingEvent {
        uint32_t frame;
        uint8_t data[3];
    };

    static constexpr size_t MAX_PENDING_EVENTS = 512;
    
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
        
        // Initialize scheduled Note Offs to -1 (not playing)
        for (int i = 0; i < 128; ++i) { 
            fActiveNoteOffTicks[i] = -1; 
            fActiveNoteOffVels[i] = 64;
        }
        
        fBank.patterns[fActiveTriggerNote].lengthTicks = 768;
        fBank.patterns[fActiveTriggerNote].notes.clear();
    }

    void process(const TimePosition& timePos, bool triggerReceived, bool triggerReleased, uint8_t triggerNote, uint32_t frames, double sampleRate) {
        
        fPendingEventCount = 0;

        // --- NEW AUDITION QUEUE PROCESSING ---
        int read = fAuditionReadIdx.load(std::memory_order_relaxed);
        while (read != fAuditionWriteIdx.load(std::memory_order_acquire)) {
            AuditionEvent ev = fAuditionQueue[read];
            
            if (ev.vel > 0) {
                pushMidi(0, 0x90, ev.pitch, ev.vel); // Note On
            } else {
                pushMidi(0, 0x80, ev.pitch, 0);      // Note Off
            }
            read = (read + 1) % 32;
        }
        fAuditionReadIdx.store(read, std::memory_order_release);

        if (triggerReleased) {
            killAllActiveNotes(0);
            fIsPlaying = false;
        }

        if (triggerReceived) {
            killAllActiveNotes(0);
            fIsPlaying = true;
            fInternalTick = 0.0;
            fLastProcessedTick = -1;
            fActiveTriggerNote = triggerNote;
        }

        if (!fIsPlaying) return;

        Pattern& fPattern = fBank.patterns[fActiveTriggerNote];

        double beatsPerSecond = timePos.bbt.beatsPerMinute / 60.0;
        double ticksPerSecond = beatsPerSecond * Voye::PPQN;
        double ticksPerSample = ticksPerSecond / sampleRate;

        for (uint32_t f = 0; f < frames; ++f) {
            uint32_t currentTick = (uint32_t)fInternalTick;

            if ((int)currentTick != fLastProcessedTick) {
                
                // 1. Process all scheduled Note Offs FIRST
                // This guarantees back-to-back notes don't mute each other!
                for (int i = 0; i < 128; ++i) {
                    if (fActiveNoteOffTicks[i] == (int)currentTick) {
                        pushMidi(f, 0x80, i, fActiveNoteOffVels[i]);
                        fActiveNoteOffTicks[i] = -1; // Mark as successfully turned off
                    }
                }

                // 2. Process Note Ons from the pattern
                for (const auto& note : fPattern.notes) {
                    if (currentTick == note.startTick) {
                        // Trigger the note
                        pushMidi(f, 0x90, note.pitch, note.velocity);
                        
                        // Schedule the Note Off independently
                        int offTick = (note.startTick + note.lengthTicks) % fPattern.lengthTicks;
                        fActiveNoteOffTicks[note.pitch] = offTick;
                        fActiveNoteOffVels[note.pitch] = note.off_velocity;
                    }
                }
                
                fLastProcessedTick = (int)currentTick;
            }

            fInternalTick += ticksPerSample;
            if (fInternalTick >= (double)fPattern.lengthTicks) {
                fInternalTick -= (double)fPattern.lengthTicks;
                fLastProcessedTick = -1; 
            }
        }
    }
    
    size_t getEventCount() const { return fPendingEventCount; }
    const PendingEvent& getEvent(size_t index) const { return fPendingEvents[index]; }
    
    uint32_t getCurrentTick() const { 
        return (uint32_t)fInternalTick % fBank.patterns.at(fActiveTriggerNote).lengthTicks; 
    }

    struct AuditionEvent { uint8_t pitch; uint8_t vel; };
    std::atomic<int> fAuditionWriteIdx{0};
    std::atomic<int> fAuditionReadIdx{0};
    std::array<AuditionEvent, 32> fAuditionQueue{};

    void pushAudition(uint8_t pitch, uint8_t vel) {
        int write = fAuditionWriteIdx.load(std::memory_order_relaxed);
        int nextWrite = (write + 1) % 32;
        if (nextWrite != fAuditionReadIdx.load(std::memory_order_acquire)) {
            fAuditionQueue[write] = {pitch, vel};
            fAuditionWriteIdx.store(nextWrite, std::memory_order_release);
        }
    }

private:
    void pushMidi(uint32_t frame, uint8_t status, uint8_t d1, uint8_t d2) {
        if (fPendingEventCount < MAX_PENDING_EVENTS) {
            fPendingEvents[fPendingEventCount++] = {frame, {status, d1, d2}};
        }
    }

    void killAllActiveNotes(uint32_t frame) {
        // Iterate through all 128 scheduled ticks
        for (uint8_t i = 0; i < 128; ++i) {
            if (fActiveNoteOffTicks[i] != -1) { 
                pushMidi(frame, 0x80, i, fActiveNoteOffVels[i]);
                fActiveNoteOffTicks[i] = -1;
            }
        }
    }

    PatternBank   fBank;
    uint8_t       fActiveTriggerNote = 64;
    bool          fIsPlaying;
    double        fInternalTick;       
    int           fLastProcessedTick;
    uint8_t       fLastAuditionPitch = 255;
    
    std::array<PendingEvent, MAX_PENDING_EVENTS> fPendingEvents;
    size_t fPendingEventCount = 0;
    
    // Independent scheduling arrays
    int     fActiveNoteOffTicks[128]; 
    uint8_t fActiveNoteOffVels[128];
};

END_NAMESPACE_DISTRHO