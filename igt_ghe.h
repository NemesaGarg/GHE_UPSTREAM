/* SPDX-License-Identifier: MIT
 *
 * Copyright © 2022 Intel Corporation
 */

#ifndef IGT_GHEALGORITHM_H_
#define IGT_GHEALGORITHM_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "igt_core.h"

#ifndef IN
#define IN
#endif

#define GLOBALHIST_MAX_BIN_INDEX (GLOBALHIST_BIN_COUNT - 1) // Index of last histogram bin
#define DD_MAX(a, b) ((a) < (b) ? (b) : (a))
#define DD_MIN(a, b) ((a) < (b) ? (a) : (b))
#define DD_ROUNDTONEARESTINT(p) ((uint32_t)(p + 0.5))
#define DD_ABS(x) ((x) < 0 ? -(x) : (x))
#define DD_DIFF(a, b) (((a) > (b)) ? ((a) - (b)) : ((b) - (a)))
#define GLOBALHIST_DEFAULT_BOOST 1.0
// IET factor programmed in 1.9 format, so fraction shift is (1 << 9)
#define GLOBALHIST_IET_SCALE_FACTOR 512
#define MILLIUNIT_TO_UNIT(n) (n / 1000) // Convert milli unit to unit
#define GLOBALHIST_IIR_FILTER_ORDER 3
#define GLOBALHIST_IET_MAX_VAL 1023 // IET values are in 1.9 format (1 bit integer, 9 bit fraction)
#define GLOBALHIST_MAX_IET_INDEX (GLOBALHIST_IET_LUT_LENGTH - 1) // Index of last histogram bin
#define SMOOTHENING_MIN_STABLE_CUT_OFF_FREQUNCY_DURATION 500.0 // 500ms
#define PHASE_GLOBALHIST_PERIOD 50 // 50ms
// Default GlobalHist temporal filter Min cutoff frequency in Milli H
#define GLOBALHIST_SMOOTHENING_MIN_SPEED_DEFAULT 500
// Default GlobalHist temporal filter Min cutoff frequency in Milli Hz
#define GLOBALHIST_SMOOTHENING_MAX_SPEED_DEFAULT 800
#define GLOBALHIST_SMOOTHENING_PERIOD_DEFAULT 30    // Default GlobalHist temporal timer period
// Default Minimum step percent to stop the timer. In 100 * percent
#define GLOBALHIST_SMOOTHENING_TOLERANCE_DEFAULT 50
// IET values are in 1.9 format (1 bit integer, 9 bit fraction)
#define SOLID_COLOR_POWER_THRESHOLD 0.95
#define SOLID_COLOR_SEARCH_WINDOW_SIZE 4
#define GLOBALHIST_BIN_COUNT 32   // Total number of segments in GlobalHist
#define GLOBALHIST_IET_LUT_LENGTH 33 // Total number of IET entries

enum pipe_id {
	GlobalHist_PIPE_A = 0,
	GlobalHist_PIPE_B = 1,
	GlobalHist_PIPE_C = 2,
	GlobalHist_PIPE_D = 3
};

struct globalhist_args {
	enum pipe_id pipeid;
	bool isprogramdiet;
	uint32_t dietfactor[GLOBALHIST_IET_LUT_LENGTH];
	uint32_t histogram[GLOBALHIST_BIN_COUNT];
	uint32_t resolution_x;
	uint32_t resolution_y;
};

// GlobalHist Algorithm function pointer.
typedef void (*globalhistalgorithm)(IN void *, struct globalhist_args *gheargs);
typedef void (*globalhistresetalgorithm)(IN void *);
typedef void (*globalhistprogramietregisters)(IN void *, struct globalhist_args *gheargs);

// Function table for GlobalHist functions.
struct globalhist_functbl {
	globalhistalgorithm ghealgorithm;
	globalhistresetalgorithm gheresetalgorithm;
	globalhistprogramietregisters ghesetiet;
};

struct globalhist_algorithm {
	uint32_t imagesize;
};

struct globalhist_ie {
	uint32_t lutapplied[GLOBALHIST_IET_LUT_LENGTH];
	uint32_t luttarget[GLOBALHIST_IET_LUT_LENGTH];
	uint32_t lutdelta[GLOBALHIST_IET_LUT_LENGTH];
};

struct globalhist_temporal_filter_params {
	uint64_t smootheningiteration;
	uint32_t min_cut_off_freq_in_milli_hz; // Value from the INF or Default
	uint32_t max_cut_off_freq_in_milli_hz; // Value from the INF or Default
	uint32_t current_min_cut_off_freq_in_milli_hz;
	uint32_t current_max_cut_off_freq_in_milli_hz;
	uint32_t prevhistogram[GLOBALHIST_BIN_COUNT];
	double iethistory[GLOBALHIST_IET_LUT_LENGTH][GLOBALHIST_IIR_FILTER_ORDER];
	double targetboost;
	double minimum_step_percent;
};

struct globalhist_context {
	struct globalhist_functbl ghefunctable;      // GlobalHist Algorithm Function Table
	enum pipe_id pipe;
	uint32_t histogram[GLOBALHIST_BIN_COUNT]; // Bin wise histogram data for current frame.
	uint32_t lut[GLOBALHIST_BIN_COUNT];
	struct globalhist_algorithm algorithm;
	struct globalhist_ie imageenhancement;
	struct globalhist_temporal_filter_params filterparams;
	double degammalut[GLOBALHIST_BIN_COUNT];
};

void set_histogram_data_bin(struct globalhist_args *gheargs);
static void display_initialize_algorithm(struct globalhist_context *ghecontext,
			struct globalhist_args *gheargs);
static void display_initialize_temporal_filter_params(struct globalhist_context *ghecontext);
static double apply_1dlut(double inval, double *lut, double maxindex);
static void display_ghe_algorithm(struct globalhist_context *ghecontext,
				struct globalhist_args *gheargs);
static void temporal_smoothen_iet(struct globalhist_context *ghecontext,
				struct globalhist_args *gheargs);
static bool is_target_iet_reached(struct globalhist_context *ghecontext);
static void display_set_diet_reg(struct globalhist_context *ghecontext,
				struct globalhist_args *gheargs);
static void display_reset_algorithm(struct globalhist_context *ghecontext);
static double calculate_filter_coefficient(struct globalhist_context *ghecontext);
static double display_pcphase_coordinator_temporal_filter_Nthorder(double filtercoefficient,
		double input, double *inputhistoryphase);
static double get_srgb_decoding_value(double input);
static double estimate_probability_Of_full_screen_solidcolor(double *powerhistogram,
		double totalpower);
static double get_relative_frame_brightness_change(struct globalhist_context *ghecontext,
		uint32_t *prevhist);
#endif
