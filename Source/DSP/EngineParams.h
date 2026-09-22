#pragma once

namespace heat::dsp
{
    enum class Mode : int { clean = 0, warm = 1, drive = 2 };
    enum class Detector : int { peak = 0, rms = 1, optical = 2 };
    enum class SidechainSource : int { internal = 0, external = 1 };
    enum class StereoLink : int { linked = 0, partial = 1, dualMono = 2 };
    enum class Quality : int { normal = 0, high = 1, ultra = 2 };
    enum class StereoMode : int { leftRight = 0, midSide = 1, midOnly = 2, sideOnly = 3 };
    enum class MultibandMode : int { off = 0, twoBand = 1, threeBand = 2 };
    enum class IronModel : int { classic = 0, hysteresis = 1 };

    inline constexpr int numModes = 3;
    inline constexpr int numDetectors = 3;

    // Plain-value snapshot of every user-facing control. Units are the
    // units the user sees (dB, ms, Hz, 0..1 for percentages).
    struct EngineParams
    {
        float inputDb   = 0.0f;
        float outputDb  = 0.0f;
        float compress  = 0.5f;     // 0..1
        float attackMs  = 3.0f;
        float releaseMs = 250.0f;
        Mode mode = Mode::warm;
        Detector detector = Detector::peak;
        float tube = 0.0f;          // 0..1
        float iron = 0.0f;          // 0..1
        float mix  = 1.0f;          // 0 = dry, 1 = wet
        float hpfHz = 20.0f;
        SidechainSource scSource = SidechainSource::internal;
        StereoLink link = StereoLink::linked;
        bool scListen = false;
        bool autoMakeup = true;
        bool autoRelease = false;
        Quality quality = Quality::high;
        bool bypass = false;

        // --- 2.1 ------------------------------------------------------------
        StereoMode stereoMode = StereoMode::leftRight;
        float lookaheadMs = 0.0f;   // 0 = off; changes the reported latency
        float scLpfHz = 20000.0f;   // sidechain low-pass (20 kHz = off)
        float scEqHz = 3000.0f;     // sidechain bell
        float scEqDb = 0.0f;        // 0 dB = off
        float scEqQ = 1.0f;
        bool limiter = false;       // output true-peak limiter (adds latency)
        float ceilingDb = -1.0f;
        IronModel ironModel = IronModel::hysteresis;
        MultibandMode multiband = MultibandMode::off;
        float xoverLowHz = 150.0f;
        float xoverHighHz = 2500.0f;
        float bandAmount[3] { 1.0f, 1.0f, 1.0f }; // LOW / MID / HIGH, 0..2 scales COMPRESS
    };
}
