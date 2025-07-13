#define UNICODE
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")

static const wchar_t CLASS_NAME[] = L"CustomInjectorWindow";
static const UINT IDC_EDIT_DLL = 101;
static const UINT IDC_BUTTON_BROWSE = 102;
static const UINT IDC_EDIT_PID = 103;
static const UINT IDC_BUTTON_INJECT = 104;
static const UINT IDC_STATIC_STATUS = 105;

// Prototypen
BOOL InjectDLL(DWORD pid, const char* dllPath);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Einstiegsfunktion
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icex{ sizeof(icex), ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icex);

    // Fensterklasse registrieren
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wc);

    // Fenster erstellen
    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"DLL Injector v1.0",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 200,
        nullptr, nullptr, hInst, nullptr
    );
    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Nachrichtenloop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

// Message-Handler
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static std::wstring dllPathW;
    switch (msg) {
    case WM_CREATE: {
        // Label DLL-Pfad
        CreateWindowW(L"STATIC", L"DLL-Pfad:",
            WS_VISIBLE | WS_CHILD, 10, 10, 60, 20, hwnd, nullptr, nullptr, nullptr);
        // Edit-Control für DLL-Pfad
        CreateWindowW(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
            80, 10, 220, 20, hwnd, (HMENU)IDC_EDIT_DLL, nullptr, nullptr);
        // Browse-Button
        CreateWindowW(L"BUTTON", L"Browse…",
            WS_VISIBLE | WS_CHILD,
            310, 10, 70, 20, hwnd, (HMENU)IDC_BUTTON_BROWSE, nullptr, nullptr);

        // Label PID
        CreateWindowW(L"STATIC", L"PID:",
            WS_VISIBLE | WS_CHILD, 10, 45, 60, 20, hwnd, nullptr, nullptr, nullptr);
        // Edit-Control für PID
        CreateWindowW(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
            80, 45, 80, 20, hwnd, (HMENU)IDC_EDIT_PID, nullptr, nullptr);

        // Inject-Button
        CreateWindowW(L"BUTTON", L"Inject",
            WS_VISIBLE | WS_CHILD,
            310, 45, 70, 20, hwnd, (HMENU)IDC_BUTTON_INJECT, nullptr, nullptr);

        // Status-Static
        CreateWindowW(L"STATIC", L"",
            WS_VISIBLE | WS_CHILD,
            10, 80, 370, 60, hwnd, (HMENU)IDC_STATIC_STATUS, nullptr, nullptr);
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == IDC_BUTTON_BROWSE) {
            // DLL-Datei auswählen
            OPENFILENAMEW ofn{};
            wchar_t szFile[MAX_PATH] = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"DLL Files\0*.dll\0All Files\0*.*\0";
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileNameW(&ofn)) {
                dllPathW = szFile;
                SetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_DLL), szFile);
            }
        }
        else if (id == IDC_BUTTON_INJECT) {
            // Pfad und PID auslesen
            wchar_t bufPath[MAX_PATH]{};
            GetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_DLL), bufPath, MAX_PATH);

            wchar_t bufPid[16]{};
            GetWindowTextW(GetDlgItem(hwnd, IDC_EDIT_PID), bufPid, 16);

            DWORD pid = _wtoi(bufPid);
            if (pid == 0 || wcslen(bufPath) == 0) {
                SetWindowTextW(GetDlgItem(hwnd, IDC_STATIC_STATUS), L"Ungültige Eingabe!");
                break;
            }

            // WideChar → ANSI
            int len = WideCharToMultiByte(CP_ACP, 0, bufPath, -1, nullptr, 0, nullptr, nullptr);
            std::vector<char> pathA(len);
            WideCharToMultiByte(CP_ACP, 0, bufPath, -1, pathA.data(), len, nullptr, nullptr);

            // Injection versuchen
            bool ok = InjectDLL(pid, pathA.data());
            if (ok)
                SetWindowTextW(GetDlgItem(hwnd, IDC_STATIC_STATUS), L"Injektion erfolgreich!");
            else {
                wchar_t err[64];
                wsprintfW(err, L"Injektion fehlgeschlagen: %d", GetLastError());
                SetWindowTextW(GetDlgItem(hwnd, IDC_STATIC_STATUS), err);
            }
        }
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

// Deine bekannte InjectDLL-Funktion (nur minimal angepasst)
BOOL InjectDLL(DWORD pid, const char* dllPath) {
    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD |
        PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE |
        PROCESS_QUERY_INFORMATION |
        PROCESS_VM_READ,
        FALSE, pid);
    if (!hProcess) return FALSE;

    SIZE_T size = strlen(dllPath) + 1;
    LPVOID pRemote = VirtualAllocEx(hProcess, nullptr, size, MEM_COMMIT, PAGE_READWRITE);
    if (!pRemote) { CloseHandle(hProcess); return FALSE; }

    if (!WriteProcessMemory(hProcess, pRemote, dllPath, size, nullptr)) {
        VirtualFreeEx(hProcess, pRemote, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    HMODULE hKernel = GetModuleHandleA("kernel32.dll");
    auto pLoadLib = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel, "LoadLibraryA");
    if (!pLoadLib) {
        VirtualFreeEx(hProcess, pRemote, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, pLoadLib, pRemote, 0, nullptr);
    if (!hThread) {
        VirtualFreeEx(hProcess, pRemote, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemote, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return TRUE;
}