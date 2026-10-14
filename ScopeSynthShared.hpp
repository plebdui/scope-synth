// What the plugin (sound) and its window share: the sound for the tube, the
// keys clicked in the window, and what the ENV and LFO are doing right now.
#pragma once

#include "DistrhoPlugin.hpp"
#include "SynthEngine.hpp"
#include <atomic>
#include <memory>
#include <mutex>

START_NAMESPACE_DISTRHO

// Single writer (audio thread), single reader (window). The writer never waits.
struct AudioRing {
    static constexpr uint32_t kSize = 1u << 17;  // ~2.7s at 48kHz
    static constexpr uint32_t kMask = kSize - 1;
    float left[kSize] = {};
    float right[kSize] = {};
    std::atomic<uint32_t> written{0};

    void write(const float* l, const float* r, uint32_t n) noexcept
    {
        uint32_t w = written.load(std::memory_order_relaxed);
        for (uint32_t i = 0; i < n; ++i, ++w) {
            left[w & kMask] = l[i];
            right[w & kMask] = r[i];
        }
        written.store(w, std::memory_order_release);
    }
};

// Keys clicked in the window, on their way to the audio thread (one writer, one reader)
struct NoteQueue {
    static constexpr uint32_t kSize = 256;
    std::atomic<uint32_t> head{0}, tail{0};
    uint32_t events[kSize] = {};  // note | velocity << 8 (velocity 0 = let go)

    void push(int note, int velocity) noexcept
    {
        const uint32_t h = head.load(std::memory_order_relaxed);
        if (h - tail.load(std::memory_order_acquire) >= kSize)
            return;  // full: drop it rather than wait
        events[h % kSize] = (uint32_t)(note & 127) | (uint32_t)(velocity & 127) << 8;
        head.store(h + 1, std::memory_order_release);
    }
    template <class F>
    void drain(F f) noexcept
    {
        uint32_t t = tail.load(std::memory_order_relaxed);
        const uint32_t h = head.load(std::memory_order_acquire);
        for (; t != h; ++t)
            f((int)(events[t % kSize] & 127), (int)(events[t % kSize] >> 8 & 127));
        tail.store(t, std::memory_order_release);
    }
};

// What the window shows, written by the audio thread after every block
struct LiveState {
    std::atomic<float> env{0}, lfo{0}, pitch{0};
    std::atomic<int> note{-1}, voices{0};
    std::atomic<double> anim{0};
    std::atomic<uint64_t> keys[2] = {{0}, {0}};  // keys held down, from MIDI or the window
};

class ScopeSynthPlugin : public Plugin {
public:
    ScopeSynthPlugin();
    AudioRing ring;
    NoteQueue uiNotes;
    LiveState live;

protected:
    const char* getLabel() const override { return "ScopeSynth"; }
    const char* getDescription() const override
    {
        return "A polysynth you watch on a CRT: each note traces a curve on the tube, and the key sets how fast the beam goes round.";
    }
    const char* getMaker() const override { return "plebdui"; }
    const char* getHomePage() const override { return "https://github.com/plebdui/scope-synth"; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(1, 0, 1); }

    void initAudioPort(bool input, uint32_t index, AudioPort& port) override;
    void initParameter(uint32_t index, Parameter& parameter) override;
    float getParameterValue(uint32_t index) const override;
    void setParameterValue(uint32_t index, float value) override;
    void activate() override;
    void sampleRateChanged(double newSampleRate) override;
    void initState(uint32_t index, State& state) override;
    void setState(const char* key, const char* value) override;
    void run(const float** inputs, float** outputs, uint32_t frames,
             const MidiEvent* midiEvents, uint32_t midiEventCount) override;

private:
    float fParams[ss::kParamCount];
    ss::Engine fEngine{fParams};
    // the shapes, built off the audio thread and handed over through fShapes.
    // The one before stays alive until the next change, in case the audio
    // thread is still finishing a block with it.
    std::atomic<ss::ShapeSet*> fShapes{nullptr};
    std::unique_ptr<ss::ShapeSet> fShapesCur, fShapesOld;
    std::mutex fShapesLock;
    void handleMidi(const uint8_t* data, uint32_t size);
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScopeSynthPlugin)
};

END_NAMESPACE_DISTRHO
