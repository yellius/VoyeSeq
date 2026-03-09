#pragma once

#include "DistrhoPluginInfo.h" // Must be before DistrhoUI.hpp
#include "DistrhoUI.hpp"
#include "Constants.hpp"
#include "PatternModel.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <atomic>

START_NAMESPACE_DISTRHO

/**
 * VoyeSeqUI: The Controller class for the Sequencer interface.
 * Handles keyboard events, parameter updates from the host, 
 * and state synchronization with the Audio Engine.
 */
class VoyeSeqUI : public UI,
                  public IdleCallback {
public:
    VoyeSeqUI();
    ~VoyeSeqUI() override;

protected:
    /* DPF Callbacks */
    void parameterChanged(uint32_t index, float value) override;
    void stateChanged(const char* key, const char* value) override;
    void onNanoDisplay() override;
    void auditionNote(uint8_t pitch, uint8_t velocity);
    bool onKeyboard(const KeyboardEvent& event) override;
    void idleCallback() override;

private:
    std::atomic<bool> fNeedsOscToggle;
    int fSock{-1};
    void  sendOscToggle();
    
    /* Logic Helpers */
    EditState getNextMode(EditState current);
    void      applyDelta(int delta);
    uint32_t  constrainNoteLength(uint8_t pitch, uint32_t start, uint32_t requestedLength);
    void      resolveCollisions(uint8_t pitch, uint32_t start, uint32_t length, int skipIndex);
    
    bool  handleKeyGridEdit(const KeyboardEvent& event);
    bool  handleKeyPatternSelect(const KeyboardEvent& event);
    bool  handleKeyNameEdit(const KeyboardEvent& event);
    
    // Helper to get current ticks based on the selected quantization
    uint32_t getQuantizeTicks() const { return QuantizeTable[fQuantizeIndex].ticks; }

    int fFont = -1;
    
    // The source of truth for the drawing loop
    PatternBank   fLocalBank;
    uint8_t       fCurrentPattern   = 64;
    uint8_t       fCopyPattern      = 64;
    //Pattern       fLocalPattern;            // Local copy of notes for rendering
    GridViewState fView;                    // Grouped 'Camera', Cursor, and Transport state
    EditState     fEdit = EditState::GridEdit;
    
    // Note Editing Logic
    VoyeNote      fNoteEditBuffer;          // Temporary storage for the note being tweaked
    VoyeNote      fDefaultNoteSettings;     // Template for new note creation
    EditField     fCurrentEditField;        // Which field in the matrix has focus (Pitch, Vel, etc.)
    int           fQuantizeIndex = 4;       // Index into QuantizeTable (default 16th)
    int           fSelectedNoteIndex = -1;  // -1 means no note selected
    void          updateSelection();        // Helper to find note at carrot
    std::string   fTempName;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoyeSeqUI)
};

END_NAMESPACE_DISTRHO
