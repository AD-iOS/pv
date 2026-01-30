/*
 * Output version information to stdout.
 *
 * Copyright 2002-2008, 2010, 2012-2015, 2017, 2021, 2023-2025 Andrew Wood
 *
 * License GPLv3+: GNU GPL version 3 or later; see `docs/COPYING'.
 */

#include "config.h"
#include <stdio.h>

/*
 * Display current package version as per GNU standards.
 */
void display_version(void)
{
	/*@-mustfreefresh@ */
	/*
	 * splint note: the gettext calls made by _() cause memory leak
	 * warnings, but in this case it's unavoidable, and mitigated by the
	 * fact we only translate each string once.
	 */
	/* GNU standard first line format: program and version only */
	printf("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
	/* GNU standard second line format - "Copyright" always in English */
	printf("Copyright %s %s\n", "2025", "Andrew Wood");
	/* GNU standard license line and free software notice */
	printf("%s\n", _("License: GPLv3+ <https://www.gnu.org/licenses/gpl-3.0.html>"));
	printf("%s\n", _("This is free software: you are free to change and redistribute it."));
	printf("%s\n", _("There is NO WARRANTY, to the extent permitted by law."));
	/* Project web site link */
	printf("\n%s: <%s>\n", _("Project web site"), PACKAGE_URL);
}
