#include "AudioEngine.hpp"

#include <cmath>
#include <iostream>
#include <vector>

int main() {
    AudioEngine engine;
    double phase = 0.0;

    // 1. Push more than one full ring capacity (65536) to test wraparound cleanly.
    const std::uint64_t initialCursor = engine.latestXYFrame();
    engine.pushSyntheticFrames(70000, phase, 48000);

    if (engine.latestXYFrame() != initialCursor + 70000) {
        std::cerr << "latestXYFrame did not advance by 70000 frames\n";
        return 1;
    }

    // Reading from stale cursor 0 after 70000 writes must report dropped = true.
    const XYInputChunk overflowChunk = engine.readXYSince(initialCursor, 4096);
    if (!overflowChunk.dropped || overflowChunk.samples.empty()) {
        std::cerr << "Ring wraparound did not flag dropped samples\n";
        return 1;
    }

    // 2. Continuous incremental read without drop.
    const std::uint64_t midCursor = engine.latestXYFrame();
    engine.pushSyntheticFrames(512, phase, 48000);
    const XYInputChunk contiguous = engine.readXYSince(midCursor, 4096);
    if (contiguous.dropped || contiguous.samples.size() != 512) {
        std::cerr << "Expected 512 contiguous samples without drop, got "
                  << contiguous.samples.size() << " (dropped=" << contiguous.dropped << ")\n";
        return 1;
    }

    // 3. Snapshot and channel buffers return finite non-zero stereo/mono data.
    const XYInputChunk snap = engine.snapshotXY(1024);
    if (snap.samples.size() != 1024) {
        std::cerr << "snapshotXY returned wrong length\n";
        return 1;
    }
    std::vector<float> mixed, left, right;
    engine.getChannelBuffer(mixed, 1024, 0);
    engine.getChannelBuffer(left, 1024, 1);
    engine.getChannelBuffer(right, 1024, 2);
    if (mixed.size() != 1024 || left.size() != 1024 || right.size() != 1024) {
        std::cerr << "Channel buffer sizes mismatch\n";
        return 1;
    }
    float energy = 0.0f;
    for (size_t i = 0; i < 1024; ++i) {
        const float expectedMix = (left[i] + right[i]) * 0.5f;
        if (std::abs(mixed[i] - expectedMix) > 1e-5f) {
            std::cerr << "Mixed channel does not equal (L+R)/2 at index " << i << "\n";
            return 1;
        }
        energy += left[i] * left[i] + right[i] * right[i];
    }
    if (energy <= 0.1f) {
        std::cerr << "Synthetic audio buffer unexpectedly silent\n";
        return 1;
    }

    return 0;
}
