#include "ecg_processing.h"
#include "ecg_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <float.h>
#include <string.h>

ECG_Context *ecg_create(const ECG_Params *params) {
    ECG_Context *ctx = malloc(sizeof(ECG_Context));
    ctx->params = *params;

    return ctx;
}

void ecg_destroy(ECG_Context *ctx) {
    free(ctx);
}

size_t maximum_index(const double *x, size_t size) {
    size_t max_index = 0;
    for (size_t i = 0; i < size; ++i) {
        if (x[i] > x[max_index]) {
            max_index = i;
        }
    }

    return max_index;
}

ECG_Status ecg_analyze(
    ECG_Context *ctx,
    const double *signal,
    size_t n_samples,
    int lead_idx,
    ECG_Peaks *peaks,
    ECG_Intervals *intervals
) {
    const int high_frequency = 15;
    const int low_frequency = 5;
    const double window_time = 0.15; // s

    double *tmp1 = malloc(n_samples * sizeof(double));
    double *tmp2 = malloc(n_samples * sizeof(double));

    memcpy(tmp1, signal, sizeof(double) * n_samples);

    ecg_remove_dc(tmp1, n_samples);

    ecg_apply_gain(tmp1, n_samples, ctx->params.gain);

    ecg_moving_average(tmp1, tmp2, n_samples, ctx->params.sampling_rate_hz / high_frequency);

    ecg_highpass_ma(tmp2, tmp1, n_samples, ctx->params.sampling_rate_hz / low_frequency);

    ecg_derivative_1(tmp1, tmp2, n_samples);

    ecg_square(tmp2, tmp1, n_samples);

    ecg_mwi(tmp1, tmp2, n_samples, ctx->params.sampling_rate_hz * window_time);

    double threashold = tmp2[maximum_index(tmp2, n_samples)] * 0.9;

    double lookup_window_time = 0.2; // s
    size_t lookup_window_size = ctx->params.sampling_rate_hz * lookup_window_time;
    size_t peakIndex = 0;
    size_t peakValue = 0;
    bool found = false;
    for (size_t i = 0; i < n_samples; ++i) {
        double value = tmp2[i];
        if (value > threashold && (!found || value > peakValue)) {
            found = true;
            peakIndex = i;
            peakValue = value;
        } else if (value < threashold && found) {
            size_t start = (peakIndex > lookup_window_size) ? peakIndex - lookup_window_size : 0;
            size_t end = peakIndex + lookup_window_size >= n_samples ? n_samples - 1 : peakIndex + lookup_window_size;

            size_t max_index = start;
            double max = signal[start];

            for (size_t i = start + 1; i < end; ++i) {
                if (signal[i] > max) {
                    max = signal[i];
                    max_index = i;
                }
            }

            max_index = maximum_index(signal + start + 1, end - start);

            peaks->R[peaks->R_count++] = max_index;
            found = false;
        }
    }

    return ECG_OK;
}