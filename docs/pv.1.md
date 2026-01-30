# NAME

pv - monitor and manage the progress of data through a pipe

# SYNOPSIS

**pv** \[*OPTION*\]\... \[*FILE*\]\...

**pv** \[*OPTION*\]\... **-d**\|**\--watchfd**
*PID*\[:*FD*\]\|=*NAME*\|@*LISTFILE*\...

**pv** **-R**\|**\--remote** *PID* \[*OPTION*\]\...

**pv** **-Q**\|**\--query** *PID* \[*OPTION*\]\...

# DESCRIPTION

Show the progress of data through a pipeline by giving information such
as time elapsed, percentage completed (with progress bar), current
throughput rate, total data transferred, and ETA. The data flow rate,
error handling strategy, buffer size, and cache interaction can all be
adjusted.

Each *FILE* is copied to standard output. With no *FILE*, or when *FILE*
is "-", standard input is read.

In "**\--watchfd**" mode, inspect another process and show its progress
through the files it has open.

# OPTIONS

## Display switches

If no display switches are specified, **pv** behaves as if
"**\--progress**", "**\--timer**", "**\--eta**", "**\--rate**", and
"**\--bytes**" had been given. Otherwise, only those display types that
are explicitly switched on will be shown.

**-p, \--progress**

:   Turn the progress bar on. If any inputs are not files, or are
    unreadable, and no size was explicitly given with "**\--size**", the
    progress bar cannot indicate how close to completion the transfer
    is, so it will just move left and right to indicate that data is
    moving - or, with "**\--gauge**", the bar will indicate the current
    rate as a percentage of the maximum rate seen so far.

**-t, \--timer**

:   Turn the timer on. This will display the total elapsed time that
    **pv** has been running for.

**-e, \--eta**

:   Turn the ETA countdown on. This will estimate, based on current
    transfer rates and the total data size, how long it will be before
    completion. The countdown is prefixed with "ETA". This option will
    have no effect if the total data size cannot be determined.

**-I, \--fineta**

:   Turn the ETA countdown on, but display the estimated local time at
    which the transfer will finish, instead of the amount of time
    remaining. When the estimated time is more than 6 hours in the
    future, the date is shown as well. The time is prefixed with "FIN"
    for finish time. As with "**\--eta**", this option will have no
    effect if the total data size cannot be determined.

**-r, \--rate**

:   Turn the rate counter on. This will display the current rate of data
    transfer. The rate is shown in square brackets "\[\]".

**-a, \--average-rate**

:   Turn the average rate counter on. This will display the current
    average rate of data transfer, over the last 30 seconds by default
    (see "**\--average-rate-window**"). The average rate is shown in
    brackets "()".

**-b, \--bytes**

:   Turn the total byte counter on. This will display the total amount
    of data transferred so far.

**-T, \--buffer-percent**

:   Turn on the transfer buffer percentage display. This will show the
    percentage of the transfer buffer in use. Implies
    "**\--no-splice**". The transfer buffer percentage is shown in curly
    brackets "{}".

**-A NUM, \--last-written NUM**

:   Show the last *NUM* bytes written. Implies "**\--no-splice**".

**-F FORMAT, \--format FORMAT**

:   Ignore all of the above options and instead use the format string
    *FORMAT* to determine the output format. See the **FORMATTING**
    section below.

**-n, \--numeric**

:   Numeric output. Instead of giving a visual indication of progress,
    write an integer percentage, one per line, on standard error,
    suitable for passing to a tool such as **dialog**(1). Note that
    "**\--force**" is not required if "**\--numeric**" is being used.

    Combining "**\--numeric**" with "**\--bytes**" will cause the number
    of bytes processed so far to be output instead of a percentage.
    Adding "**\--line-mode**" as well as "**\--bytes**" writes the
    number of lines instead of bytes or a percentage. Adding
    "**\--rate**" adds the transfer rate to each output line (if
    "**\--bytes**" is also in use, the rate comes after the byte/line
    count). Adding "**\--timer**" prefixes each output line with the
    elapsed time so far, as a decimal number of seconds.

    Combining "**\--numeric**" with "**\--format**" allows for custom
    output. The default format string components for "**\--numeric**"
    are "**%t %b %r %{progress-amount-only}**" in that order, each item
    being active or inactive according to the rules above (so the
    default with no other options is "**%{progress-amount-only}**".

**-q, \--quiet**

:   No output. Useful if the "**\--rate-limit**" option is being used on
    its own to limit the transfer rate of a pipe.

## Output modifiers

**-8, \--bits**

:   Use bits instead of bytes for the byte and rate counters. The output
    suffix will be "b" instead of "B".

**-k, \--si**

:   Display and interpret suffixes as multiples of 1000 rather than the
    default of 1024. Note that this only takes effect on options after
    this one, so for consistency, specify this option first.

**-W, \--wait**

:   Wait until the first byte has been transferred before showing any
    progress information or calculating any ETAs. Useful if the program
    you are piping to or from requires extra information before it
    starts, such as when piping data into **gpg**(1) or **mcrypt**(1)
    which require a passphrase before data can be processed.

**-D SEC, \--delay-start SEC**

:   Wait until *SEC* seconds have passed before showing any progress
    information, for example in a script where you only want to show a
    progress bar if it starts taking a long time. The value of *SEC* can
    be a decimal such as "0.5".

**-s SIZE, \--size SIZE**

:   Assume the total amount of data to be transferred is *SIZE* bytes
    when calculating percentages and ETAs. A suffix of "K", "M", "G", or
    "T" can be added to denote kibibytes (\*1024), mebibytes, gibibytes,
    tebibytes. If "**\--si**" appears before this option, suffixes will
    denote kilobytes (\*1000), megabytes, and so on instead.

    If *SIZE* starts with "**@**", the size of file whose name follows
    the @ will be used.

**-g, \--gauge**

:   If the progress bar is shown but the size is not known, then instead
    of moving the bar left and right to show progress, show the current
    transfer rate as a percentage of the maximum rate seen so far.

**-l, \--line-mode**

:   Instead of counting bytes, count lines (newline characters). The
    progress bar will only move when a new line is found, and the value
    passed to "**\--size**" will be interpreted as a line count.

    If this option is used without "**\--size**", the \"total size\" (in
    this case, total line count) is calculated by reading through all
    input files once before transfer starts. If any inputs are pipes or
    non-regular files, or are unreadable, the total size will not be
    calculated.

**-0, \--null**

:   Count lines as terminated with a null byte instead of with a
    newline. This option implies "**\--line-mode**".

**-i SEC, \--interval SEC**

:   Wait *SEC* seconds between updates. The default is to update every
    second. The value of *SEC* can be a decimal such as "0.1".

**-m SEC, \--average-rate-window SEC**

:   Compute current average rate over a *SEC* seconds window for average
    rate and ETA calculations. The default is 30 seconds. The value must
    be an integer.

**-w WIDTH, \--width WIDTH**

:   Assume the terminal is *WIDTH* columns wide, instead of trying to
    work it out (or assuming 80 if it cannot be guessed). If this option
    is used, the output width will not be adjusted if the width of the
    terminal changes while the transfer is running.

**-H HEIGHT, \--height HEIGHT**

:   Assume the terminal is *HEIGHT* rows high, instead of trying to work
    it out (or assuming 25 if it cannot be guessed). If this option is
    used, the output height will not be adjusted if the height of the
    terminal changes while the transfer is running.

**-N NAME, \--name NAME**

:   Prefix the output information with *NAME*. Useful in conjunction
    with "**\--cursor**" if you have a complicated pipeline and you want
    to be able to tell different parts of it apart.

**-u STYLE, \--bar-style STYLE**

:   Change the default progress bar style shown by "**\--progress**", or
    by the "**\--format**" sequences "**%{progress}**" or
    "**%{progress-bar-only}**", to *STYLE*. The *STYLE* can be one of
    **plain** (the default), **block**, **granular**, or **shaded**.
    These styles are described in the **FORMATTING** section below.

**-x SPEC, \--extra-display SPEC**

:   As well as displaying progress to the terminal, also write it to
    *SPEC*. The *SPEC* must start with a comma-separated list of
    destinations, and can optionally be followed by a colon and a format
    string. The destinations can be **windowtitle** or **window** for
    the xterm window title, and **processtitle**, **proctitle**,
    **process**, or **proc** for the process title displayed by
    **ps**(1). If a format string is not supplied, the same format is
    used as for the terminal. For example,
    "**-x \'window,process:%t %b %r\'**" will show the elapsed time,
    bytes transferred, and rate, in both the window title and the
    process title.

**-v, \--stats**

:   At the end of the transfer, write an additional line showing the
    transfer rate minimum, maximum, mean, and standard deviation. The
    values are always in bytes per second (or bits, with "**\--bits**").

**-f, \--force**

:   Force output. Normally, **pv** will not output any visual display if
    standard error is not a terminal. This option forces it to do so.

**-c, \--cursor**

:   Use cursor positioning escape sequences instead of just using
    carriage returns. This is useful in conjunction with "**\--name**"
    if you are using multiple **pv** invocations in a single pipeline.

## Data transfer modifiers

**-o FILE, \--output FILE**

:   Write data to *FILE* instead of standard output. If the file already
    exists, it will be truncated.

**-L RATE, \--rate-limit RATE**

:   Limit the transfer to a maximum of *RATE* bytes per second. The same
    suffixes as "**\--size**" can be used.

**-B BYTES, \--buffer-size BYTES**

:   Use a transfer buffer size of *BYTES* bytes. The same suffixes as
    "**\--size**" can be used. The default buffer size is the block size
    of the input file\'s filesystem multiplied by 32 (512KiB max), or
    400KiB if the block size cannot be determined. This can be useful on
    platforms like macOS with pipelines that perform better with
    specific buffer sizes such as 1024. Implies "**\--no-splice**".

**-C, \--no-splice**

:   Never use **splice**(2), even if it would normally be possible. The
    **splice**(2) system call is a more efficient way of transferring
    data from or to a pipe than regular **read**(2) and **write**(2),
    but means that the transfer buffer may not be used. This prevents
    "**\--buffer-percent**" and "**\--last-written**" from working,
    cannot work with "**\--sparse**" or "**\--discard**", and makes
    "**\--buffer-size**" redundant, so using any of those options
    automatically switches on "**\--no-splice**". Switching on this
    option results in a small loss of transfer efficiency. It has no
    effect on systems where **splice**(2) is unavailable.

**-E, \--skip-errors**

:   Ignore read errors by attempting to skip past the offending
    sections. The corresponding parts of the output will be null bytes.
    At first only a few bytes will be skipped, but if there are many
    errors in a row then the skips will move up to chunks of 512. This
    is intended to be similar to "*dd conv=sync,noerror*".

    Specify "**\--skip-errors**" twice to only report a read error once
    per file, instead of reporting each byte range skipped.

**-Z BYTES, \--error-skip-block BYTES**

:   When ignoring read errors with "**\--skip-errors**", instead of
    trying to adaptively skip by reading small amounts and skipping
    progressively larger sections until a read succeeds, move to the
    next file block of *BYTES* bytes as soon as an error occurs. There
    may still be some shorter skips where the block being skipped
    coincides with the end of the transfer buffer. The same suffixes as
    "**\--size**" can be used.

    This option can only be used with "**\--skip-errors**" and is
    intended for use when reading from a block device, such as
    "**\--skip-errors \--error-skip-block 4K**" to skip in 4 kibibyte
    blocks. This will speed up reads from faulty media, at the expense
    of potentially losing more data.

**-S, \--stop-at-size**

:   If a size was specified with "**\--size**", stop transferring data
    once that many bytes have been written, instead of continuing to the
    end of input.

**-Y, \--sync**

:   After every write operation, synchronise the buffer caches to disk
    with **fdatasync**(2). This has no effect when the output is a pipe.
    Using "**\--sync**" may improve the accuracy of the progress bar
    when writing to a slow disk.

**-K, \--direct-io**

:   Set the **O_DIRECT** flag on all inputs and outputs, if it is
    available. This will minimise the effect of caches, at the cost of
    performance. Due to memory alignment requirements, it also may cause
    read or write failures with an error of "Invalid argument",
    especially if reading and writing files across a variety of
    filesystems in a single **pv** call. Use this option with caution.

**-O, \--sparse**

:   When writing null bytes, try to seek, producing a sparse output
    file. Implies "**\--no-splice**". On filesystems without sparse file
    support, or when the output is not seekable, this option will have
    no effect other than to turn on "**\--no-splice**".

**-X, \--discard**

:   Instead of transferring input data to standard output, discard it.
    This is equivalent to redirecting standard output to */dev/null*,
    except that **write**(2) is never called. Implies
    "**\--no-splice**".

**-U FILE, \--store-and-forward FILE**

:   Instead of passing data through immediately, do it in two stages -
    first read all input and write it to *FILE*, and then once the input
    is exhausted, read all of *FILE* and write it to the output. *FILE*
    remains in place afterwards, unless it is "**-**", in which case
    **pv** creates a temporary file for this purpose, and automatically
    removes it afterwards.

    This can be useful if you have a pipeline which generates data (your
    input) quickly but you don\'t know the size, and you wish to pass it
    to some slower process, once all of the input has been generated and
    you know its size, so you can see its progress. Note that when doing
    this with relatively small amounts of data, "**\--no-splice**" may
    be preferable so that pipe buffering doesn\'t affect the progress
    display.

## Alternative operating modes

**-d**, **\--watchfd** *PID*\[:*FD*\]\|=*NAME*\|@*LISTFILE*\...

:   Instead of transferring data, watch file descriptor *FD* of process
    *PID*, and show its progress. Other data transfer modifiers - and
    remote control - may not be used with this option.

    If a *PID* is specified without an *FD*, then that process will be
    watched, and all regular files and block devices it opens will be
    shown with a progress bar.

    If a *NAME* is specified, prefixed with \"=\", then processes with
    that name will be found with **pgrep**(1), and as watched described
    above.

    If a *LISTFILE* is specified, prefixed with \"@\", the lines in that
    file will be used as additional arguments.

    All remaining non-option arguments will also be treated as
    additional arguments.

    The **pv** process will exit when all *FD*s have either changed to a
    different file, changed read/write mode, or have closed, and all
    *PID*s (without a specific *FD*) have exited.

**-R PID, \--remote PID**

:   Remotely control another instance of **pv** with process ID *PID*,
    making it act as though it had been given this instance\'s command
    line. For example, if "**pv \--rate-limit 123K**" is running with
    process ID 9876, then running
    "**pv \--remote 9876 \--rate-limit 321K**" will cause process 9876
    to start using a rate limit of 321KiB instead of 123KiB. Note that
    some options cannot be changed while running, such as
    "**\--cursor**", "**\--line-mode**", "**\--force**",
    "**\--delay-start**", "**\--skip-errors**", and
    "**\--stop-at-size**".

**-Q PID, \--query PID**

:   Display the transfer progress of another instance of **pv** with
    process ID *PID*, owned by the same user.

    Some output modifiers can only be used while querying another
    process if they match those of that process - such as
    "**\--line-mode**", "**\--null**", and "**\--average-rate-window**".
    Data transfer modifiers will have no effect.

## Other options

**-P FILE, \--pidfile FILE**

:   Save the process ID of **pv** in *FILE*. The file will be replaced
    if it already exists, and will be removed when **pv** exits. While
    **pv** is running, *FILE* will contain a single number - the process
    ID of **pv** - followed by a newline.

**-h, \--help**

:   Print a usage message on standard output and exit successfully.

**-V, \--version**

:   Print version information on standard output and exit successfully.

# FORMATTING

Format strings used by "**\--format**" and "**\--extra-display**" can
contain the following sequences:

**%p**, **%{progress}**

:   Progress bar (suffixed with a percentage if the size is known).
    Equivalent to "**\--progress**". Expands to fill the remaining space
    unless prefixed by a number to set the width, such as "**%20p**" or
    "**%20{progress}**".

**%{progress-bar-only}**

:   Progress bar, without any sides, and without any percentage
    displayed afterwards. Expands to fill the remaining space unless
    prefixed by a number.

**%{progress-amount-only}**

:   The percentage completion (or maximum rate, with "**\--gauge**" when
    the size is unknown).

**%{bar-plain}**

:   Progress bar in the standard plain format, without any sides, and
    without any percentage displayed afterwards. Expands to fill the
    remaining space unless prefixed by a number.

**%{bar-block}**

:   Progress bar using Unicode full blocks, without any sides, and
    without any percentage displayed afterwards. Expands to fill the
    remaining space unless prefixed by a number. If UTF-8 output is not
    available, the plain format is used.

**%{bar-granular}**

:   Progress bar using Unicode full blocks, and 1/8th blocks for partial
    fills, providing a more granular display. Like the other "%{bar}"
    strings this shows the bar without any sides, and without any
    percentage displayed afterwards, and expands to fill the remaining
    space unless prefixed by a number. If UTF-8 output is not available,
    the plain format is used.

**%{bar-shaded}**

:   Progress bar using Unicode full blocks and shade characters - dark
    and medium shade are used for partial fills, and the light shade is
    used for the background. Like the other "%{bar}" strings this shows
    the bar without any sides, and without any percentage displayed
    afterwards, and expands to fill the remaining space unless prefixed
    by a number. If UTF-8 output is not available, the plain format is
    used.

**%t**, **%{timer}**

:   Elapsed time. Equivalent to "**\--timer**".

**%e**, **%{eta}**

:   ETA as time remaining. Equivalent to "**\--eta**".

**%I**, **%{fineta}**

:   ETA as local time at which the transfer will finish. Equivalent to
    "**\--fineta**".

**%r**, **%{rate}**

:   Current data transfer rate. Equivalent to "**\--rate**".

**%a**, **%{average-rate}**

:   Average data transfer rate. Equivalent to "**\--average-rate**".

**%b**, **%{bytes}**, **%{transferred}**

:   Bytes transferred so far (or lines if "**\--line-mode**" was
    specified). Equivalent to "**\--bytes**". If "**\--bits**" was
    specified, "**%b**" shows the bits transferred so far, not bytes.

**%T**, **%{buffer-percent}**

:   Percentage of the transfer buffer in use. Equivalent to
    "**\--buffer-percent**". Displays "{\-\-\--}" if the transfer is
    being done with **splice**(2), since splicing to or from pipes does
    not use the buffer.

**%nA**, **%n{last-written}**

:   Show the last *n* bytes written (for example, "**%16A**" shows the
    last 16 bytes). Shows only dots if the transfer is being done with
    **splice**(2), since splicing to or from pipes does not use the
    buffer.

**%nL**, **%n{previous-line}**

:   Show the first *n* bytes of the most recently written line (for
    example, "**%40L**" shows the first 40 bytes). If no *n* is given,
    then this expands to fill the available space. Shows only spaces if
    the transfer is being done with **splice**(2).

**%N**, **%{name}**

:   Show the name prefix given by "**\--name**". Padded to 9 characters
    with spaces, and suffixed with ":".

**%{sgr:colour,\...}**

:   Emit ECMA-48 SGR (Select Graphic Rendition) codes if the terminal
    supports colours, where *colour,\...* is a comma-separated list of
    any of the keywords below, or the numeric values from
    **console_codes**(4). If colour support is not available, nothing is
    emitted.

    Supported keywords are: **reset** or **none**, **black**, **red**,
    **green**, **brown** or **yellow**, **blue**, **magenta**, **cyan**,
    **white**, **fg-black**, **fg-red**, **fg-green**, **fg-brown** or
    **fg-yellow**, **fg-blue**, **fg-magenta**, **fg-cyan**,
    **fg-white**, **fg-default**, **bg-black**, **bg-red**,
    **bg-green**, **bg-brown** or **bg-yellow**, **bg-blue**,
    **bg-magenta**, **bg-cyan**, **bg-white**, **bg-default**, **bold**,
    **dim**, **italic**, **underscore** or **underline**, **blink**,
    **reverse**, **no-bold** or **no-dim**, **no-italic**,
    **no-underscore** or **no-underline**, **no-blink**, **no-reverse**.

    With colours, the optional \"fg-\" prefix indicates foreground; a
    prefix of \"bg-\" indicates background.

    For example, "**%{sgr:green,bold}TEXT%{sgr:reset}**" will make
    *TEXT* bold green on supported terminals.

**%%**

:   A single "%".

Any other contents are reproduced in the progress display as-is.

The format string equivalent of the default display switches is
"**%b %t %r %p %e**".

# EXAMPLES

Some suggested common switch combinations:

**pv -ptebar**

:   Show a progress bar, elapsed time, estimated completion time, byte
    counter, average rate, and current rate.

**pv -betlap**

:   Show a progress bar, elapsed time, estimated completion time, line
    counter, and average rate, counting lines instead of bytes.

**pv -btrpg**

:   Show the amount transferred, elapsed time, current rate, and a gauge
    showing the current rate as a percentage of the maximum rate seen -
    useful in a pipeline where the total size is unknown. (If the size
    *is* known, these options will show the percentage completion
    instead of the rate gauge).

**pv -t**

:   Show only the elapsed time - useful as a simple timer, such as
    "**sleep 10m \| pv -t**".

**pv -pterb**

:   The default behaviour: progress bar, elapsed time, estimated
    completion time, current rate, and byte counter.

On macOS, it may be useful to specify "**\--buffer-size 1024**" in a
pipeline, as this may improve performance.

To watch how quickly a file is transferred using **nc**(1):

    pv file | nc -w 1 somewhere.com 3000

A similar example, transferring a file from another process and passing
the expected size to **pv**:

    cat file | pv --size 12345 | nc -w 1 somewhere.com 3000

To watch the progress of creating a tar.gz archive:

    tar cf - directory/ \
    | pv --size $(du -sb directory/ | awk '{print $1}') \
    | gzip -9 \
    > out.tar.gz

Taking an image of a disk, skipping errors:

    pv -EE /dev/your/disk/device > disk-image.img

Writing an image back to a disk:

    pv disk-image.img > /dev/your/disk/device

Zeroing a disk:

    pv < /dev/zero > /dev/your/disk/device

Note that if the input size cannot be calculated, and the output is a
block device, then the size of the block device will be used and **pv**
will automatically stop at that size as if "**\--stop-at-size**" had
been given.

(Linux and macOS only): Watching file descriptor 3 opened by another
process 1234:

    pv --watchfd 1234:3

(Linux and macOS only): Watching all file descriptors used by process
1234:

    pv --watchfd 1234

Rate-limiting the transfer between two processes in a pipeline, with no
display:

    producer | pv --quiet --rate-limit 1M | consumer

Sending logs to a processing script, showing the most recent line as
part of the progress display:

    pv --format '%a %p : %L' big.log | processing-script

Showing progress as lines of JSON data:

    pv --numeric --format '{"elapsed":%t,"bytes":%b,"rate":%r,"percentage":%{progress-amount-only}}' big.log | processing-script

# EXIT STATUS

An exit status of 1 indicates a problem with the "**\--remote**",
"**\--query**", or "**\--pidfile**" options.

Any other exit status is a bitmask of the following:

 **2**

:   One or more files could not be accessed, **stat**(2)ed, or opened.

 **4**

:   An input file was the same as the output file.

 **8**

:   Internal error with closing a file or moving to the next file.

 **16**

:   There was an error while transferring data from one or more input
    files.

 **32**

:   A signal was caught that caused an early exit.

 **64**

:   Memory allocation failed.

A zero exit status indicates no problems.

# ENVIRONMENT

The following environment variables may affect **pv**:

**HOME**

:   The current user\'s home directory. This may be used by
    "**\--remote**" and "**\--query**" to exchange messages between
    **pv** instances: if the */run/user/UID/* directory does not exist
    (where *UID* is the current user ID), then *\$HOME/.pv/* will be
    used instead.

**TMPDIR**, **TMP**

:   The directory to create per-tty lock files for the terminal when
    using "**\--cursor**". If **TMPDIR** is set to a non-empty value, it
    is the directory under which lock files are created. Otherwise,
    **TMP** is used. If neither are set, then */tmp* is used.

# NOTES

In some versions of **bash**(1) and **zsh**(1), the construct
"**\<(pv filename)**" will not output any progress to the terminal when
run from an interactive shell, due to the subprocess being run in a
separate process group from the one that owns the terminal. In these
cases, use "**\--force**".

If **pv** is used in a pipeline in **zsh** version 5.8, and the last
command in the pipeline is based on shell builtins, **zsh** takes
control of the terminal away from **pv**, preventing progress from being
displayed. For example, this will produce no progress bar:

    pv InputFile | { while read -r line; do sleep 0.1; done; }

To work around this, put the last commands of the pipeline in normal
brackets to force the use of a subshell:

    pv InputFile | ( while read -r line; do sleep 0.1; done; )

Refer to [issue #105](https://codeberg.org/ivarch/pv/issues/105) for
full details.

The "**\--remote**" and "**\--query**" options require that either
*/run/user/\<uid\>/* or *\$HOME/* can be written to, for inter-process
communication.

The "**\--size**" option has no effect if used with
"**\--watchfd** *PID*" to watch all file descriptors of a process, but
will work with "**\--watchfd** *PID*:*FD*" to watch a single file
descriptor.

If the input size cannot be calculated, and the output is a block
device, then **pv** will read the output device\'s size, use that as if
it had been passed to "**\--size**", and activate "**\--stop-at-size**".

The "**%nA**" and "**%nL**" format sequences may not be effective with
small input files, and "**%nL**" may be a few lines out due to buffering
within the pipeline itself.

Numbers passed to "**\--size**", "**\--rate-limit**",
"**\--buffer-size**", and "**\--error-skip-block**" may all be expressed
as decimals if followed by a suffix, so for example "*\--size 1.5G*" is
equivalent to "*\--size 1536M*".

Numbers passed to "**\--interval**" and "**\--delay-start**" may be
integers or decimals, but may not have a suffix.

Numbers passed to "**\--last-written**", "**\--width**",
"**\--height**", "**\--average-rate-window**", "**\--remote**", and
"**\--query**" must be integers with no suffix.

When "**\--sparse**" is in effect and output is being written to a file,
that file may look like it has stopped growing if inspected with
**ls**(1) while null bytes are being written, even though **pv** still
reports progress. This is because of the way sparse output is achieved,
and the file will be the correct size when the transfer ends.

# REPORTING BUGS

Please report bugs or feature requests via the issue tracker linked from
the [**pv** home page](https://ivarch.com/p/pv).

# SEE ALSO

**cat**(1), **splice**(2), **fdatasync**(2), **open**(2) (for
**O_DIRECT**), **console_codes**(4)

# COPYRIGHT

Copyright © 2002-2008, 2010, 2012-2015, 2017, 2021, 2023-2025 Andrew
Wood.

License GPLv3+: [GNU GPL version 3 or
later](https://www.gnu.org/licenses/gpl-3.0.html).

This is free software: you are free to change and redistribute it. There
is NO WARRANTY, to the extent permitted by law.

Please see the package\'s ACKNOWLEDGEMENTS file for a complete list of
contributors.
