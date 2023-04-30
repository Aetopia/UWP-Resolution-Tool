#include <windows.h>
#include <psapi.h>
#include <shlwapi.h>
#define printf __builtin_printf

// Store information like the UWP app's main process name, window handle and display mode information.
struct PROCESS
{
    WCHAR *ProcessName;
    HWND hWnd;
    DEVMODEW DevMode;
    MONITORINFOEXW MonitorInfoEx;
} Process = {.MonitorInfoEx = {.cbSize = sizeof(MONITORINFOEXW)},
             .DevMode = {.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY,
                         .dmSize = sizeof(DEVMODEW)}};

// To get the main process of an UWP app, we need to enumarate through its child windows.
BOOL EnumChildWindowsProc(HWND hWnd, LPARAM lParam)
{
    DWORD Pid;
    WCHAR FileName[MAX_PATH];
    HANDLE hProcess;
    BOOL bNotImmersiveProcess;

    // Gather required data from the window handle.
    GetWindowThreadProcessId(hWnd, &Pid);
    hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, Pid);
    GetModuleFileNameExW(hProcess, 0, FileName, MAX_PATH);
    bNotImmersiveProcess = !IsImmersiveProcess(hProcess);
    CloseHandle(hProcess);

    // Extract the process name.
    PathStripPathW(FileName);
    wcslwr(FileName);

    // Make sure the process name matches and it is an immersive process.
    if (wcscmp(FileName, Process.ProcessName) ||
        bNotImmersiveProcess)
        return TRUE;
    return FALSE;
}

LRESULT WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    // Obtain Shell messages, this allows the program to certain messages to detect what window is activated and which window has been destroyed.
    static UINT WM_SHELLHOOKMESSAGE;
    switch (Msg)
    {
    case WM_CREATE:
        WM_SHELLHOOKMESSAGE = RegisterWindowMessageW(L"SHELLHOOK");

        // Hook onto the target process and obtain a window handle to it.
        do
        {
            Process.hWnd = GetForegroundWindow();
            if (!EnumChildWindows(Process.hWnd, EnumChildWindowsProc, 0))
                break;
        } while (!SleepEx(1, TRUE));

        // Set the resolution.
        RegisterShellHookWindow(hWnd);
        SendMessageW(hWnd, WM_SHELLHOOKMESSAGE, HSHELL_WINDOWACTIVATED, (LPARAM)GetForegroundWindow());
        break;

    default:
        if (Msg == WM_SHELLHOOKMESSAGE)
        {
            switch (wParam)
            {
            case HSHELL_RUDEAPPACTIVATED:
            case HSHELL_WINDOWACTIVATED:

                // Get the name of the monitor, the hooked UWP app is on currently.
                HWND hAWnd = (HWND)lParam;
                GetMonitorInfoW(MonitorFromWindow(Process.hWnd, MONITOR_DEFAULTTONEAREST), (MONITORINFO *)&Process.MonitorInfoEx);

                // Apply the resolution and restore the window.
                if (hAWnd == Process.hWnd)
                {
                    printf("Foreground Window: %ld\n", ChangeDisplaySettingsExW(Process.MonitorInfoEx.szDevice, &Process.DevMode, 0, CDS_FULLSCREEN, 0));
                    ShowWindow(Process.hWnd, SW_RESTORE);
                }

                // Reset the resolution and minimize the window.
                else if (hAWnd != Process.hWnd &&
                         !IsIconic(Process.hWnd))
                {
                    printf("Not Foreground Window: %ld\n", ChangeDisplaySettingsExW(Process.MonitorInfoEx.szDevice, 0, 0, CDS_FULLSCREEN, 0));
                    ShowWindow(Process.hWnd, SW_MINIMIZE);
                };
                break;

            // Check if the hooked UWP's app is destroyed or not.
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

// A "secure" implementation of _wtoi, returns zero if the provided string contains anything else than integers.
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
        MessageBoxW(0, L"<Process Name> <Width> <Height <Refresh Rate>", L"UWP Resolution Tool", 0);
        return 1;
    };

    // Get the main process of the target UWP app.
    Process.ProcessName = ArgV[1];
    wcslwr(Process.ProcessName);

    // Check if the specified display mode doesn't have the width, height or refresh rate set to 0.
    Process.DevMode.dmPelsWidth = wtoi_s(ArgV[2]);
    Process.DevMode.dmPelsHeight = wtoi_s(ArgV[3]);
    Process.DevMode.dmDisplayFrequency = wtoi_s(ArgV[4]);
    if (!(Process.DevMode.dmPelsWidth |
          Process.DevMode.dmPelsHeight |
          Process.DevMode.dmDisplayFrequency))
        return 1;

    // Create an invisible window to handle setting the resolution depending if UWP app's window is in the foreground or not.
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