#include "VoyeRenderer.hpp"
#include "VoyeUIUtils.hpp"

START_NAMESPACE_DISTRHO

const char* VoyeRenderer::getPitchName(uint8_t midiNote) {
    static const char* noteNames[] = {
        "C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B "
    };

    int octave = (int)(midiNote / 12) - 1;
    int nameIdx = midiNote % 12;

    static char buf[16];
    std::sprintf(buf, "%s %d", noteNames[nameIdx], octave);
    return buf;
}

const char* VoyeRenderer::getGridLabel(uint8_t midiNote) {
    static const char* gridNames[] = {
        "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b"
    };
    return gridNames[midiNote % 12];
}

void VoyeRenderer::drawTransportBar(UI& ui, float w, float h, const GridViewState& view, EditField currentField, const VoyeNote& editBuffer) {
    ui.textAlign(UI::ALIGN_LEFT | UI::ALIGN_TOP);
    float padding = UI_PADDING;
    const auto& t = view.transport;

    // --- 1. THE HEAVY RED BORDERS ---
    ui.strokeColor(Voye::Colors::Red);
    ui.strokeWidth(UI_REDBORDERW);
    ui.beginPath();
    ui.rect(padding, padding, w - (padding * 2.0f), h - (padding * 2.0f));
    ui.moveTo(padding, UI_NOTEEDITH); ui.lineTo(w - padding, UI_NOTEEDITH);
    ui.moveTo(UI_NOTEEDITW, padding); ui.lineTo(UI_NOTEEDITW, UI_NOTEEDITH);
    ui.moveTo(padding, h - padding - UI_MENUH); ui.lineTo(w - padding, h - padding - UI_MENUH);
    ui.stroke();

    // --- 2. TRANSPORT STATUS ---
    ui.fontSize(UI_FONTSIZE);
    float textX = UI_NOTEEDITX;
    float textY = UI_NOTEEDITY;
    float charWidth = UI_FONTWIDTH;
    float linSpc = UI_LINESPACE;
    float boxP = 2.0f;
    float boxW = charWidth * 5.0f + (2.0f * boxP);
    float boxH = linSpc + (2.0f * boxP) - 3.0f;

    if (t.playing) {
        ui.beginPath();
        ui.rect(textX - boxP, textY - boxP, boxW, boxH);
        ui.fillColor(Voye::Colors::Green); ui.fill();
        ui.fillColor(Voye::Colors::Navy);
        ui.text(textX + (boxW - (3.0f * charWidth)) / 2.0f, textY, "RUN", nullptr);
    } else {
        ui.beginPath();
        ui.rect(textX - boxP, textY - boxP, boxW, boxH);
        ui.fillColor(Voye::Colors::Magenta); ui.fill();
        ui.fillColor(Voye::Colors::White);
        ui.text(textX + (boxW - (4.0f * charWidth)) / 2.0f, textY, "STOP", nullptr);
    }

    // Helper for Highlighted Labels
    auto drawLabel = [&](float x, float y, const char* label, EditField field) {
        Rectangle<float> bounds;
        ui.textBounds(x, y, label, nullptr, bounds);
        float tw = bounds.getWidth();

        if (currentField == field) {
            ui.beginPath();
            ui.rect(x - 2.0f, y - 1.0f, tw + 4.0f, UI_LINESPACE - 2.0f);
            ui.fillColor(Voye::Colors::Magenta);
            ui.fill();
            ui.fillColor(Voye::Colors::White); 
        } else {
            ui.fillColor(Voye::Colors::Cyan);
        }
        ui.text(x, y, label, nullptr);
    };

    // Key metrics
    ui.fillColor(Voye::Colors::Silver);
    char buf[32];
    std::sprintf(buf, "%d:%d", t.bar, t.tick); 
    ui.text(textX + boxW + 2 * charWidth, textY, buf, nullptr);

    float text2X = UI_NOTEEDITW / 2.0f;
    std::sprintf(buf, "BPM %d", (int)t.bpm);     
    ui.text(text2X, textY, buf, nullptr);

    ui.textAlign(UI::ALIGN_RIGHT | UI::ALIGN_TOP);
    std::sprintf(buf, "%d/%d", t.sigNum, t.sigDen);
    ui.text(UI_NOTEEDITW - padding, textY, buf, nullptr);
    ui.textAlign(UI::ALIGN_LEFT | UI::ALIGN_TOP);
    
    // --- NOTE EDIT MATRIX ---
    textY += UI_LINESPACE; 
    float colX = (text2X / 2.0f) + (charWidth * 3.0f); 
    float fineX = charWidth * 5.0f;

    ui.fillColor(Voye::Colors::Cyan);
    ui.text(textX, textY, "CURRENT NOTE", nullptr);
    drawLabel(text2X, textY, "Units:", EditField::Units);

    ui.fillColor(Voye::Colors::Silver);
    ui.textAlign(UI::ALIGN_RIGHT | UI::ALIGN_TOP);

    const char* qLabel = "???"; // determine units, default is unknown
    for (const auto& option : QuantizeTable) {
        if (option.ticks == view.quantizeTicks) {
            qLabel = option.name;
            break; } }
    ui.text(text2X + colX, textY, qLabel, nullptr);
    ui.text(text2X + colX + fineX, textY, "Fine", nullptr);
    
    textY += linSpc;
    ui.textAlign(UI::ALIGN_LEFT | UI::ALIGN_TOP);
    drawLabel(textX, textY, "Pitch:", EditField::Pitch);
    drawLabel(text2X, textY, "Start:", EditField::Start);

    ui.fillColor(Voye::Colors::Silver);
    ui.textAlign(UI::ALIGN_RIGHT | UI::ALIGN_TOP);
    ui.text(textX + colX, textY, getPitchName(editBuffer.pitch), nullptr); 
    
    std::sprintf(buf, "%u", (editBuffer.startTick / view.quantizeTicks) + 1);
    ui.text(text2X + colX, textY, buf, nullptr);
    std::sprintf(buf, "%u", (editBuffer.startTick % view.quantizeTicks));
    ui.text(text2X + colX + fineX, textY, buf, nullptr);

    textY += linSpc;
    ui.textAlign(UI::ALIGN_LEFT | UI::ALIGN_TOP);
    drawLabel(textX, textY, "Velocity:", EditField::Velocity);
    drawLabel(text2X, textY, "Length:", EditField::Length);

    ui.fillColor(Voye::Colors::Silver);
    ui.textAlign(UI::ALIGN_RIGHT | UI::ALIGN_TOP);
    std::sprintf(buf, "%d", editBuffer.velocity);
    ui.text(textX + colX, textY, buf, nullptr);

    std::sprintf(buf, "%u", (editBuffer.lengthTicks / view.quantizeTicks) + 1);
    ui.text(text2X + colX, textY, buf, nullptr);
    std::sprintf(buf, "%d", editBuffer.lengthTicks % view.quantizeTicks);
    ui.text(text2X + colX + fineX, textY, buf, nullptr);

    textY += linSpc;
    ui.textAlign(UI::ALIGN_LEFT | UI::ALIGN_TOP);
    drawLabel(textX, textY, "Off Vel:", EditField::OffVel);
    ui.fillColor(Voye::Colors::Cyan);
    ui.text(text2X, textY, "MIDI:", nullptr);

    ui.fillColor(Voye::Colors::Silver);
    ui.textAlign(UI::ALIGN_RIGHT | UI::ALIGN_TOP);
    std::sprintf(buf, "%d", editBuffer.off_velocity);
    ui.text(textX + colX, textY, buf, nullptr);
    std::sprintf(buf, "%d", editBuffer.pitch);
    ui.text(text2X + colX, textY, buf, nullptr);
}

void VoyeRenderer::drawGrid(UI& ui, float w, float h, const GridViewState& view, const Pattern& pattern, int selectedNoteIndex) {
    float startX = UI_GRIDX;
    float startY = UI_GRIDY;
    
    ui.fontSize(UI_FONTSIZE);
    
    // --- 1. Draw Background Grid ---
    for (int i = 0; i < NUMROWS; ++i) {
        uint8_t currentPitch = view.topNote - i;
        float rowY = startY + (i * UI_LINESPACE);
        ui.textAlign(UI::ALIGN_LEFT | UI::ALIGN_TOP);
        
        float labelX = startX - UI_NOTESW + 10.0f; 
        if (currentPitch % 12 == 0) {
            ui.fillColor(Voye::Colors::White);
            ui.text(labelX, rowY, "C", nullptr);
        } else {
            ui.fillColor(Voye::Colors::Cyan);
            ui.text(labelX, rowY, getGridLabel(currentPitch), nullptr); 
        }

        for (int d = 0; d < 64; ++d) {
            float cellX = startX + (d * UI_FONTWIDTH);
            ui.fillColor(Voye::Colors::Silver);
            if (d % 16 == 0) {
                ui.text(cellX, rowY, "■", nullptr); 
            } else {
                ui.beginPath();
                ui.rect(cellX + (UI_FONTWIDTH / 2.0f) - 1.0f, rowY + (UI_LINESPACE / 2.0f) - 1.0f, 2.0f, 2.0f);
                ui.fill();
            }
        }
    }
    
    // --- 2. Draw Pattern Notes ---
    for (size_t i = 0; i < pattern.notes.size(); ++i) {
        bool isSelected = ((int)i == selectedNoteIndex);
        // We only show the "blinking block" on the note if it's the one currently selected
        bool showCarrot = isSelected && view.carrotVisible;
        
        drawNote(ui, pattern.notes[i].pitch, pattern.notes[i].startTick, 
                 pattern.notes[i].lengthTicks, isSelected, view.topNote, showCarrot, false, pattern);
    }
    
    // --- 3. Draw the Blinking Prompt (Only if no note is selected) ---
    if (selectedNoteIndex == -1 && view.carrotVisible) {
        // Draw as a specialized "prompt" (e.g., a blinking underscore or hollow box)
        drawNote(ui, view.carrotPitch, view.carrotTick, view.quantizeTicks, 
                 true, view.topNote, true, true, pattern);
    }
    
    // --- DRAW THE PLAYHEAD ---
    if (view.transport.playing) { 
        int relativeBar = (view.transport.bar - 1) % 4;
        double absoluteTicks = (double)(relativeBar * view.transport.sigNum + (view.transport.beat - 1)) 
                             * view.transport.tpb 
                             + view.transport.tick;
        double totalLoopTicks = view.transport.sigNum * view.transport.tpb;

        // 3. Map to UI_GRIDW
        float playheadX = UI_GRIDX + (float)(absoluteTicks / totalLoopTicks) * UI_GRIDW;

        // 3. Draw the line as before
        ui.beginPath();
        ui.strokeWidth(1.5f);
        ui.strokeColor(Voye::Colors::Green);
        ui.moveTo(playheadX, UI_GRIDY);
        ui.lineTo(playheadX, UI_GRIDY + (NUMROWS * UI_LINESPACE));
        ui.stroke();
    }
}

void VoyeRenderer::drawNote(UI& ui, uint8_t pitch, uint32_t start, uint32_t length, 
                            bool isSelected, uint8_t topNote, bool carrotVisible, 
                            bool isPrompt, const Pattern& pattern) {
    if (pitch > topNote || pitch <= (topNote - NUMROWS)) return;

    float tickToPix = UI_FONTWIDTH / 12.0f;
    float x = UI_GRIDX + (start * tickToPix);
    float y = UI_GRIDY + ((topNote - pitch) * UI_LINESPACE);
    float w = length * tickToPix;
    float h = UI_LINESPACE - 4.0f;

    if (isPrompt) {
        // --- SPG Style Blinking Underscore ---
        ui.beginPath();
        ui.fillColor(Voye::Colors::Yellow);
        ui.rect(x, y + UI_LINESPACE - 4.0f, UI_FONTWIDTH, 3.0f);
        ui.fill();
    } else {
        // --- Real Note (Red or Yellow) ---
        ui.beginPath();
        ui.rect(x, y + 2.0f, w - 2.0f, h);
        ui.fillColor(isSelected ? Voye::Colors::Yellow : Voye::Colors::Crimson);
        ui.fill();

        // --- Legato Check: Wavy Equal Sign (≈) ---
        bool isTruncated = false;
        for (const auto& other : pattern.notes) {
            // Check if another note on the same pitch starts EXACTLY where this ends
            if (other.pitch == pitch && other.startTick == (start + length)) {
                isTruncated = true;
                break;
            }
        }

        if (isTruncated) {
            // We use Black or Navy for the symbol to contrast with Red/Yellow
            ui.fillColor(Voye::Colors::Navy);
            ui.fontSize(UI_FONTSIZE * 1.1f); // Slightly smaller for the symbol
            ui.textAlign(UI::ALIGN_RIGHT | UI::ALIGN_MIDDLE);
            
            // Draw slightly inside the right edge of the note block
            ui.text(x + w - 1.0f, y + (UI_LINESPACE / 2.0f), "≈", nullptr);
        }

        // If selected and blinking, draw the "active" block indicator
        if (isSelected && carrotVisible) {
            ui.fillColor(Voye::Colors::Navy);
            ui.textAlign(UI::ALIGN_LEFT | UI::ALIGN_TOP);
            ui.text(x, y, "■", nullptr);
        }
    }
}

END_NAMESPACE_DISTRHO
