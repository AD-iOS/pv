/*
 * Functions for portably managing strings.
 *
 * Copyright 2023-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include "pv.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#if defined(ENABLE_NLS) && defined(HAVE_WCHAR_H)
#include <wchar.h>
#if defined(HAVE_WCTYPE_H)
#include <wctype.h>
#endif
#endif


/*
 * Wrapper for sprintf(), falling back to sprintf() on systems without that
 * function.
 *
 * Returns -1 if "str" or "format" are NULL or if "size" is 0.
 *
 * Otherwise, ensures that the buffer "str" is always terminated with a '\0'
 * byte, before returning whatever the system's vsnprintf() or vsprintf()
 * returned.
 */
int pv_snprintf(char *str, size_t size, const char *format, ...)
{
	va_list ap;
	int ret;

	if (NULL == str)
		return -1;
	if (0 == size)
		return -1;
	if (NULL == format)
		return -1;

	str[0] = '\0';

	va_start(ap, format);
#ifdef HAVE_VSNPRINTF
	ret = vsnprintf(str, size, format, ap);	/* flawfinder: ignore */
#else				/* ! HAVE_VSNPRINTF */
	ret = vsprintf(str, format, ap);    /* flawfinder: ignore */
#endif				/* HAVE_VSNPRINTF */
	va_end(ap);

	str[size - 1] = '\0';

	/*
	 * flawfinder rationale: this function replaces snprintf so
	 * explicitly takes a non-constant format; also it explicitly
	 * \0-terminates the output buffer, as flawfinder warns that some
	 * sprintf() variants do not.
	 */

	return ret;
}

/*
 * Implementation of strlcat() where it is unavailable: append a string to a
 * buffer, constraining the buffer to a particular size and ensuring
 * termination with '\0'.
 *
 * Appends the string "src" to the buffer "dst", assuming "dst" is "dstsize"
 * bytes long, and ensuring that "dst" is always terminated with a '\0'
 * byte.
 *
 * Returns the intended length of the string, not including the terminating
 * '\0', i.e.  strlen(src)+strlen(dst), regardless of whether truncation
 * occurred.
 *
 * Note that this implementation has the side effect that "dst" will always
 * be terminated with a '\0' even if "src" was zero bytes long.
 */
size_t pv_strlcat(char *dst, const char *src, size_t dstsize)
{
#ifdef HAVE_STRLCAT
	size_t result;

	result = strlcat(dst, src, dstsize);

	if ((NULL != dst) && (dstsize > 0))
		dst[dstsize - 1] = '\0';

	return result;
#else
	size_t dstlen, srclen, available;

	if (NULL == dst)
		return 0;
	if (NULL == src)
		return 0;
	if (0 == dstsize)
		return 0;

	dst[dstsize - 1] = '\0';
	dstlen = strlen(dst);		    /* flawfinder: ignore */
	srclen = strlen(src);		    /* flawfinder: ignore */

	/*
	 * flawfinder rationale: src must explicitly be \0 terminated, so
	 * this is up to the caller; with dst, we enforce \0 termination
	 * before calling strlen().
	 */

	available = dstsize - dstlen;
	if (available > 1)
		(void) pv_snprintf(dst + dstlen, available, "%.*s", available - 1, src);

	return dstlen + srclen;
#endif
}


/*
 * Allocate and return a duplicate of a \0-terminated string, ensuring that
 * the duplicate is also \0-terminated.  Returns NULL on error.
 */
/*@null@ */
/*@only@ */
char *pv_strdup(const char *original)
{
	size_t length;
	char *duplicate;

	if (NULL == original) {
		errno = EINVAL;
		return NULL;
	}

	length = strlen(original);	    /* flawfinder: ignore */
	/*
	 * flawfinder rationale: the original string is explicitly required
	 * to be \0 terminated.
	 */
	duplicate = calloc(1, 1 + length);
	if (NULL == duplicate)
		return NULL;

	memcpy(duplicate, original, length);	/* flawfinder: ignore */
	/*
	 * flawfinder rationale: the buffer is explicitly allocated to be
	 * large enough.
	 */

	duplicate[length] = '\0';

	return duplicate;
}


/*
 * Return a pointer to the last matching character in the buffer, or NULL if
 * not found.
 */
/*@null@ */
/*@temp@ */
void *pv_memrchr(const void *buffer, int match, size_t length)
{
#ifdef HAVE_MEMRCHR
	/*@-unrecog@ *//* splint doesn't know of memrchr() */
	return memrchr(buffer, match, length);
	/*@+unrecog@ */
#else
	unsigned char *ptr;

	if (length < 1)
		return NULL;

	ptr = ((unsigned char *) buffer) + length - 1;
	while (ptr >= (unsigned char *) buffer) {
		if ((int) (ptr[0]) == match)
			return (void *) ptr;
		ptr--;
	}

	return NULL;
#endif
}


/*
 * Return the number of display columns needed to show the
 * non-null-terminated string "string" whose length in bytes is "bytes".
 *
 * Skips ECMA-48 CSI (ESC [ ...) sequences, but any other control characters
 * are treated as printable.
 *
 * To do this, we convert it to a wide character string, and use the wide
 * character display width function "wcswidth()" on it.
 *
 * If NLS is disabled, or the string cannot be converted, this just returns
 * the value of "bytes".
 *
 * Note that this function uses internal buffers if the string is short
 * enough, otherwise it has to call malloc() and free(), so it becomes less
 * efficient with larger strings.
 */
size_t pv_strwidth(const char *string, size_t bytes)
{
	char *allocated_raw = NULL;
	static char internal_raw[256];	 /* flawfinder: ignore - bounded */
	char *raw_string = NULL;
	size_t read_pos, write_pos;
	size_t raw_bytes, width;
#if defined(ENABLE_NLS) && defined(HAVE_WCHAR_H)
	size_t wide_char_count;
	size_t wide_string_buffer_size;
	wchar_t *allocated_wide = NULL;
	static wchar_t internal_wide[256];	/* flawfinder: ignore - bounded */
	wchar_t *wide_string = NULL;
#endif				/* defined(ENABLE_NLS) && defined(HAVE_WCHAR_H) */

	if (NULL == string)
		return 0;
	if (0 == bytes)
		return 0;

	if (bytes < sizeof(internal_raw) - 1) {
		raw_string = internal_raw;
	} else {
		allocated_raw = calloc(1, 1 + bytes);
		if (NULL == allocated_raw)
			return bytes;
		raw_string = allocated_raw;
	}

	/* Copy the original string, skipping ECMA-48 CSI sequences. */
	for (read_pos = 0, write_pos = 0; read_pos < bytes; read_pos++) {
		if ((string[read_pos] != '\033') || (read_pos >= bytes - 1) || (string[read_pos + 1] != '[')) {
			raw_string[write_pos++] = string[read_pos];
			continue;
		}
		read_pos += 2;
		while ((read_pos < bytes - 1)
		       && ((string[read_pos] >= '0' && string[read_pos] <= '9')
			   || (';' == string[read_pos])
		       )
		    ) {
			read_pos++;
		}
	}
	raw_string[write_pos] = '\0';
	raw_bytes = write_pos;

	width = raw_bytes;

#if defined(ENABLE_NLS) && defined(HAVE_WCHAR_H)
	/*@-nullpass@ */
	/*
	 * splint note: mbstowcs() manual page on Linux explicitly says it
	 * takes NULL.
	 */
	wide_char_count = mbstowcs(NULL, raw_string, 0);
	/*@+nullpass@ */
	if (wide_char_count == (size_t) -1) {
		debug("%s: %s: %s", "mbstowcs", raw_string, strerror(errno));
		if (NULL != allocated_raw)
			free(allocated_raw);
		return raw_bytes;
	}

	wide_string_buffer_size = sizeof(*wide_string) * (1 + wide_char_count);

	if (wide_string_buffer_size < sizeof(internal_wide)) {
		wide_string = internal_wide;
	} else {
		allocated_wide = malloc(wide_string_buffer_size);
		if (NULL == allocated_wide) {
			perror("malloc");
			if (NULL != allocated_raw)
				free(allocated_raw);
			return raw_bytes;
		}
		wide_string = allocated_wide;
	}
	memset(wide_string, 0, wide_string_buffer_size);

	if (mbstowcs(wide_string, raw_string, 1 + wide_char_count) == (size_t) -1) {
		debug("%s: %s: %s", "mbstowcs", raw_string, strerror(errno));
	} else if (NULL != wide_string) {
		/*@-unrecog@ *//* splint seems unable to see the prototype. */
		width = wcswidth(wide_string, wide_char_count);
		/*@+unrecog@ */
	} else {
		width = 0;
	}

	if (NULL != allocated_wide)
		free(allocated_wide);
#endif				/* defined(ENABLE_NLS) && defined(HAVE_WCHAR_H) */

	if (NULL != allocated_raw)
		free(allocated_raw);

	return width;
}


/*
 * Return true if the character is printable.  This function is used instead
 * of the macro from <ctype.h> to avoid causing versioned glibc dependencies
 * on some systems.
 */
bool pv_isprint(char c)
{
	return ((c >= (char) 32) && (c <= (char) 126)) ? true : false;
}
