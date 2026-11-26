#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

// Имена искомых файлов (при необходимости поменяй на otdowm.exe / otdowm.ini)
const std::wstring TARGET_EXE = L"otvdm.exe";
const std::wstring TARGET_INI = L"otvdm.ini";

// Глобальные состояния экрана MS-DOS
std::wstring g_bgPath;
std::wstring g_otvdmExe;
std::wstring g_currentDir = L"A:\\";
std::wstring g_inputBuffer;
std::vector<std::wstring> g_screenLines;
bool g_dosReady = false;

// Добавление строки на экран MS-DOS
void DosPrint(const std::wstring& text) {
    g_screenLines.push_back(text);
    if (g_screenLines.size() > 42) {
        g_screenLines.erase(g_screenLines.begin());
    }
}

// Кастомное окно ошибки со шрифтом Segoe UI
void ShowSegoeError(HINSTANCE hInstance, const std::wstring& title, const std::wstring& text) {
    HFONT hFont = CreateFontW(
        -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
    );

    WNDCLASSW wc = {};
    wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
        switch (msg) {
        case WM_COMMAND:
            if (LOWORD(wp) == IDOK) DestroyWindow(hwnd);
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
        };
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SegoeErrorBoxClass";
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    RegisterClassW(&wc);

    int w = 340, h = 155;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_DLGMODALFRAME,
        L"SegoeErrorBoxClass", title.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, w, h, NULL, NULL, hInstance, NULL
    );

    HWND hIcon = CreateWindowW(L"STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_ICON, 20, 22, 32, 32, hwnd, NULL, hInstance, NULL);
    SendMessageW(hIcon, STM_SETICON, (WPARAM)LoadIconW(NULL, IDI_ERROR), 0);

    HWND hText = CreateWindowW(L"STATIC", text.c_str(), WS_CHILD | WS_VISIBLE, 68, 28, 240, 25, hwnd, NULL, hInstance, NULL);
    SendMessageW(hText, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hBtn = CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 125, 78, 88, 28, hwnd, (HMENU)IDOK, hInstance, NULL);
    SendMessageW(hBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

    MessageBeep(MB_ICONHAND);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && (msg.wParam == VK_RETURN || msg.wParam == VK_ESCAPE)) {
            DestroyWindow(hwnd);
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    DeleteObject(hFont);
}

// Запуск 16-битной программы (.exe/.com) с диска A:\ через otvdm
bool LaunchDosBinary(const std::wstring& progName, const std::wstring& args) {
    std::vector<std::wstring> candidates = {
        g_currentDir + (g_currentDir.back() == L'\\' ? L"" : L"\\") + progName,
        g_currentDir + (g_currentDir.back() == L'\\' ? L"" : L"\\") + progName + L".EXE",
        g_currentDir + (g_currentDir.back() == L'\\' ? L"" : L"\\") + progName + L".COM",
        L"A:\\DOS71\\" + progName,
        L"A:\\DOS71\\" + progName + L".EXE",
        L"A:\\DOS71\\" + progName + L".COM"
    };

    std::wstring foundBin;
    for (const auto& c : candidates) {
        std::error_code ec;
        if (fs::exists(c, ec) && fs::is_regular_file(c, ec)) {
            foundBin = c;
            break;
        }
    }

    if (foundBin.empty()) return false;

    std::wstring cmdLine = L"\"" + g_otvdmExe + L"\" \"" + foundBin + L"\"" + (args.empty() ? L"" : L" " + args);
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (CreateProcessW(NULL, cmdLine.data(), NULL, NULL, FALSE, 0, NULL, g_currentDir.c_str(), &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return true;
}

// Обработка введённой команды MS-DOS 7.10 прямо на чёрном фоне
void ExecuteDosCommand(HWND hwnd, const std::wstring& rawCmd) {
    DosPrint(g_currentDir + L">" + rawCmd);

    // Убираем пробелы по краям
    size_t first = rawCmd.find_first_not_of(L" \t");
    if (first == std::wstring::npos) return;
    std::wstring cmdLine = rawCmd.substr(first);

    std::wstring cmd = cmdLine;
    std::wstring args;
    size_t sp = cmdLine.find(L' ');
    if (sp != std::wstring::npos) {
        cmd = cmdLine.substr(0, sp);
        args = cmdLine.substr(sp + 1);
    }

    std::wstring upperCmd = cmd;
    std::transform(upperCmd.begin(), upperCmd.end(), upperCmd.begin(), ::towupper);

    if (upperCmd == L"EXIT") {
        DestroyWindow(hwnd);
        return;
    }
    else if (upperCmd == L"CLS") {
        g_screenLines.clear();
        return;
    }
    else if (upperCmd == L"VER") {
        DosPrint(L"");
        DosPrint(L"MS-DOS 7.1 [Version 7.10.1999]");
        DosPrint(L"");
        return;
    }
    else if (upperCmd == L"HELP") {
        DosPrint(L"Built-in commands: DIR, CD, CLS, VER, ECHO, EXIT");
        DosPrint(L"Programs in A:\\DOS71: EDIT, QBASIC, DOSSHELL, MEM, MSD, SCANDISK");
        DosPrint(L"");
        return;
    }
    else if (upperCmd == L"ECHO") {
        DosPrint(args);
        return;
    }
    else if (upperCmd == L"CD" || upperCmd == L"CHDIR") {
        if (args.empty()) {
            DosPrint(g_currentDir);
        }
        else {
            fs::path target = (args == L"\\") ? fs::path(L"A:\\") : (fs::path(g_currentDir) / args).lexically_normal();
            std::error_code ec;
            if (fs::exists(target, ec) && fs::is_directory(target, ec)) {
                g_currentDir = target.wstring();
                if (g_currentDir.size() > 3 && g_currentDir.back() == L'\\') g_currentDir.pop_back();
            }
            else {
                DosPrint(L"Invalid directory");
            }
        }
        return;
    }
    else if (upperCmd == L"DIR") {
        DosPrint(L" Volume in drive A is MS-DOS_71");
        DosPrint(L" Directory of " + g_currentDir);
        DosPrint(L"");
        std::error_code ec;
        int fileCount = 0, dirCount = 0;
        for (const auto& entry : fs::directory_iterator(g_currentDir, ec)) {
            std::wstring name = entry.path().filename().wstring();
            if (entry.is_directory(ec)) {
                DosPrint(L"  " + name + L"        <DIR>");
                dirCount++;
            }
            else {
                DosPrint(L"  " + name + L"        " + std::to_wstring(entry.file_size(ec)) + L" bytes");
                fileCount++;
            }
        }
        DosPrint(L"        " + std::to_wstring(fileCount) + L" file(s), " + std::to_wstring(dirCount) + L" dir(s)");
        DosPrint(L"");
        return;
    }

    // Пробуем запустить 16-битный .EXE / .COM с диска A:\ через otvdm
    if (!LaunchDosBinary(cmd, args)) {
        DosPrint(L"Bad command or file name");
        DosPrint(L"");
    }
}

// Оконная процедура полноэкранного MS-DOS поверх Windows
LRESULT CALLBACK FullscreenBgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HBITMAP hBitmap = NULL;
    static std::wstring loadedPath;
    static HFONT hDosFont = NULL;

    if (g_bgPath != loadedPath) {
        if (hBitmap) {
            DeleteObject(hBitmap);
            hBitmap = NULL;
        }
        if (!g_bgPath.empty()) {
            hBitmap = (HBITMAP)LoadImageW(NULL, g_bgPath.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
        }
        loadedPath = g_bgPath;
    }

    switch (uMsg) {
    case WM_CREATE:
        hDosFont = CreateFontW(
            24, 12, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            OEM_CHARSET, OUT_RASTER_PRECIS, CLIP_DEFAULT_PRECIS,
            NONANTIALIASED_QUALITY, FIXED_PITCH | FF_DONTCARE, L"Terminal"
        );
        return 0;

    case WM_CHAR:
        if (!g_dosReady) return 0;
        if (wParam == VK_RETURN) {
            std::wstring cmd = g_inputBuffer;
            g_inputBuffer.clear();
            ExecuteDosCommand(hwnd, cmd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (wParam == VK_BACK) {
            if (!g_inputBuffer.empty()) {
                g_inputBuffer.pop_back();
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        else if (wParam >= 32) {
            g_inputBuffer.push_back((wchar_t)wParam);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);

        HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &rect, blackBrush);
        DeleteObject(blackBrush);

        if (hBitmap) {
            HDC hdcMem = CreateCompatibleDC(hdc);
            SelectObject(hdcMem, hBitmap);
            BITMAP bm;
            GetObject(hBitmap, sizeof(bm), &bm);
            SetStretchBltMode(hdc, COLORONCOLOR);
            StretchBlt(hdc, 0, 0, rect.right, rect.bottom, hdcMem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
            DeleteDC(hdcMem);
        }

        // Отрисовка текста MS-DOS 7.10 поверх чёрного фона
        if (g_dosReady) {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(192, 192, 192));
            SelectObject(hdc, hDosFont);
            int y = 18;
            for (const auto& line : g_screenLines) {
                TextOutW(hdc, 24, y, line.c_str(), (int)line.size());
                y += 24;
            }
            std::wstring promptLine = g_currentDir + L">" + g_inputBuffer + L"_";
            TextOutW(hdc, 24, y, promptLine.c_str(), (int)promptLine.size());
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        if (hBitmap) DeleteObject(hBitmap);
        if (hDosFont) DeleteObject(hDosFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}
// Поиск exe и ini во всех дисках и папках
void FindFilesAcrossAllDrives(std::wstring& foundExe, std::wstring& foundIni) {
    DWORD drivesMask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (drivesMask & (1 << i)) {
            std::wstring root = std::wstring(1, L'A' + i) + LR"(:\)";
            UINT type = GetDriveTypeW(root.c_str());
            if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE || type == DRIVE_RAMDISK) {
                std::error_code ec;
                for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
                    it != fs::recursive_directory_iterator(); it.increment(ec)) {
                    if (ec) { ec.clear(); continue; }
                    if (it->is_regular_file(ec)) {
                        std::wstring fname = it->path().filename().wstring();
                        if (_wcsicmp(fname.c_str(), TARGET_EXE.c_str()) == 0 && foundExe.empty()) {
                            foundExe = it->path().wstring();
                        }
                        if (_wcsicmp(fname.c_str(), TARGET_INI.c_str()) == 0 && foundIni.empty()) {
                            foundIni = it->path().wstring();
                        }
                        if (!foundExe.empty() && !foundIni.empty()) return;
                    }
                }
            }
        }
    }
}

// Извлечение ресурса RCDATA
bool ExtractResourceToFile(int resId, const std::wstring& outPath) {
    HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hRes) return false;
    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) return false;
    DWORD size = SizeofResource(NULL, hRes);
    void* ptr = LockResource(hData);
    if (!ptr || size == 0) return false;

    std::ofstream out(outPath, std::ios::binary);
    if (!out) return false;
    out.write(static_cast<const char*>(ptr), size);
    return true;
}

// Скрытый запуск команды распаковки tar.exe
void RunHiddenAndWait(std::wstring cmd) {
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    if (CreateProcessW(NULL, cmd.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}
// Универсальное диалоговое окно со шрифтом Segoe UI (поддерживает OK или Да/Нет)
int ShowSegoeDialog(HINSTANCE hInstance, const std::wstring& title, const std::wstring& text, bool yesNo, bool isRu, LPCWSTR iconId) {
    struct DlgState { int result = IDNO; bool yesNo; };
    DlgState state{ yesNo ? IDNO : IDOK, yesNo };

    HFONT hFont = CreateFontW(
        -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
    );

    WNDCLASSW wc = {};
    wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
        DlgState* st = reinterpret_cast<DlgState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id == IDOK || id == IDYES || id == IDNO) {
                if (st) st->result = id;
                DestroyWindow(hwnd);
            }
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
        };
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SegoeCustomDlgClass";
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    RegisterClassW(&wc);

    int w = 410, h = 165;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_DLGMODALFRAME,
        L"SegoeCustomDlgClass", title.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, w, h, NULL, NULL, hInstance, &state
    );

    HWND hIcon = CreateWindowW(L"STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_ICON, 20, 24, 32, 32, hwnd, NULL, hInstance, NULL);
    SendMessageW(hIcon, STM_SETICON, (WPARAM)LoadIconW(NULL, iconId), 0);

    HWND hText = CreateWindowW(L"STATIC", text.c_str(), WS_CHILD | WS_VISIBLE, 68, 22, 315, 45, hwnd, NULL, hInstance, NULL);
    SendMessageW(hText, WM_SETFONT, (WPARAM)hFont, TRUE);

    if (yesNo) {
        HWND hYes = CreateWindowW(L"BUTTON", isRu ? L"Да" : L"Yes", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 110, 86, 90, 28, hwnd, (HMENU)IDYES, hInstance, NULL);
        HWND hNo = CreateWindowW(L"BUTTON", isRu ? L"Нет" : L"No", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 212, 86, 90, 28, hwnd, (HMENU)IDNO, hInstance, NULL);
        SendMessageW(hYes, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hNo, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    else {
        HWND hOk = CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 160, 86, 90, 28, hwnd, (HMENU)IDOK, hInstance, NULL);
        SendMessageW(hOk, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    MessageBeep(iconId == IDI_ERROR ? MB_ICONHAND : MB_ICONQUESTION);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_RETURN) {
                state.result = yesNo ? IDYES : IDOK;
                DestroyWindow(hwnd);
            }
            else if (msg.wParam == VK_ESCAPE) {
                state.result = yesNo ? IDNO : IDOK;
                DestroyWindow(hwnd);
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    DeleteObject(hFont);
    return state.result;
}
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    // Определяем текущий язык интерфейса пользователя Windows
    LANGID langId = GetUserDefaultUILanguage();
    bool isRu = (PRIMARYLANGID(langId) == LANG_RUSSIAN);

    std::wstring exePath, iniPath;

    // 1. Поиск otvdm.exe и otvdm.ini во всех дисках и папках
    FindFilesAcrossAllDrives(exePath, iniPath);

    // 2. Если otvdm не найден — предлагаем установить его из ресурса 103 RCDATA
    if (exePath.empty() || iniPath.empty()) {
        int ans = ShowSegoeDialog(
            hInstance,
            L"MS-DOS 7.1",
            isRu ? L"Установить программу для запуска 16-битных приложений?"
            : L"Install the program to run 16-bit applications?",
            true, isRu, IDI_QUESTION
        );

        if (ans != IDYES) {
            return 0; // Пользователь нажал «Нет» — закрываем программу
        }

        // Создаём папку %LOCALAPPDATA%\otvdm для установки без прав администратора
        wchar_t appDataBuf[MAX_PATH] = {};
        GetEnvironmentVariableW(L"LOCALAPPDATA", appDataBuf, MAX_PATH);
        std::wstring installDir = std::wstring(appDataBuf) + LR"(\otvdm)";
        fs::create_directories(installDir);

        std::wstring otvdmZip = installDir + LR"(\otvdm_pkg.zip)";
        if (!ExtractResourceToFile(103, otvdmZip)) {
            ShowSegoeDialog(hInstance, L"Unknown error", L"0xC000000", false, isRu, IDI_ERROR);
            return 1;
        }

        // Распаковываем otvdm из 103 RCDATA и удаляем временный архив
        RunHiddenAndWait(LR"(tar.exe -xf ")" + otvdmZip + LR"(" -C ")" + installDir + LR"(")");
        std::error_code ecDel;
        fs::remove(otvdmZip, ecDel);

        // Регистрируем консольный .inf файл в системе, если он есть внутри архива
        std::wstring infFile = installDir + LR"(\install (console).inf)";
        if (!fs::exists(infFile)) infFile = installDir + LR"(\install.inf)";
        if (fs::exists(infFile)) {
            RunHiddenAndWait(LR"(rundll32.exe setupapi.dll,InstallHinfSection DefaultInstall 132 )" + infFile);
        }

        // Сообщаем об успешной установке и спрашиваем о перезапуске MS-DOS
        int restartAns = ShowSegoeDialog(
            hInstance,
            L"MS-DOS 7.1",
            isRu ? L"Готово! Запустить повторно MS-DOS?"
            : L"Done! Restart MS-DOS?",
            true, isRu, IDI_INFORMATION
        );

        if (restartAns == IDYES) {
            wchar_t selfPath[MAX_PATH] = {};
            GetModuleFileNameW(NULL, selfPath, MAX_PATH);
            STARTUPINFOW siRestart = { sizeof(siRestart) };
            PROCESS_INFORMATION piRestart = {};
            if (CreateProcessW(selfPath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &siRestart, &piRestart)) {
                CloseHandle(piRestart.hProcess);
                CloseHandle(piRestart.hThread);
            }
        }
        return 0;
    }

    g_otvdmExe = exePath;

    // 3. Создаём скрытую системную папку в %TEMP%
    wchar_t tempBuf[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempBuf);
    std::wstring secretDir = std::wstring(tempBuf) + L".msdos_sys_" + std::to_wstring(GetCurrentProcessId());
    fs::create_directories(secretDir);
    SetFileAttributesW(secretDir.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);

    // 4. Извлекаем чёрный фон (101) и архив msd.zip (102)
    std::wstring blackBgPath = secretDir + LR"(\bg.bmp)";
    std::wstring zipPath = secretDir + LR"(\msd.zip)";
    ExtractResourceToFile(101, blackBgPath);
    ExtractResourceToFile(102, zipPath);

    // 5. Распаковываем msd.zip в подпапку ROOT через tar.exe
    std::wstring dosRoot = secretDir + LR"(\ROOT)";
    fs::create_directories(dosRoot);
    RunHiddenAndWait(LR"(tar.exe -xf ")" + zipPath + LR"(" -C ")" + dosRoot + LR"(")");

    // Настраиваем otvdm.ini для поддержки MS-DOS 7.10
    WritePrivateProfileStringW(L"otvdm", L"VDMDOSVER", L"7.10", iniPath.c_str());
    WritePrivateProfileStringW(L"otvdm", L"WINVER", L"4.10", iniPath.c_str());
    WritePrivateProfileStringW(L"otvdm", L"IAmNotNT", L"1", iniPath.c_str());

    // 6. Проверяем наличие LOGO.SYS и копируем его как logo.bmp для надёжной загрузки
    std::wstring logoSysPath = dosRoot + LR"(\LOGO.SYS)";
    std::wstring logoBmpPath = secretDir + LR"(\logo.bmp)";
    bool hasLogo = fs::exists(logoSysPath);
    if (hasLogo) {
        std::error_code ecCopy;
        fs::copy_file(logoSysPath, logoBmpPath, fs::copy_options::overwrite_existing, ecCopy);
    }
    g_bgPath = hasLogo ? logoBmpPath : blackBgPath;

    // 7. Создаём полноэкранное окно MS-DOS поверх рабочего стола
    WNDCLASSW wc = {};
    wc.lpfnWndProc = FullscreenBgProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    wc.lpszClassName = L"MsDosBackdropClass";
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwndBg = CreateWindowExW(
        0, L"MsDosBackdropClass", L"MS-DOS 7.1",
        WS_POPUP | WS_VISIBLE,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        NULL, NULL, hInstance, NULL
    );

    // Если есть LOGO.SYS — показываем заставку 2.5 секунды с активной отрисовкой окна
    if (hasLogo) {
        InvalidateRect(hwndBg, NULL, TRUE);
        UpdateWindow(hwndBg);
        ULONGLONG startTick = GetTickCount64();
        MSG msgLogo;
        while (GetTickCount64() - startTick < 2500) {
            while (PeekMessageW(&msgLogo, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msgLogo);
                DispatchMessageW(&msgLogo);
            }
            Sleep(10);
        }
        g_bgPath = blackBgPath;
        InvalidateRect(hwndBg, NULL, TRUE);
        UpdateWindow(hwndBg);
    }

    // 8. Монтируем скрытую папку ROOT как невидимый виртуальный диск A:
    bool mountedA = DefineDosDeviceW(0, L"A:", dosRoot.c_str());
    g_currentDir = mountedA ? LR"(A:\)" : dosRoot;

    // 9. Воспроизводим вывод AUTOEXEC.BAT прямо на полноэкранном чёрном фоне
    DosPrint(L"Starting MS-DOS 7.10...");
    DosPrint(L"");
    DosPrint(L"DOSKEY installed.");
    DosPrint(L"DOSLFN 0.41c enabled (CP437UNI.TBL).");
    DosPrint(L"CuteMouse v2.1 beta4 installed.");
    DosPrint(L"");
    DosPrint(L"Now you are in MS-DOS 7.10 prompt. Type 'HELP' for help.");
    DosPrint(L"");
    g_dosReady = true;
    InvalidateRect(hwndBg, NULL, TRUE);

    // 10. Основной цикл обработки клавиатуры и 16-битных программ
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 11. Отключаем диск A: и удаляем скрытую папку в %TEMP%
    if (mountedA) {
        DefineDosDeviceW(DDD_REMOVE_DEFINITION, L"A:", NULL);
    }
    std::error_code ec;
    fs::remove_all(secretDir, ec);

    return 0;
}