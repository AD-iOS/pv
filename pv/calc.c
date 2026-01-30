/*
 * Functions for updating the calculated state of the transfer.
 *
 * Copyright 2024-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include "pv.h"
#include "pv-internal.h"


/*
 * Update the current average rate, using a ring buffer of past transfer
 * positions - if this is the first entry, use the provided instantaneous
 * rate, otherwise calulate the average rate from the difference between the
 * current position + elapsed time pair, and the oldest pair in the buffer.
 */
static void pv__update_average_rate_history(pvtransfercalc_t calc, readonly_pvtransferstate_t transfer,
					    unsigned int history_interval, long double rate)
{
	size_t first = calc->history_first;
	size_t last = calc->history_last;
	long double last_elapsed;

	if (NULL == calc->history)
		return;

	last_elapsed = calc->history[last].elapsed_sec;

	/*
	 * Do nothing if this is not the first call but not enough time has
	 * elapsed since the previous call yet.
	 */
	if ((last_elapsed > 0.0)
	    && (transfer->elapsed_seconds < (last_elapsed + history_interval)))
		return;

	/*
	 * If this is not the first call, add a new entry to the circular
	 * buffer.
	 */
	if (last_elapsed > 0.0) {
		size_t len = calc->history_len;
		last = (last + 1) % len;
		calc->history_last = last;
		if (last == first) {
			first = (first + 1) % len;
			calc->history_first = first;
		}
	}

	calc->history[last].elapsed_sec = transfer->elapsed_seconds;
	calc->history[last].transferred = transfer->transferred;

	if (first == last) {
		calc->current_avg_rate = rate;
	} else {
		off_t bytes = (calc->history[last].transferred - calc->history[first].transferred);
		long double sec = (calc->history[last].elapsed_sec - calc->history[first].elapsed_sec);
		/* Safety check to avoid division by zero. */
		if (sec < 0.000001 && sec > -0.000001)
			sec = 0.000001;
		calc->current_avg_rate = (long double) bytes / sec;
	}
}


/*
 * Update all calculated transfer state in calc (usually from state->calc).
 *
 * If "final" is true, this is the final update, so calc->transfer_rate and
 * calc->average_rate are given as an average over the whole transfer;
 * otherwise they are the current transfer rate and current average rate.
 *
 * The value of calc->percentage will reflect the percentage completion if
 * control->size is greater than zero, otherwise it will increase by 2 each
 * call and wrap at 200.
 */
void pv_calculate_transfer_rate(pvtransfercalc_t calc, readonly_pvtransferstate_t transfer,
				readonly_pvcontrol_t control, readonly_pvdisplay_t display, bool final)
{
	off_t bytes_since_last;
	long double time_since_last, transfer_rate, average_rate;

	/* Quick safety checks for null pointers. */
	if (NULL == calc)
		return;
	if (NULL == transfer)
		return;
	if (NULL == control)
		return;
	if (NULL == display)
		return;

	bytes_since_last = 0;
	if (transfer->transferred >= 0) {
		bytes_since_last = transfer->transferred - calc->prev_transferred;
		calc->prev_transferred = transfer->transferred;
	}

	/*
	 * In case the time since the last update is very small, we keep
	 * track of amount transferred since the last update, and just keep
	 * adding to that until a reasonable amount of time has passed to
	 * avoid rate spikes or division by zero.
	 */
	time_since_last = transfer->elapsed_seconds - calc->prev_elapsed_sec;
	if (time_since_last <= 0.01) {
		transfer_rate = calc->prev_rate;
		calc->prev_trans += bytes_since_last;
	} else {
		long double measured_rate;

		transfer_rate = ((long double) bytes_since_last + calc->prev_trans) / time_since_last;
		measured_rate = transfer_rate;

		calc->prev_elapsed_sec = transfer->elapsed_seconds;
		calc->prev_trans = 0;

		if (control->bits)
			measured_rate = 8.0 * measured_rate;

		if ((calc->measurements_taken < 1) || (measured_rate < calc->rate_min)) {
			calc->rate_min = measured_rate;
		}
		if (measured_rate > calc->rate_max) {
			calc->rate_max = measured_rate;
		}
		calc->rate_sum += measured_rate;
		calc->ratesquared_sum += (measured_rate * measured_rate);
		calc->measurements_taken++;
	}
	calc->prev_rate = transfer_rate;

	/* Update history and current average rate for ETA. */
	pv__update_average_rate_history(calc, transfer, control->history_interval, transfer_rate);
	average_rate = calc->current_avg_rate;

	/*
	 * If this is the final update at the end of the transfer, we
	 * recalculate the rate - and the average rate - across the whole
	 * period of the transfer.
	 */
	if (final) {
		long double total_elapsed_seconds = (long double) (transfer->elapsed_seconds);
		/* Safety check to avoid division by zero. */
		if (total_elapsed_seconds < 0.000001)
			total_elapsed_seconds = 0.000001;
		average_rate =
		    (((long double) (transfer->transferred)) -
		     ((long double) display->initial_offset)) / total_elapsed_seconds;
		transfer_rate = average_rate;
	}

	calc->transfer_rate = transfer_rate;
	calc->average_rate = average_rate;

	if (control->size <= 0) {
		/*
		 * If we don't know the total size of the incoming data,
		 * then for a percentage, we gradually increase the
		 * percentage completion as data arrives, to a maximum of
		 * 200, then reset it - we use this if we can't calculate
		 * it, so that the numeric percentage output will go
		 * 0%-100%, 100%-0%, 0%-100%, and so on.
		 */
		if (transfer_rate > 0)
			calc->percentage += 2;
		if (calc->percentage > 199)
			calc->percentage = 0;
	} else {
		calc->percentage = pv_percentage(transfer->transferred, control->size);
	}

	/* Ensure the percentage is never negative or huge. */
	if (calc->percentage < 0.0)
		calc->percentage = 0.0;
	if (calc->percentage > 100000.0)
		calc->percentage = 100000.0;
}
