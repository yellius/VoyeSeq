#include "DistrhoUI.hpp"
#include "FiraMono.h"
#include <cstdio>
#include <algorithm>

START_NAMESPACE_DISTRHO

class VoyeSeqUI : public UI {
public:
    VoyeSeqUI() 
        : UI(800, 500), 
          fNote(0), fTranspose(0), fBPM(120), fFont(-1), fPlaying(false),
          fBar(1), fBeat(1), fTick(0), fSigNum(4), fSigDen(4) 
    {}

protected:
    void parameterChanged(uint32_t index, float value) override {
        switch (index) {
            case 0: fNote = (int)value; break;
            case 1: fTranspose = (int)value; break;
            case 2: fBPM = (int)value; break;
            case 3: fPlaying = (value > 0.5f); break;
            case 4: fBar = (int)value; break;
            case 5: fBeat = (int)value; break;
            case 6: fTick = (int)value; break;
            case 7: fSigNum = (int)value; break;
            case 8: fSigDen = (int)value; break;
        }
        repaint();
    }

    bool onKeyboard(const KeyboardEvent& event) override {
        if (!event.press) return false;
        bool changed = false;
        if (event.key == 0xEF52 || event.key == 0xFF52 || event.key == 'w') { 
            fTranspose = std::min(24, fTranspose + 1); changed = true; 
        }
        else if (event.key == 0xEF54 || event.key == 0xFF54 || event.key == 's') { 
            fTranspose = std::max(-24, fTranspose - 1); changed = true; 
        }
        else if (event.key == ' ' || event.key == 'r') { 
            fTranspose = 0; changed = true; 
        }
        if (changed) { setParameterValue(1, (float)fTranspose); repaint(); return true; }
        return false;
    }

    void onNanoDisplay() override {
        const float w = getWidth();
        const float h = getHeight();

        if (fFont == -1) 
            fFont = createFontFromMemory("fira", FiraMono_Medium_ttf, FiraMono_Medium_ttf_len, false);

        // Background
        beginPath(); rect(0.0f, 0.0f, w, h); fillColor(Color(0.0f, 0.0f, 0.55f)); fill();
        // Red Top Bar
        beginPath(); rect(0.0f, 0.0f, w, 22.0f); fillColor(Color(0.8f, 0.0f, 0.0f)); fill();
        
        if (fFont != -1) {
            fontFaceId(fFont); fontSize(14.0f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(Color(1.0f, 1.0f, 0.0f)); text(w / 2.0f, 11.0f, "Note Edit", nullptr);
            drawTransportBar(w);
            drawGrid(w, h);
        }
    }

private:
    void drawTransportBar(float w) {
        Color cyan(0.0f, 1.0f, 1.0f); Color white(1.0f, 1.0f, 1.0f);
        textAlign(ALIGN_LEFT | ALIGN_TOP);

        // 1. POSITION (Now Large and Central)
        fontSize(13.0f); fillColor(cyan); text(15.0f, 30.0f, "POSITION", nullptr);
        fontSize(22.0f); fillColor(white);
        char posBuf[32]; std::sprintf(posBuf, "%03d:%02d:%03d", fBar, fBeat, fTick);
        text(15.0f, 46.0f, posBuf, nullptr);

        // 2. TEMPO / METER
        fontSize(13.0f); fillColor(cyan); 
        text(w * 0.35f, 30.0f, "TEMPO", nullptr); 
        text(w * 0.48f, 30.0f, "METER", nullptr);
        fontSize(18.0f); fillColor(white);
        char bpmBuf[16]; std::sprintf(bpmBuf, "%d BPM", fBPM); text(w * 0.35f, 46.0f, bpmBuf, nullptr);
        char sigBuf[16]; std::sprintf(sigBuf, "%d/%d", fSigNum, fSigDen); text(w * 0.48f, 46.0f, sigBuf, nullptr);

        // 3. NOTE Monitor
        fontSize(13.0f); fillColor(cyan); text(w * 0.65f, 30.0f, "CURRENT NOTE", nullptr);
        fontSize(18.0f); fillColor(white);
        static const char* nms[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        char nBuf[32]; std::sprintf(nBuf, "%s%d (Tr: %+d)", nms[std::abs(fNote)%12], (fNote/12)-1, fTranspose);
        text(w * 0.65f, 46.0f, nBuf, nullptr);

        // 4. STATUS BOX (Top Right)
        beginPath(); rect(w - 75.0f, 2.0f, 60.0f, 18.0f);
        if (fPlaying) { 
            fillColor(Color(0.0f, 0.8f, 0.0f)); fill(); 
            fillColor(Color(0.0f, 0.0f, 0.0f)); fontSize(13.0f); text(w - 68.0f, 4.0f, "RUN", nullptr); 
        } else { 
            fillColor(Color(0.8f, 0.0f, 0.8f)); fill(); 
            fillColor(white); fontSize(13.0f); text(w - 68.0f, 4.0f, "STOP", nullptr); 
        }
    }

    void drawGrid(float w, float h) {
        strokeColor(Color(1.0f, 1.0f, 1.0f)); strokeWidth(1.0f);
        beginPath();
        moveTo(5.0f, 75.0f); lineTo(w - 5.0f, 75.0f);
        moveTo(5.0f, 145.0f); lineTo(w - 5.0f, 145.0f);
        moveTo(70.0f, 145.0f); lineTo(70.0f, h - 5.0f);
        stroke();

        fontSize(12.0f);
        for (int i = 0; i < 14; ++i) {
            int pitch = 67 - i;
            float y = 160.0f + (i * 20.0f);
            fillColor(Color(0.0f, 1.0f, 1.0f));
            static const char* pms[] = {"c ","c#","d ","d#","e ","f ","f#","g ","g#","a ","a#","b "};
            text(10.0f, y, pms[std::abs(pitch) % 12], nullptr);
            strokeColor(Color(0.3f, 0.3f, 0.3f)); beginPath();
            for (float x = 80.0f; x < w - 10.0f; x += 12.0f) { moveTo(x, y + 8.0f); lineTo(x + 2.0f, y + 8.0f); }
            stroke();
        }
    }

    int fNote, fTranspose, fBPM, fFont, fBar, fBeat, fTick, fSigNum, fSigDen;
    bool fPlaying;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoyeSeqUI)
};

UI* createUI() { return new VoyeSeqUI(); }
END_NAMESPACE_DISTRHO
