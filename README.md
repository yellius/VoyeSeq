# VoyeSeq

A lightweight, pattern-based MIDI sequencer plugin built with the [DISTRHO Plugin Framework (DPF)](https://distrho.github.io/DPF/). 

VoyeSeq mimics the Voyetra Sequencer Plus Gold workflow to create patterns, which can be triggered using MIDI notes from the host.

## Features
- **Pattern Bank:** 128 patterns (0-127).
- **Hex Serialization:** Compressed state strings for fast loading/saving.
- **Keyboard-Driven Workflow:** Optimized for speed without heavy mouse reliance.
- **Cross-Platform:** Built with DPF to run as JACK standalone, LV2, VST3, or CLAP.

### Prerequisites
- Build tools (gcc, make, cmake)
- Development headers for X11, Cairo, or OpenGL (depending on your DPF backend)

### Installation
```bash
# Clone the repository
git clone --recursive [https://github.com/YOUR_USERNAME/VoyeSeq.git](https://github.com/YOUR_USERNAME/VoyeSeq.git)
cd VoyeSeq

# Build the project
mkdir build && cd build
cmake ..
make
