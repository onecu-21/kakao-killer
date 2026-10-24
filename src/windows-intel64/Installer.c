
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <strsafe.h>
#include <wchar.h>

#define APP_NAME L"Kakao Killer"
#define EXE_NAME L"kkokiler.exe"
#define RESOURCE_ID 101

#define STARTUP_KEY \
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"

#define UNINSTALL_KEY \
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\KakaoKiller"

static void error(const wchar_t *msg) {
    MessageBoxW(NULL, msg, APP_NAME, MB_OK | MB_ICONERROR);
}

static BOOL get_paths(
    wchar_t *dir,
    wchar_t *exe,
    wchar_t *uninstaller
) {
    wchar_t local[MAX_PATH];

    if (FAILED(SHGetFolderPathW(
        NULL, CSIDL_LOCAL_APPDATA,
        NULL, SHGFP_TYPE_CURRENT, local
    ))) return FALSE;

    if (FAILED(StringCchPrintfW(
        dir, MAX_PATH, L"%s\\Programs\\KakaoKiller", local
    ))) return FALSE;

    if (FAILED(StringCchPrintfW(
        exe, MAX_PATH, L"%s\\%s", dir, EXE_NAME
    ))) return FALSE;

    return SUCCEEDED(StringCchPrintfW(
        uninstaller, MAX_PATH, L"%s\\uninstall.exe", dir
    ));
}

static BOOL extract_exe(const wchar_t *path) {
    HRSRC resource = FindResourceW(
        NULL, MAKEINTRESOURCEW(RESOURCE_ID), RT_RCDATA
    );

    if (!resource) return FALSE;

    HGLOBAL loaded = LoadResource(NULL, resource);
    if (!loaded) return FALSE;

    const void *data = LockResource(loaded);
    DWORD size = SizeofResource(NULL, resource);

    if (!data || !size) return FALSE;

    HANDLE file = CreateFileW(
        path, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    );

    if (file == INVALID_HANDLE_VALUE) return FALSE;

    DWORD written = 0;
    BOOL result = WriteFile(
        file, data, size, &written, NULL
    );

    CloseHandle(file);
    return result && written == size;
}

static void autostart(const wchar_t *exe, BOOL enable) {
    HKEY key;

    if (RegCreateKeyExW(
        HKEY_CURRENT_USER, STARTUP_KEY,
        0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL
    ) != ERROR_SUCCESS) return;

    if (enable) {
        wchar_t command[MAX_PATH + 4];

        if (SUCCEEDED(StringCchPrintfW(
            command, MAX_PATH + 4, L"\"%s\"", exe
        ))) {
            RegSetValueExW(
                key, L"KakaoKiller", 0, REG_SZ,
                (const BYTE *)command,
                (DWORD)((wcslen(command) + 1) * sizeof(wchar_t))
            );
        }
    } else {
        RegDeleteValueW(key, L"KakaoKiller");
    }

    RegCloseKey(key);
}

static void register_uninstall(const wchar_t *path) {
    HKEY key;

    if (RegCreateKeyExW(
        HKEY_CURRENT_USER, UNINSTALL_KEY,
        0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL
    ) != ERROR_SUCCESS) return;

    const wchar_t *name = APP_NAME;

    RegSetValueExW(
        key, L"DisplayName", 0, REG_SZ,
        (const BYTE *)name,
        (DWORD)((wcslen(name) + 1) * sizeof(wchar_t))
    );

    wchar_t command[MAX_PATH + 32];

    if (SUCCEEDED(StringCchPrintfW(
        command, MAX_PATH + 32, L"\"%s\" /uninstall", path
    ))) {
        RegSetValueExW(
            key, L"UninstallString", 0, REG_SZ,
            (const BYTE *)command,
            (DWORD)((wcslen(command) + 1) * sizeof(wchar_t))
        );
    }

    RegCloseKey(key);
}

static void stop_killer(void) {
    // Stop the installed instance, not unrelated processes.
    // Graceful shutdown support can be added to main.c later.
}

static int uninstall_app(
    const wchar_t *dir,
    const wchar_t *exe,
    const wchar_t *uninstaller
) {
    if (MessageBoxW(
        NULL, L"Uninstall Kakao Killer?",
        APP_NAME, MB_YESNO | MB_ICONQUESTION
    ) != IDYES) return 0;

    autostart(exe, FALSE);
    stop_killer();

    RegDeleteKeyW(HKEY_CURRENT_USER, UNINSTALL_KEY);

    if (!DeleteFileW(exe)) {
        MoveFileExW(
            exe, NULL, MOVEFILE_DELAY_UNTIL_REBOOT
        );
    }

    MoveFileExW(
        uninstaller, NULL, MOVEFILE_DELAY_UNTIL_REBOOT
    );

    RemoveDirectoryW(dir);

    MessageBoxW(
        NULL,
        L"Uninstallation requested.\n"
        L"Close any running Kakao Killer instance.\n"
        L"A restart may be required.",
        APP_NAME, MB_OK | MB_ICONINFORMATION
    );

    return 0;
}

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE previous,
    PWSTR args,
    int show
) {
    (void)instance;
    (void)previous;
    (void)show;

    wchar_t dir[MAX_PATH];
    wchar_t exe[MAX_PATH];
    wchar_t uninstaller[MAX_PATH];

    if (!get_paths(dir, exe, uninstaller)) {
        error(L"Cannot resolve installation path.");
        return 1;
    }

    if (wcscmp(args, L"/uninstall") == 0) {
        return uninstall_app(dir, exe, uninstaller);
    }

    if (MessageBoxW(
        NULL,
        L"Install Kakao Killer?\n\n"
        L"This program forcibly terminates KakaoTalk.exe "
        L"every minute, even while it is being used.\n\n"
        L"Unsaved messages and transfers may be lost.",
        APP_NAME, MB_YESNO | MB_ICONWARNING
    ) != IDYES) return 0;

    int result = SHCreateDirectoryExW(NULL, dir, NULL);

    if (result != ERROR_SUCCESS &&
        result != ERROR_ALREADY_EXISTS) {
        error(L"Cannot create installation directory.");
        return 1;
    }

    if (!extract_exe(exe)) {
        error(L"Cannot extract kkokiler.exe.");
        return 1;
    }

    wchar_t self[MAX_PATH];

    if (!GetModuleFileNameW(NULL, self, MAX_PATH) ||
        !CopyFileW(self, uninstaller, FALSE)) {
        error(L"Cannot create uninstaller.");
        return 1;
    }

    register_uninstall(uninstaller);

    BOOL startup = MessageBoxW(
        NULL,
        L"Start Kakao Killer automatically "
        L"when Windows starts?",
        APP_NAME, MB_YESNO | MB_ICONQUESTION
    ) == IDYES;

    autostart(exe, startup);

    if (MessageBoxW(
        NULL,
        L"Installation complete.\n\n"
        L"Run Kakao Killer now?",
        APP_NAME, MB_YESNO | MB_ICONQUESTION
    ) == IDYES) {
        ShellExecuteW(
            NULL, L"open", exe,
            NULL, dir, SW_SHOWNORMAL
        );
    }

    return 0;
}
