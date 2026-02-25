#include "ecg_processing.h"
#include "ecg_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <float.h>
#include <string.h>

ECG_Context *ecg_create(const ECG_Params *params) {
    ECG_Context *ctx = malloc(sizeof(ECG_Context));
    if (!ctx) {
        return NULL;
    }

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

void swap(double **x, double **y) {
    double **tmp = x;
    *x = *y;
    *y = *tmp;
}

ECG_Status ecg_analyze(
    ECG_Context *ctx,
    const double *signal,
    size_t n_samples,
    int lead_idx,
    ECG_Peaks *peaks,
    ECG_Intervals *intervals
) {
    if (!ctx || !signal || !peaks || !intervals) {
        return ECG_ERR_NULL;
    }

    const int high_frequency = 15;
    const int low_frequency = 5;
    const double window_time = 0.15; // s
    const size_t window_size = ctx->params.sampling_rate_hz * window_time;

    double *in = malloc(n_samples * sizeof(double));
    if (!in) {
        return ECG_ERR_ALLOC;
    }

    double *out = malloc(n_samples * sizeof(double));
    if (!out) {
        return ECG_ERR_ALLOC;
    }

    memcpy(in, signal, sizeof(double) * n_samples);

    ecg_remove_dc(in, n_samples);
    ecg_apply_gain(in, n_samples, ctx->params.gain);
    
    ecg_moving_average(in, out, n_samples, ctx->params.sampling_rate_hz / high_frequency);
    swap(&in, &out);
    
    ecg_highpass_ma(in, out, n_samples, ctx->params.sampling_rate_hz / low_frequency);
    swap(&in, &out);
    
    ecg_derivative_1(in, out, n_samples);
    swap(&in, &out);
    
    ecg_square(in, out, n_samples);
    swap(&in, &out);
    
    ecg_mwi(in, out, n_samples, window_size);
    swap(&in, &out);

    double threashold = out[maximum_index(out, n_samples)] * 0.9;

    size_t peakIndex = 0;
    bool found = false;
    for (size_t i = 0; i < n_samples; ++i) {
        double value = out[i];
        if (value > threashold && (!found || value > out[peakIndex])) {
            found = true;
            peakIndex = i;
        } else if (value < threashold && found) {
            size_t start = peakIndex > window_size ?
                peakIndex - window_size :
                0;
            size_t end = peakIndex + window_size < n_samples ?
                peakIndex + window_size :
                n_samples - 1;

            max_index = start + maximum_index(signal + start, end - start);

            peaks->R[peaks->R_count++] = max_index;
            found = false;
        }
    }

    return ECG_OK;
}