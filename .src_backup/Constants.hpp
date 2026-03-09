#pragma once
#include <cstdint>

namespace Voye {
    static constexpr uint32_t PPQN = 192;

    // Updated Parameter Indices
    enum Params {
        PARAM_NOTE = 0,     // Incoming MIDI Trigger Monitor
        PARAM_BPM,
        PARAM_TRANSPORT,    // 0 = Stop, 1 = Run
        PARAM_BAR,
        PARAM_BEAT,
        PARAM_TICK,
        PARAM_TICKSPERBEAT,
        PARAM_SIG_NUM,
        PARAM_SIG_DEN,

        PARAM_COUNT
    };
};

static constexpr float    UI_WIDTH      = 800.0f;
static constexpr float    UI_HEIGHT     = 600.0f;
static constexpr float    UI_PADDING    = 8.0f;
static constexpr float    UI_NOTEEDITX  = 20.0f;
static constexpr float    UI_NOTEEDITY  = 20.0f;
static constexpr float    UI_NOTEEDITW  = 370.0f;
static constexpr float    UI_NOTEEDITH  = 130.0f;
static constexpr float    UI_LINESPACE  = 20.0f;
static constexpr float    UI_FONTSIZE   = 18.0f;
static constexpr float    UI_FONTWIDTH  = 11.0f;
static constexpr float    UI_REDBORDERW = 3.0f;
static constexpr float    UI_REDBORDERB = 140.0f;
static constexpr float    UI_MENUH      = 50.0f;
// In Constants.hpp
static constexpr float UI_GRIDX    = 80.0f;
static constexpr float UI_GRIDY    = 140.0f;
static constexpr float UI_GRIDW    = 700.0f;
static constexpr int   NUMROWS     = 20;     // Total visible pitch rows
static constexpr float UI_GRIDH    = NUMROWS * UI_LINESPACE;
static constexpr float UI_NOTESW   = 60.0f;  // Width of the vertical note name column
//static constexpr float    UI_

static constexpr uint32_t MAX_GRID_TICKS = 192 * 4;

struct QuantizeOption {
        const char* name;
        uint32_t ticks;
};

static const QuantizeOption QuantizeTable[] = {
    {"4th",  192}, {"4T",   128},
    {"8th",   96}, {"8T",    64},
    {"16th",  48}, {"16T",   32},
    {"32nd",  24}, {"32T",   16},
    {"64th",  12}, {"64T",    8},
    {"High",   1}
};

static constexpr int QUANTIZE_COUNT = sizeof(QuantizeTable) / sizeof(QuantizeOption);

enum class EditField {
    None,
    Pitch,
    Velocity,
    OffVel,
    Start,
    Length,
    Units // The "Time Units" / Quantize setting
};
