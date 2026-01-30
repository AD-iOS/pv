/*
 * Formatter function for ECMA-48 SGR codes.
 *
 * Copyright 2024-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include "pv.h"
#include "pv-internal.h"

#include <string.h>

struct sgr_keyword_map_s {
	/*@null@ */ const char *keyword;
	uint8_t bytes;
	uint8_t code;
};


/*
 * Return an array mapping keywords to ECMA-48 SGR code numbers.
 */
/*@keep@ */ static struct sgr_keyword_map_s *sgr_keywords(void)
{
	/*@keep@ */ static struct sgr_keyword_map_s keywords[] = {
		{ "reset", 0, 0 },
		{ "none", 0, 0 },
		{ "bold", 0, 1 },
		{ "dim", 0, 2 },
		{ "italic", 0, 3 },
		{ "underscore", 0, 4 },
		{ "underline", 0, 4 },
		{ "blink", 0, 5 },
		{ "reverse", 0, 7 },
		{ "no-bold", 0, 22 },	    /* same as no-dim */
		{ "no-dim", 0, 22 },
		{ "no-italic", 0, 23 },
		{ "no-underscore", 0, 24 },
		{ "no-underline", 0, 24 },
		{ "no-blink", 0, 25 },
		{ "no-reverse", 0, 27 },
		{ "black", 0, 30 },
		{ "red", 0, 31 },
		{ "green", 0, 32 },
		{ "brown", 0, 33 },
		{ "yellow", 0, 33 },
		{ "blue", 0, 34 },
		{ "magenta", 0, 35 },
		{ "cyan", 0, 36 },
		{ "white", 0, 37 },
		{ "fg-black", 0, 30 },
		{ "fg-red", 0, 31 },
		{ "fg-green", 0, 32 },
		{ "fg-brown", 0, 33 },
		{ "fg-yellow", 0, 33 },
		{ "fg-blue", 0, 34 },
		{ "fg-magenta", 0, 35 },
		{ "fg-cyan", 0, 36 },
		{ "fg-white", 0, 37 },
		{ "fg-default", 0, 39 },
		{ "bg-black", 0, 40 },
		{ "bg-red", 0, 41 },
		{ "bg-green", 0, 42 },
		{ "bg-brown", 0, 43 },
		{ "bg-yellow", 0, 43 },
		{ "bg-blue", 0, 44 },
		{ "bg-magenta", 0, 45 },
		{ "bg-cyan", 0, 46 },
		{ "bg-white", 0, 47 },
		{ "bg-default", 0, 49 },
		{ NULL, 0, 0 }
	};
	int keyword_index;

	/* Calculate the lengths of each keyword on the first call. */
	if (0 == keywords[0].bytes) {
		for (keyword_index = 0; NULL != keywords[keyword_index].keyword; keyword_index++) {
			keywords[keyword_index].bytes = strlen(keywords[keyword_index].keyword);	/* flawfinder: ignore */
			/* flawfinder - strlen() on a static null-terminated string is OK. */
		}
	}

	/*@-compmempass@ */
	return keywords;
	/*@+compmempass@ */
	/*
	 * splint - found no other way to pass static back without a false
	 * positive warning about a memory leak.
	 */
}


/*
 * Display SGR codes if colour output is supported.
 */
pvdisplay_bytecount_t pv_formatter_sgr(pvformatter_args_t args)
{
	/*@keep@ */ static struct sgr_keyword_map_s *keywords;
	char content[1024];		 /* flawfinder: ignore */
	pvdisplay_bytecount_t write_position, read_position, keyword_start, keyword_length;
	int numeric_value, code_count, most_recent_code;

	/* flawfinder - null-terminated and bounded with pv_snprintf(). */

	if (!args->display->colour_permitted)
		return 0;

	args->display->format_uses_colour = true;

	if (!args->status->terminal_supports_colour)
		return 0;
	if (NULL == args->segment->string_parameter)
		return 0;
	if (0 == args->segment->string_parameter_bytes)
		return 0;

	keywords = sgr_keywords();

	write_position = 0;
	content[0] = '\0';

	debug("%p+%d [%.*s]", args->segment->string_parameter, args->segment->string_parameter_bytes,
	      args->segment->string_parameter_bytes, args->segment->string_parameter);

	read_position = 0;
	keyword_start = 0;
	keyword_length = 0;
	numeric_value = -1;
	code_count = 0;
	most_recent_code = -1;

	while (read_position < args->segment->string_parameter_bytes) {
		char read_char = args->segment->string_parameter[read_position++];
		if ((',' == read_char) || (';' == read_char)) {
			keyword_length = read_position - keyword_start;
			if (keyword_length > 0)
				keyword_length--;
		} else {
			if ((read_char >= '0' && read_char <= '9')
			    && (numeric_value >= 0 || keyword_start == read_position - 1)) {
				if (keyword_start == read_position - 1)
					numeric_value = 0;
				numeric_value = numeric_value * 10 + (int) (read_char - '0');
			} else {
				numeric_value = -1;
			}
			if (read_position >= args->segment->string_parameter_bytes) {
				keyword_length = read_position - keyword_start;
			}
		}
		if (keyword_length > 0) {
			int code = -1;

			debug("keyword@%d+%d: [%.*s]; numeric=%d", keyword_start, keyword_length, keyword_length,
			      &(args->segment->string_parameter[keyword_start]), numeric_value);

			if (numeric_value >= 0 && numeric_value < 255) {
				code = numeric_value;
			} else {
				int keyword_index;
				for (keyword_index = 0; -1 == code && NULL != keywords[keyword_index].keyword;
				     keyword_index++) {
					if (keyword_length != keywords[keyword_index].bytes)
						continue;
					if (0 !=
					    strncmp(keywords[keyword_index].keyword,
						    &(args->segment->string_parameter[keyword_start]), keyword_length))
						continue;
					code = (int) (keywords[keyword_index].code);
				}
			}

			debug("code=%d", code);

			if (code >= 0) {
				if (code_count > 15) {
					write_position +=
					    pv_snprintf(content + write_position, sizeof(content) - write_position,
							"%s", "m");
					code_count = 0;
				}
				if (0 == code_count) {
					write_position +=
					    pv_snprintf(content + write_position, sizeof(content) - write_position,
							"%s", "\033[");
				} else {
					write_position +=
					    pv_snprintf(content + write_position, sizeof(content) - write_position,
							"%s", ";");
				}
				write_position +=
				    pv_snprintf(content + write_position, sizeof(content) - write_position, "%d", code);
				code_count++;
				most_recent_code = code;
			}

			keyword_length = 0;
			keyword_start = read_position;
		}
	}

	if (code_count > 0)
		write_position += pv_snprintf(content + write_position, sizeof(content) - write_position, "%s", "m");

	if (most_recent_code > 0) {
		args->display->sgr_code_active = true;
	} else if (0 == most_recent_code) {
		args->display->sgr_code_active = false;
	}

	return pv_formatter_segmentcontent(content, args);
}
