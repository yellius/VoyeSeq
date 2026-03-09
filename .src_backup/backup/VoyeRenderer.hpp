#pragma once

#include "DistrhoUI.hpp"
#include "Constants.hpp"
#include "PatternModel.hpp"

START_NAMESPACE_DISTRHO

class VoyeRenderer {
public:
    static const char* getPitchName(uint8_t midiNote);
    static const char* getGridLabel(uint8_t midiNote);

    static void drawTransportBar(UI& ui, float w, float h, 
                                 const GridViewState& view, 
                                 EditField currentField, 
                                 const VoyeNote& editBuffer);
    
    static void drawGrid(UI& ui, float w, float h, 
                         const GridViewState& view, 
                         const Pattern& pattern,
                         const int fSelectedNoteIndex);

private:
    static void drawNote(UI& ui, uint8_t pitch, uint32_t start, uint32_t length, 
                         bool isSelected, uint8_t topNote, bool carrotVisible, bool isPrompt, const Pattern& pattern);
};

END_NAMESPACE_DISTRHO
