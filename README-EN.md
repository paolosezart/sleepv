# sleepv

A visual countdown for timed pauses. The repository contains two separate implementations:

- **Linux:** Bash script, version 1.0 (`sleepv`).
- **Windows:** native Win32 program, version 1.1.0 (`sleepv-x86.exe` and `sleepv-x64.exe`), targeting Windows Vista SP2 and later.

Russian documentation: [README.md](README.md).

## Releases and downloads

Open [GitHub Releases](https://github.com/paolosezart/sleepv/releases). Future releases created from a repository revision containing the updated workflow will publish three runnable files and one checksum file:

- `sleepv-linux` — the Bash script for Linux;
- `sleepv-x86.exe` — Windows x86;
- `sleepv-x64.exe` — Windows x64;
- `SHA256SUMS.txt` — SHA-256 checksums for all three files.

The workflow does not modify older releases retroactively. On Linux, save `sleepv-linux` and run:

```sh
chmod +x sleepv-linux
./sleepv-linux 30s
```

Verify the downloaded assets on Linux with:

```sh
sha256sum -c SHA256SUMS.txt
```

On Windows, you can check a checksum with PowerShell:

```powershell
Get-FileHash .\sleepv-x64.exe -Algorithm SHA256
```

## Basic usage

A normal invocation waits for the requested duration. When stderr is attached to a terminal, sleepv shows a countdown; otherwise it waits without a progress bar. `stdout` remains available for data, while progress is written to `stderr`.

```sh
sleepv 30       # 30 seconds
sleepv 30s      # same
sleepv 5m       # 5 minutes
sleepv 1h 30m   # summed: 1 hour 30 minutes
sleepv 1.5m     # 90 seconds (Linux requires a digit before the decimal point)
```

Suffixes `s`, `m`, `h` and `d` mean seconds, minutes, hours and days; no suffix means seconds. Duration arguments are added together. Negative values and unsupported suffixes are rejected.

## Linux

### Installation

To run a downloaded file:

```sh
chmod +x sleepv-linux       # or chmod +x sleepv when using the repository file
./sleepv-linux --install
```

`--install` copies the script to `/usr/sbin/sleepv`; if needed, the script tries to use `sudo`. To remove it, run `sudo rm /usr/sbin/sleepv`. The Linux script has no `--dir` or `--uninstall` option.

### Linux commands and options

```text
sleepv NUMBER[SUFFIX] [NUMBER[SUFFIX] ...]
sleepv --help | -h
sleepv --version
sleepv --install
```

- `--help` / `-h` prints help, including the help from the system `sleep` command it finds.
- `--version` prints the script version and, if found, the system `sleep` version.
- `--install` installs the script as `/usr/sbin/sleepv`.
- Other options, including Windows-specific ones, are not supported by the Linux script.

### Compatibility and timing

The script uses Bash (`#!/usr/bin/env bash`), a system `sleep` command and either `bc` or `awk` for arithmetic. `tput` is used only to control cursor visibility and is optional. Execute the script directly (`./sleepv`); do not pass it to `sh` or `dash`.

The Linux parser requires a digit before the decimal point: `1.5s` is accepted, `.5s` is not. For its countdown display, it sums durations and rounds down to whole seconds. Therefore, a sub-second duration in an interactive terminal may finish immediately, and the display is not fractional-second accurate. In non-interactive mode the script passes the original arguments to the system `sleep` command it finds; fractional support there depends on that command. The script does not set a formal maximum duration.

In an interactive terminal, Ctrl+C interrupts the script with exit code 130 and restores the cursor when `tput` is available. In non-interactive mode control is handed to the system `sleep`, so signal behaviour depends on that command and the calling environment.

## Windows

The Windows version is a native Win32 application, not a Bash or PowerShell script. It is statically linked and does not require .NET or the Visual C++ Redistributable. The stated minimum supported OS is Windows Vista SP2.

- `sleepv-x86.exe` is the 32-bit build; it can also run on 64-bit Windows through emulation.
- `sleepv-x64.exe` is the 64-bit build.
- The executable is portable; installation is optional.

### Windows installation

```cmd
sleepv-x64.exe --install
sleepv-x64.exe --install --dir "D:\tools"
sleepv-x64.exe --uninstall
```

`--install` copies the executable into the Windows system directory and requests UAC elevation when needed. `--install --dir PATH` installs into the chosen folder and creates it if necessary; adding that folder to `PATH` is up to the user. `--uninstall` removes the installed system copies and requires administrator rights.

### Windows options

| Option | Action |
| --- | --- |
| `--help`, `-h` | Show help. |
| `--version` | Show version, target OS, binary architecture and build information. |
| `--install [--dir PATH]` | Install into the system directory or the specified folder. |
| `--uninstall` | Remove installed system copies. |
| `--run "COMMAND"` | Queue a command after the pause; repeatable. |
| `-a`, `--after` | Wait for `--run` commands; also applies to the immediately preceding command and following commands. |
| `-w`, `--workdir PATH` | Set the starting working directory for following `--run` commands; inherited until changed. Requires at least one `--run`. |
| `--beep` | Play a short sound after an interactive countdown; quiet mode suppresses it. |
| `--msg [TEXT]` | Show a message box after the countdown; uses a default message if text is omitted. |
| `--ascii` | Draw the progress bar with `#` and `-`. |
| `--unicode` | Use block glyphs for the progress bar. |
| `-q`, `--quiet` | Suppress normal progress, completion text and beep. Does not suppress errors, the `--msg` dialog or child-command output. |
| `--strict-cancel` | Return exit code 130 on Ctrl+C instead of 0. |

When supplying custom `--msg` text, put all duration arguments before it: the immediately following argument is used as the dialog text only if it does not start with `-`. The dialog appears after the countdown and before the queued commands start.

```cmd
sleepv-x64.exe 14s --msg Complete --run quser
sleepv-x64.exe 14s -a --msg Complete --run quser --after --run ver --run "echo done"
sleepv-x64.exe 5s -w "C:\Projects\demo" --run "dir && echo done"
sleepv-x64.exe 5m --beep --msg
sleepv-x64.exe 30s --msg 765
```

By default, `--run` starts processes asynchronously: sleepv does not wait for them and cannot report their eventual exit codes. `-a` / `--after` enables waiting for the previous command (if any) and all following commands; waited commands run sequentially. sleepv returns the exit code of the last waited command. If the final command is asynchronous, sleepv normally exits with code 0 after starting it.

`-w` / `--workdir` sets the directory in which the corresponding process starts, and is inherited by subsequent queue entries until another `--workdir` is specified. A `cd` inside one `--run` command can affect later parts of that same command string, but it does not change the starting directory of later processes in the queue.

Each `--run` command is executed through `cmd.exe`, so standard CMD operators `&&`, `&`, `|` and `||` work inside the command string. These operators belong to CMD, not to sleepv itself.

### Windows cancellation and quiet mode

Ctrl+C cancels the countdown. By default the program returns 0; `--strict-cancel` returns 130. On cancellation, the `--msg` dialog is not shown and queued commands are not started. When stderr is not a console, progress output is automatically suppressed. `--quiet` also suppresses normal screen output, but not errors, the message box or output from launched commands.

## Linux and Windows differences

| Feature | Linux Bash | Windows executable |
| --- | --- | --- |
| Fractional values | A digit is required before the decimal point; countdown accounting rounds down to whole seconds. | Fractions are supported, including `.5`; the timer accounts for the fractional part. |
| Maximum duration | No formal limit is set in the script. | 30 days; larger values are rejected. |
| Actions after the pause | No built-in command options. The surrounding shell handles `sleepv ... && command`. | `--run` queue, `--after` waiting mode, working directory, message box and other options are available. |
| Ctrl+C | Exit code 130 in an interactive terminal; without a terminal, the system `sleep` / caller handles the signal. | Exit code 0 by default; `--strict-cancel` selects 130. The command queue is not started after cancellation. |
| Progress | Filled area represents remaining time and shrinks. Updates about once per second. | Filled area represents elapsed time and grows. Updates about 10 times per second. |
| Text | Russian UTF-8. | Program messages are in English; an ASCII bar is available, and block glyphs depend on mode/console. |

These are two implementations with overlapping, but not identical, syntax; full compatibility with every GNU `sleep` form is not promised. Windows options (`--run`, `--workdir`, `--msg`, and others) are not available in the Linux script.

## Building and automated checks

Build the Windows executables from Linux with MinGW-w64, or natively in MSYS2/MinGW:

```sh
sudo apt-get install mingw-w64 binutils-mingw-w64
make -C win all
make -C win gate
```

GitHub Actions builds x86 and x64, checks the import tables for forbidden dependencies and enforces an executable size budget. A separate Wine smoke test covers selected parser cases, exit codes, the command queue and installation, but it is marked **non-blocking** (`continue-on-error`): a passing Wine test is not required for release publication. For a `v*` tag, the workflow attaches the Linux script, both Windows builds and `SHA256SUMS.txt`; the checksums are also included in the release notes.

## Project files

- `sleepv` — Linux Bash source.
- `win/sleepv.c` — Windows application source.
- `win/Makefile` — x86/x64 cross-build and import check.
- `win/dist/sleepv-x86.exe`, `win/dist/sleepv-x64.exe` — binaries tracked in the repository. Do not assume they are byte-identical to an asset from any particular Release; verify the checksum of the downloaded release itself.
- `.github/workflows/windows.yml` — build, tests and GitHub Release publication.
- `README.md` — Russian documentation.
