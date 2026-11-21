/*
 * sleepv - a visual wrapper around sleep with a live countdown.
 * Windows port of the Linux/Bash original, targeting Windows Vista SP2 and later.
 * Native Win32, statically linked, no runtime dependencies beyond core OS DLLs.
 */

#define WINVER          0x0600
#define _WIN32_WINNT    0x0600
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shellapi.h>
#include <sddl.h>
#include <accctrl.h>
#include <aclapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SLEEPV_VERSION   "1.1.0"
#define SLEEPV_NAME      "sleepv"
#define SLEEPV_EXE       L"sleepv.exe"
#define BAR_MAX          30
#define MAX_TOTAL_SEC    2592000.0        /* 30 days */
#define REFRESH_MS       100
#define EXIT_CANCEL_STRICT 130

typedef struct {
    const WCHAR *command;
    const WCHAR *workdir;
    int          wait_after;
} run_item_t;

typedef struct {
    double       total;                  /* seconds to wait */
    WCHAR        label[200];             /* original argument text, for the tail message */
    WCHAR        msg_text[200];          /* optional text for the completion message box */
    int          quiet;                  /* -q / --quiet */
    int          blocks;                 /* 1 = draw bar with U+2588/U+2591, 0 = ASCII */
    int          beep;
    int          msgbox;
    int          strict_cancel;          /* 1 => exit 130 after Ctrl+C, 0 => exit 0 */
    run_item_t  *runs;                   /* commands to launch when the pause ends */
    int          run_count;
    int          run_capacity;
    int          after_mode;             /* sticky --after mode for --run commands */
    const WCHAR *workdir;                /* inherited initial working directory */
    int          elevated_hint;          /* internal: already running elevated */
    int          want_install;
    int          want_uninstall;
} opts_t;

typedef LONG NTSTATUS;
typedef NTSTATUS (WINAPI *rtl_get_version_t)(POSVERSIONINFOW);

static volatile LONG g_cancel       = 0;
static HANDLE        g_con_out      = INVALID_HANDLE_VALUE;
static HANDLE        g_child        = NULL;
static CONSOLE_CURSOR_INFO g_saved_cci;
static int           g_cci_saved    = 0;

/* ------------------------------------------------------------------ utils */

static void die(const char *msg)
{
    DWORD code = (DWORD)EXIT_FAILURE;
    DWORD gle  = GetLastError();

    fprintf(stderr, SLEEPV_NAME ": %s", msg);
    if (gle)
        fprintf(stderr, " (Win32 error %lu)", (unsigned long)gle);
    fputc('\n', stderr);
    fflush(stderr);
    ExitProcess(code);
}

static void os_version(int *major, int *minor)
{
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");

    *major = 6;
    *minor = 1;
    if (ntdll != NULL) {
        FARPROC fn = GetProcAddress(ntdll, "RtlGetVersion");
        if (fn != NULL) {
            OSVERSIONINFOW v;
            v.dwOSVersionInfoSize = sizeof v;
            if (((rtl_get_version_t)(void *)fn)(&v) == 0) {
                *major = (int)v.dwMajorVersion;
                *minor = (int)v.dwMinorVersion;
            }
        }
    }
}

static int __attribute__((unused)) os_is_64(void)
{
#ifdef _WIN64
    return 1;
#else
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    return si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ||
           si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64;
#endif
}

/* A handle that is both FILE_TYPE_CHAR and a real console: the exact analogue
 * of bash's [[ -t 2 ]] test in the original script. */
static int stderr_is_console(void)
{
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    DWORD  mode;

    if (h == NULL || h == INVALID_HANDLE_VALUE)
        return 0;
    if (GetFileType(h) != FILE_TYPE_CHAR)
        return 0;
    if (!GetConsoleMode(h, &mode) && !GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode))
        return 0;
    return 1;
}

static int wcs_len(const WCHAR *s)
{
    return (int)wcslen(s);
}

static int put_wchar_to_mb(const WCHAR *w, char *a, int cap)
{
    int n = WideCharToMultiByte(CP_ACP, 0, w, -1, a, cap, NULL, NULL);
    return n > 0 ? n - 1 : 0;
}

/* ---------------------------------------------------------------- signals */

static void restore_console(void)
{
    if (g_cci_saved && g_con_out != INVALID_HANDLE_VALUE) {
        SetConsoleCursorInfo(g_con_out, &g_saved_cci);
        g_cci_saved = 0;
    }
}

static BOOL WINAPI ctrl_handler(DWORD type)
{
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
        InterlockedExchange(&g_cancel, 1);
        /* Handled: we choose the exit code ourselves, so a parent cmd.exe never
         * sees STATUS_CONTROL_C_EXIT and therefore never asks
         * "Terminate batch job (Y/N)?" - the batch simply continues. */
        return type == CTRL_CLOSE_EVENT ? FALSE : TRUE;
    }
    return FALSE;
}

static void install_ctrl_handler(void)
{
    SetConsoleCtrlHandler(ctrl_handler, TRUE);
}

/* ----------------------------------------------------------------- parsing */

static void print_version(void);
static void usage(void);

static int parse_one(const WCHAR *arg, double *secs)
{
    WCHAR *end = NULL;
    double  v;
    double  mult = 1.0;
    WCHAR   tail;

    if (*arg == L'\0')
        return 0;
    if (!((*arg >= L'0' && *arg <= L'9') || *arg == L'.'))
        return 0;

    v = wcstod(arg, &end);
    if (end == arg || !isfinite(v) || v < 0.0)
        return 0;

    tail = *end;
    if (tail != L'\0') {
        switch (tail) {
        case L's': mult = 1.0;      break;
        case L'm': mult = 60.0;     break;
        case L'h': mult = 3600.0;   break;
        case L'd': mult = 86400.0;  break;
        default:  return 0;                 /* reject "5cat", "10x", "1m30s" */
        }
        if (end[1] != L'\0')
            return 0;
    }
    *secs = v * mult;
    return 1;
}

/* 1 = durations parsed, 0 = usage asked for / nothing to do, -1 = bad syntax.
 * Flags and durations may be mixed in any order: "sleepv 30s --run ipconfig"
 * and "sleepv --beep 30s" are both valid. */
static int parse_args(int argc, WCHAR **argv, opts_t *o, WCHAR **install_dir)
{
    double total = 0.0;
    int    seen  = 0;
    int    i;
    int    len   = 0;
    const  int   cap = (int)(sizeof(o->label) / sizeof(WCHAR)) - 1;

    o->label[0] = L'\0';

    for (i = 1; i < argc; i++) {
        const WCHAR *a = argv[i];

        if (a[0] == L'-' && a[1] != L'\0') {
            if (a[1] >= L'0' && a[1] <= L'9') {
                fprintf(stderr, SLEEPV_NAME ": duration must not be negative ('%ls').\n", a);
                fprintf(stderr, SLEEPV_NAME ": use sleepv --help\n");
                return -1;
            }
            if      (!lstrcmpiW(a, L"--help") || !lstrcmpiW(a, L"-h")) { usage(); return 0; }
            else if (!lstrcmpiW(a, L"--version")) { print_version(); return 0; }
            else if (!lstrcmpiW(a, L"--install"))       o->want_install = 1;
            else if (!lstrcmpiW(a, L"--uninstall"))    o->want_uninstall = 1;
            else if (!lstrcmpiW(a, L"--_elevated"))    o->elevated_hint = 1;
            else if (!lstrcmpiW(a, L"-q") || !lstrcmpiW(a, L"--quiet"))  o->quiet = 1;
            else if (!lstrcmpiW(a, L"--ascii"))        o->blocks = 0;
            else if (!lstrcmpiW(a, L"--unicode"))      o->blocks = 1;
            else if (!lstrcmpiW(a, L"--beep"))         o->beep = 1;
            else if (!lstrcmpiW(a, L"--msg")) {
                o->msgbox = 1;
                /* An optional following token is the popup text. Put any
                 * duration arguments before --msg when supplying custom text. */
                if (i + 1 < argc && argv[i + 1][0] != L'-') {
                    wcsncpy(o->msg_text, argv[++i],
                            sizeof(o->msg_text) / sizeof(o->msg_text[0]) - 1);
                    o->msg_text[sizeof(o->msg_text) / sizeof(o->msg_text[0]) - 1] = L'\0';
                }
            }
            else if (!lstrcmpiW(a, L"--strict-cancel")) o->strict_cancel = 1;
            else if (!lstrcmpiW(a, L"-a") || !lstrcmpiW(a, L"--after")) {
                o->after_mode = 1;
                /* Also make the immediately preceding command synchronous. */
                if (o->run_count > 0)
                    o->runs[o->run_count - 1].wait_after = 1;
            }
            else if ((!lstrcmpiW(a, L"-w") || !lstrcmpiW(a, L"--workdir")) && i + 1 < argc) {
                o->workdir = argv[++i];
            }
            else if (!lstrcmpiW(a, L"--run") && i + 1 < argc) {
                if (o->run_count >= o->run_capacity) {
                    fputs("sleepv: too many --run commands.\n", stderr);
                    return -1;
                }
                o->runs[o->run_count].command = argv[++i];
                o->runs[o->run_count].workdir = o->workdir;
                o->runs[o->run_count].wait_after = o->after_mode;
                o->run_count++;
            }
            else if (!lstrcmpiW(a, L"--dir") && i + 1 < argc) {
                if (!*install_dir) *install_dir = argv[++i]; else ++i;
            }
            else {
                fprintf(stderr, SLEEPV_NAME ": unknown or incomplete option '%ls'.\n", a);
                fprintf(stderr, SLEEPV_NAME ": use sleepv --help\n");
                return -1;
            }
            continue;
        }

        {
            double secs;
            if (!parse_one(a, &secs)) {
                fprintf(stderr, SLEEPV_NAME ": invalid argument '%ls'.\n", a);
                fprintf(stderr, SLEEPV_NAME ": use NUMBER[SUFFIX] - s, m, h, d (see --help)\n");
                return -1;
            }
            total += secs;
            seen   = 1;
            if (len && len + 1 < cap) o->label[len++] = L' ';
            if (len < cap - 1) {
                wcsncpy(o->label + len, a, cap - len - 1);
                len += wcs_len(a);
                if (len > cap - 1) len = cap - 1;
            }
            o->label[len] = L'\0';
        }
    }

    if (!seen)
        return 0;
    if (total > MAX_TOTAL_SEC) {
        fprintf(stderr, SLEEPV_NAME ": duration too large (maximum is 30 days).\n");
        return -1;
    }
    o->total = total;
    return 1;
}

static void usage(void)
{
    printf(
      "sleepv " SLEEPV_VERSION " - sleep with a visual countdown (Windows)\n"
      "Target: Windows Vista SP2 and later. No runtime dependencies.\n"
      "\n"
      "Usage:\n"
      "  sleepv NUMBER[SUFFIX]... [options]     pause with a live countdown\n"
      "  sleepv --install [--dir PATH]          copy this exe into the system directory\n"
      "  sleepv --uninstall                     remove the installed copy (needs admin)\n"
      "  sleepv --help | --version              this text / version\n"
      "\n"
      "Suffixes:  s seconds (default), m minutes, h hours, d days\n"
      "Arguments add up, fractions allowed:  sleepv 1h 30m 12.5s\n"
      "\n"
      "Options:\n"
      "  -q, --quiet          no output at all (auto-enabled when stderr is not a console)\n"
      "  --ascii              draw the bar with '#' and '-' (auto-default on Vista/2008)\n"
      "  --unicode            draw the bar with block glyphs\n"
      "  --beep               beep when the pause ends\n"
      "  --msg [TEXT]         pop a message box when the pause ends\n"
      "  --run \"COMMAND\"      queue COMMAND after the pause; repeatable\n"
      "  -a, --after          wait for --run commands; applies to previous and later ones\n"
      "  -w, --workdir PATH   set working directory for following --run commands\n"
      "  --strict-cancel      exit with code 130 on Ctrl+C (stops '&&' chains)\n"
      "\n"
      "Examples:\n"
      "  sleepv 5m && calc\n"
      "  sleepv 30s --run \"ipconfig /all\"\n"
      "  sleepv 14s -a --msg Complete --run quser --run ver --run \"echo done\"\n"
      "  sleepv 5s --workdir \"C:\\Projects\\demo\" --run \"dir && echo done\"\n"
      "  for /L %%i in (1,1,5) do ( echo try %%i & sleepv 10s )\n"
      "\n"
      "Ctrl+C cancels the countdown and the following commands in a batch file\n"
      "still run (default). Use --strict-cancel to abort the chain instead.\n");
}

/* ---------------------------------------------------------------- drawing */

static void format_hms(double secs, WCHAR *out, int cap)
{
    unsigned long s = (unsigned long)(secs < 0 ? 0 : ceil(secs));
    swprintf(out, cap, L"%02lu:%02lu:%02lu",
             s / 3600UL, (s % 3600UL) / 60UL, s % 60UL);
}

static void console_width(int *width)
{
    CONSOLE_SCREEN_BUFFER_INFO sbi;
    *width = 80;
    if (g_con_out != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(g_con_out, &sbi)) {
        int w = sbi.dwSize.X - 1;
        if (w < 40) w = 40;
        *width = w;
    }
}

/* One in-place line: "  HH:MM:SS  [bar]  NNN%  (label)", padded to erase the
 * previous frame. No ANSI: SetConsoleCursorPosition-free, we simply start the
 * line with CR and never emit LF, so the frame always lands on one row. */
static void draw(double elapsed, double total, const opts_t *o)
{
    static int width  = 80;
    static int inited = 0;
    int   bar_w, filled, i, pct;
    WCHAR tbuf[16];

    if (!inited) {
        console_width(&width);
        inited = 1;
    }

    bar_w = BAR_MAX;
    if (width - 34 < bar_w)
        bar_w = (width - 34) < 10 ? 10 : (width - 34);

    filled = total > 0.0 ? (int)(elapsed * bar_w / total) : bar_w;
    if (filled > bar_w) filled = bar_w;
    if (filled < 0)     filled = 0;
    pct  = total > 0.0 ? (int)(elapsed * 100.0 / total) : 100;
    if (pct > 100) pct = 100;

    format_hms(total - elapsed, tbuf, 16);

    if (o->blocks) {
        WCHAR line[512];
        int   len = 0;
        DWORD written = 0;
        char  labmb[128];

        line[len++] = L'\r';
        line[len++] = L' ';
        line[len++] = L' ';
        wcsncpy(line + len, tbuf, 512 - (int)len); len += wcs_len(tbuf);
        line[len++] = L' ';
        line[len++] = L'[';
        for (i = 0; i < bar_w; i++)
            line[len++] = (i < filled) ? (WCHAR)0x2588 : (WCHAR)0x2591;
        line[len++] = L']';
        len += swprintf(line + len, 512 - len, L"  %3d%%", pct);
        if (o->label[0] && WideCharToMultiByte(CP_UTF8, 0, o->label, -1, labmb, sizeof labmb, NULL, NULL) > 1) {
            int k = 0;
            len += swprintf(line + len, 512 - len, L"  (");
            while (labmb[k] && len < 490) line[len++] = (WCHAR)(unsigned char)labmb[k++];
            len += swprintf(line + len, 512 - len, L")");
        }
        while (len < width - 1 && len < 500) line[len++] = L' ';
        line[len] = L'\0';
        WriteConsoleW(GetStdHandle(STD_ERROR_HANDLE), line, len, &written, NULL);
    } else {
        char  line[512];
        char  labmb[128];
        int   n = 0;
        DWORD written = 0;

        n += sprintf(line + n, "\r  ");
        n += sprintf(line + n, "%ls", tbuf);
        n += sprintf(line + n, "  [");
        for (i = 0; i < bar_w && n < 480; i++)
            line[n++] = (i < filled) ? '#' : '-';
        n += sprintf(line + n, "]  %3d%%", pct);
        if (o->label[0] && WideCharToMultiByte(CP_ACP, 0, o->label, -1, labmb, sizeof labmb, NULL, NULL) > 1)
            n += sprintf(line + n, "  (%s)", labmb);
        while (n < width - 1 && n < 500) line[n++] = ' ';
        line[n] = '\0';
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), line, (DWORD)n, &written, NULL);
    }
}

/* ---------------------------------------------------------------- pause */

static double mono_now(void)
{
    LARGE_INTEGER c, f;
    static int    ok = -1;

    if (ok != 1) {
        ok = QueryPerformanceFrequency(&f) && f.QuadPart > 0 ? 1 : 0;
        if (ok) {
            QueryPerformanceCounter(&c);
            return (double)c.QuadPart / (double)f.QuadPart;
        }
        return (double)GetTickCount() / 1000.0;
    }
    QueryPerformanceCounter(&c);
    QueryPerformanceFrequency(&f);
    return (double)c.QuadPart / (double)f.QuadPart;
}

static void sleep_ms(DWORD ms)
{
    DWORD step;
    DWORD t0 = GetTickCount();

    while ((LONG)(GetTickCount() - t0) < (LONG)ms) {
        if (InterlockedCompareExchange(&g_cancel, 0, 0))
            return;
        step = ms - (DWORD)(GetTickCount() - t0);
        if (step > REFRESH_MS) step = REFRESH_MS;
        if (step == 0) step = 1;
        Sleep(step);
    }
}

static int run_command(const WCHAR *cmdline, const WCHAR *workdir, int wait_after)
{
    WCHAR  full[1024];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD  code = 1;

    if (_snwprintf(full, 1024, L"cmd.exe /d /s /c \"%s\"", cmdline) >= 1023)
        return 1;

    ZeroMemory(&si, sizeof si);
    si.cb = sizeof si;
    ZeroMemory(&pi, sizeof pi);

    if (!CreateProcessW(NULL, full, NULL, NULL, TRUE, 0, NULL, workdir, &si, &pi)) {
        fprintf(stderr, SLEEPV_NAME ": cannot start command (Win32 error %lu).\n",
                (unsigned long)GetLastError());
        return 1;
    }
    if (!wait_after) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 0;
    }

    g_child = pi.hProcess;
    while (WaitForSingleObject(pi.hProcess, 100) == WAIT_TIMEOUT) {
        if (InterlockedCompareExchange(&g_cancel, 0, 0)) {
            Sleep(200);                          /* let it finish gracefully */
            TerminateProcess(pi.hProcess, 1);
            break;
        }
    }
    GetExitCodeProcess(pi.hProcess, &code);
    g_child = NULL;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}

static int run_commands(const opts_t *o)
{
    int i, rc = 0;

    for (i = 0; i < o->run_count; i++) {
        if (InterlockedCompareExchange(&g_cancel, 0, 0))
            break;
        rc = run_command(o->runs[i].command, o->runs[i].workdir,
                         o->runs[i].wait_after);
    }
    return rc;
}

static void announce(const opts_t *o, int cancelled)
{
    char  label[256];
    char  msg[320];

    label[0] = '\0';
    if (o->label[0])
        put_wchar_to_mb(o->label, label, sizeof(label) - 1);

    if (cancelled)
        sprintf(msg, "\r%s: countdown cancelled.\n", SLEEPV_NAME);
    else if (label[0])
        sprintf(msg, "\n" SLEEPV_NAME ": finished! (was: %s)\n", label);
    else
        sprintf(msg, "\n" SLEEPV_NAME ": finished!\n");

    fputs(msg, stderr);
    fflush(stderr);
}

static int countdown(const opts_t *o)
{
    double t0       = mono_now();
    double deadline = o->total;
    int    cancelled;

    if (!o->quiet) {
        if (GetConsoleCursorInfo(g_con_out, &g_saved_cci)) {
            CONSOLE_CURSOR_INFO hidden_cci = g_saved_cci;
            g_cci_saved = 1;
            hidden_cci.bVisible = FALSE;
            SetConsoleCursorInfo(g_con_out, &hidden_cci);
        }
        fflush(stderr);
    }

    for (;;) {
        double elapsed = mono_now() - t0;
        double left    = deadline - elapsed;

        if (InterlockedCompareExchange(&g_cancel, 0, 0))
            break;
        if (left <= 0.0) {
            if (!o->quiet) draw(deadline, deadline, o);
            break;
        }
        if (!o->quiet) draw(elapsed < deadline ? elapsed : deadline, deadline, o);
        sleep_ms(left * 1000.0 > REFRESH_MS ? REFRESH_MS : (DWORD)(left * 1000.0) + 1);
    }

    cancelled = (int)InterlockedCompareExchange(&g_cancel, 0, 0);
    restore_console();
    if (!o->quiet) {
        announce(o, cancelled);
        if (!cancelled && o->beep)
            Beep(1200, 120);
    }
    return cancelled && o->strict_cancel ? EXIT_CANCEL_STRICT : 0;
}

/* --------------------------------------------------------------- --install */

static int token_is_admin(void)
{
    HANDLE tok = NULL;
    BOOL   member = FALSE;
    PSID   sid = NULL;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok))
        return 0;
    if (ConvertStringSidToSidW(L"S-1-5-32-544", &sid)) {
        CheckTokenMembership(tok, sid, &member);
        LocalFree(sid);
    }
    CloseHandle(tok);
    return member ? 1 : 0;
}

static int path_native_dir(WCHAR *out, int cap, int syswow64)
{
    WCHAR windir[MAX_PATH];
    DWORD n = GetSystemWindowsDirectoryW(windir, MAX_PATH);

    if (n == 0 || n > MAX_PATH - 12)
        return 0;
    /* \\?\ disables WOW64 file-system redirection, so a 32-bit build can still
     * write the real 64-bit System32. */
    if (_snwprintf(out, cap, L"\\\\?\\%s\\%s", windir, syswow64 ? L"SysWOW64" : L"System32") >= cap - 1)
        return 0;
    return 1;
}

static int copy_file_plain(const WCHAR *src, const WCHAR *dst)
{
    HANDLE  hs = INVALID_HANDLE_VALUE, hd = INVALID_HANDLE_VALUE;
    char    buf[65536];
    DWORD   rd = 0, wr = 0;
    int     ok = 0;

    hs = CreateFileW(src, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL, NULL);
    if (hs == INVALID_HANDLE_VALUE)
        return 0;
    hd = CreateFileW(dst, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                     FILE_ATTRIBUTE_NORMAL, NULL);
    if (hd == INVALID_HANDLE_VALUE) {
        CloseHandle(hs);
        return 0;
    }
    ok = 1;
    while (ReadFile(hs, buf, sizeof buf, &rd, NULL) && rd > 0) {
        if (!WriteFile(hd, buf, rd, &wr, NULL) || wr != rd) {
            ok = 0;
            break;
        }
    }
    CloseHandle(hs);
    CloseHandle(hd);
    return ok;
}

/* MinGW's fwprintf treats %s as char*, so paths go through an explicit
 * conversion and are always written as narrow text. */
static const WCHAR *clean_path(const WCHAR *p)
{
    return !wcsncmp(p, L"\\\\?\\", 4) ? p + 4 : p;
}

static void wpath(const WCHAR *path, const char *suffix)
{
    char mb[4 * MAX_PATH];
    put_wchar_to_mb(path, mb, (int)sizeof(mb) - 1);
    fprintf(stderr, "%s%s", mb, suffix);
    fflush(stderr);
}

static int self_path(WCHAR *out, int cap)
{
    DWORD n = GetModuleFileNameW(NULL, out, cap);
    return n > 0 && n < (DWORD)cap;
}

/* \??\  disables WOW64 redirection but also disables slash normalisation, so
 * any user-supplied path must be back-slashed before the prefix is added. */
static void normalize_dir_arg(const WCHAR *in, WCHAR *out, int cap)
{
    int i, n = 0;
    const WCHAR *p = in;

    if (!wcsncmp(in, L"\\\\?\\", 4)) { out[n++] = in[0]; out[n++] = in[1]; out[n++] = in[2]; out[n++] = in[3]; p += 4; }
    else if (!wcsncmp(in, L"\\\\.\\", 4)) { out[n++] = in[0]; out[n++] = in[1]; out[n++] = in[2]; out[n++] = in[3]; p += 4; }
    else { out[n++] = L'\\'; out[n++] = L'\\'; out[n++] = L'?'; out[n++] = L'\\'; }

    for (i = 0; p[i] && n < cap - 2; i++)
        out[n++] = (p[i] == L'/') ? L'\\' : p[i];
    while (n > 4 && out[n - 1] == L'\\')
        n--;
    out[n] = L'\0';
}

/* System32 grants BUILTIN\Users read+execute by inheritance, but an existing
 * file may carry a narrower DACL. Verify, and widen only if needed: the whole
 * point of --install is that every user of the machine gets the command. */
static int ensure_users_rx(const WCHAR *path)
{
    PACL                 dacl = NULL, new_dacl = NULL;
    PSECURITY_DESCRIPTOR sd   = NULL;
    PSID                 users = NULL;
    ACL_SIZE_INFORMATION asi;
    DWORD                err;
    int                  i, has = 0;
    EXPLICIT_ACCESS_W    ea;
    const DWORD          want = FILE_GENERIC_READ | FILE_GENERIC_EXECUTE;

    ConvertStringSidToSidW(L"S-1-5-32-545", &users);

    SetLastError(0);
    err = GetNamedSecurityInfoW((WCHAR *)path, SE_FILE_OBJECT,
                                DACL_SECURITY_INFORMATION, NULL, NULL, &dacl, NULL, &sd);
    if (err != ERROR_SUCCESS) {
        fprintf(stderr, "sleepv: note: cannot read the file ACL (error %lu); check with  icacls \"%s\"\n",
                (unsigned long)err, "");
        LocalFree(sd);
        return -(int)err;
    }
    if (dacl == NULL || !GetAclInformation(dacl, &asi, sizeof asi, AclSizeInformation)) {
        fputs("sleepv: note: ACL could not be verified; check with icacls.\n", stderr);
        LocalFree(sd);
        return -1;
    }
    for (i = 0; i < (int)asi.AceCount; i++) {
        void *ace = NULL;
        if (!GetAce(dacl, (DWORD)i, &ace) || ace == NULL)
            continue;
        if (((ACCESS_ALLOWED_ACE *)ace)->Header.AceType != ACCESS_ALLOWED_ACE_TYPE)
            continue;
        if (users != NULL && IsValidSid((PSID)&((ACCESS_ALLOWED_ACE *)ace)->SidStart) &&
            EqualSid((PSID)&((ACCESS_ALLOWED_ACE *)ace)->SidStart, users)) {
            if ((((ACCESS_ALLOWED_ACE *)ace)->Mask & want) == want)
                has = 1;
        }
    }
    if (has || users == NULL) {
        if (has)
            fputs("sleepv: file ACL already grants Users read+execute.\n", stderr);
        LocalFree(sd);
        return has ? 1 : 0;
    }

    ZeroMemory(&ea, sizeof ea);
    ea.grfAccessPermissions = want;
    ea.grfAccessMode        = GRANT_ACCESS;
    ea.grfInheritance       = NO_INHERITANCE;
    ea.Trustee.TrusteeForm  = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType  = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea.Trustee.ptstrName    = (LPWCH)users;

    if (SetEntriesInAclW(1, &ea, dacl, &new_dacl) != ERROR_SUCCESS ||
        SetSecurityInfo((WCHAR *)path, SE_FILE_OBJECT,
                        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                        NULL, NULL, new_dacl, NULL) != ERROR_SUCCESS) {
        fputs("sleepv: note: ACL could not be widened; check with icacls.\n", stderr);
        LocalFree(sd);
        return -2;
    }
    fputs("sleepv: granted BUILTIN\\Users read+execute on the installed file.\n", stderr);
    LocalFree(sd);
    return 0;
}

/* --dir may name a folder that does not exist yet; copying into it then fails
 * with ERROR_PATH_NOT_FOUND. Create every missing component here instead, with
 * plain kernel32 CreateDirectoryW - no extra DLL, no extra import, so the Vista
 * compatibility gate stays untouched. SHCreateDirectoryExW would do this too but
 * it is shell32/shlobj.h and drags more surface in for one loop. */
static void ensure_dir_exists(const WCHAR *dir)
{
    WCHAR       path[MAX_PATH];
    const WCHAR *s = clean_path(dir);
    size_t      i = 0, n;

    while (s[i] && i + 1 < MAX_PATH) {
        path[i] = (s[i] == L'/') ? L'\\' : s[i];
        i++;
    }
    path[i] = L'\0';
    n = i;
    while (n > 0 && path[n - 1] == L'\\')
        path[--n] = L'\0';

    for (i = 0; i < n; i++) {
        if (path[i] != L'\\')
            continue;
        if (i == 1 && path[0] == L'\\')      /* leading part of a UNC path */
            continue;
        path[i] = L'\0';
        CreateDirectoryW(path, NULL);         /* already there: harmless */
        path[i] = L'\\';
    }
    if (!CreateDirectoryW(path, NULL)) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
            fprintf(stderr, "sleepv: cannot create target directory (Win32 error %lu): ",
                    (unsigned long)err);
            wpath(path, "\n");
            exit(1);
        }
    }
}

static int do_install(const WCHAR *custom_dir)
{
    WCHAR src[MAX_PATH], dir[MAX_PATH], dst[MAX_PATH];
    DWORD attr;
    int   to_system = !(custom_dir && custom_dir[0]);

    if (!self_path(src, MAX_PATH))
        die("cannot determine own path");

    if (custom_dir && custom_dir[0]) {
        normalize_dir_arg(custom_dir, dir, MAX_PATH);
        ensure_dir_exists(dir);
    } else
    if (!path_native_dir(dir, MAX_PATH, 0))
        die("cannot resolve the Windows directory");

    if (_snwprintf(dst, MAX_PATH, L"%s\\%ls", dir, SLEEPV_EXE) >= MAX_PATH - 1)
        return 1;

    attr = GetFileAttributesW(dst);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
        die("target is a directory, not a file");
    if (attr != INVALID_FILE_ATTRIBUTES &&
        (attr & FILE_ATTRIBUTE_READONLY))
        SetFileAttributesW(dst, FILE_ATTRIBUTE_NORMAL);

    if (lstrcmpiW(src, dst) == 0)
        die("already running from the install location");

    if (!copy_file_plain(src, dst)) {
        fprintf(stderr, "sleepv: cannot write (Win32 error %lu): ", (unsigned long)GetLastError());
        wpath(dst, "\n");
        fputs("sleepv: hint: --dir PATH installs into any writable Windows path,\n", stderr);
        fputs("sleepv:       without --dir the target is the Windows system directory (admin needed).\n", stderr);
        return 1;
    }

    if (to_system)
        ensure_users_rx(dst);           /* every user must be able to run it */
    fputs("sleepv: installed to ", stderr);
    wpath(clean_path(dst), "\n");
    if (to_system) {
        fputs("sleepv: available to every user; the system directory is already on PATH.\n", stderr);
        fputs("sleepv: open a new console and type:  sleepv 5m\n", stderr);
    } else {
        fputs("sleepv: note: this folder is not on PATH. Add it there, or run the file\n", stderr);
        fputs("sleepv:       by its full path, for example:  ", stderr);
        wpath(clean_path(dst), " 30s\n");
    }
    return 0;
}

static int do_uninstall(void)
{
    WCHAR dir[MAX_PATH], dst[MAX_PATH];
    int   removed = 0, i;

    for (i = 0; i < 2; i++) {
        if (!path_native_dir(dir, MAX_PATH, i))
            continue;
        if (_snwprintf(dst, MAX_PATH, L"%s\\%ls", dir, SLEEPV_EXE) >= MAX_PATH - 1)
            continue;
        if (GetFileAttributesW(dst) != INVALID_FILE_ATTRIBUTES) {
            SetFileAttributesW(dst, FILE_ATTRIBUTE_NORMAL);
            if (DeleteFileW(dst)) {
                fputs("sleepv: removed ", stderr); wpath(dst, "\n");
                removed = 1;
            } else {
                fputs("sleepv: failed to remove ", stderr); wpath(dst, "\n");
                fprintf(stderr, "sleepv:   (Win32 error %lu)\n", (unsigned long)GetLastError());
            }
        }
    }
    if (!removed)
        fputs("sleepv: nothing to remove.\n", stderr);
    return 0;
}

static int relaunch_elevated(const WCHAR *args)
{
    WCHAR             self[MAX_PATH];
    SHELLEXECUTEINFOW si;
    DWORD             code = 1;

    if (!self_path(self, MAX_PATH))
        return 1;
    ZeroMemory(&si, sizeof si);
    si.cbSize       = sizeof si;
    si.fMask        = SEE_MASK_NOCLOSEPROCESS;   /* we want the handle to wait on */
    si.lpVerb       = L"runas";
    si.lpFile       = self;
    si.lpParameters = args;
    si.nShow        = SW_SHOW;                   /* the child prints its own result */
    if (!ShellExecuteExW(&si)) {
        DWORD e = GetLastError();
        fprintf(stderr, "sleepv: elevation refused (Win32 error %lu%s).\n",
                (unsigned long)e, e == ERROR_CANCELLED ? " - the UAC prompt was cancelled" : "");
        return 1;
    }
    if (!si.hProcess)                            /* cannot verify - assume it ran */
        return 0;
    WaitForSingleObject(si.hProcess, INFINITE);
    GetExitCodeProcess(si.hProcess, &code);
    CloseHandle(si.hProcess);
    if (code != 0) {
        fprintf(stderr, "sleepv: the elevated copy exited with code %lu.\n", (unsigned long)code);
        return (int)code;
    }
    return 0;
}

/* ------------------------------------------------------------------ version */

static void print_version(void)
{
    int maj, min;
    os_version(&maj, &min);
    printf("sleepv " SLEEPV_VERSION " (Windows) - native, no runtime dependencies\n");
    printf("sleepv.exe " SLEEPV_VERSION "\n");
    printf("OS reported: %d.%d   binary: %s   console: %s\n", maj, min,
#ifdef _WIN64
           "x64",
#else
           os_is_64() ? "x86 (on x64 OS)" : "x86",
#endif
           stderr_is_console() ? "attached" : "none - silent mode");
    printf("Build: static MinGW-w64, PE subsystem 6.00, no msvc/redist dependency\n");
}

/* -------------------------------------------------------------------- main */

int wmain(int argc, WCHAR **argv)
{
    opts_t  o;
    WCHAR  *install_dir = NULL;
    int     rc, pr;
    BOOL    elevated = FALSE;
    HANDLE  tok = NULL;

    ZeroMemory(&o, sizeof o);
    o.runs = (run_item_t *)calloc(argc > 0 ? (size_t)argc : 1, sizeof(*o.runs));
    if (o.runs == NULL) {
        fputs("sleepv: out of memory.\n", stderr);
        return 1;
    }
    o.run_capacity = argc;
    {
        int maj, min;
        os_version(&maj, &min);
        o.blocks = (maj > 6 || (maj == 6 && min >= 1));   /* Win7 and newer */
    }

    pr = parse_args(argc, argv, &o, &install_dir);
    if (pr < 0) {
        free(o.runs);
        return 1;
    }

    if (o.want_install || o.want_uninstall) {
        TOKEN_ELEVATION elev;
        DWORD needed = 0;

        if (install_dir == NULL && !o.elevated_hint) {
            WCHAR args[512];
            int   n = 0;
            if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
                elev.TokenIsElevated = 0;
                if (GetTokenInformation(tok, TokenElevation, &elev, sizeof elev, &needed))
                    elevated = elev.TokenIsElevated;
                CloseHandle(tok);
            }
            if (!token_is_admin() || !elevated) {
                n = swprintf(args, 512, L"%ls --_elevated",
                             o.want_install ? L"--install" : L"--uninstall");
                if (n > 0 && n < 500) args[n] = L'\0';
                fputs("sleepv: administrator rights are required for the system directory.\n", stderr);
                fputs("sleepv: approving the UAC prompt ...\n", stderr);
                rc = relaunch_elevated(args);
                free(o.runs);
                return rc;
            }
        }
        rc = o.want_uninstall ? do_uninstall() : do_install(install_dir);
        free(o.runs);
        return rc;
    }

    if (o.workdir && o.run_count == 0) {
        fputs("sleepv: --workdir requires --run.\n", stderr);
        free(o.runs);
        return 1;
    }

    if (pr == 0) {                     /* nothing but flags: show help, like the original */
        usage();
        free(o.runs);
        return 0;
    }

    if (o.total <= 0.0) {
        rc = run_commands(&o);
        if (!o.quiet && rc)
            fprintf(stderr, SLEEPV_NAME ": command sequence exited with code %d.\n", rc);
        free(o.runs);
        return rc;
    }

    /* No console on stderr (service, scheduled task, redirected output, pipe):
     * exactly what the original does with [[ -t 2 ]] - wait silently. */
    if (!stderr_is_console()) {
        DWORD left = (DWORD)(o.total * 1000.0 + 0.5);
        install_ctrl_handler();
        while (left > 0) {
            DWORD step = left > 200 ? 200 : left;
            if (InterlockedCompareExchange(&g_cancel, 0, 0))
                break;
            Sleep(step);
            left -= step;
        }
        if (InterlockedCompareExchange(&g_cancel, 0, 0)) {
            fprintf(stderr, SLEEPV_NAME ": countdown cancelled.\n");
            rc = o.strict_cancel ? EXIT_CANCEL_STRICT : 0;
            free(o.runs);
            return rc;
        }
        if (o.msgbox)
            MessageBoxW(NULL, o.msg_text[0] ? o.msg_text : L"sleepv: countdown finished.",
                        L"sleepv", MB_OK | MB_ICONINFORMATION);
        rc = run_commands(&o);
        free(o.runs);
        return rc;
    }

    g_con_out = GetStdHandle(STD_OUTPUT_HANDLE);
    install_ctrl_handler();
    if (GetFileType(g_con_out) != FILE_TYPE_CHAR)
        g_con_out = GetStdHandle(STD_ERROR_HANDLE);

    rc = countdown(&o);
    restore_console();

    if (InterlockedCompareExchange(&g_cancel, 0, 0)) {
        if (o.run_count > 0)
            fprintf(stderr, SLEEPV_NAME ": cancelled, the --run command was not started.\n");
        free(o.runs);
        return rc;
    }
    if (o.msgbox)
        MessageBoxW(NULL, o.msg_text[0] ? o.msg_text : L"sleepv: countdown finished.",
                    L"sleepv", MB_OK | MB_ICONINFORMATION);
    if (o.run_count > 0) {
        int crc = run_commands(&o);
        if (rc == 0)
            rc = crc;
    }
    free(o.runs);
    return rc;
}
