#include "VoyeSeqUI.hpp"
#include "VoyeUIUtils.hpp"
#include "VoyeRenderer.hpp"
#include "VoyeSeqPlugin.hpp"
#include "FiraMono.h"
#include <cstdio>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

START_NAMESPACE_DISTRHO

//--Main Constructor for Plugin UI
//------------------------------------------------------------------------------
VoyeSeqUI::VoyeSeqUI() 
    : UI(UI_WIDTH, UI_HEIGHT) {
    std::printf("UI Constructor: Initializing...\n");
    std::fflush(stdout);
    // Initialize the "Camera" and Cursor state
    fView.topNote = 72;
    fView.carrotPitch = 60;
    fView.carrotTick = 0;
    fView.carrotVisible = true;
    fView.quantizeTicks = getQuantizeTicks();
    fView.transport.playing = false;

    // Default "Template" for new notes
    fDefaultNoteSettings.pitch = 60;
    fDefaultNoteSettings.velocity = 100;
    fDefaultNoteSettings.off_velocity = 64;
    fDefaultNoteSettings.startTick = 0;
    fDefaultNoteSettings.lengthTicks = 48; // 16th note default

    // On start, the Edit Buffer is a copy of the defaults
    fNoteEditBuffer = fDefaultNoteSettings;
    fCurrentEditField = EditField::None;
    
    fCurrentPattern = 64;
    if (fLocalBank.patterns.find(fCurrentPattern) == fLocalBank.patterns.end()) {
        fLocalBank.patterns[fCurrentPattern].lengthTicks = 768;
    }

    fSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (fSock < 0) {
      std::fprintf(stderr, "VoyeSeq: Failed to create OSC socket\n");
      std::fflush(stderr);
    }
 
    fFont = -1;
    addIdleCallback(this);
}

//--Main Destructor for Plugin UI
//------------------------------------------------------------------------------
VoyeSeqUI::~VoyeSeqUI() {
      if (fSock >= 0) close(fSock);
      fSock = -1;
}

//--Interface to allow Plugin to send parameter changes to UI
//------------------------------------------------------------------------------
void VoyeSeqUI::parameterChanged(uint32_t index, float value) {
    switch (index) {
        case Voye::PARAM_TRANSPORT: fView.transport.playing = (value > 0.5f); break;
        case Voye::PARAM_BPM:       fView.transport.bpm     = value;         break;
        case Voye::PARAM_BAR:       fView.transport.bar     = (int)value;    break;
        case Voye::PARAM_BEAT:       fView.transport.beat     = (int)value;    break;
        case Voye::PARAM_TICKSPERBEAT: fView.transport.tpb    = value;    break;
        case Voye::PARAM_TICK:      fView.transport.tick    = (int)value;    break;
        case Voye::PARAM_SIG_NUM:   fView.transport.sigNum  = (int)value;    break;
        case Voye::PARAM_SIG_DEN:   fView.transport.sigDen  = (int)value;    break;
    }
    repaint();
}

//--Interface to allow Plugin to send state changes to UI
//------------------------------------------------------------------------------
/*void VoyeSeqUI::stateChanged(const char* key, const char* value) {
    if (std::strcmp(key, "active_pattern") == 0) {
        std::printf("UI: Data -> %s\n", value);
        fLocalPattern.deserialize(value);
        repaint();
    }
}*/
void VoyeSeqUI::stateChanged(const char* key, const char* value) {
    if (std::strcmp(key, "pattern_data") == 0) {
        // Update the entire local bank
        fLocalBank.deserializeData(value);
        
        // Refresh the grid based on the current editing note
        // (Note: we use fLocalBank.patterns[fEditingNote] in onNanoDisplay)
        repaint();
    }
    else if (std::strcmp(key, "pattern_names") == 0) {
        fLocalBank.deserializeNames(value);
        repaint();
    }    
}

//--Send play/stop (loop) toggle over UDP / OSC to Host
//------------------------------------------------------------------------------
void VoyeSeqUI::sendOscToggle() {
    if (fSock < 0) return;

    struct sockaddr_in servaddr;
    std::memset(&servaddr, 0, sizeof(servaddr)); 
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(3819); 
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Total 20 bytes: 16 for address (aligned) + 4 for type-tag (aligned)
    const char oscMsgP[20] = {
        '/', 't', 'o', 'g', 'g', 'l', 'e', '_', 'r', 'o', 'l', 'l', 
        0, 0, 0, 0, // 4-byte null padding for the address
        ',', 0, 0, 0 // 4-byte null-padded type tag string
    };
/*    const char oscMsgP[20] = {
        '/', 't', 'r', 'a', 'n', 's', 'p', 'o', 'r', 't', '_', 's', 
        't', 'o', 'p', 0, // 4-byte null padding for the address
        ',', 0, 0, 0 // 4-byte null-padded type tag string
    };*/
    const char oscMsgL[20] = {
        '/', 'l', 'o', 'o', 'p', '_', 't', 'o', 'g', 'g', 'l', 'e', 
        0, 0, 0, 0, // 4-byte null padding for the address
        ',', 0, 0, 0 // 4-byte null-padded type tag string
    };
    
    if (fView.transport.playing) {
      sendto(fSock, oscMsgP, 20, 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    } else {
      sendto(fSock, oscMsgL, 20, 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    };
}

//--Main non-RT critical loop to deal with non-audio stuff (blinking, network)
//------------------------------------------------------------------------------
void VoyeSeqUI::idleCallback() {
    auto now = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(now.time_since_epoch()).count();

    // SPG-style blinking
    bool newVisibility = (std::fmod(seconds, 0.4) < 0.2);

    if (newVisibility != fView.carrotVisible) {
        fView.carrotVisible = newVisibility;
        repaint();
    }
    
    // Check if we need to send start/stop toggle over OSC
    if (fNeedsOscToggle.exchange(false)) {
        sendOscToggle();
    }
}

//--Main display trigger for Plugin UI
//------------------------------------------------------------------------------
void VoyeSeqUI::onNanoDisplay() {
    const float w = getWidth();
    const float h = getHeight();

    if (fFont == -1) 
        fFont = createFontFromMemory("fira", FiraMono_Medium_ttf, FiraMono_Medium_ttf_len, false);

    // Background: Voyetra Navy
    beginPath(); 
    rect(0.0f, 0.0f, w, h); 
    fillColor(Voye::Colors::Navy); 
    fill();
    
    if (fFont != -1) {
        fontFaceId(fFont);
    
        const Pattern& activePattern = fLocalBank.patterns[fCurrentPattern];
        const VoyeNote& displayNote = (fSelectedNoteIndex != -1) 
                                      ? activePattern.notes[fSelectedNoteIndex] 
                                      : fNoteEditBuffer;

        VoyeRenderer::drawTransportBar(*this, w, h, fEdit, fView, fCurrentEditField, displayNote);
        VoyeRenderer::drawList(*this, w, h, fEdit, fView, fCurrentPattern, fCopyPattern, fLocalBank, fTempName);
        VoyeRenderer::drawGrid(*this, w, h, fEdit, fView, activePattern, fSelectedNoteIndex);
        VoyeRenderer::drawBottomBar(*this, w, h, fEdit, fView);
    }
}

//--Send MIDI note for Audition (when selecting note, inserting note)
//------------------------------------------------------------------------------
void VoyeSeqUI::auditionNote(uint8_t pitch, uint8_t velocity) {
    //if (pitch > 127) return;
    //this->getPlugin().previewNote(pitch, velocity);
    sendNote(15, pitch, velocity);
}

//--Helper function to cycle through EditStates (tab key)
//------------------------------------------------------------------------------
EditState VoyeSeqUI::getNextMode(EditState current) {
    int next = static_cast<int>(current) + 1;
    if (next >= static_cast<int>(EditState::Count)) {
        return EditState::GridEdit;
    }
    return static_cast<EditState>(next);
}

//--Helper function to apply changes to buffer or note
//------------------------------------------------------------------------------
void VoyeSeqUI::applyDelta(int delta) {
    if (fCurrentEditField == EditField::Units) {
        if (delta > 0) { if (fQuantizeIndex > 0) fQuantizeIndex--; }
        else { if (fQuantizeIndex < QUANTIZE_COUNT - 1) fQuantizeIndex++; }
        fView.quantizeTicks = getQuantizeTicks();
        return;
    }

    VoyeNote& target = (fSelectedNoteIndex != -1) 
                       ? fLocalBank.patterns[fCurrentPattern].notes[fSelectedNoteIndex] 
                       : fNoteEditBuffer;

    uint32_t stepSize = getQuantizeTicks();

    switch (fCurrentEditField) {
        case EditField::Pitch:
            target.pitch = (uint8_t)std::clamp((int)target.pitch + delta, 0, 127);
            if (fSelectedNoteIndex != -1) {
                resolveCollisions(target.pitch, target.startTick, target.lengthTicks, fSelectedNoteIndex);
                auditionNote(target.pitch, target.velocity);
            }
            break;

        case EditField::Start: {
            int32_t moveAmount = delta * (int32_t)stepSize;
            if (moveAmount < 0 && (uint32_t)std::abs(moveAmount) > target.startTick) {
                target.startTick = 0;
            } else {
                target.startTick += moveAmount;
                if (target.startTick >= MAX_GRID_TICKS) target.startTick = 0;
            }
            
            if (fSelectedNoteIndex != -1) {
                // First, truncate notes we moved INTO
                resolveCollisions(target.pitch, target.startTick, target.lengthTicks, fSelectedNoteIndex);
                // Second, shrink ourselves if we moved UP TO another note
                target.lengthTicks = constrainNoteLength(target.pitch, target.startTick, target.lengthTicks);
            }
            break;
        }

        case EditField::Length: {
            int32_t moveAmount = delta * (int32_t)stepSize;
            int32_t newLength = (int32_t)target.lengthTicks + moveAmount;
            if (newLength < (int32_t)stepSize) newLength = stepSize; 

            target.lengthTicks = constrainNoteLength(target.pitch, target.startTick, (uint32_t)newLength);
            break;
        }

        case EditField::Velocity:
            target.velocity = (uint8_t)std::clamp((int)target.velocity + delta, 1, 127);
            break;

        case EditField::OffVel:
            target.off_velocity = (uint8_t)std::clamp((int)target.off_velocity + delta, 1, 127);
            break;

        default: break;
    }
    if (fSelectedNoteIndex != -1) {
        // 1. Sync the carrot to the note's new position
        fView.carrotPitch = target.pitch;
        fView.carrotTick = target.startTick;

        // 2. Update topNote so the selected note stays visible
        // If the note moved above the top of the screen
        if (target.pitch > fView.topNote) {
            fView.topNote = target.pitch;
        } 
        // If the note moved below the bottom of the screen
        else if (target.pitch < (fView.topNote - NUMROWS + 1)) {
            fView.topNote = target.pitch + NUMROWS - 1;
        }
        // 3. Sync to Plugin
        setState("pattern_data", fLocalBank.serializeData().c_str());
    }
}

//--Helper function to determine which note is selected
//------------------------------------------------------------------------------
void VoyeSeqUI::updateSelection() {
    fSelectedNoteIndex = -1;
    
    // Use find() to check if the pattern exists before iterating
    auto it = fLocalBank.patterns.find(fCurrentPattern);
    if (it == fLocalBank.patterns.end()) return;

    auto& notes = it->second.notes;
    for (size_t i = 0; i < notes.size(); ++i) {
        if (notes[i].pitch == fView.carrotPitch && notes[i].startTick == fView.carrotTick) {
            fSelectedNoteIndex = (int)i;
            fNoteEditBuffer.pitch = notes[i].pitch;
            fNoteEditBuffer.startTick = notes[i].startTick;
            return;
        }
    }
}

//--Helper function to change new note length depending on existing pattern
//------------------------------------------------------------------------------
uint32_t VoyeSeqUI::constrainNoteLength(uint8_t pitch, uint32_t start, uint32_t requestedLength) {
    uint32_t maxLength = requestedLength;
    
    // 1. Safety check: if the pattern doesn't exist, use default constraints
    if (fLocalBank.patterns.find(fCurrentPattern) == fLocalBank.patterns.end()) {
        uint32_t patternEnd = 768; // Or use fNoteEditBuffer.lengthTicks/MAX_GRID_TICKS
        return (start + maxLength > patternEnd) ? (patternEnd - start) : maxLength;
    }

    // 2. Reference the specific pattern to keep the loop clean
    const auto& currentPattern = fLocalBank.patterns[fCurrentPattern];
    uint32_t patternEnd = currentPattern.lengthTicks;

    for (const auto& note : currentPattern.notes) {
        // Look for notes on the same pitch that start AFTER this one
        if (note.pitch == pitch && note.startTick > start) {
            uint32_t distance = note.startTick - start;
            if (distance < maxLength) {
                maxLength = distance;
            }
        }
    }
    
    // 3. Ensure we don't go past the end of the loop
    if (start + maxLength > patternEnd) {
        maxLength = (start >= patternEnd) ? 0 : (patternEnd - start);
    }

    return maxLength;
}

//--Helper function to resolve collisions when inserting new notes
//------------------------------------------------------------------------------
void VoyeSeqUI::resolveCollisions(uint8_t pitch, uint32_t start, uint32_t length, int skipIndex) {
    // 1. Safety check: if pattern doesn't exist, nothing to collide with
    if (fLocalBank.patterns.find(fCurrentPattern) == fLocalBank.patterns.end()) {
        return;
    }

    uint32_t end = start + length;
    // Get a reference to the note vector for the current pattern
    auto& notes = fLocalBank.patterns[fCurrentPattern].notes;

    for (size_t i = 0; i < notes.size(); ++i) {
        // Skip the note we are currently editing (the "new" note)
        if ((int)i == skipIndex) continue;

        VoyeNote& note = notes[i];
        if (note.pitch != pitch) continue;

        // Case: The new note starts INSIDE an existing note. 
        // We truncate the existing note so it ends where the new one starts.
        if (start > note.startTick && start < (note.startTick + note.lengthTicks)) {
            note.lengthTicks = start - note.startTick;
        }

        // Case: The new note completely covers an existing note or starts at the same time.
        // We delete the existing note because it's now obscured.
        else if (start <= note.startTick && end > note.startTick) {
            notes.erase(notes.begin() + i);
            
            // Adjust the index of the note we are "skipping" if it was after the deleted one
            if (skipIndex > (int)i) {
                // We need to update fSelectedNoteIndex or whatever is tracking the 'new' note
                fSelectedNoteIndex--; 
                skipIndex--;
            }
            
            i--;
        }
    }
}

//--Main keyboard handler for GridEdit mode
//------------------------------------------------------------------------------
bool VoyeSeqUI::handleKeyGridEdit(const KeyboardEvent& event) {
    switch (event.key) {
        case 'p': case 'P': fCurrentEditField = EditField::Pitch;    break;
        case 'v': case 'V': fCurrentEditField = EditField::Velocity; break;
        case 'o': case 'O': fCurrentEditField = EditField::OffVel;   break;
        case 's': case 'S': fCurrentEditField = EditField::Start;    break;
        case 'l': case 'L': fCurrentEditField = EditField::Length;   break;
        case 'u': case 'U': fCurrentEditField = EditField::Units;    break;
        case kKeyEscape:    fCurrentEditField = EditField::None;     break;
    }

    uint32_t stepSize = getQuantizeTicks();
    bool isNavigation = false;

    switch (event.key) {
        case kKeyUp:
            if (fView.carrotPitch < 127) {
                fView.carrotPitch++;
                if (fView.carrotPitch > fView.topNote) fView.topNote = fView.carrotPitch;
            }
            isNavigation = true;
            break;

        case kKeyPageUp:
            fView.carrotPitch = std::min(fView.carrotPitch + 10, 127); 
            if (fView.carrotPitch > fView.topNote) fView.topNote = fView.carrotPitch;
            isNavigation = true;
            break;

        case kKeyDown:
            if (fView.carrotPitch > 0) {
                fView.carrotPitch--;
                if (fView.carrotPitch < (fView.topNote - NUMROWS + 1)) {
                    fView.topNote = fView.carrotPitch + NUMROWS - 1;
                }
            }
            isNavigation = true;
            break;

        case kKeyPageDown:
            fView.carrotPitch = std::max(fView.carrotPitch - 10, 0); 
                if (fView.carrotPitch < (fView.topNote - NUMROWS + 1)) {
                    fView.topNote = fView.carrotPitch + NUMROWS - 1;
                }
            isNavigation = true;
            break;

        case kKeyLeft:
            if (fView.carrotTick >= stepSize) fView.carrotTick -= stepSize;
            else fView.carrotTick = MAX_GRID_TICKS - stepSize;
            isNavigation = true;
            break;

        case kKeyRight:
            fView.carrotTick += stepSize;
            if (fView.carrotTick >= MAX_GRID_TICKS) fView.carrotTick = 0;
            isNavigation = true;
            break;
    }
    
    if (isNavigation) {
        fNoteEditBuffer.pitch = fView.carrotPitch;
        fNoteEditBuffer.startTick = fView.carrotTick;
        updateSelection();
        
        // Safety check: Ensure the pattern exists before trying to access index
        if (fSelectedNoteIndex != -1 && fLocalBank.patterns.count(fCurrentPattern)) {
            auto& note = fLocalBank.patterns[fCurrentPattern].notes[fSelectedNoteIndex];
            auditionNote(note.pitch, note.velocity);
        }
        repaint();
        return true;
    }

    if (event.key == kKeyTab) {
        fEdit = EditState::PatternSelect;
        repaint();
        return true;
    }

    if (event.key == kKeySpace) {
        fNeedsOscToggle = true;
        return true;
    }

    // 3. Insert Logic
    if (event.key == kKeyEnter || event.key == kKeyInsert) {
        if (fSelectedNoteIndex == -1) {
            // Ensure the pattern exists in the bank
            auto& notes = fLocalBank.patterns[fCurrentPattern].notes;

            // 1. Clean the spot
            for (auto it = notes.begin(); it != notes.end(); ) {
                if (it->pitch == fNoteEditBuffer.pitch && it->startTick == fNoteEditBuffer.startTick) {
                    it = notes.erase(it);
                } else {
                    ++it;
                }
            }

            // 2. Truncate & Constrain
            resolveCollisions(fNoteEditBuffer.pitch, fNoteEditBuffer.startTick, fNoteEditBuffer.lengthTicks, -1);
            fNoteEditBuffer.lengthTicks = constrainNoteLength(fNoteEditBuffer.pitch, fNoteEditBuffer.startTick, fNoteEditBuffer.lengthTicks);

            // 3. Commit
            notes.push_back(fNoteEditBuffer);
            auditionNote(fNoteEditBuffer.pitch, fNoteEditBuffer.velocity);
            
            updateSelection();
            // IMPORTANT: Use serializeData() for the pattern_data key
            this->setState("pattern_data", fLocalBank.serializeData().c_str());
        }
        updateSelection();
        repaint();
        return true;
    }

    // 3b. THE DELETE LOGIC
    if (event.key == kKeyDelete || event.key == kKeyBackspace) {
        bool found = false;
        
        if (fLocalBank.patterns.count(fCurrentPattern)) {
            auto& notes = fLocalBank.patterns[fCurrentPattern].notes;

            for (auto it = notes.begin(); it != notes.end(); ) {
                if (it->pitch == fView.carrotPitch && it->startTick == fView.carrotTick) {
                    fNoteEditBuffer.pitch = it->pitch;
                    fNoteEditBuffer.startTick = it->startTick;

                    it = notes.erase(it);
                    found = true;
                } else {
                    ++it;
                }
            }
        }

        if (found) {
            fSelectedNoteIndex = -1;
            updateSelection();
            // Sync the change to the plugin
            setState("pattern_data", fLocalBank.serializeData().c_str());
            repaint();
        }
        return true;
    }
    
    // 4. Value Modification
    int delta = 0;
    if (event.key == '+' || event.key == '=')      delta = 1;
    else if (event.key == '-' || event.key == '_') delta = -1;
    else if (event.key == ']')                     delta = 10;
    else if (event.key == '[')                     delta = -10;

    if (delta != 0 && fCurrentEditField != EditField::None) {
        // 1. Apply the change (applyDelta uses fSelectedNoteIndex to choose between bank and brush)
        applyDelta(delta);

        if (fSelectedNoteIndex != -1) {
            // Grab a reference to the updated note in the bank
            auto& note = fLocalBank.patterns[fCurrentPattern].notes[fSelectedNoteIndex];

            // 2. Sync Carrot: If we shifted Pitch or Start, move the UI cursor to follow it
            fView.carrotPitch = note.pitch;
            fView.carrotTick  = note.startTick;
            
            // Update the tool buffer to match
            fNoteEditBuffer.pitch = note.pitch;
            fNoteEditBuffer.startTick = note.startTick;

            // 3. Sync to DSP: Use the Bank's specific serialization method
            setState("pattern_data", fLocalBank.serializeData().c_str());
        }
        
        // 4. Refresh selection index (important if sorting occurred) and UI
        updateSelection();
        repaint();
        return true;
    }
    return false;
}

//--Main keyboard handler for PatternSelect mode
//------------------------------------------------------------------------------
bool VoyeSeqUI::handleKeyPatternSelect(const KeyboardEvent& event) {
    if (event.key == '+' || event.key == '=' || event.key == kKeyUp) {
        if (fCurrentPattern < 127) fCurrentPattern++;
    }
    else if (event.key == '-' || event.key == '_' || event.key == kKeyDown) {
        if (fCurrentPattern > 0) fCurrentPattern--;
    }
    else if (event.key == ']') {
    fCurrentPattern = std::min((int)fCurrentPattern + 10, 127);
    }
    else if (event.key == '[') {
        fCurrentPattern = std::max((int)fCurrentPattern - 10, 0);
    }
    else if (event.key == kKeyEscape || event.key == kKeyTab) {
        if (fEdit == EditState::CopyPaste) {
            fEdit = EditState::PatternSelect;
        } else {
            fEdit = EditState::GridEdit;   
        }
        repaint();
        return true;
    }
    else if (event.key == kKeyEnter) {
        auto it = fLocalBank.names.find(fCurrentPattern);
        fTempName = (it != fLocalBank.names.end()) ? it->second : "";
        fEdit = EditState::NameEdit;
        repaint();
        return true;
    }
    else if (event.key == 'D' || event.key == 'd') {
        fLocalBank.names.erase(fCurrentPattern);
        fLocalBank.patterns.erase(fCurrentPattern);
        fLocalBank.patterns[fCurrentPattern].lengthTicks = 768;

        setState("pattern_data", fLocalBank.serializeData().c_str());
        setState("pattern_names", fLocalBank.serializeNames().c_str());

        updateSelection();
        repaint();
        return true;
    }
    else if (event.key == 'C' || event.key == 'c') {
        fCopyPattern = fCurrentPattern;
        fEdit = EditState::CopyPaste;
        repaint();
        return true;
    }
    else if ((event.key == 'P' || event.key == 'p') && (fEdit == EditState::CopyPaste)) {
        auto itPat = fLocalBank.patterns.find(fCopyPattern);
        if (itPat != fLocalBank.patterns.end()) {
            fLocalBank.patterns[fCurrentPattern] = itPat->second;
        } else {
            fLocalBank.patterns.erase(fCurrentPattern);
        }

        auto itName = fLocalBank.names.find(fCopyPattern);
        if (itName != fLocalBank.names.end()) {
            fLocalBank.names[fCurrentPattern] = itName->second;
        } else {
            fLocalBank.names.erase(fCurrentPattern);
        }

        if (fLocalBank.patterns.find(fCurrentPattern) == fLocalBank.patterns.end()) {
            fLocalBank.patterns[fCurrentPattern].lengthTicks = 768;
        }

        setState("pattern_data", fLocalBank.serializeData().c_str());
        setState("pattern_names", fLocalBank.serializeNames().c_str());

        updateSelection();
        repaint();
        return true;
    }
    return false;
}

//--Main keyboard handler for NameEdit mode (change pattern names)
//------------------------------------------------------------------------------
bool VoyeSeqUI::handleKeyNameEdit(const KeyboardEvent& event) {
    if (event.key == kKeyEnter) {
        if (!fTempName.empty()) {
            fLocalBank.names[fCurrentPattern] = fTempName;
            setState("pattern_names", fLocalBank.serializeNames().c_str());
        }
        fEdit = EditState::PatternSelect;
        repaint();
        return true;
    }
    if (event.key == kKeyEscape) {
        fEdit = EditState::PatternSelect;
        repaint();
        return true;
    }
    if (event.key == kKeyBackspace) {
        if (!fTempName.empty()) fTempName.pop_back();
        repaint();
        return true;
    }
    if (event.key == kKeyShiftL || event.key == kKeyShiftR ||
        event.key == kKeyControlL || event.key == kKeyControlR ||
        event.key == kKeyAltL || event.key == kKeyAltR) {
        return true;
    }
    // Standard ASCII printable range is roughly 32 (space) to 126 (~)
    if (event.key >= 32 && event.key <= 126) {
        char c = (char)event.key;

        // Check if Shift is held (kModifierShift)
        bool isShiftDown = (event.mod & kModifierShift);

        // If it's a lowercase letter and shift is down, make it uppercase
        if (isShiftDown && c >= 'a' && c <= 'z') {
            c -= 32; // Offset to ASCII Uppercase
        }
        // Handle number-row symbols (Optional: add if you want ! @ # etc.)
        else if (isShiftDown && c >= '0' && c <= '9') {
            // You could map these manually if needed, e.g., '1' -> '!'
        }

        // Validation: Allow the character if it's alphanumeric, space, _, or -
        if (std::isalnum((unsigned char)c) || c == ' ' || c == '_' || c == '-') {
            if (fTempName.length() < 16) {
                fTempName += c;
                repaint();
                return true;
            }
        }
    }
    return false;
}
//--Main keyboard handler onKeyboard
//------------------------------------------------------------------------------
bool VoyeSeqUI::onKeyboard(const KeyboardEvent& event) {
    if (!event.press) return false;
    
    switch (fEdit) {
        case EditState::GridEdit: return handleKeyGridEdit(event); break;
        case EditState::PatternSelect: return handleKeyPatternSelect(event); break;
        case EditState::CopyPaste: return handleKeyPatternSelect(event); break;
        case EditState::NameEdit: return handleKeyNameEdit(event); break;
        // Add future modes here
        default: return false;
    }
}

//--Create UI
//------------------------------------------------------------------------------
UI* createUI() {
    return new VoyeSeqUI();
}

END_NAMESPACE_DISTRHO
