╔══════════════════════════════════════════════════════════════════════════════╗
║  sleepv - a visual wrapper around the sleep command                          ║
║  Version 1.1  |  Linux: bash script  |  Windows: native exe (Vista SP2+)     ║
╚══════════════════════════════════════════════════════════════════════════════╝

Russian documentation: README.md


━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 WHAT IT IS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

sleepv wraps the standard sleep command.

Plain sleep just pauses, without any output. You run "sleep 5m" and sit there
wondering whether the script is hung or honestly waiting.

sleepv fixes that: while waiting, the terminal shows a live progress bar with a
countdown.

    ⏱  00:01:23  [████████████░░░░░░░░░░░░░░░░░░]   58%  (2m 3s)

  • 00:01:23  - time left until the pause ends (HH:MM:SS)
  • [████░░░░] - the bar; in the Linux version the filled part is what is LEFT,
                so the bar shrinks (the Windows one grows - see "Differences")
  • 58%       - share of elapsed time
  • (2m 3s)   - the arguments you passed

When the pause is over:

    ✓  Done! (was: 2m 3s)


━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 INSTALLATION (LINUX)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    chmod +x sleepv
    ./sleepv --install

The script copies itself to /usr/sbin/sleepv and sets the permissions. If your
user lacks the rights, it asks for sudo automatically.

What --install does:
  1. finds its own current location
  2. copies itself to /usr/sbin/sleepv (always under the name "sleepv")
  3. sets the executable bit (chmod +x)
  4. retries through sudo if needed
  5. prints manual steps if even sudo failed

After that sleepv is available everywhere, from any terminal and any script,
exactly like the ordinary sleep.

    which sleepv              # /usr/sbin/sleepv

To uninstall, delete /usr/sbin/sleepv with administrative rights
(sudo + the file name, as shown in README.md).


━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 TIME SYNTAX AND SUFFIXES
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    sleepv NUMBER[SUFFIX] [NUMBER[SUFFIX] ...]

┌─────────┬──────────────┬────────────────────┐
│ Suffix  │ Meaning      │ Example            │
├─────────┼──────────────┼────────────────────┤
│ s       │ seconds      │ sleepv 30s         │
│ m       │ minutes      │ sleepv 5m          │
│ h       │ hours        │ sleepv 2h          │
│ d       │ days         │ sleepv 1d          │
│ (none)  │ seconds      │ sleepv 90          │
└─────────┴──────────────┴────────────────────┘

Arguments add up, fractions are supported:

    sleepv 1m 30s     -> 90 seconds
    sleepv 1h 5m 3s   -> 3903 seconds
    sleepv 2d 12h     -> 2.5 days
    sleepv 1.5m       -> 90 seconds
    sleepv 0.5h       -> 30 minutes


━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 ALL COMMANDS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    sleepv 10 / sleepv 10s      exactly 10 seconds ('s' may be omitted)
    sleepv 5m / sleepv 2.5m     5 minutes / 2 minutes 30 seconds
    sleepv 1h / sleepv 0.5h     1 hour / 30 minutes
    sleepv 1h 30m               summed left to right: 5400 seconds
    sleepv --help               description, syntax, suffix list, and the help
                                of the system GNU sleep
    sleepv --version            sleepv version plus system GNU sleep version
    ./sleepv --install          install to /usr/sbin/sleepv


━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 USING IT INSIDE YOUR OWN SCRIPTS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

sleepv is a drop-in replacement for sleep: write sleepv instead of sleep. The
syntax is identical.

    #!/bin/bash
    echo "waiting 30 seconds before start..."
    sleepv 30s
    echo "5 minute pause..."
    sleepv 5m
    echo "continuing."

Behaviour per context:

1. Terminal - progress bar, countdown, percent. The main mode.
2. No terminal (cron, systemd, background) - sleepv notices and pauses silently,
   exactly like plain sleep. No extra output at all.
3. In a pipe:  sleepv 5s | other_command - stdout stays completely clean, the
   progress goes to stderr.
4. In command substitution:  result=$(sleepv 5s; echo "data") - the variable
   receives only "data".
5. With  2>/dev/null  - the pause happens quietly, no bar.

Cancelling: press Ctrl+C, the cursor is restored, documented exit code 130.
(The 1.0 bash script really returns 0 - see "Differences"; the Windows build does
what you ask of it.)


━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 WINDOWS VERSION (Vista SP2 and later)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

This is not a translation of the bash script and not a PowerShell wrapper. sleepv
for Windows is a native Win32 application (win/sleepv.c), statically linked, with
zero external dependencies: no .NET, no Visual C++ redistributable, no
PowerShell, no GNU coreutils. It imports only kernel32.dll, msvcrt.dll,
user32.dll, shell32.dll and advapi32.dll - all of them are Windows components.

    sleepv-x86.exe   32-bit systems and mixed fleets (also runs on 64-bit
                     Windows, including ARM64 under emulation)
    sleepv-x64.exe   64-bit Windows 7/8/10/11
    about 80-90 KB each

Supported: Windows Vista SP2, 7, 8, 8.1, 10, 11 and the matching server
editions. No api-ms-win-* forwarders and no UCRT are used - that is precisely
the technical condition for running on Vista, and CI checks the import table for
it automatically.

── Getting it ───────────────────────────────────────────────────────────────

1) In the tree itself: win/dist/sleepv-x86.exe and win/dist/sleepv-x64.exe.
   Both are committed on purpose, so the tool can be taken away by cloning the
   repository, with no Releases tab and no toolchain involved.
2) Releases tab: the same two executables plus SHA256SUMS.txt, built by GitHub
   Actions when a v* tag is pushed; the hash values are also printed in the body
   of the release.
3) Portable: drop the exe next to your .bat files - no install required
4) Build it yourself (cross-compile on Linux, or natively in MSYS2/MinGW):

       sudo apt-get install mingw-w64
       make -C win all
       make -C win gate        # Vista compatibility check on the imports

   ABOUT THE HASHES. win/dist and the release assets come from the same Makefile
   with the same flags, but from different binutils/gcc versions (a local build
   versus the CI runner), so they may not be byte-identical: SHA256SUMS.txt
   describes the release files. Behaviour is unaffected; if byte-for-byte proof
   matters to you, build from source and compare against your own build.

── Same time syntax as on Linux ─────────────────────────────────────────────

    sleepv 5m
    sleepv 1h 30m 12.5s
    sleepv 30s && calc

Suffixes s/m/h/d, arguments add up, fractions allowed, negative values and junk
tails are rejected, maximum pause 30 days. Console look, 10 refreshes per second
and millisecond accuracy:

      00:04:12  [#############░░░░░░░░░░░░░░░░░░]   33%  (5m)

── Install for all users ────────────────────────────────────────────────────

    sleepv --install

The program asks for administrator rights through UAC and copies itself into
C:\Windows\System32. Details that matter:

  • A 32-bit build writing to System32 would be redirected to SysWOW64 by the
    File System Redirector, so the path is built with the "\\?\" prefix and
    lands in the real System32.
  • System32 is already in the system PATH, so every user on the machine gets
    the command straight away - no PATH editing, no reboot, just open a new
    console.
  • The installer verifies that BUILTIN\Users have Read & Execute on the file
    and adds that right explicitly when it is missing.
  • The manifest declares asInvoker: without it Vista would enable VirtualStore
    and the "install" would silently end up in %LOCALAPPDATA% instead.

    sleepv --install --dir "D:\tools"    # any folder you choose, no UAC; missing
                                         # directory levels are created, and since
                                         # such a folder is not on PATH you get a note
    sleepv --uninstall                   # remove installed copies (admin)

── Windows-only options ─────────────────────────────────────────────────────

    --install [--dir PATH]   copy itself into the system directory (UAC)
    --uninstall              remove the installed copies
    --run "COMMAND"          run a command after the pause, return its exit code
    --beep                   beep when the pause ends
    --msg                    message box when the pause ends
    --ascii                  bar drawn with # and - (default on Vista/2008)
    --unicode                bar drawn with block glyphs (default on Win7+)
    --strict-cancel          exit 130 on Ctrl+C (stops the && chain)
    -q, --quiet              no output at all
    --help, --version        help / version and build parameters

    sleepv 30s --run "ipconfig /all"
    sleepv 5m --beep --msg
    for /L %%i in (1,1,5) do ( echo try %%i & sleepv 10s )

── Behaviour in .bat / cmd / PowerShell ─────────────────────────────────────

Silent mode is identical to Linux: when stderr is not a console (service, Task
Scheduler, redirect, pipe) the pause happens silently, stdout stays clean,
progress goes to stderr.

Ctrl+C: the interruption ends the countdown and returns exit code 0, so the next
statement in a .bat file keeps running and cmd.exe does not print "Terminate
batch job (Y/N)?" - that prompt only appears when the child process returns
0xC000013A. If you want the opposite, use --strict-cancel, which returns 130.

One honest caveat: inside PowerShell itself, Ctrl+C aborts the pipeline by the
shell's own rules and a child process cannot override that. If a .ps1 must
continue after an interrupted pause, call it as:
cmd /c "sleepv 30s && next_command".

── Differences between the Windows and Linux versions (in detail) ───────────

   The 1.0 bash script was deliberately left untouched: the port lives next to it
   and every divergence is documented here. Verified against bash 5.2.15, GNU
   coreutils sleep 9.1 and wine-8.0 (used to run the exe from Linux); exit codes
   and messages below were taken from real runs, the commands are listed at the end.

   1. ACCURACY AND REFRESH RATE
      Linux: a 1-second loop on top of the background sleep. Bar and timer redraw
             once per second, about 18 ms of accounting drift per iteration, so an
             hour-long countdown can be a second behind visually.
      Windows: an absolute deadline from QueryPerformanceCounter, 10 redraws per
             second, drift in single milliseconds.
      In practice both are fine as a "the script is alive" indicator, and the real
      duration of the pause is equally exact on both, because Linux delegates the
      wait itself to the system sleep.

   2. BAR DIRECTION AND GLYPHS
      Linux: the filled part is what is LEFT, so the bar shrinks from full to empty.
      Windows: the filled part is what has PASSED, so the bar grows.
      The percentage is computed the same way in both (share of elapsed time), so
      "58%" means the same thing - only the graphic is mirrored. Linux draws █ and
      ░; Windows defaults to them on Win7+, and to ASCII #/- on Vista/2008.

   3. EXIT CODE ON Ctrl+C - THE ONE THAT MATTERS FOR SCRIPTS
      Linux, interactive terminal: 130. The 1.0 documentation was right.
      Linux, no terminal (cron, pipe, redirect, service): 0, because in silent mode
             the script execs the system sleep directly and its own "exit 130" trap
             never runs. After Ctrl+C in that mode, "sleepv 30s && next" CONTINUES.
      Windows, any mode: 0 by default, so the next statement in a .bat file runs and
             cmd.exe does not ask "Terminate batch job (Y/N)?". With --strict-cancel:
             130 and the chain stops.
      In short: on Linux the code depends on whether a TTY is present, on Windows it
      depends only on an explicit flag. In PowerShell, Ctrl+C aborts the pipeline by
      the shell's own rules either way; cmd /c "sleepv 30s && next" is the way to
      keep going.

   4. CURSOR RESTORE
      Linux: tput cnorm in the trap handler; after SIGKILL the cursor can stay
             hidden (fix: tput cnorm or reset).
      Windows: SetConsoleCursorInfo on every exit path it controls, including
             Ctrl+C; if the process is killed from Task Manager the same applies
             (fix: reopen the console, or mode con: rate=0 delay=0).

   5. TIME PARSER - NO DIFFERENCE
      Both are exactly as strict: s/m/h/d, summed arguments, fractions (1.5m, 3.75),
      leading zeros (007), zero (0). Both reject with exit 1: 10x, 1w, 10s5m, "5 m",
      1e3, -5. Only the wording of the error message differs (see item 7).

   6. OVERFLOW AND MAXIMUM DURATION - DIFFERENT
      Linux: no formal limit; 999999999999999999999999 makes bash print "syntax
             error in expression" and the pause becomes effectively infinite.
      Windows: a clean refusal, "sleepv: duration too large (maximum is 30 days).",
             exit 1, with 30 days as the explicit ceiling.
      Consequence: values computed in bash should be sanity-checked - the Windows
      limit is stricter than the Linux behaviour.

   7. OUTPUT LANGUAGE AND ENCODING - DIFFERENT
      Linux: Russian, UTF-8, block graphics, the ⏱ and ✓ glyphs, box-drawing frames.
      Windows: ASCII only, English only - a deliberate decision, because the Windows
             console lives in an OEM code page (866/1251 for Cyrillic), the raster
             Vista font cannot render half of Unicode, and a garbled localisation is
             worse than a short English line. --unicode adds exactly two glyphs, █
             and ░.
      Consequence: grepping sleepv's messages across platforms will not match; rely
      on exit codes, not on strings.

   8. NO ARGUMENTS - SAME
      Short usage text on stdout, exit 0 (GNU sleep would exit 1 here).

   9. UNKNOWN OPTIONS
      Linux rejects everything that is not --install/--help/--version, including the
      Windows set: sleepv --beep 1s gives "неверный аргумент: '--beep'" and exit 1.
      Windows does not understand Linux-only behaviour either. One command line with
      --run/--beep/--msg works on Windows only.

  10. SILENT MODE - SAME LOGIC, DIFFERENT IMPLEMENTATION
      Linux: [[ -t 2 ]] (is stderr attached to a TTY?). Windows: GetFileType
      (STD_ERROR_HANDLE) == FILE_TYPE_CHAR plus GetConsoleMode. Streams match: stdout
      stays clean, progress goes to stderr, 2>/dev/null silences the bar on both.

  11. --install - DIFFERENT TARGETS AND MECHANISMS
      Linux: /usr/sbin/sleepv, calling sudo itself when rights are missing.
      Windows: C:\Windows\System32\sleepv.exe through UAC, with the File System
             Redirector bypassed via the "\\?\" prefix (otherwise the 32-bit build
             lands in SysWOW64), a check that BUILTIN\Users keep Read & Execute, and
             an asInvoker manifest so Vista VirtualStore cannot intercept the write.
      Windows extras: --uninstall removes the copies from System32 and SysWOW64;
             --install --dir PATH creates missing directory levels itself and, since
             such a folder is not on PATH, says so explicitly.

  12. --version
      Linux: script version 1.0 plus the system GNU sleep banner.
      Windows: sleepv 1.1.0, target platform, binary bitness, whether a console is
             attached, and a line stating the static build with no redist.

  13. COMPLETION MESSAGE
      Linux: "✓  Готово! (было: 2m 3s)" (interactive mode only).
      Windows: "sleepv: finished! (was: 2m 3s)".

  14. HOW TO CHECK IT YOURSELF
      bash -c './sleepv 30 >/dev/null 2>&1 & p=$!; sleep 1; kill -INT $p; wait $p; echo $?'
        -> 0   (silent mode: the chain is not interrupted)
      in an interactive terminal: sleepv 30, press Ctrl+C, then echo $?  -> 130
      ./sleepv 10x; echo $?       -> 1 on both platforms (message wording differs)
      ./sleepv 999999999999999999999999   -> bash arithmetic errors, infinite wait
      wine sleepv-x64.exe --install --dir 'C:\tools'    -> installs, creating the folder

── A risk worth knowing about ───────────────────────────────────────────────

An unsigned exe that copies itself into System32 behaves like malware, so
AppLocker, WDAC or Attack Surface Reduction rules in a corporate environment may
block the install, just as SmartScreen and the Mark-of-the-Web can block a
downloaded file. The normal way round is portable use from your own folder, or
--install --dir. Code signing is a decision for the repository owner.

── What CI checks automatically ─────────────────────────────────────────────

.github/workflows/windows.yml builds both architectures, validates the import
table (api-ms-*, ext-ms-*, UCRT, vcruntime, msvcp are forbidden), enforces a
200 KB size budget, then runs a behavioural smoke test under Wine: the parser
both accepting and rejecting with the right exit codes, the 30-day ceiling,
--run propagating the child code, and --install --dir creating the folder and
leaving a runnable sleepv.exe. It uploads the artifacts and, on a v* tag,
publishes a release holding both executables, SHA256SUMS.txt and the same hashes
in the release notes.


━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 PRACTICAL EXAMPLES
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    systemctl restart networking
    echo "waiting for the network..."
    sleepv 15s
    ping -c 3 8.8.8.8

    for i in 1 2 3; do
        echo "attempt $i..."
        connect_to_server && break
        echo "failed, retrying in 30 seconds"
        sleepv 30s
    done

    while true; do
        check_status
        echo "next check in 5 minutes"
        sleepv 5m
    done

    echo "killing processes..."
    kill -TERM "$some_pid"
    sleepv 3s          # you can see the script is waiting, not hung
    kill -KILL "$some_pid"

    echo "WARNING: formatting starts in 10 seconds. Ctrl+C to cancel."
    sleepv 10s
    mkfs.ext4 /dev/sdX

    # crontab - sleepv prints nothing extra when there is no terminal:
    */5 * * * * /usr/sbin/goinet && sleepv 30s && /usr/sbin/other_script


━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 COMPATIBILITY AND LIMITS (LINUX)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Requires:
  • bash 4.0+ (standard in Debian / Raspberry Pi OS / Kali / Ubuntu)
  • GNU sleep (part of coreutils - always present)
  • bc or awk (arithmetic - both ship with Debian)
  • tput (cursor control - package ncurses-bin)

    which bash sleep bc awk tput

Shells: bash (full), sh, dash (the shebang pins bash), zsh (as external command).
Terminals: xfce4-terminal, xterm, lxterminal, gnome-terminal, konsole, SSH
sessions, tmux / screen, and script execution without a terminal (silent mode).

The only case where progress could leak into your data:

    output=$(command_that_calls_sleepv 2>&1)

Fix: add 2>/dev/null to the sleepv call inside such a substitution.


━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 TECHNICAL DETAILS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Terminal detection (Linux): [[ -t 2 ]] checks whether stderr is attached to a
TTY - visual mode if yes, plain sleep if not. The Windows port uses the exact
equivalent: GetFileType(STD_ERROR_HANDLE) == FILE_TYPE_CHAR plus GetConsoleMode.

Progress bar (Linux): once per second the script rewrites the current line with
\r and ESC[K, so the progress always stays on one row and never scrolls. The
Windows build redraws 10 times per second using a padded CR line (no ANSI
needed) and hides/restores the cursor through SetConsoleCursorInfo.

Timing: the Linux version runs the real sleep in the background and counts
visually on top of it, so overall accuracy equals the system sleep, ±1 s on the
display. The Windows version computes an absolute deadline with
QueryPerformanceCounter, so the drift is in the low milliseconds.

Streams: stdout (1) is clean, exactly like sleep; stderr (2) carries the bar and
the timer.


━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 QUICK REFERENCE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    sleepv --install        install to /usr/sbin/sleepv (Linux)
    sleepv --help           help
    sleepv --version        version

    sleepv 30               30 seconds
    sleepv 30s              same
    sleepv 5m               5 minutes
    sleepv 1h               1 hour
    sleepv 1d               1 day
    sleepv 1h 30m           1 hour 30 minutes
    sleepv 2m 30s           2 minutes 30 seconds
    sleepv 1.5m             90 seconds

    Ctrl+C                  cancel the pause

    --- Windows (the same, plus native options) --------------------------------

    sleepv --install        copy to System32 for all users (UAC)
    sleepv 30s && calc      works in .bat and cmd, no wrappers needed
    sleepv 1h 30m 12.5s     summed arguments and fractions, like on Linux
    sleepv 30s --run "calc" run after the pause, exit code propagates
    sleepv 5m --ascii       bar from # and - (if the console font lacks █░)
    sleepv --strict-cancel 1m   Ctrl+C => 130, the && chain stops


━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 FILES
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    /usr/sbin/sleepv                installed bash script (after --install)
    /usr/bin/sleep                  the original GNU sleep (untouched)
    sleepv                          Linux source (this repository)
    README.md                       full Russian documentation
    README-EN.md                    this document
    win/sleepv.c                    Windows source (Win32, no dependencies)
    win/Makefile                    x86 + x64 build and the Vista gate
    win/sleepv.exe.manifest         asInvoker + supportedOS (required for Vista)
    win/dist/sleepv-x86.exe         32-bit binary (build / CI output)
    win/dist/sleepv-x64.exe         64-bit binary
    C:\Windows\System32\sleepv.exe  result of --install on Windows

License: the repository carries no license file yet; the Windows build reports
"Copyright (c) 2026 sleepv contributors" in its version resource.
