#pragma once

#include <vector>
#include "Params.h"

namespace pinkpresets
{
struct PV
{
    const char* id;
    float value;
};

struct FactoryPreset
{
    const char* name;
    std::vector<PV> values;
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> list = []
    {
        std::vector<FactoryPreset> v;

        // 1. Init : toutes les valeurs par défaut de Params.h
        v.push_back({ "Init", {} });

        // 2. Glass Pluck : corde cristalline pincée, légère instabilité analogique
        v.push_back({ "Glass Pluck", {
            { Params::pitch,       0.0f },
            { Params::feedback,    0.90f },
            { Params::pluck,       0.45f },
            { Params::inharm,      0.08f },
            { Params::inputCutoff, 12000.0f },
            { Params::loopCutoff,  6500.0f },
            { Params::jitter,      0.12f },
            { Params::crossFb,     0.15f },
            { Params::dryWet,      0.55f },
            { Params::wetGain,     1.10f },
            { Params::lfo1Rate,    0.35f },
            { Params::lfo1Shape,   0.0f },   // Sine
            { Params::m1_drift,    0.15f }
        } });

        // 3. Metallic Bell : cloche inharmonique brillante, large diffusion stéréo
        v.push_back({ "Metallic Bell", {
            { Params::pitch,       12.0f },
            { Params::feedback,    0.93f },
            { Params::pluck,       0.25f },
            { Params::inharm,      0.72f },
            { Params::inputCutoff, 14500.0f },
            { Params::loopCutoff,  9500.0f },
            { Params::crossFb,     0.35f },
            { Params::jitter,      0.08f },
            { Params::dryWet,      0.60f },
            { Params::wetGain,     1.15f },
            { Params::lfo1Rate,    0.22f },
            { Params::lfo1Shape,   1.0f },   // Tri
            { Params::m1_inharm,   0.10f }
        } });

        // 4. Hollow Tube : polarité négative (harmoniques impaires type tuyau fermé)
        v.push_back({ "Hollow Tube", {
            { Params::polarity,    1.0f },   // Negative
            { Params::pitch,      -12.0f },
            { Params::feedback,    0.91f },
            { Params::pluck,       0.12f },
            { Params::inputCutoff, 3400.0f },
            { Params::loopCutoff,  2400.0f },
            { Params::jitter,      0.18f },
            { Params::crossFb,     0.22f },
            { Params::dryWet,      0.65f },
            { Params::wetGain,     1.20f },
            { Params::lfo1Sync,    7.0f },   // 1/2
            { Params::lfo1Shape,   1.0f },   // Tri
            { Params::m1_loopcut,  0.18f }
        } });

        // 5. Self-Mod Drone : nappe organique en auto-modulation (LFO SOMA + Smth Rnd)
        v.push_back({ "Self-Mod Drone", {
            { Params::pitch,      -7.0f },
            { Params::feedback,    0.95f },
            { Params::inharm,      0.28f },
            { Params::selfMod,     0.48f },
            { Params::crossFb,     0.42f },
            { Params::jitter,      0.30f },
            { Params::inputCutoff, 6000.0f },
            { Params::loopCutoff,  3000.0f },
            { Params::dryWet,      0.72f },
            { Params::wetGain,     1.15f },
            { Params::lfo1Shape,   12.0f },  // SOMA
            { Params::lfo1Rate,    0.15f },
            { Params::lfo1Smooth,  0.40f },
            { Params::m1_self,     0.25f },
            { Params::m1_inharm,   0.18f },
            { Params::lfo2Shape,   8.0f },   // Smth Rnd
            { Params::lfo2Rate,    0.28f },
            { Params::lfo2Smooth,  0.55f },
            { Params::m2_loopcut,  0.22f }
        } });

        // 6. Polyrhythm Wash : modulation polyrythmique 5/4 vs 7/4 avec LFO croisés
        v.push_back({ "Polyrhythm Wash", {
            { Params::pitch,       0.0f },
            { Params::feedback,    0.92f },
            { Params::pluck,       0.35f },
            { Params::inharm,      0.16f },
            { Params::crossFb,     0.52f },
            { Params::inputCutoff, 9500.0f },
            { Params::loopCutoff,  5200.0f },
            { Params::dryWet,      0.65f },
            { Params::wetGain,     1.10f },
            { Params::lfo1Sync,    19.0f },  // 5/4
            { Params::lfo1Shape,   10.0f },  // Stairs
            { Params::lfo1Smooth,  0.25f },
            { Params::m1_loopcut,  0.32f },
            { Params::m1_hbeta,    0.30f },
            { Params::m1_cross,    0.20f },
            { Params::lfo2Sync,    21.0f },  // 7/4
            { Params::lfo2Shape,   0.0f },   // Sine
            { Params::lfo2Smooth,  0.15f },
            { Params::m2_drywet,   0.15f },
            { Params::m2_lfo1,     0.15f }
        } });

        return v;
    }();
    return list;
}
} // namespace pinkpresets
