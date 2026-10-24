#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <wchar.h>

#define INTERVAL 60000  // 5분

void kill_kakao(void) {
    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPPROCESS, 0
    );

    if (snapshot == INVALID_HANDLE_VALUE)
        return;

    PROCESSENTRY32W pe = {0};
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(snapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"KakaoTalk.exe") == 0) {
                HANDLE process = OpenProcess(
                    PROCESS_TERMINATE,
                    FALSE,
                    pe.th32ProcessID
                );

                if (process) {
                    TerminateProcess(process, 0);
                    CloseHandle(process);
                }
            }
        } while (Process32NextW(snapshot, &pe));
    }

    CloseHandle(snapshot);
}

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE previous,
    PWSTR cmdline,
    int show
) {
    while (1) {
        kill_kakao();
        Sleep(INTERVAL);
    }
}