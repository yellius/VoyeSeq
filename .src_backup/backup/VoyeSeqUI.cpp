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
    
    fSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (fSock < 0) {
      std::fprintf(stderr, "VoyeSeq: Failed to create OSC socket\n");
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
void VoyeSeqUI::stateChanged(const char* key, const char* value) {
    if (std::strcmp(key, "active_pattern") == 0) {
        std::printf("UI: Data -> %s\n", value);
        fLocalPattern.deserialize(value);
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
    
    const VoyeNote& displayNote = (fSelectedNoteIndex != -1) 
                                  ? fLocalPattern.notes[fSelectedNoteIndex] 
                                  : fNoteEditBuffer;
                                  
    VoyeRenderer::drawTransportBar(*this, w, h, fView, fCurrentEditField, displayNote);
    VoyeRenderer::drawGrid(*this, w, h, fView, fLocalPattern, fSelectedNoteIndex);
    }
}

//--Send MIDI note for Audition (when selecting note, inserting note)
//------------------------------------------------------------------------------
void VoyeSeqUI::auditionNote(uint8_t pitch, uint8_t velocity) {
    //if (pitch > 127) return;
    //this->getPlugin().previewNote(pitch, velocity);
    sendNote(15, pitch, velocity);
}

//--Helper function to cycle through editmodes (tab key)
//------------------------------------------------------------------------------
EditMode VoyeSeqUI::getNextMode(EditMode current) {
    int next = static_cast<int>(current) + 1;
    if (next >= static_cast<int>(EditMode::Count)) {
        return EditMode::GridEdit;
    }
    return static_cast<EditMode>(next);
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
                       ? fLocalPattern.notes[fSelectedNoteIndex] 
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
        setState("active_pattern", fLocalPattern.serialize().c_str());
    }
}

//--Helper function to determine which note is selected
//------------------------------------------------------------------------------
void VoyeSeqUI::updateSelection() {
    fSelectedNoteIndex = -1;
    for (size_t i = 0; i < fLocalPattern.notes.size(); ++i) {
        if (fLocalPattern.notes[i].pitch == fView.carrotPitch && 
            fLocalPattern.notes[i].startTick == fView.carrotTick) {
            fSelectedNoteIndex = (int)i;
            
            // DO NOT sync the whole note! 
            // Only sync the buffer's position so it stays under the carrot
            fNoteEditBuffer.pitch = fLocalPattern.notes[i].pitch;
            fNoteEditBuffer.startTick = fLocalPattern.notes[i].startTick;
            
            // This leaves fNoteEditBuffer.lengthTicks and .velocity 
            // untouched, preserving your "tool settings".
            return;
        }
    }
}

//--Helper function to change new note length depending on existing pattern
//------------------------------------------------------------------------------
uint32_t VoyeSeqUI::constrainNoteLength(uint8_t pitch, uint32_t start, uint32_t requestedLength) {
    uint32_t maxLength = requestedLength;
    uint32_t patternEnd = MAX_GRID_TICKS;

    for (const auto& note : fLocalPattern.notes) {
        // Look for notes on the same pitch that start AFTER this one
        if (note.pitch == pitch && note.startTick > start) {
            uint32_t distance = note.startTick - start;
            if (distance < maxLength) {
                maxLength = distance;
            }
        }
    }
    
    // Also ensure we don't go past the end of the loop/grid
    if (start + maxLength > patternEnd) {
        maxLength = patternEnd - start;
    }

    return maxLength;
}

//--Helper function to resolve collisions when inserting new notes
//------------------------------------------------------------------------------
void VoyeSeqUI::resolveCollisions(uint8_t pitch, uint32_t start, uint32_t length, int skipIndex) {
    uint32_t end = start + length;

    for (size_t i = 0; i < fLocalPattern.notes.size(); ++i) {
        // Skip the note we are currently editing
        if ((int)i == skipIndex) continue;

        VoyeNote& note = fLocalPattern.notes[i];
        if (note.pitch != pitch) continue;

        // Case: The new note starts INSIDE an existing note. 
        // We truncate the existing note so it ends where the new one starts.
        if (start > note.startTick && start < (note.startTick + note.lengthTicks)) {
            note.lengthTicks = start - note.startTick;
        }

        // Case: The new note completely covers an existing note or starts at the same time.
        // We delete the existing note because it's now obscured.
        else if (start <= note.startTick && end > note.startTick) {
            fLocalPattern.notes.erase(fLocalPattern.notes.begin() + i);
            
            // Adjust the index and selection if we deleted a note before our selection
            if ((int)i < skipIndex) fSelectedNoteIndex--;
            i--; // Step back to account for the removal
        }
    }
}

//--Main keyboard handler for GridEdit mode
//------------------------------------------------------------------------------
bool VoyeSeqUI::handleKeyGridEdit(const KeyboardEvent& event) {
    if (!event.press) return false;

    // 1. Hotkeys for Edit Matrix
    switch (event.key) {
        case 'p': case 'P': fCurrentEditField = EditField::Pitch;    break;
        case 'v': case 'V': fCurrentEditField = EditField::Velocity; break;
        case 'o': case 'O': fCurrentEditField = EditField::OffVel;   break;
        case 's': case 'S': fCurrentEditField = EditField::Start;    break;
        case 'l': case 'L': fCurrentEditField = EditField::Length;   break;
        case 'u': case 'U': fCurrentEditField = EditField::Units;    break;
        case kKeyEscape:    fCurrentEditField = EditField::None;     break;
    }

    // 2. Carrot Navigation (Arrows)
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
        if (fSelectedNoteIndex != -1) {
            auto& note = fLocalPattern.notes[fSelectedNoteIndex];
            auditionNote(note.pitch, note.velocity);
        }
        repaint();
        return true;
    }

    // 3. Insert Logic
    if (event.key == kKeyEnter || event.key == kKeyInsert) {
        if (fSelectedNoteIndex == -1) {
                // 1. Clean the spot: Remove any note starting exactly here
                for (auto it = fLocalPattern.notes.begin(); it != fLocalPattern.notes.end(); ) {
                    if (it->pitch == fNoteEditBuffer.pitch && it->startTick == fNoteEditBuffer.startTick) {
                        it = fLocalPattern.notes.erase(it);
                    } else {
                        ++it;
                    }
                }

                // 2. Truncate neighbors: If we are landing in the middle of a long note, cut it.
                resolveCollisions(fNoteEditBuffer.pitch, fNoteEditBuffer.startTick, fNoteEditBuffer.lengthTicks, fSelectedNoteIndex);

                // 3. Final Constraint: Ensure our new note doesn't overlap the NEXT neighbor
                fNoteEditBuffer.lengthTicks = constrainNoteLength(
                    fNoteEditBuffer.pitch, fNoteEditBuffer.startTick, fNoteEditBuffer.lengthTicks);

                // 4. Commit
                fLocalPattern.notes.push_back(fNoteEditBuffer);
                auditionNote(fNoteEditBuffer.pitch, fNoteEditBuffer.velocity);
                updateSelection();
                this->setState("active_pattern", fLocalPattern.serialize().c_str());
            }
            updateSelection();
            repaint();
            return true;
        }

    // 3b. THE DELETE LOGIC
    if (event.key == kKeyDelete || event.key == kKeyBackspace) {
        bool found = false;
        
        // Search for a note that matches the current carrot position
        // We use an iterator so we can safely erase while looping
        for (auto it = fLocalPattern.notes.begin(); it != fLocalPattern.notes.end(); ) {
            if (it->pitch == fView.carrotPitch && it->startTick == fView.carrotTick) {
                //fNoteEditBuffer = *it; // <- this would copy velocity etc as well
                fNoteEditBuffer.pitch = it->pitch;
                fNoteEditBuffer.startTick = it->startTick;

                it = fLocalPattern.notes.erase(it);
                //fView.carrotVisible = true;
                found = true;
            } else {
                ++it;
            }
        }

        if (found) {
            fSelectedNoteIndex = -1;
            updateSelection();
            setState("active_pattern", fLocalPattern.serialize().c_str());
            updateSelection();
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
        if (fSelectedNoteIndex != -1) {
            // Modify the REAL note in the pattern
            // We use a temporary pointer for clarity
            VoyeNote& note = fLocalPattern.notes[fSelectedNoteIndex];
            
            // We need a version of applyDelta that acts on a specific note
            applyDelta(delta);
            
            // IMMEDIATELY sync to DSP
            setState("active_pattern", fLocalPattern.serialize().c_str());
            updateSelection();
        } else {
            // Only modifying the "phantom" buffer
            applyDelta(delta);
        }
        
        // Ensure carrot follows if we changed Pitch or Start via delta
        if (fSelectedNoteIndex != -1) {
            // fNoteEditBuffer = fLocalPattern.notes[fSelectedNoteIndex]; // <- this copies velocity etc
            fNoteEditBuffer.pitch = fLocalPattern.notes[fSelectedNoteIndex].pitch;
            fNoteEditBuffer.startTick = fLocalPattern.notes[fSelectedNoteIndex].startTick;
            fView.carrotPitch = fLocalPattern.notes[fSelectedNoteIndex].pitch;
            fView.carrotTick = fLocalPattern.notes[fSelectedNoteIndex].startTick;
        }
        
        updateSelection();
        repaint();
        return true;
    }
    
    if (event.key == kKeySpace) {
        fNeedsOscToggle = true;
        return true;
    }
    return true;
}

//--Main keyboard handler for GridEdit mode
//------------------------------------------------------------------------------
bool VoyeSeqUI::handleKeyNameEdit(const KeyboardEvent& event) {
    if (!event.press) return false;

    return true;
}


//--Main keyboard handler onKeyboard
//------------------------------------------------------------------------------
bool VoyeSeqUI::onKeyboard(const KeyboardEvent& event) {
    if (!event.press) return false;
    
    if (event.key == kKeyTab) {
      fEditMode = getNextMode(fEditMode);
      return true;
    }

    // The "Traffic Controller"
    switch (fEditMode) {
        case EditMode::GridEdit: return handleKeyGridEdit(event);
        case EditMode::NameEdit: return handleKeyNameEdit(event);
        // Add future modes here
        // case EditMode::PatternSelect: return handlePatternSelectKey(event);
        default: return false;
    }
}

//--Create UI
//------------------------------------------------------------------------------
UI* createUI() {
    return new VoyeSeqUI();
}

END_NAMESPACE_DISTRHO
