#ifndef RENDER_PROGRESS_H
#define RENDER_PROGRESS_H

#include <chrono>

struct RenderProgress {
    int pass = 0;
    int n_passes = 0;
    int spp = 0;
    int spp_done = 0;
    int spp_total = 0;
    int tiles_done = 0;
    int n_tiles = 0;
    int threads = 0;
    double pass_fraction = 0;
    double total_fraction = 0;
    bool running = false;
    bool finished = false;
    std::chrono::steady_clock::time_point start, end;

    double elapsed_seconds() const {
        const auto stop = finished ? end : std::chrono::steady_clock::now();
        return std::chrono::duration<double>(stop - start).count();
    }
};

inline RenderProgress render_progress;

#endif
