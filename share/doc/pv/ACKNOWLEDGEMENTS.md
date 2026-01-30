The following people have contributed to this project, and their assistance
is acknowledged and greatly appreciated:

 * Antoine Beaupré <anarcat@debian.org> - Debian package maintainer
 * Kevin Coyner <kcoyner@debian.org> - previous Debian package maintainer
 * Cédric Delfosse <cedric@debian.org> - previous Debian package maintainer
 * Jakub Hrozek <jhrozek@redhat.com> - Fedora package maintainer
 * Eduardo Aguiar <eduardo.oliveira@sondabrasil.com.br> - provided Portuguese (Brazilian) translation
 * Stéphane Lacasse <stephane@gorfou.ca> - provided French translation
 * Marcos Kreinacke <public@kreinacke.com> - provided German translation
 * Bartosz Feński <fenio@o2.pl> <http://skawina.eu.org/> - provided Polish translation along with Krystian Zubel
 * Joshua Jensen - reported RPM installation bug
 * Boris Folgmann <http://www.folgmann.com/en/> - reported cursor handling bug
 * Mathias Gumz - reported NLS bug
 * Daniel Roethlisberger - submitted patch to use lockfiles for **-c** if terminal locking fails
 * Adam Buchbinder - lots of help with a Cygwin port of **-c**
 * Mark Tomich <http://metuchen.dyndns.org> - suggested **-B** option
 * Gert Menke - reported bug when piping to `dd` with a large input buffer size
 * Ville Herva <Ville.Herva@iki.fi> - informative bug report about rate limiting performance
 * Elias Pipping - patch to compile properly on Darwin 9; potential NULL deref report
 * Patrick Collison - similar patch for OS X
 * Boris Lohner - reported problem that **-L** does not complain if given non-numeric value
 * Sebastian Kayser - supplied testing for *SIGPIPE*, demonstrated internationalisation problem
 * Laszlo Ersek <http://phptest11.atw.hu/> - reported shared memory leak on *SIGINT* with **-c**
 * Phil Rutschman <http://bandgap.rsnsoft.com/> - provided a patch for fully restoring terminal state on exit
 * Henry Precheur <http://henry.precheur.org/> - reporting and suggestions for **--rate-limit** bug when rate is under 10
 * E. Rosten <http://mi.eng.cam.ac.uk/~er258/> - supplied patch for block buffering in line mode
 * Kjetil Torgrim Homme - reported compilation error with default *CFLAGS* on non-GCC compilers
 * Alexandre de Verteuil - reported bug in OS X build and supplied test environment to fix in
 * Martin Baum - supplied patch to return nonzero exit status if terminated by signal
 * Sam Nelson <http://www.siliconfuture.net/> - supplied patch to fix trailing slash on *DESTDIR*
 * Daniel Pape - reported Cygwin installation problem due to *DESTDIR*
 * Philipp Beckers - ported to the Syabas PopcornHour A-100 series
 * Henry Gebhard <hsggebhardt@googlemail.com> - supplied patches to improve SI prefixes and add **--average-rate**
 * Vladimir Kokarev, Alexander Leo - reported that exit status did not reflect file errors
 * Thomas Rachel - submitted patches for IEEE1541 (MiB suffixes), 1+e03 bug
 * Guillaume Marcais - submitted speedup patch for line mode
 * Moritz Barsnick - submitted patch for compile warning in size calculation
 * Pawel Piatek - submitted RPM and patches for AIX
 * Sami Liedes - submitted patch for **--timer** and **--bytes** with **--numeric**
 * Steven Willis - reported problem with **-R** killing non-PV remote processes
 * Vladimir Pal, Vladimir Ermakov - submitted patch which led to development of **--format** option
 * Peter Samuelson <peter@p12n.org> - submitted patch to calculate size if stdout is a block device
 * Miguel Diaz - much Cygwin help (and packaging), found narrow-terminal bug
 * Jim Salter <http://ubuntuwiki.net> - commissioned work on the **--skip-errors** option
 * Wouter Pronk - reported build problem on SCO
 * Bryan Dongray <http://www.dongrays.com> - provided patches for test scripts failing on older Red Hats
 * Zev Weiss <www.bewilderbeest.net> - provided patch to fix `splice()` not using stdin
 * Zing Shishak - provided patch for **--null** / **-0** (count null terminated lines)
 * Jacek Wielemborek <http://deetah.jogger.pl/kategorie/english> - implemented fdwatch in Python, suggested PV port; reported bug with **-l** and ETA / size; many other contributions
 * Kim Krecht - suggested buffer fill status and last bytes output display options
 * Cristian Ciupitu <http://ciupicri.github.io>, Josh Stone - pointed out file descriptor leak with helpful suggestions (Josh Stone initially noticed the missing close)
 * Jan Seda - found issue with `splice()` and *SPLICE_F_NONBLOCK* causing slowdown
 * André Stapf - pointed out formatting problem e.g. 13GB -> 13.1GB which should be shown 13.0GB -> 13.1GB; highlighted on-startup row swapping in **-c**, and suggested **--discard**; suggested making **--fineta** have a "FIN" prefix
 * Damon Harper <http://www.usrbin.ca/> - suggested **-D** / **--delay-start** option
 * Ganaël Laplanche <http://www.martymac.org> - provided patch for `lstat64()` on systems that do not support it
 * Peter Korsgaard <http://www.buildroot.net/> - provided similar patch for `lstat64()`, specifically for uClibc support; provided AIX cross-compilation patch to fix bug in **-lc128** check
 * Ralf Ramsauer <https://blog.ramses-pyramidenbau.de/> - reported bug which dropped transfer rate on terminal resize
 * Michiel Van Herwegen - reported and discussed bug with **-l** and ETA / size
 * Erkki Seppälä <http://www.inside.org/~flux/> - provided patch implementing **-I**
 * Eric A. Borisch - provided details of compatibility fix for "`%Lu`" in watchpid code
 * Jan Venekamp - reported MacOS buffer size interactions with pipes
 * Matt <https://github.com/lemonsqueeze/pv> - provided "rate-window" patches for rate calculation
 * [Filippo Valsorda](https://github.com/FiloSottile) - provided patch for stat64 issue on Apple Silicon
 * Matt Koscica, William Dillon - also reported stat64 issue on Apple Silicon
 * [Demitri Muna](https://github.com/demitri) - assisted with stat64 patch on Apple Silicon
 * Norman Rasmussen - suggested **-c** with **-d PID:FD**, reject **-N** with **-d PID**
 * Andriy Gapon, Jonathan Elchison - reported bug where "`pv /dev/zero >/dev/null &`" stops immediately
 * Marcelo Chiesa - reported unused-result warnings when compiling PV 1.6.6
 * Jered Floyd - provided patches to improve **--rate-limit**
 * Christoph Biedl - provided ETA and dynamic interval patches
 * Richard Fonfara - provided German translations for "`pv --help`"
 * Johannes Gerer <http://johannesgerer.com> - suggested that **-B** should enable **-C**
 * Sam James - provided fix for number.c build issue caused by missing stddef.h
 * Jakub Wilk <jwilk@jwilk.net> - corrected README encoding
 * Frederik Eaton - reported issue with `<()` shell constructs
 * [gray](https://github.com/gray) - reported issue with **--force** and terminal process groups, and proposed a patch
 * [Luc Gommans](https://github.com/lgommans) / https://lgms.nl/ - provided a "momentary ETA" patch
 * [ikasty](https://github.com/ikasty) - added relative filename display to **--watchfd**
 * [Michael Weiß](https://github.com/quitschbo) - corrected behaviour when not attached to a terminal
 * [christoph-zededa](https://github.com/christoph-zededa) - provided OS X support for **--watchfd**
 * [Dave Beckett](https://github.com/dajobe) - added "`@filename`" syntax to **--size**, and corrected an autoconf problem with stat64 on OS X
 * [Volodymyr Bychkovyak](https://github.com/vbychkoviak) - provided fix for rate limit behaviour with bursty traffic
 * [Nick Black](https://nick-black.com) - added **--bits** option
 * [Andrew Schulman](https://github.com/andrew-schulman) - provided reproducible example of terminal size detection issue in 1.7.17/1.7.18
 * [fuschia74](https://github.com/fuchsia74) - provided **--enable-static**  patch for "`configure`"
 * [Wilhelm von Thiele](https://github.com/TurtleWilly) - assisted with OS X cleanups ([#73](https://codeberg.org/ivarch/pv/issues/73), [#74](https://codeberg.org/ivarch/pv/issues/74))
 * Matějů Miroslav, Ing. - suggested fix for ETA and elapsed time faults when suspending and resuming a machine ([#13](https://codeberg.org/ivarch/pv/issues/13))
 * Anthony DeRobertis - suggested the **--error-skip-block** option ([#37](https://codeberg.org/ivarch/pv/issues/37))
 * Benoit Pierre - provided patches for compilation without IPC support, without HAVE_STRUCT_STAT_ST_BLKSIZE, and on MacOS
 * [gustav-b](https://codeberg.org/gustav-b) - suggested percentage formatting correction ([#80](https://codeberg.org/ivarch/pv/issues/80))
 * [Thomas Bertels](https://codeberg.org/tbertels) - updated French translations ([#83](https://codeberg.org/ivarch/pv/pulls/83))
 * [kevinruddy](https://codeberg.org/kevinruddy) - added decimal units option ([#85](https://codeberg.org/ivarch/pv/pulls/85))
 * [xmort](https://codeberg.org/xmort) - added **--output** option ([#90](https://codeberg.org/ivarch/pv/pulls/90))
 * [bogiord](https://codeberg.org/bogiord) - reported the loss of output block device size detection in 1.8.10, and suggested the fix ([#91](https://codeberg.org/ivarch/pv/issues/91))
 * [eborisch](https://codeberg.org/eborisch) - provided fix for misbehaviour when used with "`zfs send`" due to treating zero sized writes (generally due to timer interruption) as end of file ([#92](https://codeberg.org/ivarch/pv/pulls/92), [#93](https://codeberg.org/ivarch/pv/pulls/93))
 * [alexanderperlis](https://codeberg.org/alexanderperlis) - provided initial support for block devices with **--size `@`FILE** ([#94](https://codeberg.org/ivarch/pv/pulls/94))
 * [jettero](https://codeberg.org/jettero) - reported double-free coredump when using **--watchfd** after 1.8.10 ([#96](https://codeberg.org/ivarch/pv/issues/96))
 * Venky.N.Iyer - suggested **--stats** ([#49](https://codeberg.org/ivarch/pv/issues/49))
 * Roland Kletzing - suggested **--rate** with **--numeric** ([#17](https://codeberg.org/ivarch/pv/issues/17))
 * [Michael Mior](https://codeberg.org/michaelmior) - suggested **--store-and-forward** ([#100](https://codeberg.org/ivarch/pv/issues/100))
 * [dbungert](https://codeberg.org/dbungert) - reported issue with *valgrind* 3.23 misreporting leaking file descriptors ([#97](https://codeberg.org/ivarch/pv/issues/97))
 * Joachim Haga - suggested showing progress in the window title ([#3](https://codeberg.org/ivarch/pv/issues/3))
 * [Martin Sarsale](https://github.com/runa) - suggested showing progress in the process title ([#4](https://codeberg.org/ivarch/pv/issues/4))
 * [2bits](https://github.com/2bits) - reported position sensing fault with **--watchfd** in 1.9.0 ([#118](https://codeberg.org/ivarch/pv/issues/118))
 * [heitbaum](https://codeberg.org/heitbaum) - assisted with cross-compilation fault introduced in 1.9.7 ([#120](https://codeberg.org/ivarch/pv/issues/120))
 * [meego](https://codeberg.org/meego) - suggested "**%nL**", the last line preview ([#121](https://codeberg.org/ivarch/pv/issues/121))
 * [Alexander Petrossian](https://github.com/neopaf) - suggested using Unicode output for a more granular progress bar, and provided the codepoints to use ([#15](https://codeberg.org/ivarch/pv/issues/15))
 * [jmealo](https://codeberg.org/jmealo) - suggested JSON progress output ([#127](https://codeberg.org/ivarch/pv/issues/127))
 * [ipaqmaster](https://codeberg.org/ipaqmaster) - investigated 100%-CPU utilisation bug when piping to a command that has exited ([#164](https://codeberg.org/ivarch/pv/issues/164))
 * Andriy Galetski, and also [thinrope](https://codeberg.org/thinrope) - suggested the "**--sparse**" option ([#45](https://codeberg.org/ivarch/pv/issues/45))
 * [KimHansen](https://codeberg.org/KimHansen) - suggested that "**--watchfd**" should keep its display when the process ends ([#81](https://codeberg.org/ivarch/pv/issues/81))

Translations provided through [Codeberg Weblate](https://translate.codeberg.org/projects/pv/):

 * **Czech**
   * [mmatous](https://translate.codeberg.org/user/mmatous/) (135)
 * **Finnish**
   * [artnay](https://translate.codeberg.org/user/artnay/) (65)
 * **French**
   * anonymous suggestions (7)
   * [milimarg](https://codeberg.org/milimarg) (13)
   * [Maxi_Mega](https://translate.codeberg.org/user/Maxi_Mega/) (11)
 * **German**
   * [Hartmut Goebel](https://translate.codeberg.org/user/htgoebel/) (77)
   * [m0yellow](https://translate.codeberg.org/user/m0yellow/) (9)
   * [Benny](https://translate.codeberg.org/user/Benny/) (7)
   * [kre](https://translate.codeberg.org/user/kre/) (2)
   * [fnetX](https://translate.codeberg.org/user/fnetX/) (1)
 * **Polish**
   * [coralpink](https://translate.codeberg.org/user/coralpink/) (106)
 * **Russian**
   * [0ko](https://translate.codeberg.org/user/0ko/) (157)
 * **Turkish**
   * [omerdduran](https://translate.codeberg.org/user/omerdduran/) (129)
   * [Oğuz Ersen](https://codeberg.org/ersen) (8)

---
