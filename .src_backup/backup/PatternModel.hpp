#pragma once
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <map>

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

enum class EditMode {
    GridEdit,
    NameEdit,
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
        ss << lengthTicks << "|";
        for (const auto& n : notes) {
            ss << (int)n.pitch << ":" 
               << n.startTick << ":" 
               << n.lengthTicks << ":" 
               << (int)n.velocity << ":" 
               << (int)n.off_velocity << ";";
        }
        return ss.str();
    }

    void deserialize(const std::string& data) {
        notes.clear();
        if (data.empty()) return;
        std::stringstream ss(data);
        std::string segment;

        if (std::getline(ss, segment, '|')) lengthTicks = std::stoul(segment);

        while (std::getline(ss, segment, ';')) {
            if (segment.empty()) continue;
            VoyeNote n;
            std::stringstream nss(segment);
            std::string val;
            
            // Order: pitch -> start -> length -> vel -> off_vel
            if (std::getline(nss, val, ':')) n.pitch = (uint8_t)std::stoi(val);
            if (std::getline(nss, val, ':')) n.startTick = std::stoul(val);
            if (std::getline(nss, val, ':')) n.lengthTicks = std::stoul(val);
            if (std::getline(nss, val, ':')) n.velocity = (uint8_t)std::stoi(val);
            if (std::getline(nss, val, ':')) n.off_velocity = (uint8_t)std::stoi(val);
            
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
            ss << "P" << (int)note << "{" << pattern.serialize() << "};";
        }
        return ss.str();
    }

    void deserializeData(const std::string& data) {
        patterns.clear();
        if (data.empty()) return;
        std::stringstream ss(data);
        std::string segment;
        while (std::getline(ss, segment, ';')) {
            size_t braceStart = segment.find('{');
            size_t braceEnd = segment.find('}');
            if (braceStart != std::string::npos && braceEnd != std::string::npos) {
                uint8_t note = (uint8_t)std::stoi(segment.substr(1, braceStart - 1));
                std::string pData = segment.substr(braceStart + 1, braceEnd - braceStart - 1);
                patterns[note].deserialize(pData);
            }
        }
    }

    // Serializes names: "P064:Lead;P065:Bass;"
    std::string serializeNames() const {
        std::stringstream ss;
        for (const auto& [note, name] : names) {
            ss << "P" << (int)note << ":" << name << ";";
        }
        return ss.str();
    }

    void deserializeNames(const std::string& data) {
        names.clear();
        std::stringstream ss(data);
        std::string segment;
        while (std::getline(ss, segment, ';')) {
            size_t colon = segment.find(':');
            if (colon != std::string::npos) {
                uint8_t note = (uint8_t)std::stoi(segment.substr(1, colon - 1));
                names[note] = segment.substr(colon + 1);
            }
        }
    }
};
