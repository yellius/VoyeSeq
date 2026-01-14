#pragma once
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <map>
#include <iomanip>

struct TransportState {
    float bpm = 120.0f;
    int   bar = 1;
    int   beat = 1;
    int   tick = 0;
    double   tpb = 192.0f;
    int   sigNum = 4;
    int   sigDen = 4;
    bool  playing = false;
};

enum class EditState {
    GridEdit,
    PatternSelect,
    NameEdit,
    CopyPaste,
    Count
};

struct GridViewState {
    uint8_t  topNote = 72;      // Vertical scroll position
    uint8_t  carrotPitch = 60;  // Cursor MIDI pitch
    uint32_t carrotTick = 0;    // Cursor horizontal position
    uint32_t quantizeTicks = 48; // Current grid snapping (e.g., 192/4)
    bool     carrotVisible = true; // For blinking logic
    bool     playing = false;
    
    TransportState transport; 
};

struct VoyeNote {
    uint8_t  pitch;
    uint32_t startTick;
    uint32_t lengthTicks;
    uint8_t  velocity;
    uint8_t  off_velocity; // Added for release expression
};

struct Pattern {
    uint32_t lengthTicks = 768;
    std::vector<VoyeNote> notes;

    std::string serialize() const {
        std::stringstream ss;
        // Keep lengthTicks as decimal + pipe for readability, or hex too
        ss << std::hex << lengthTicks << "|"; 
        
        for (const auto& n : notes) {
            ss << std::setfill('0') << std::setw(2) << std::hex << (int)n.pitch
               << std::setfill('0') << std::setw(4) << std::hex << (uint32_t)n.startTick
               << std::setfill('0') << std::setw(4) << std::hex << (uint32_t)n.lengthTicks
               << std::setfill('0') << std::setw(2) << std::hex << (int)n.velocity
               << std::setfill('0') << std::setw(2) << std::hex << (int)n.off_velocity;
        }
        return ss.str();
    }

    void deserialize(const std::string& data) {
        notes.clear();
        if (data.empty()) return;

        size_t pipePos = data.find('|');
        if (pipePos == std::string::npos) return;

        // 1. Get length
        lengthTicks = std::stoul(data.substr(0, pipePos), nullptr, 16);

        // 2. Parse fixed-width blocks (14 chars per note)
        std::string noteData = data.substr(pipePos + 1);
        for (size_t i = 0; i + 13 < noteData.length(); i += 14) {
            VoyeNote n;
            n.pitch        = (uint8_t) std::stoul(noteData.substr(i, 2),    nullptr, 16);
            n.startTick    = (uint32_t)std::stoul(noteData.substr(i + 2, 4), nullptr, 16);
            n.lengthTicks  = (uint32_t)std::stoul(noteData.substr(i + 6, 4), nullptr, 16);
            n.velocity     = (uint8_t) std::stoul(noteData.substr(i + 10, 2), nullptr, 16);
            n.off_velocity = (uint8_t) std::stoul(noteData.substr(i + 12, 2), nullptr, 16);
            notes.push_back(n);
        }
    }
};

struct PatternBank {
    // MIDI Note -> Pattern data
    std::map<uint8_t, Pattern> patterns;
    // MIDI Note -> Custom Name
    std::map<uint8_t, std::string> names;

    // Serializes only non-empty patterns: "P064{data};P065{data};"
    std::string serializeData() const {
        std::stringstream ss;
        for (const auto& [note, pattern] : patterns) {
            if (pattern.notes.empty()) continue; 
            
            // Use 2-digit hex for the trigger note (e.g., P40 for note 64)
            ss << "P" << std::setfill('0') << std::setw(2) << std::hex << (int)note 
               << "{" << pattern.serialize() << "}#"; 
        }
        return ss.str();
    }

    void deserializeData(const std::string& data) {
        std::map<uint8_t, Pattern> newPatterns;
        newPatterns[64].lengthTicks = 768; // Default fallback

        if (!data.empty()) {
            std::stringstream ss(data);
            std::string segment;
            
            while (std::getline(ss, segment, '#')) {
                if (segment.empty()) continue;

                size_t braceStart = segment.find('{');
                size_t braceEnd = segment.find('}');
                
                if (braceStart != std::string::npos && braceEnd != std::string::npos) {
                    try {
                        // Parse the note number as Hex
                        std::string hexNote = segment.substr(1, braceStart - 1);
                        uint8_t note = (uint8_t)std::stoul(hexNote, nullptr, 16);
                        
                        std::string pData = segment.substr(braceStart + 1, braceEnd - braceStart - 1);
                        newPatterns[note].deserialize(pData);
                    } catch (...) { continue; }
                }
            }
        }
        patterns.swap(newPatterns);
    }
    
    std::string serializeNames() const {
        std::stringstream ss;
        for (const auto& [note, name] : names) {
            if (name.empty()) continue;
            // Use # to match the Bank Data format
            ss << "P" << (int)note << ":" << name << "#"; 
        }
        return ss.str();
    }

    void deserializeNames(const std::string& data) {
        std::map<uint8_t, std::string> newNames;
        if (!data.empty()) {
            std::stringstream ss(data);
            std::string segment;
            // Match the # separator
            while (std::getline(ss, segment, '#')) { 
                if (segment.empty()) continue;
                size_t colon = segment.find(':');
                if (colon != std::string::npos) {
                    try {
                        uint8_t note = (uint8_t)std::stoi(segment.substr(1, colon - 1));
                        newNames[note] = segment.substr(colon + 1);
                    } catch (...) { continue; }
                }
            }
        }
        names.swap(newNames);
    }
};
