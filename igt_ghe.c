// SPDX-License-Identifier: MIT
/*
 * Copyright © 2022 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include "igt_ghe.h"

void set_histogram_data_bin(struct globalhist_args *gheargs)
{
	struct globalhist_context *ghecontext =
		(struct globalhist_context *)malloc(sizeof(struct globalhist_context));

	igt_assert(ghecontext);

	memcpy(ghecontext->histogram, gheargs->histogram, GLOBALHIST_BIN_COUNT * sizeof(uint32_t));
	ghecontext->algorithm.imagesize = (gheargs->resolution_x * gheargs->resolution_y);
	display_initialize_algorithm(ghecontext, gheargs);
}

static void display_initialize_algorithm(struct globalhist_context *ghecontext,
		struct globalhist_args *gheargs)
{
	double hist_lut_step_size;

	ghecontext->ghefunctable.ghealgorithm = (globalhistalgorithm)display_ghe_algorithm;
	ghecontext->ghefunctable.ghesetiet =
		(globalhistprogramietregisters)display_set_diet_reg;
	ghecontext->ghefunctable.gheresetalgorithm =
		(globalhistresetalgorithm)display_reset_algorithm;

	/* Reset GlobalHist data structures */
	ghecontext->ghefunctable.gheresetalgorithm(ghecontext);
	display_initialize_temporal_filter_params(ghecontext);
	hist_lut_step_size = 1.0 / (double)GLOBALHIST_MAX_BIN_INDEX;
	for (uint8_t binindex = 0; binindex < GLOBALHIST_MAX_BIN_INDEX; binindex++)
		ghecontext->degammalut[binindex] =
			get_srgb_decoding_value((double)binindex * hist_lut_step_size);

	/* This call will manage the GlobalHist algorithm and activate Phase-In. */
	ghecontext->ghefunctable.ghealgorithm(ghecontext, gheargs);

	/* Program calculated DIET factor */
	ghecontext->ghefunctable.ghesetiet(ghecontext, gheargs);
}

/*
 * Get normalized SRGB tranformed value.
 * sRGB can be converted to gamma-2.2 curve with 0.04045 as
 * intersection point.
 * To convert sRGB to CIE XYX use conversion values as
 * X= 0.04045 and 12.92.
 */
static double get_srgb_decoding_value(double input)
{
	double output;

	if (input <= 0.04045)
		output = input / 12.92;
	else
		output = pow(((input + 0.055) / 1.055), 2.4);

	return output;
}

static void display_initialize_temporal_filter_params(struct globalhist_context *ghecontext)
{
	ghecontext->filterparams.min_cut_off_freq_in_milli_hz =
		GLOBALHIST_SMOOTHENING_MIN_SPEED_DEFAULT;
	ghecontext->filterparams.max_cut_off_freq_in_milli_hz =
		GLOBALHIST_SMOOTHENING_MAX_SPEED_DEFAULT;
	ghecontext->filterparams.current_max_cut_off_freq_in_milli_hz =
		DD_MAX(ghecontext->filterparams.max_cut_off_freq_in_milli_hz,
			ghecontext->filterparams.min_cut_off_freq_in_milli_hz);
	ghecontext->filterparams.minimum_step_percent = GLOBALHIST_SMOOTHENING_TOLERANCE_DEFAULT;

	for (uint8_t ietbinindex = 0; ietbinindex < GLOBALHIST_IET_LUT_LENGTH; ietbinindex++) {
		for (uint8_t filterorder = 0; filterorder < GLOBALHIST_IIR_FILTER_ORDER;
								filterorder++)
			ghecontext->filterparams.iethistory[ietbinindex][filterorder] =
								GLOBALHIST_IET_SCALE_FACTOR;
	}

	for (uint8_t count = 0; count < GLOBALHIST_IET_LUT_LENGTH; count++)
		ghecontext->imageenhancement.luttarget[count] = GLOBALHIST_IET_SCALE_FACTOR;
}

static void display_ghe_algorithm(struct globalhist_context *ghecontext,
		struct globalhist_args *gheargs)
{
	uint32_t totalnumofpixel = 0;
	uint32_t *multiplierlut = ghecontext->imageenhancement.luttarget;
	double enhancementtable[GLOBALHIST_BIN_COUNT];
	double filtered_enhancement_table[GLOBALHIST_BIN_COUNT];
	double binindexnormalized, ietval;
	double cdfrange, cdfnormalizingfactor;
	double probability_of_full_screen_solid_color;
	double sumpower = 0; /* Total Power of input frame */
	/* Histogram bin wise distribution of power */
	double powerdistribution[GLOBALHIST_BIN_COUNT];
	const double maxhistbinindex = GLOBALHIST_MAX_BIN_INDEX;
	const double ietlutstepsize = 1.0 / (double)GLOBALHIST_MAX_IET_INDEX;
	const double histbinstepsize = 1.0 / maxhistbinindex;
	const double maxslope = 7.0;
	const double minslope = 0.3;

	for (uint8_t binindex = 0; binindex < GLOBALHIST_BIN_COUNT; binindex++) {
		totalnumofpixel += ghecontext->histogram[binindex];
		enhancementtable[binindex] = totalnumofpixel;
	}
	ghecontext->algorithm.imagesize = totalnumofpixel;

	/* Calculate histogram bin wise power distribution and total frame power */
	for (uint8_t binindex = 0; binindex < GLOBALHIST_BIN_COUNT; binindex++) {
		double binweight = ghecontext->degammalut[binindex];

		powerdistribution[binindex] = binweight * (double)ghecontext->histogram[binindex];
		sumpower += powerdistribution[binindex];
	}
	probability_of_full_screen_solid_color =
		estimate_probability_Of_full_screen_solidcolor(powerdistribution, sumpower);

	/* Do not modify pixel values for Solid Color */
	if (probability_of_full_screen_solid_color == 1) {
		for (uint8_t binindex = 0; binindex < GLOBALHIST_IET_LUT_LENGTH; binindex++)
			multiplierlut[binindex] = GLOBALHIST_IET_SCALE_FACTOR;
	}

	cdfrange = enhancementtable[GLOBALHIST_BIN_COUNT - 1] - ghecontext->histogram[0];
	cdfnormalizingfactor = 1.0 / cdfrange;
	enhancementtable[0] = (double)(enhancementtable[0] - ghecontext->histogram[0])
				* cdfnormalizingfactor;
	for (uint8_t binindex = 1; binindex < GLOBALHIST_BIN_COUNT; binindex++) {
		double outval = (enhancementtable[binindex] - ghecontext->histogram[0])
					* cdfnormalizingfactor;
		double prevsampleval = enhancementtable[binindex - 1];
		double slope = maxhistbinindex * (outval - prevsampleval);

		slope = DD_MAX(slope, minslope);
		slope = DD_MIN(slope, maxslope);
		outval = prevsampleval + slope * histbinstepsize;
		outval = DD_MIN(outval, 1.0);
		enhancementtable[binindex] = outval;
	}

	/* No filtering for 0th and last values of the IET. */
	filtered_enhancement_table[0] = enhancementtable[0];
	filtered_enhancement_table[GLOBALHIST_MAX_BIN_INDEX] =
		enhancementtable[GLOBALHIST_MAX_BIN_INDEX];

	/* Smoothen jerks in FilteredEnhancementTable by averaging with nearby bins. */
	for (uint8_t binindex = 1; binindex < GLOBALHIST_MAX_BIN_INDEX; binindex++) {
		/* Average out current and two nearby sampels.*/
		filtered_enhancement_table[binindex] = 0.333333 * (enhancementtable[binindex - 1] +
			enhancementtable[binindex] + enhancementtable[binindex + 1]);
	}

	/* Convert LUT of size GlobalHist_BIN_COUNT to IET LUT of size GLOBALHIST_IET_LUT_LENGTH */
	for (uint32_t binindex = 1; binindex < GLOBALHIST_IET_LUT_LENGTH; binindex++) {
		binindexnormalized = (double)binindex * ietlutstepsize;
		ietval = apply_1dlut(binindexnormalized, filtered_enhancement_table,
						maxhistbinindex);
		/* Compute sample for multiplier LUT */
		ietval = ietval / binindexnormalized;
		/* Cap IET val to the value that will not cause clipping. */
		ietval = DD_MIN(ietval, 1.0 / binindexnormalized);
		ietval = (double)GLOBALHIST_IET_SCALE_FACTOR * ietval + 0.5;
		ietval = DD_MIN(ietval, GLOBALHIST_IET_MAX_VAL);
		ietval = DD_MAX(ietval, GLOBALHIST_IET_SCALE_FACTOR); /* Never dim any pixel */
		multiplierlut[binindex] = ietval;
	}

	/* 0th multiplier sample can't be computed. Extend 1st sample to the 0th */
	multiplierlut[0] = multiplierlut[1];
	temporal_smoothen_iet(ghecontext, gheargs);
	memcpy(ghecontext->filterparams.prevhistogram, ghecontext->histogram,
				sizeof(ghecontext->histogram));
}

static double estimate_probability_Of_full_screen_solidcolor(double *powerhistogram,
				double totalpower)
{
	const double solidcolorpowerthreshold = SOLID_COLOR_POWER_THRESHOLD * totalpower;
	double windowsizetoprobabilitymapping[SOLID_COLOR_SEARCH_WINDOW_SIZE] = {1, 1, 0.75, 0.375};
	uint8_t n;

	/*
	 * Find N number of consecutive bins which contain SOLID_COLOR_POWER_THRESHOLD
	 * amount of frame power.
	 * N = 1 means solid color for sure.
	 * Since SOLID_COLOR_POWER_THRESHOLD is not 1.0, it detects almost solid color.
	 * N = 2 may mean near solid color. One pixel value shift will shift energy
	 * to next or prev bin.
	 * For example, image with solid color patches 247 and 248 will look almost single solid,
	 * but  histogram will be spread across two bins.
	 * N >= 3 means probability of solid color is less.
	 * Return value is gradullay reduced to 0 for N >= 3.
	 */
	for (n = 1; n <= SOLID_COLOR_SEARCH_WINDOW_SIZE; n++) {
		for (uint8_t binindex = 0; binindex < (GLOBALHIST_BIN_COUNT - n + 1); binindex++) {
			double sumpower = 0;

			for (uint8_t i = binindex; i < (binindex + n); i++) {
				sumpower += powerhistogram[i];
				if (sumpower >= solidcolorpowerthreshold)
					return windowsizetoprobabilitymapping[n - 1];
			}
		}
	}
	return 0;
}

/*
 * Interpolate IET LUT from histogram based LUT.
 * This is required because IET has more entries than histogram LUT
 * Interpolation Logic => MinValue + (MaxValue - MinValue) * Interpolator.
 */
static double apply_1dlut(double inval, double *lut, double maxindex)
{
	uint32_t index1, index2;
	double val1, val2, interpolator;
	double DIndex = inval * maxindex;

	index1 = DIndex;
	index2 = ceil(DIndex);

	interpolator = (DIndex - (double)index1);
	val1         = lut[index1];
	val2         = lut[index2];

	return val1 + interpolator * (val2 - val1);
}

static void display_set_diet_reg(struct globalhist_context *ghecontext,
			struct globalhist_args *gheargs)
{
	gheargs->pipeid = ghecontext->pipe;
	gheargs->isprogramdiet = true;

	for (uint8_t count = 0; count < GLOBALHIST_IET_LUT_LENGTH; count++)
		gheargs->dietfactor[count] = ghecontext->imageenhancement.lutapplied[count];
}

static void display_reset_algorithm(struct globalhist_context *ghecontext)
{
	for (uint8_t count = 0; count < GLOBALHIST_IET_LUT_LENGTH; count++) {
		ghecontext->imageenhancement.lutapplied[count] = GLOBALHIST_IET_SCALE_FACTOR;
		for (uint8_t filterorder = 0; filterorder < GLOBALHIST_IIR_FILTER_ORDER;
								filterorder++)
			ghecontext->filterparams.iethistory[count][filterorder] =
						GLOBALHIST_IET_SCALE_FACTOR;
	}

	/* Reset filter params */
	ghecontext->filterparams.targetboost = GLOBALHIST_DEFAULT_BOOST;
	ghecontext->filterparams.smootheningiteration = 0;
}

static void temporal_smoothen_iet(struct globalhist_context *ghecontext,
				struct globalhist_args *gheargs)
{
	double temporalfiltercoefficient;

	if (is_target_iet_reached(ghecontext) == false) {
		temporalfiltercoefficient = calculate_filter_coefficient(ghecontext);
		for (uint8_t count = 0; count < GLOBALHIST_IET_LUT_LENGTH; count++) {
			double ietval = display_pcphase_coordinator_temporal_filter_Nthorder(
						temporalfiltercoefficient,
						ghecontext->imageenhancement.luttarget[count],
						&ghecontext->filterparams.iethistory[count][0])
			    + 0.5;
			ietval = DD_MIN(ietval, GLOBALHIST_IET_MAX_VAL);
			ghecontext->imageenhancement.lutapplied[count] = ietval;
		}
	} else {
		memcpy(ghecontext->imageenhancement.lutapplied,
			ghecontext->imageenhancement.luttarget,
			sizeof(ghecontext->imageenhancement.luttarget));
		for (uint8_t count = 0; count < GLOBALHIST_IET_LUT_LENGTH; count++) {
			for (uint8_t HistoryIndex = 0; HistoryIndex < GLOBALHIST_IIR_FILTER_ORDER;
									HistoryIndex++)
				ghecontext->filterparams.iethistory[count][HistoryIndex] =
					ghecontext->imageenhancement.luttarget[count];
		}
	}
	memcpy(ghecontext->filterparams.prevhistogram, ghecontext->histogram,
			sizeof(ghecontext->filterparams.prevhistogram));

}

static bool is_target_iet_reached(struct globalhist_context *ghecontext)
{
	double maxacceptabledelta, ietdelta;

	maxacceptabledelta = ghecontext->filterparams.minimum_step_percent / 100.0;

	for (uint32_t i = 0; i < GLOBALHIST_IET_LUT_LENGTH; i++) {
		ietdelta = ((double)ghecontext->imageenhancement.lutapplied[i] -
			       (double)ghecontext->imageenhancement.luttarget[i])
				/ (double)ghecontext->imageenhancement.luttarget[i];
		if (ietdelta > maxacceptabledelta)
			return false;
	}

	return true;
}

static double calculate_filter_coefficient(struct globalhist_context *ghecontext)
{
	double angularfrequency, temporalfiltercoefficient;
	double mincutofffreq, maxcutofffreq;
	double cutofffreq = 0;
	double minghesmootheningperiod, relativeframebrightnesschange;
	struct globalhist_temporal_filter_params *pfilterparams;

	pfilterparams = &ghecontext->filterparams;
	mincutofffreq = MILLIUNIT_TO_UNIT(
			(double)pfilterparams->current_min_cut_off_freq_in_milli_hz);
	maxcutofffreq = MILLIUNIT_TO_UNIT(
			(double)pfilterparams->current_max_cut_off_freq_in_milli_hz);
	minghesmootheningperiod = (double)(332225.9136) / (double)(10 * 1000 * 1000);
	relativeframebrightnesschange = get_relative_frame_brightness_change(ghecontext,
				pfilterparams->prevhistogram);
	cutofffreq = mincutofffreq +
		(maxcutofffreq - mincutofffreq) * relativeframebrightnesschange;
	angularfrequency = 6.2831853 * cutofffreq;
	temporalfiltercoefficient = (minghesmootheningperiod * angularfrequency) /
		(1 + (minghesmootheningperiod * angularfrequency));

	return temporalfiltercoefficient;
}

static double display_pcphase_coordinator_temporal_filter_Nthorder(double filtercoefficient,
		double input, double *inputhistoryPhase)
{
	double adjustedvalue = input;

	for (uint8_t count = 0; count < GLOBALHIST_IIR_FILTER_ORDER; count++) {
		adjustedvalue = filtercoefficient * adjustedvalue;
		adjustedvalue += (1 - filtercoefficient) * inputhistoryPhase[count];
		inputhistoryPhase[count] = adjustedvalue;
	}

	return adjustedvalue;
}

static double get_relative_frame_brightness_change(struct globalhist_context *ghecontext,
		uint32_t *prevhist)
{
	double frameaveragepowerprev = 0;
	double frameaveragepowercurr = 0;
	double histchangepercentage = 0;

	for (uint32_t count = 0; count < GLOBALHIST_BIN_COUNT; count++) {
		frameaveragepowerprev += ghecontext->degammalut[count] * (double)prevhist[count];
		frameaveragepowercurr += ghecontext->degammalut[count] *
			(double)ghecontext->histogram[count];
	}

	if (frameaveragepowerprev > 0)
		histchangepercentage = DD_ABS(frameaveragepowercurr - frameaveragepowerprev)
			/ frameaveragepowerprev;
	else if (frameaveragepowerprev != frameaveragepowercurr)
		histchangepercentage = 1.0;

	return DD_MIN(histchangepercentage, 1.0);
}
