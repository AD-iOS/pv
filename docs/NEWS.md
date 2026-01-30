### 1.10.3 - 15 December 2025

 * *fix:* stop truncating the process title set by **--extra-display**

### 1.10.2 - 22 November 2025

 * *i18n:* Finnish translations updated
 * *cleanup:* adjust valgrind suppressions so memory safety tests work on LTO builds ([#174](https://codeberg.org/ivarch/pv/issues/174)/[Debian bug 1121157](https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=1121157))

### 1.10.1 - 12 November 2025

 * *fix:* correct **--watchfd** memory allocation error when given multiple arguments ([#170](https://codeberg.org/ivarch/pv/issues/170))

### 1.10.0 - 7 November 2025

 * *feature:* new **--query** option to show the transfer status of another running **pv** ([#101](https://codeberg.org/ivarch/pv/issues/101))
 * *fix:* make the timers work consistently in **--watchfd** if paused and resumed ([#169](https://codeberg.org/ivarch/pv/issues/169))

### 1.9.44 - 18 October 2025

 * *feature:* new **--sparse** option to write sparse output ([#45](https://codeberg.org/ivarch/pv/issues/45))
 * *fix:* don't clear the **--watchfd** display when the watched process ends ([#81](https://codeberg.org/ivarch/pv/issues/81))

### 1.9.42 - 11 October 2025

 * *feature:* permit multiple arguments to **--watchfd** ([#12](https://codeberg.org/ivarch/pv/issues/12))
 * *feature:* new "**=NAME**" syntax for **--watchfd** to watch processes by name using **pgrep**(1) ([#95](https://codeberg.org/ivarch/pv/issues/95))
 * *feature:* new "**@LISTFILE**" syntax for **--watchfd** to watch processes listed in a file
 * *fix:* improve support for changing the format of a **--watchfd** process with **--remote**
 * *fix:* don't read more than **--size** bytes when using **--stop-at-size** ([#166](https://codeberg.org/ivarch/pv/issues/166))
 * *fix:* transfer nothing if **--stop-at-size** is given a **--size** of 0 ([#166](https://codeberg.org/ivarch/pv/issues/166))
 * *i18n:* Finnish translation stub added
 * *i18n:* Turkish translations updated

### 1.9.34 - 26 July 2025

 * *fix:* prevent tight loop consuming CPU when waiting for a partially filled output pipe to be drained ([#164](https://codeberg.org/ivarch/pv/issues/164))
 * *fix:* correct a memory handling fault when using **--bar-style** with **--watchfd** ([#163](https://codeberg.org/ivarch/pv/issues/163))
 * *i18n:* French translations updated

### 1.9.31 - 28 January 2025

 * *feature:* the **--format** option can now be used with **--numeric** for customised numeric output, such as JSON ([#127](https://codeberg.org/ivarch/pv/issues/127))
 * *i18n:* German translations updated
 * *i18n:* Polish translations updated
 * *i18n:* Russian translations updated

### 1.9.27 - 12 January 2025

 * *fix:* turn off IPC support if _sys/shm.h_ is not available, for compilation on Termux
 * *fix:* bypass valgrind checks on ARM by default due to false positives

### 1.9.25 - 22 December 2024

 * *fix:* test failure of **--watchfd** on macOS corrected ([#124](https://codeberg.org/ivarch/pv/issues/124))

### 1.9.24 - 19 December 2024

 * *feature:* new **--format** sequences for graphical progress bars - "**%{bar-block}**", "**%{bar-granular}**", and "**%{bar-shaded}**" ([#15](https://codeberg.org/ivarch/pv/issues/15))
 * *feature:* new **--format** sequence "**%{sgr:colour,...}**" to use ECMA-48 Select Graphic Rendition codes to add colours
 * *feature:* new **--bar-style** option to change the default bar style ([#15](https://codeberg.org/ivarch/pv/issues/15))
 * *feature:* allow decimal values such as "1.5G" with "**--size**", "**--rate-limit**", "**--buffer-size**", and "**error-skip-block**" ([#35](https://codeberg.org/ivarch/pv/issues/35))
 * *cleanup:* improve progress bar granularity on wide displays by internally tracking the transfer percentage as a decimal number
 * *cleanup:* correct detection of **--remote** usability on GNU Hurd
 * *cleanup:* reduce likelihood of race conditions in tests when running on slow systems
 * *cleanup:* reduce memory footprint
 * *docs:* simplified the synopsis section of the manual

### 1.9.15 - 8 December 2024

 * *feature:* new **--format** sequence "**%nL**", showing the most recent line written ([#121](https://codeberg.org/ivarch/pv/issues/121))
 * *feature:* each **--format** sequence now has a more readable equivalent name, for example "**%r**" can be written as "**%{rate}**"
 * *feature:* new **--format** sequences "**%{progress-bar-only}**" and "**%{progress-amount-only}**"
 * *fix:* allow **--format** to include "%nA" more than once, with different "n" values ([#122](https://codeberg.org/ivarch/pv/issues/122))
 * *fix:* allow **--format** to include "%p" more than once, with optional width prefix
 * *fix:* calculate width correctly when wide characters are in **--format** strings
 * *fix:* add _configure_ script fallback for **--remote** check when cross-compiling ([#120](https://codeberg.org/ivarch/pv/issues/120))
 * *fix:* allow **extra-display** to be changed by **--remote** ([#123](https://codeberg.org/ivarch/pv/issues/123))
 * *cleanup:* refactored display formatters into separate functions
 * *cleanup:* improve format parser handling of dangling or invalid "%" sequences

### 1.9.7 - 2 December 2024

 * *feature:* new **--extra-display** option to update window and process titles ([#3](https://codeberg.org/ivarch/pv/issues/3), [#4](https://codeberg.org/ivarch/pv/issues/4))
 * *fix:* correct failure to report file positions in **--watchfd** ([#118](https://codeberg.org/ivarch/pv/issues/118))
 * *i18n:* Russian translations added
 * *cleanup:* tests added for **--watchfd** ([#10](https://codeberg.org/ivarch/pv/issues/10))
 * *cleanup:* worked around file descriptor leak false positives in valgrind 3.23 ([#97](https://codeberg.org/ivarch/pv/issues/97))
 * *cleanup:* cleared all *shellcheck* warnings in the test scripts
 * *cleanup:* check at compile-time whether **--remote** is going to be usable ([#119](https://codeberg.org/ivarch/pv/issues/119))

### 1.9.0 - 15 October 2024

 * *feature:* new **--store-and-forward** option to read input to a file first, then write it to the output ([#100](https://codeberg.org/ivarch/pv/issues/100))
 * *feature:* new **--stats** option to show transfer stats at the end, like "`ping`" ([#49](https://codeberg.org/ivarch/pv/issues/49))
 * *feature:* **--rate** can now be used with **--numeric** ([#17](https://codeberg.org/ivarch/pv/issues/17))
 * *feature:* **--gauge** with **--progress** to show rate gauge when size is unknown ([#46](https://codeberg.org/ivarch/pv/issues/46))
 * *i18n:* comprehensive German translations update
 * *i18n:* comprehensive Polish translations update
 * *i18n:* complete Turkish translations added
 * *i18n:* complete Czech translations added
 * *i18n:* updates to French translations
 * *fix:* resume stopped pipelines when running in the background (part of [#56](https://codeberg.org/ivarch/pv/issues/56))
 * *fix:* inspect the output pipe buffer to give a more accurate progress indicator of how much the next command has consumed
 * *fix:* prefix completion time (**--fineta**) with *FIN* rather than *ETA* ([#43](https://codeberg.org/ivarch/pv/issues/43))
 * *fix:* surround average rate (**--average-rate**) with brackets rather than square brackets
 * *fix:* correct a memory leak in **--watchfd PID**
 * *fix:* make **--direct-io** work correctly with **--output** instead of assuming stdout
 * *fix:* call `posix_fadvise()` on every input, not just the first one
 * *fix:* write UTC timestamps in debugging mode to avoid lockups in signal handlers
 * *security:* added a signed *MANIFEST* file to releases
 * *cleanup:* removed TODO.md, since it's just an outdated copy of the issue tracker
 * *cleanup:* re-ordered structure members to reduce padding
 * *cleanup:* improved readability of *SIGTTOU* handling code
 * *cleanup:* refactored to separate display, transfer, and calculation more cleanly
 * *cleanup:* instead of moving stderr when backgrounded, set a suspend-output flag

### 1.8.14 - 7 September 2024

 * *fix:* correct double-free on exit when using **--watchfd** ([#96](https://codeberg.org/ivarch/pv/issues/96)) reported by [jettero](https://codeberg.org/jettero)

### 1.8.13 - 18 August 2024

 * *feature:* when using **--size @FILE**, *FILE* can be a block device, and its size will be used (pull request [#94](https://codeberg.org/ivarch/pv/pulls/94)) supplied by [alexanderperlis](https://codeberg.org/alexanderperlis)

### 1.8.12 - 18 July 2024

 * *fix:* correct the detection of output block device size that was broken in 1.8.10 ([#91](https://codeberg.org/ivarch/pv/issues/91))
 * *fix:* do not treat a zero/interrupted write as an end of file (pull requests [#92](https://codeberg.org/ivarch/pv/pulls/92) and [#93](https://codeberg.org/ivarch/pv/pulls/93))

### 1.8.10 - 15 June 2024

 * *feature:* new **--output** option to write to a file instead of standard output (pull request [#90](https://codeberg.org/ivarch/pv/pulls/90)) supplied by [xmort](https://codeberg.org/xmort)

### 1.8.9 - 21 April 2024

 * *feature:* new **--si** option to display and interpret size suffixes in multiples of 1000 rather than 1024 (pull request [#85](https://codeberg.org/ivarch/pv/pulls/85)) supplied by [kevinruddy](https://codeberg.org/kevinruddy)
 * *fix:* continue producing progress output when the output is blocking writes ([#34](https://codeberg.org/ivarch/pv/issues/34), [#86](https://codeberg.org/ivarch/pv/issues/86), [#87](https://codeberg.org/ivarch/pv/issues/87))
 * *fix:* honour the *TMPDIR* / *TMP* environment variables again, rather than hard-coding "`/tmp`", when using a terminal lock file (originally removed in 1.8.0) ([#88](https://codeberg.org/ivarch/pv/issues/88))
 * *i18n:* corrections and missing strings added to French translations (pull request [#83](https://codeberg.org/ivarch/pv/pulls/83)) supplied by [Thomas Bertels](https://codeberg.org/tbertels)

### 1.8.5 - 19 November 2023

 * *fix:* corrected percentage formatting so it doesn't jump from 2 to 3 characters wide at 100% ([#80](https://codeberg.org/ivarch/pv/issues/80))
 * *fix:* replaced **--remote** mechanism, using a temporary file instead of SysV IPC, so it can work reliably even when there are multiple PV instances
 * *fix:* corrected compilation failure when without IPC support
 * *security:* addressed all issues highlighted by the software auditing tools "`splint`" and "`flawfinder`" (see "`make analyse`") ([#77](https://codeberg.org/ivarch/pv/issues/77))
 * *cleanup:* compilation warnings fixed on non-IPC and MacOS systems

### 1.8.0 - 24 September 2023

#### Features

 * *feature:* new **--discard** option to discard input as if writing to */dev/null* ([#42](https://codeberg.org/ivarch/pv/issues/42))
 * *feature:* new **--error-skip-block** option to make **--skip-errors** skip whole blocks ([#37](https://codeberg.org/ivarch/pv/issues/37))
 * *feature:* use `posix_fadvise()` like `cat`(1) does, to improve efficiency ([#39](https://codeberg.org/ivarch/pv/issues/39))
 * *feature:* new **--enable-static** option to "`configure`" for static builds ([#75](https://codeberg.org/ivarch/pv/pull/75))

#### Security

 * *security:* with **--pidfile**, write to a temporary file and rename it into place, to improve security
 * *security:* keep self-contained copies of name and format string in PV internal state for memory safety
 * *security:* ignore *TMP* / *TMPDIR* environment variables when using a terminal lock file

#### Fixes

 * *fix:* only report errors about missing files when starting to transfer from them, not while calculating size, and behave more like `cat`(1) by skipping them and moving on
 * *fix:* auto-calculate total line count with **--line-mode** when all inputs are regular files
 * *fix:* use `clock_gettime()` in ETA calculation to cope with machine suspend/resume ([#13](https://codeberg.org/ivarch/pv/issues/13))
 * *fix:* if **--width** or **--height** were provided, do not change them when the window size changes ([#36](https://codeberg.org/ivarch/pv/issues/36))
 * *fix:* when a file descriptor position in **--watchfd** moves backwards, show the rate using the correct prefix ([#41](https://codeberg.org/ivarch/pv/issues/41))
 * *fix:* rewrite terminal state save/restore so state is not intermittently garbled on exit when using **--cursor** ([#20](https://codeberg.org/ivarch/pv/issues/20)), ([#24](https://codeberg.org/ivarch/pv/issues/24))

#### Cleanups

 * *cleanup:* addressed many potential issues highlighted by the software auditing tools "`splint`" and "`flawfinder`" (see new target "`make analyse`")
 * *cleanup:* switched the build system to GNU Automake
 * *cleanup:* replaced the test harness with the one native to GNU Automake
 * *cleanup:* added a test for terminal width detection to "`make check`"
 * *cleanup:* added a test to "`make check`" to ensure that "`make install`" installs everything expected
 * *cleanup:* replaced *AC_HEADER_TIOCGWINSZ* with *AC_CHECK_HEADERS(sys/ioctl.h)* for better MacOS compatibility ([#74](https://codeberg.org/ivarch/pv/issues/74))
 * *cleanup:* with **--sync**, call `fsync()` instead of `fdatasync()` on incapable systems ([#73](https://codeberg.org/ivarch/pv/issues/73))
 * *cleanup:* the manual is now a static file instead of needing to be built with "`configure`"

#### Dropped items

 * *dropped:* dropped support for **--enable-static-nls**
 * *dropped:* removed the Linux Software Map file, as the LSM project appears to be long dead
 * *dropped:* will no longer publish to SourceForge as it has a chequered history and is unnecessary
 * *dropped:* removed project from GitHub and moved to Codeberg - see "[Give Up GitHub](https://giveupgithub.org/)"

#### Other items

 * licensing change from Artistic 2.0 to GPLv3+

### 1.7.24 - 30 July 2023

 * *fix:* correct terminal size detection, broken in 1.7.17 by the configuration script rewrite ([#72](https://codeberg.org/ivarch/pv/issues/72))
 * *security:* removed *DEBUG* environment variable in debug mode, added **--debug** instead
 * *cleanup:* added "`make analyse`" to run "`splint`" and "`flawfinder`" on all source files
 * *cleanup:* corrected detection of boolean capability
 * *cleanup:* word wrapping of **--help** output is now multi-byte locale aware
 * *cleanup:* adjusted "`indent`" rules to line length of 120 and reformatted code

### 1.7.18 - 28 July 2023

 * *fix:* language file installation had been broken by the configuration script rewrite

### 1.7.17 - 27 July 2023

 * *feature:* new **--sync** option to flush cache to disk after every write (related to [#6](https://codeberg.org/ivarch/pv/issues/6), to improve accuracy when writing to slow disks)
 * *feature:* new **--direct-io** option to bypass cache - implements [#29 "Option to enable O_DIRECT"](https://codeberg.org/ivarch/pv/issues/29) - requested by Romain Kang, Jacek Wielemborek
 * *fix:* correct byte prefix size to 2 spaces in rate display, so progress display size remains constant at low transfer rates
 * *cleanup:* rewrote `configure.in` as per suggestions in newer "`autoconf`" manuals
 * *cleanup:* replaced `header.in` with one generated by "`autoheader`", moving custom logic to a separate header file "`config-aux.h`"
 * *cleanup:* added copyright notice to all source files as per GNU standards
 * *cleanup:* changed **--version** output to conform to GNU standards
 * *cleanup:* replaced backticks with `$()` in all shell scripts that did not come from elsewhere, as backticks are deprecated and harder to read
 * *cleanup:* improved the output formatting of "`make test`"
 * *cleanup:* extended the "`make test`" mechanism to allow certain tests to be skipped on platforms that cannot support them
 * *cleanup:* skip the "pipe" test (for *SIGPIPE*) if GNU "`head`" is not available, so that "`make test`" on stock OpenBSD 7.3 works
 * *cleanup:* added a lot more tests to "`make test`"
 * *cleanup:* replace all calls to `sprintf()` and `snprintf()` with a new wrapper function `pv_snprintf()` to improve security and compatibility
 * *cleanup:* replace all calls to `strcat()` with a wrapper `pv_strlcat()` to improve security and compatibility
 * *cleanup:* replace all `write()` calls to the terminal with a wrapper `pv_write_retry()` for consistency
 * *cleanup:* tidy up and fix compilation warning in **--watchfd** code
 * *cleanup:* rewrote all local shell scripts to pass analysis by [ShellCheck](https://www.shellcheck.net)

### 1.7.0 - 17 July 2023

 * *dropped:* support for Red Hat Enterprise Linux and its derivatives has been dropped; removed the RPM spec file, and will no longer build binaries
 * *feature:* the **--size** option now accepts "`@filename`" to use the size of another file (pull request [#57](https://codeberg.org/ivarch/pv/pull/57) supplied by [Dave Beckett](https://github.com/dajobe))
 * *feature:* the **--watchfd** option is now available on OS X (pull request [#60](https://codeberg.org/ivarch/pv/pull/60) supplied by [christoph-zededa](https://github.com/christoph-zededa))
 * *feature:* new **--bits** option to show bit count instead of byte count (adapted from pull request [#63](https://codeberg.org/ivarch/pv/pull/63) supplied by [Nick Black](https://nick-black.com))
 * *feature:* new **--average-rate-window** option, to set the window over which the average rate is calculated, also used for ETA (modified from pull request [#65](https://codeberg.org/ivarch/pv/pull/65) supplied by [lemonsqueeze](https://github.com/lemonsqueeze))
 * *feature:* the **--watchfd** option will now show relative filenames, if they are under the current directory (pull request [#66](https://codeberg.org/ivarch/pv/pull/66) supplied by [ikasty](https://github.com/ikasty))
 * *fix:* correction to `pv_in_foreground()` to behave as its comment block says it should, when not on a terminal - corrects [#19 "No output in Arch Linux initcpio after 1.6.6"](https://codeberg.org/ivarch/pv/issues/19), [#31 "No output written from inside zsh <() construct"](https://codeberg.org/ivarch/pv/issues/31), [#55 "pv Stopped Working in the Background"](https://codeberg.org/ivarch/pv/issues/55) (pull request [#64](https://codeberg.org/ivarch/pv/pull/64) supplied by [Michael Weiß](https://github.com/quitschbo))
 * *fix:* workaround for OS X 11 behaviour in configure script regarding stat64 at compile time (pull request [#57](https://codeberg.org/ivarch/pv/pull/57) supplied by [Dave Beckett](https://github.com/dajobe))
 * *fix:* workaround for macOS equivalence of stat to stat64 - patches from [Filippo Valsorda](https://github.com/FiloSottile) and [Demitri Muna](https://github.com/demitri), correcting [#33 "Fix compilation problems due to `stat64()` on Apple Silicon"](https://codeberg.org/ivarch/pv/issues/33)
 * *fix:* add burst rate limit to transfer, so rate limits are not broken by bursty traffic (pull request [#62](https://codeberg.org/ivarch/pv/pull/62) supplied by [Volodymyr Bychkovyak](https://github.com/vbychkoviak))
 * *fix:* corrected **--force** option so it will still output progress when not in the same process group as the owner of the terminal - corrects [#23 "No output with "-f" when run in background after 1.6.6"](https://codeberg.org/ivarch/pv/issues/23) and helps to correct [#31 "No output written from inside zsh <() construct"](https://codeberg.org/ivarch/pv/issues/31)
 * *fix:* corrected elapsed time display to show as D:HH:MM:SS after 1 day, like the ETA does - corrects [#16 "Show days in same format in ETA as in elapsed time"](https://codeberg.org/ivarch/pv/issues/16)
 * *fix:* corrected bug where percentages went down after 100% when in **--numeric** mode with a **--size** that was too small - corrects [#26 "Correct "-n" behaviour when going past 100% of "-s" size"](https://codeberg.org/ivarch/pv/issues/26)
 * *i18n:* recoded Polish translation file to UTF-8
 * *i18n:* removed inaccurate fuzzy translation matches
 * *docs:* moved all open issues into GitHub and updated the TODO list
 * *docs:* renamed README to README.md and altered it to Markdown format
 * *docs:* moved contributors from the README to docs/ACKNOWLEDGEMENTS.md
 * *docs:* moved TODO to TODO.md and altered it to Markdown format
 * *docs:* moved NEWS to NEWS.md, converted it to UTF-8, and altered it to Markdown format

### 1.6.20 - 12 September 2021

 * *fix:* add missing `stddef.h` include to `number.c` (Sam James)

### 1.6.19 - 5 September 2021

 * *fix:* starting pv in the background no longer immediately stops unless the transfer is to/from the terminal (Andriy Gapon, Jonathan Elchison)
 * *fix:* using **-B**, **-A**, or **-T** now switches on **-C** implicitly (Johannes Gerer, André Stapf)
 * *fix:* AIX build fixes (Peter Korsgaard)
 * *i18n:* updated German **--help** translations (Richard Fonfara)
 * *i18n:* switched to UTF-8 encoding, added missing translations (de,fr,pt)
 * *docs:* new "common switches" manual section (Jacek Wielemborek)
 * *docs:* use placeholder instead of `/dev/sda` in the manual (Pranav Peshwe)
 * *docs:* mention MacOS pipes and **-B 1024** in the manual (Jan Venekamp)
 * *docs:* correct shell in `autoconf/scripts/index.sh` (Juan Picca)
 * *cleanup:* various compiler warnings cleaned up

Full changelog is below:

 * (r181) added common switches section to manual (Jacek Wielemborek)
 * (r184) use placeholder instead of /dev/sda in the manual (Pranav Peshwe)
 * (r185) replace ash with sh in autoconf/scripts/index.sh (Juan Picca)
 * (r185) added note to manual about **-B 1024** in MacOS pipes (Jan Venekamp)
 * (r185) fix AIX config check when the CWD contains "yes" (Peter Korsgaard)
 * (r189) (#1556) updated German **--help** translations (Richard Fonfara)
 * (r189) updated missing German translations and changed to UTF-8 encoding
 * (r191) updated missing French translations and changed to UTF-8 encoding
 * (r193) updated missing Portuguese translations, changed to UTF-8 encoding
 * (r196) (#1563) using **-B**, **-A**, or **-T** now switches on **-C** implicitly (Johannes Gerer, André Stapf)
 * (r199) fixed numerous compiler warnings in newer GCC versions
 * (r200,205) fixed bug where "`pv /dev/zero >/dev/null &`" stopped immediately (Jonathan Elchison, Andriy Gapon)
 * (r203,205) marked unused arguments with GCC unused attribute, started using boolean data type for flags, corrected more compiler warnings

### 1.6.6 - 30 June 2017

 * (r161) use `%llu` instead of `%Lu` for better compatibility (Eric A. Borisch)
 * (r162) (#1532) fix target buffer size (**-B**) being ignored (AndCycle, Ilya Basin, Antoine Beaupré)
 * (r164) cap read/write sizes, and check elapsed time during read/write cycles, to avoid display hangs with large buffers or slow media; also remove `select()` call from repeated_write function as it slows the transfer down and the wrapping `alarm()` means it is unnecessary
 * (r169) (#1477) use alternate form for transfer counter, such that 13GB is shown as 13.0GB so it's the same width as 13.1GB (André Stapf)
 * (r171) cleanup: units corrections in man page, of the form kb -> KiB
 * (r175) report error in **-d** if process fd directory is unreadable, or if process disappears before we start the main loop (Jacek Wielemborek)

### 1.6.0 - 15 March 2015

 * fix lstat64 support when unavailable - separate patches supplied by Ganael Laplanche and Peter Korsgaard
 * (#1506) new option **-D** / **--delay-start** to only show bar after N seconds (Damon Harper)
 * new option **--fineta** / **-I** to show ETA as time of day rather than time remaining - patch supplied by Erkki Seppälä (r147)
 * (#1509) change ETA (**--eta** / **-e**) so that days are given if the hours remaining are 24 or more (Jacek Wielemborek)
 * (#1499) repeat read and write attempts on partial buffer fill/empty to work around post-signal transfer rate drop reported by Ralf Ramsauer
 * (#1507) do not try to calculate total size in line mode, due to bug reported by Jacek Wielemborek and Michiel Van Herwegen
 * cleanup: removed defunct RATS comments and unnecessary copyright notices
 * clean up displayed lines when using **--watchfd PID**, when PID exits
 * output errors on a new line to avoid overwriting transfer bar

### 1.5.7 - 26 August 2014

 * show KiB instead of incorrect kiB (Debian bug #706175)
 * (#1284) do not gzip man page, for non-Linux OSes (Bob Friesenhahn)
 * work around "awk" bug in `tests/016-numeric-timer` in decimal "," locales
 * fix "`make rpm`" and "`make srpm`", extend "`make release`" to sign releases

### 1.5.3 - 4 May 2014

 * remove *SPLICE_F_NONBLOCK* to fix problem with slow `splice()` (Jan Seda)

### 1.5.2 - 10 February 2014

 * allow **--watchfd** to look at block devices
 * let **--watchfd PID:FD** work with **--size N**
 * moved contributors out of the manual as the list was too long (NB everyone is still listed in the README and always will be)

### 1.5.1 - 23 January 2014

 * new option **--watchfd** - suggested by Jacek Wielemborek and "fdwatch"
 * use non-block flag with `splice()`
 * new display option **--buffer-percent**, suggested by Kim Krecht
 * new display option **--last-written**, suggested by Kim Krecht
 * new transfer option **--no-splice**
 * fix for minor bug which dropped display elements after one empty one
 * fix for single fd leak on exit (Cristian Ciupitu)

### 1.4.12 - 5 August 2013

 * new option **--null** - patch supplied by Zing Shishak
 * AIX build fix (add "`-lc128`") - with help from Pawel Piatek
 * AIX **-c** fixes - with help from Pawel Piatek
 * SCO build fix (`po2table.sh`) - reported by Wouter Pronk
 * test scripts fix for older distributions - patch from Bryan Dongray
 * fix for `splice()` not using stdin - patch from Zev Weiss

### 1.4.6 - 22 January 2013

 * added patch from Pawel Piatek to omit *O_NOFOLLOW* in AIX

### 1.4.5 - 10 January 2013

 * updated manual page to show known problem with **-R** on Cygwin

### 1.4.4 - 11 December 2012

 * added debugging, see "`pv -h`" when `configure` is run with **--enable-debugging**
 * rewrote cursor positioning code used when IPC is unavailable (Cygwin)
 * fixed cursor positioning cursor read answerback problem (Cygwin/Solaris)
 * fixed bug causing crash when progress displayed with too-small terminal

### 1.4.0 - 6 December 2012

 * new option **--skip-errors** commissioned by Jim Salter
 * if stdout is a block device, and we don't know the total size, use the size of that block device as the total (Peter Samuelson)
 * new option **--stop-at-size** to stop after **--size** bytes
 * report correct filename on read errors
 * fix use-after-free bug in remote PID cleanup code
 * refactored large chunks of code to make it more readable and to replace most static variables with a state structure

### 1.3.9 - 5 November 2012

 * allow **--format** parameters to be sent with **--remote**
 * configure option **--disable-ipc**
 * added tests for **--numeric** with **--timer** and **--bytes**
 * added tests for **--remote**

### 1.3.8 - 29 October 2012

 * new **--pidfile** option to save process ID to a file
 * integrated patch for **--numeric** with **--timer** and **--bytes** (Sami Liedes)
 * removed signalling from **--remote** to prevent accidental process kills
 * new **--format** option (originally Vladimir Pal / Vladimir Ermakov)

### 1.3.4 - 27 June 2012

 * new **--disable-splice** configure script option
 * fixed line mode size count with multiple files (Moritz Barsnick)
 * fixes for AIX core dumps (Pawel Piatek)

### 1.3.1 - 9 June 2012

 * do not use `splice()` if the write buffer is not empty (Thomas Rachel)
 * added test 15 (pipe transfers), and new test script

### 1.3.0 - 5 June 2012

 * added Tiger build patch from Olle Jonsson
 * fix 1024-boundary display garble (Debian bug #586763)
 * use `splice`(2) where available (Debian bug #601683)
 * added known bugs section of the manual page
 * fixed average rate test, 12 (Andrew Macheret)
 * use IEEE1541 units (Thomas Rachel)
 * bug with rate limit under 10 fixed (Henry Precheur)
 * speed up PV line mode (patch: Guillaume Marcais)
 * remove `LD=ld` from `vars.mk` to fix cross-compilation (paintitgray/PV#1291)

### 1.2.0 - 14 December 2010

 * integrated improved SI prefixes and **--average-rate** (Henry Gebhardt)
 * return nonzero if exiting due to *SIGTERM* (Martin Baum)
 * patch from Phil Rutschman to restore terminal properly on exit
 * fix i18n especially for **--help** (Sebastian Kayser)
 * refactored `pv_display`
 * we now have a coherent, documented, exit status
 * modified pipe test and new cksum test from Sebastian Kayser
 * default *CFLAGS* to just "`-O`" for non-GCC (Kjetil Torgrim Homme)
 * LFS compile fix for OS X 10.4 (Alexandre de Verteuil)
 * remove *DESTDIR* `/` suffix (Sam Nelson, Daniel Pape)
 * fixed potential NULL deref in transfer (Elias Pipping / LLVM/Clang)

### 1.1.4 - 6 March 2008

 * patch from Elias Pipping correcting compilation failure on Darwin 9
 * patch from Patrick Collison correcting similar problems on OS X
 * trap *SIGINT* / *SIGHUP* / *SIGTERM* so we clean up IPCs on exit (Laszlo Ersek)
 * abort if numeric option, eg **-L**, has non-numeric value (Boris Lohner)

### 1.1.0 - 30 August 2007

 * new option **--remote** (**-R**) to control an already-running process
 * new option **--line-mode** (**-l**) to count lines instead of bytes
 * fix for **-L** to be less resource intensive
 * fix for input/output equivalence check on Mac OS X
 * fix for size calculation in pipelines on Mac OS X
 * fixed "`make uninstall`"
 * removed "`/debian`" directory at request of new Debian maintainer

### 1.0.1 - 4 August 2007

 * licensing change from Artistic to Artistic 2.0
 * removed the **-l** / **--license** option

### 1.0.0 - 2 August 2007

 * act more like "`cat`" - just skip unreadable files, don't abort
 * removed text version of manual page, and obsolete Info file generation
 * code cleanup and separation of PV internals from CLI front-end

### 0.9.9 - 5 February 2007

 * new option **--buffer-size** (**-B**) suggested by Mark Tomich
 * build fix: HP/UX largefile compile fix from Timo Savinen
 * maintain better buffer filling during transfers
 * workaround: "`pv /dev/zero | dd bs=1M count=1k`" bug (reported by Gert Menke)
 * dropped support for the Texinfo manual

### 0.9.6 - 27 February 2006

 * bugfix: `key_t` incompatibility with Cygwin
 * bugfix: interval (**-i**) parameter parses numbers after decimal point
 * build fix: use static NLS if `msgfmt` is unavailable
 * on the final update, blank out the now-zero ETA

### 0.9.2 - 1 September 2005

 * Daniel Roethlisberger patch: use lockfiles if terminal locking fails

### 0.9.1 - 16 June 2005

 * minor RPM spec file fix for Fedora Core 4

### 0.9.0 - 15 November 2004

 * minor NLS bugfix

### 0.8.9 - 6 November 2004

 * decimal values now accepted for rate and size, eg **-L 1.23M**
 * code cleanup
 * developers: "`make help`" now lists Makefile targets

### 0.8.6 - 29 June 2004

 * use `uu_lock()` for terminal locking on FreeBSD

### 0.8.5 - 2 May 2004

 * cursor positioning (**-c**) reliability improved on systems with IPC
 * minor fix: made test 005 more reliable
 * new option **--height** (**-H**)

### 0.8.2 - 24 April 2004

 * allow k,m,g,t suffixes on numbers
 * added "`srpm`" and "`release`" Makefile targets

### 0.8.1 - 19 April 2004

 * bugfix in cursor positioning (**-c**)

### 0.8.0 - 12 February 2004

 * replaced GNU getopt with my library code
 * replaced GNU gettext with my very minimal replacement
 * use *DESTDIR* instead of *RPM_BUILD_ROOT* for optional installation prefix
 * looked for flaws using RATS, cleaned up code

### 0.7.0 - 8 February 2004

 * display buffer management fixes (thanks Cédric Delfosse)
 * replaced **--enable-debug** with **--enable-debugging** and **--enable-profiling**

### 0.6.4 - 14 January 2004

 * fixed minor bug in RPM installation
 * bugfix in "`make index`" (only of interest to developers)

### 0.6.3 - 22 December 2003

 * fixed transient bug that reported "resource unavailable" occasionally

### 0.6.2 - 6 August 2003

 * block devices now have their size read correctly, so "`pv /dev/hda1`" works
 * minor code cleanups (mainly removal of CVS "Id" tags)

### 0.6.0 - 3 August 2003

 * doing *^Z* then "`bg`" then "`fg`" now continues displaying

### 0.5.9 - 23 July 2003

 * fix for test 007 when not in C locale
 * fix for build process to use *CPPFLAGS*
 * fix for build process to use correct i18n libraries
 * fix for build process - more portable sed in dependency generator
 * fix for install process - remember to `mkinstalldirs` before installing
 * fixes for building on Mac OS X

### 0.5.3 - 4 May 2003

 * added Polish translation thanks to Bartosz Feński <fenio@o2.pl> <http://skawina.eu.org/> and Krystian Zubel
 * moved `doc/debian` to `./debian` at insistence of common sense
 * minor Solaris 8 compatibility fixes
 * seems to compile and test OK on Mac OS X

### 0.5.0 - 15 April 2003

 * added French translation thanks to Stéphane Lacasse <stephane@gorfou.ca>
 * added German translation thanks to Marcos Kreinacke <public@kreinacke.com>
 * switched LGPL reference from "Library" to "Lesser"

### 0.4.9 - 18 February 2003

 * support for >2GB files added where available (Debian bug #180986)
 * added `doc/debian` dir (from Cédric Delfosse)
 * added "`make rpm`" and "`make deb`" targets to build RPM and Debian packages
 * added a "`make pv-static`" rule to build a statically linked version

### 0.4.5 - 13 December 2002

 * added Portuguese (Brazilian) translation thanks to Eduardo Aguiar

### 0.4.4 - 7 December 2002

 * pause/resume support - don't count time while stopped
 * stop output when resumed in the background
 * terminal size change support
 * bugfix: "`<=>`" indicator no longer sticks at right hand edge

### 0.4.0 - 27 November 2002

 * allow decimal interval values, eg 0.1, 0.5, etc
 * some simple tests added ("`make check`")
 * smoother throughput limiting (**--rate-limit**), now done in 0.1sec chunks
 * bounds-check interval values (**-i**) - max update interval now 10 minutes
 * more reliable non-blocking output to keep display updated
 * no longer rely on `atoll()`
 * don't output final blank line if **--numeric**
 * use `fcntl()` instead of `flock()` for Solaris compatibility

### 0.3.0 - 25 November 2002

 * handle broken output pipe gracefully
 * continue updating display even when output pipe is blocking

### 0.2.6 - 21 October 2002

 * we now ignore *EINTR* on `select()`
 * variable-size buffer (still need to add code to change size)
 * added (tentative) support for internationalisation
 * removed superfluous **--no-progress**, etc options
 * optimised transfer by using bigger buffers, based on `st_blksize`
 * added **--wait** option to wait until transfer begins before showing progress
 * added **--rate-limit** option to limit rate to a maximum throughput
 * added **--quiet** option (no output at all) to be used with **--rate-limit**

### 0.2.5 - 23 July 2002

 * added *[FILE]...* arguments, like "`cat`"
 * function separation in code
 * some bug fixes related to numeric overflow

### 0.2.3 - 19 July 2002

 * Texinfo manual written, man page updated
 * byte counter added

### 0.2.0 - 18 July 2002

 * ETA counter added
 * screen width estimation added
 * progress bar added

### 0.1.0 - 17 July 2002

 * main loop created
 * rate counter added
 * elapsed time counter added
 * percentage calculation added

### 0.0.1 - 16 July 2002

 * package created
 * first draft of man page written
