# Introduction

**pv** ("Pipe Viewer") is a terminal-based tool for monitoring the progress
of data through a pipeline and modifying its flow.

It can be inserted into any normal pipeline between two processes to give a
visual indication of how quickly data is passing through, how long it has
taken, how near to completion it is, and an estimate of how long it will be
until completion.  Data flow rate, error handling strategy, buffer size, and
cache interaction can all be adjusted.

In "**\--watchfd**" mode, **pv** will inspect another process and show its
progress through the files it has open.


# Documentation

A manual page is included in this distribution ("`man pv`").  Before
installation, it is in "[docs/pv.1](./docs/pv.1.md)".

Changes are listed in "[docs/NEWS.md](./docs/NEWS.md)".

Developers and translators, please see "[docs/DEVELOPERS.md](./docs/DEVELOPERS.md)".

Translators can use the Weblate instance hosted by Codeberg:
[https://translate.codeberg.org/engage/pv/](https://translate.codeberg.org/engage/pv/)

[![Translation status](https://translate.codeberg.org/widget/pv/multi-auto.svg)](https://translate.codeberg.org/engage/pv/)

If you don't see your language listed, raise an issue on the issue tracker
(or just email the maintainer) asking for it to be added.


# Installation

See "[docs/INSTALL](./docs/INSTALL)" for more about the _configure_ script.

The typical process for a system-wide install is:

    sh ./configure --prefix=/usr
    make
    sudo make install

This requires the build toolchain ("`sudo apt install build-essential`" on
Debian or Ubuntu systems).

If this is not a packaged release, the _configure_ script is not included.
It is generated with the GNU build system tools (_autoconf_, _aclocal_,
_autopoint_, _automake_); _gettext_ is also needed.  On Debian or Ubuntu,
run "`sudo apt install automake autopoint gettext`".  Once those tools are
in place, call "`autoreconf -is`" to generate the _configure_ script, and
run it as described above.


# Copyright, bug reporting, and acknowledgements

See "[docs/ACKNOWLEDGEMENTS.md](./docs/ACKNOWLEDGEMENTS.md)" for a list of
contributors.

Copyright (C) 2002-2008, 2010, 2012-2015, 2017, 2021, 2023-2025 Andrew Wood.

License GPLv3+: GNU GPL version 3 or later <https://www.gnu.org/licenses/gpl-3.0.html>.

This program is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option)
any later version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
more details.

You should have received a copy of the GNU General Public License, version
3, in "[docs/COPYING](./docs/COPYING)".  If not, see
<https://www.gnu.org/licenses/gpl-3.0.html>.

Please report bugs or request features via the issue tracker linked from the
home page.

The **pv** home page is at:

> [https://ivarch.com/p/pv](https://ivarch.com/p/pv)

The latest version can always be found here.

---
