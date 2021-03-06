#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "compress.h"
#include "util.h"

struct compress_state {
	sample_t thresh, thresh_db, ratio, attack, release, gain;
};

sample_t * compress_effect_run(struct effect *e, ssize_t *frames, sample_t *ibuf, sample_t *obuf)
{
	ssize_t i, k, samples = *frames * e->ostream.channels;
	sample_t s, gain_target;
	struct compress_state *state = (struct compress_state *) e->data;
	for (i = 0; i < samples; i += e->ostream.channels) {
		s = 0;
		for (k = 0; k < e->ostream.channels; ++k)
			if (GET_BIT(e->channel_selector, k))
				s = MAXIMUM(fabs(ibuf[i + k]), s);
		if (s > state->thresh)
			gain_target = pow(10, (state->thresh_db - (20 * log10(s))) * state->ratio / 20);
		else
			gain_target = 1.0;
		if (state->gain > gain_target) {
			state->gain *= state->attack;
			if (state->gain < gain_target)
				state->gain = gain_target;
		}
		else if (state->gain < gain_target) {
			state->gain *= state->release;
			if (state->gain > gain_target)
				state->gain = gain_target;
		}
		for (k = 0; k < e->ostream.channels; ++k)
			if (GET_BIT(e->channel_selector, k))
				ibuf[i + k] *= state->gain;
	}
	return ibuf;
}

void compress_effect_reset(struct effect *e)
{
	struct compress_state *state = (struct compress_state *) e->data;
	state->gain = 1.0;
}

void compress_effect_destroy(struct effect *e)
{
	free(e->data);
	free(e->channel_selector);
}

struct effect * compress_effect_init(struct effect_info *ei, struct stream_info *istream, char *channel_selector, const char *dir, int argc, char **argv)
{
	char *endptr;
	struct effect *e;
	struct compress_state *state;

	if (argc != 5) {
		LOG(LL_ERROR, "%s: %s: usage: %s\n", dsp_globals.prog_name, argv[0], ei->usage);
		return NULL;
	}

	state = calloc(1, sizeof(struct compress_state));
	state->thresh_db = strtod(argv[1], &endptr);
	CHECK_ENDPTR(argv[1], endptr, "thresh", goto fail);
	state->thresh = pow(10, state->thresh_db / 20);
	state->ratio = strtod(argv[2], &endptr);
	CHECK_ENDPTR(argv[2], endptr, "ratio", goto fail);
	CHECK_RANGE(state->ratio > 0, "ratio", goto fail);
	state->ratio = 1 - 1 / state->ratio;
	state->attack = strtod(argv[3], &endptr);
	CHECK_ENDPTR(argv[3], endptr, "attack", goto fail);
	CHECK_RANGE(state->attack >= 0, "attack", goto fail);
	state->attack = (state->attack == 0) ? 0 : pow(10, -10 / state->attack / istream->fs / 20);
	state->release = strtod(argv[4], &endptr);
	CHECK_ENDPTR(argv[4], endptr, "release", goto fail);
	CHECK_RANGE(state->release >= 0, "release", goto fail);
	state->release = (state->release == 0) ? HUGE_VAL : pow(10, 10 / state->release / istream->fs / 20);
	state->gain = 1.0;

	e = calloc(1, sizeof(struct effect));
	e->name = ei->name;
	e->istream.fs = e->ostream.fs = istream->fs;
	e->istream.channels = e->ostream.channels = istream->channels;
	e->channel_selector = NEW_SELECTOR(istream->channels);
	COPY_SELECTOR(e->channel_selector, channel_selector, istream->channels);
	e->run = compress_effect_run;
	e->reset = compress_effect_reset;
	e->destroy = compress_effect_destroy;
	e->data = state;
	return e;

	fail:
	free(state);
	return NULL;
}
