#pragma once
#include "DistrhoUI.hpp"

namespace Voye {
    namespace Colors {
        // Helper to convert 0-255 to 0.0-1.0
        static inline Color fromRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
            return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        }

        const Color Navy     = fromRGB(0x00, 0x00, 0xAA);
        const Color Cyan     = fromRGB(0x00, 0xA0, 0xAA);
        const Color White    = fromRGB(0xFF, 0xFF, 0xFF);
        const Color Magenta  = fromRGB(0xAA, 0x00, 0xAA);
        const Color Green    = fromRGB(0x00, 0xFF, 0x00);
        const Color DarkGray = fromRGB(0x4D, 0x4D, 0x4D);
        const Color Red      = fromRGB(0x55, 0x00, 0x55);
        const Color Crimson  = fromRGB(0xAA, 0x00, 0x00);
        const Color Silver   = fromRGB(0xAA, 0xAA, 0xAA);
        const Color Yellow   = fromRGB(0xAA, 0xAA, 0x00);
    }
    
}
