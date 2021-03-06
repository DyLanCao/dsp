#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "gain.h"
#include "util.h"

struct gain_state {
	int channel;
	sample_t mult;
};

sample_t * gain_effect_run(struct effect *e, ssize_t *frames, sample_t *ibuf, sample_t *obuf)
{
	ssize_t i, k, samples = *frames * e->ostream.channels;
	struct gain_state *state = (struct gain_state *) e->data;
	if (state->channel == -1) {
		for (i = 0; i < samples; i += e->ostream.channels)
			for (k = 0; k < e->ostream.channels; ++k)
				if (GET_BIT(e->channel_selector, k))
					ibuf[i + k] *= state->mult;
	}
	else {
		for (i = state->channel; i < samples; i += e->ostream.channels)
			ibuf[i] *= state->mult;
	}
	return ibuf;
}

void gain_effect_plot(struct effect *e, int i)
{
	struct gain_state *state = (struct gain_state *) e->data;
	int k;
	if (state->channel == -1) {
		for (k = 0; k < e->ostream.channels; ++k) {
			if (GET_BIT(e->channel_selector, k))
				printf("H%d_%d(f)=%.15e\n", k, i, 20 * log10(fabs(state->mult)));
			else
				printf("H%d_%d(f)=0\n", k, i);
		}
	}
	else {
		for (k = 0; k < e->ostream.channels; ++k) {
			if (k == state->channel)
				printf("H%d_%d(f)=%.15e\n", k, i, 20 * log10(fabs(state->mult)));
			else
				printf("H%d_%d(f)=0\n", k, i);
		}
	}
}

void gain_effect_destroy(struct effect *e)
{
	free(e->data);
	free(e->channel_selector);
}

struct effect * gain_effect_init(struct effect_info *ei, struct stream_info *istream, char *channel_selector, const char *dir, int argc, char **argv)
{
	struct effect *e;
	struct gain_state *state;
	double mult;
	int channel = -1;
	char *endptr, *g;

	if (argc != 2 && argc != 3) {
		LOG(LL_ERROR, "%s: %s: usage: %s\n", dsp_globals.prog_name, argv[0], ei->usage);
		return NULL;
	}
	if (argc == 3) {
		channel = strtol(argv[1], &endptr, 10);
		CHECK_ENDPTR(argv[1], endptr, "channel", return NULL);
		CHECK_RANGE(channel >= 0 && channel < istream->channels, "channel", return NULL);
		g = argv[2];
	}
	else
		g = argv[1];

	if (ei->effect_number == GAIN_EFFECT_NUMBER_MULT) {
		mult = strtod(g, &endptr);
		CHECK_ENDPTR(g, endptr, "multiplier", return NULL);
	}
	else if (ei->effect_number == GAIN_EFFECT_NUMBER_GAIN) {
		mult = pow(10.0, strtod(g, &endptr) / 20.0);
		CHECK_ENDPTR(g, endptr, "gain", return NULL);
	}
	else {
		LOG(LL_ERROR, "%s: gain.c: BUG: unknown effect: %s (%d)\n", dsp_globals.prog_name, argv[0], ei->effect_number);
		return NULL;
	}

	e = calloc(1, sizeof(struct effect));
	e->name = ei->name;
	e->istream.fs = e->ostream.fs = istream->fs;
	e->istream.channels = e->ostream.channels = istream->channels;
	e->channel_selector = NEW_SELECTOR(istream->channels);
	COPY_SELECTOR(e->channel_selector, channel_selector, istream->channels);
	e->run = gain_effect_run;
	e->plot = gain_effect_plot;
	e->destroy = gain_effect_destroy;
	state = calloc(1, sizeof(struct gain_state));
	state->channel = channel;
	state->mult = mult;
	e->data = state;
	return e;
}
