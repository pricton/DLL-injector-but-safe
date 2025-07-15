#include <windows.h>
#include <tlhelp32.h>
#include <string>

#pragma comment(lib, "comdlg32.lib")

#define ID_PROCESSBOX   101
#define ID_DLLBOX       102
#define ID_BUTTON_INJECT 103
#define ID_STATUS       104
#define ID_BUTTON_SELECTDLL 105

std::wstring Wide(const std::string& str) {
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], len);
    return result;
}

std::string Narrow(const std::wstring& str) {
    int len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, NULL, 0, NULL, NULL);
    std::string result(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, &result[0], len, NULL, NULL);
    return result;
}

DWORD GetProcId(const std::wstring& procName) {
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    while (Process32NextW(snapshot, &entry)) {
        if (procName == entry.szExeFile) {
            pid = entry.th32ProcessID;
            break;
        }
    }
    CloseHandle(snapshot);
    return pid;
}

bool InjectDLL(DWORD pid, const std::string& dllPath) {
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) return false;

    LPVOID alloc = VirtualAllocEx(hProc, nullptr, dllPath.size(), MEM_COMMIT, PAGE_READWRITE);
    if (!alloc) return false;

    WriteProcessMemory(hProc, alloc, dllPath.c_str(), dllPath.size(), nullptr);

    HANDLE thread = CreateRemoteThread(hProc, nullptr, 0,
        (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA"),
        alloc, 0, nullptr);

    CloseHandle(hProc);
    return thread != nullptr;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HWND hProcBox, hDLLBox, hInjectBtn, hStatus, hSelectBtn;

    switch (msg) {
    case WM_CREATE:
        hProcBox = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            20, 20, 250, 25, hwnd, (HMENU)ID_PROCESSBOX, NULL, NULL);

        hSelectBtn = CreateWindowW(L"BUTTON", L"Datei auswählen",
            WS_CHILD | WS_VISIBLE,
            280, 20, 90, 25,
            hwnd, (HMENU)ID_BUTTON_SELECTDLL, NULL, NULL);

        hDLLBox = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            20, 60, 250, 25, hwnd, (HMENU)ID_DLLBOX, NULL, NULL);

        hInjectBtn = CreateWindowW(L"BUTTON", L"Inject DLL",
            WS_CHILD | WS_VISIBLE,
            280, 60, 90, 25, hwnd, (HMENU)ID_BUTTON_INJECT, NULL, NULL);

        hStatus = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 100, 350, 25, hwnd, (HMENU)ID_STATUS, NULL, NULL);
        break;

    case WM_COMMAND:
        if (LOWORD(wp) == ID_BUTTON_SELECTDLL) {
            wchar_t filePath[MAX_PATH] = { 0 };
            OPENFILENAMEW ofn = { 0 };
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"DLL-Dateien (*.dll)\0*.dll\0Alle Dateien\0*.*\0";
            ofn.lpstrFile = filePath;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (GetOpenFileNameW(&ofn)) {
                SetWindowTextW(hDLLBox, filePath);
                SetWindowTextW(hStatus, L"📂 DLL ausgewählt");
            }
            else {
                SetWindowTextW(hStatus, L"⚠️ Keine Datei gewählt");
            }
        }

        if (LOWORD(wp) == ID_BUTTON_INJECT) {
            wchar_t procName[256], dllPath[256];
            GetWindowTextW(hProcBox, procName, 256);
            GetWindowTextW(hDLLBox, dllPath, 256);

            DWORD pid = GetProcId(procName);
            if (!pid) {
                SetWindowTextW(hStatus, L"❌ Prozess nicht gefunden.");
                break;
            }

            bool ok = InjectDLL(pid, Narrow(dllPath));
            SetWindowTextW(hStatus, ok ? L"✅ DLL injiziert!" : L"❌ Injection fehlgeschlagen.");
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"DLLInjectorGUI";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    HWND hwnd = CreateWindowW(L"DLLInjectorGUI", L"🔧 DLL Injector by Markus",
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME,
        300, 200, 400, 180, NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
