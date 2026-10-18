#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static void fail(const wchar_t *msg) {
    MessageBoxW(NULL, msg, L"Papercut", MB_ICONERROR | MB_OK);
    ExitProcess(1);
}

static wchar_t *skip_argv0(wchar_t *p) {
    if (!p) return L"";
    while (*p == L' ' || *p == L'\t') p++;
    if (*p == L'"') {
        p++;
        while (*p && *p != L'"') p++;
        if (*p == L'"') p++;
    } else {
        while (*p && *p != L' ' && *p != L'\t') p++;
    }
    return p;
}

static int is_cli_request(void) {
    static const wchar_t *flags[] = {L"--help", L"-h", L"--version", L"--share", L"--combine", L"--export-pages",
                                     L"--folio-add", L"--export-backup", L"--import-backup", L"--headless",
                                     L"-o", L"--output"};
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int cli = 0;
    for (int i = 1; argv && i < argc && !cli; ++i)
        for (size_t f = 0; f < sizeof(flags) / sizeof(flags[0]); ++f)
            if (wcscmp(argv[i], flags[f]) == 0) {
                cli = 1;
                break;
            }
    if (argv) LocalFree(argv);
    return cli;
}

static HANDLE valid_or_console(HANDLE h, const wchar_t *conName, DWORD access) {
    if (h != NULL && h != INVALID_HANDLE_VALUE) {
        SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        return h;
    }
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    h = CreateFileW(conName, access, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    return h == INVALID_HANDLE_VALUE ? NULL : h;
}

static int run(void) {
    wchar_t self[MAX_PATH];
    wchar_t child[MAX_PATH];
    wchar_t dir[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, self, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) fail(L"Could not locate this executable.");

    lstrcpynW(dir, self, MAX_PATH);
    wchar_t *slash = wcsrchr(dir, L'\\');
    if (!slash) fail(L"Could not locate the install folder.");
    *slash = 0;

    if (_snwprintf(child, MAX_PATH, L"%s\\bin\\papercut.exe", dir) < 0) fail(L"Could not build the bin path.");
    child[MAX_PATH - 1] = 0;
    if (GetFileAttributesW(child) == INVALID_FILE_ATTRIBUTES)
        fail(L"Missing bin\\ app. Reinstall Papercut so the bin folder sits next to this launcher.");

    wchar_t *rest = skip_argv0(GetCommandLineW());
    size_t cap = wcslen(child) + wcslen(rest) + 4;
    wchar_t *cmd = (wchar_t *)malloc(cap * sizeof(wchar_t));
    if (!cmd) fail(L"Out of memory.");
    _snwprintf(cmd, cap, L"\"%s\"%s", child, rest);
    cmd[cap - 1] = 0;

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    const int cli = is_cli_request();
    if (cli) {
        HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE herr = GetStdHandle(STD_ERROR_HANDLE);
        AttachConsole(ATTACH_PARENT_PROCESS);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = valid_or_console(hin, L"CONIN$", GENERIC_READ);
        si.hStdOutput = valid_or_console(hout, L"CONOUT$", GENERIC_WRITE);
        si.hStdError = valid_or_console(herr, L"CONOUT$", GENERIC_WRITE);
    }
    BOOL ok = CreateProcessW(child, cmd, NULL, NULL, cli ? TRUE : FALSE, 0, NULL, NULL, &si, &pi);
    free(cmd);
    if (!ok) fail(L"Could not start the app in the bin folder.");

    DWORD code = 0;
    if (cli) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &code);
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
    (void)inst;
    (void)prev;
    (void)cmd;
    (void)show;
    return run();
}
