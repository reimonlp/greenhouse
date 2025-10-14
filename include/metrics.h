// Lightweight metrics helpers (pure functions) for testability
#ifndef METRICS_H
#define METRICS_H

#include <cmath>

// Exponential moving average update. If prev is 0 (unset), initializes with sample.
static inline float emaUpdate(float prev, float sample, float alpha) {
    if (alpha <= 0.0f) return sample; // degenerate -> passthrough
    if (alpha >= 1.0f) return sample; // full weight on new sample
    if (prev == 0.0f) return sample;  // initialization on first observation
    return prev * (1.0f - alpha) + sample * alpha;
}

#endif // METRICS_H
