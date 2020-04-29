# midiplay
MIDI player for the Enterprise 128, and related tools for file conversion.

midiplay: MIDI player written in C and compiled with SDCC, it supports text format instrument definitions, but displays only a blank screen during playback. Real time MIDI input requires ep128emu 2.0.11.2. Detailed documentation is available on the Enterprise Forever wiki

mididisp: assembly version of the player compiled with sjasm 0.39g6, it uses less CPU time and can visualize the MIDI input, but lacks support for text format instrument definitions

midiconv: converts standard MIDI files to a simplified format (single track, fixed 1/50 s tick time) that is playable by midiplay and mididisp, and optionally includes instrument data in binary format. The output can also be raw DAVE register data for processing or playback by other tools

