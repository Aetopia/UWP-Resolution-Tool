#include <windows.h>
#include <psapi.h>
#include <shlwapi.h>
#define printf __builtin_printf

struct PROCESS
{
    WCHAR *ProcessName;
    HWND hWnd;
    DEVMODEW DevMode;
    MONITORINFOEXW MonitorInfoEx;
} Process = {.MonitorInfoEx = {.cbSize = sizeof(MONITORINFOEXW)},
             .DevMode = {.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY,
                         .dmSize = sizeof(DEVMODEW)}};

BOOL EnumChildWindowsProc(HWND hWnd, LPARAM lParam)
{
    DWORD Pid;
    WCHAR FileName[MAX_PATH];
    HANDLE hProcess;
    BOOL bNotImmersiveProcess;
    GetWindowThreadProcessId(hWnd, &Pid);
    hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, Pid);
    GetModuleFileNameExW(hProcess, 0, FileName, MAX_PATH);
    bNotImmersiveProcess = !IsImmersiveProcess(hProcess);
    CloseHandle(hProcess);
    PathStripPathW(FileName);
    wcslwr(FileName);
    if (wcscmp(FileName, Process.ProcessName) ||
        bNotImmersiveProcess)
        return TRUE;
    return FALSE;
}

LRESULT WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    static UINT WM_SHELLHOOKMESSAGE;
    switch (Msg)
    {
    case WM_CREATE:
        WM_SHELLHOOKMESSAGE = RegisterWindowMessageW(L"SHELLHOOK");
        do
        {
            Process.hWnd = GetForegroundWindow();
            if (!EnumChildWindows(Process.hWnd, EnumChildWindowsProc, 0))
                break;
        } while (!SleepEx(1, TRUE));
        RegisterShellHookWindow(hWnd);
        SendMessage(hWnd, WM_SHELLHOOKMESSAGE, HSHELL_WINDOWACTIVATED, (LPARAM)GetForegroundWindow());
        break;

    default:
        if (Msg == WM_SHELLHOOKMESSAGE)
        {
            switch (wParam)
            {
            case HSHELL_RUDEAPPACTIVATED:
            case HSHELL_WINDOWACTIVATED:
                HWND hAWnd = (HWND)lParam;
                GetMonitorInfoW(MonitorFromWindow(Process.hWnd, MONITOR_DEFAULTTONEAREST), (MONITORINFO *)&Process.MonitorInfoEx);
                if (hAWnd == Process.hWnd)
                {
                    printf("Foreground Window: %ld\n", ChangeDisplaySettingsExW(Process.MonitorInfoEx.szDevice, &Process.DevMode, 0, CDS_FULLSCREEN, 0));
                    ShowWindow(Process.hWnd, SW_RESTORE);
                }
                else if (hAWnd != Process.hWnd &&
                         !IsIconic(Process.hWnd))
                {
                    printf("Not Foreground Window: %ld\n",  ChangeDisplaySettingsExW(Process.MonitorInfoEx.szDevice, 0, 0, CDS_FULLSCREEN, 0));
                    ShowWindow(Process.hWnd, SW_MINIMIZE);
                };
                break;
            case HSHELL_WINDOWDESTROYED:
                if ((HWND)lParam == Process.hWnd)
                {
                    printf("Process terminated!\n");
                    PostQuitMessage(0);
                };
            };
        };
        return DefWindowProcW(hWnd, Msg, wParam, lParam);
    };
    return 0;
}

int wtoi_s(const wchar_t *str)
{
    if (wcsspn(str, L"0123456789-+") != wcslen(str))
        return 0;
    return _wtoi(str);
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    int ArgC;
    MSG Msg;
    WNDCLASSW WndClassW = {
        .lpszClassName = L" ",
        .lpfnWndProc = WindowProc,
        .hInstance = hInstance};
    LPWSTR *ArgV = CommandLineToArgvW(GetCommandLineW(), &ArgC);
    if (ArgC != 5)
    {
        return 1;
    };
    Process.ProcessName = ArgV[1];
    Process.DevMode.dmPelsWidth = wtoi_s(ArgV[2]);
    Process.DevMode.dmPelsHeight = wtoi_s(ArgV[3]);
    Process.DevMode.dmDisplayFrequency = wtoi_s(ArgV[4]);
    if (!(Process.DevMode.dmPelsWidth |
          Process.DevMode.dmPelsHeight |
          Process.DevMode.dmDisplayFrequency))
        return 1;
    wcslwr(Process.ProcessName);

    if (!RegisterClassW(&WndClassW) ||
        !CreateWindowW(L" ", 0, 0, 0, 0, 0, 0, 0, 0, hInstance, 0))
        return 1;

    while (GetMessageW(&Msg, 0, 0, 0))
    {
        TranslateMessage(&Msg);
        DispatchMessageW(&Msg);
    };
    return 0;
}