#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define IDC_BTN_BROWSE     101
#define IDC_EDIT_PATH      102
#define IDC_EDIT_TID       103
#define IDC_EDIT_GAMENAME  107
#define IDC_BTN_MODIFY     104
#define IDC_BTN_COPY_ORIG  105
#define IDC_BTN_CLEAR      106

char g_filePath[512] = {0};
char g_origTid[17] = {0};
char g_origGameName[64] = {0};
FILE* g_logConsole = NULL;

typedef enum {
    FTYPE_UNKNOWN = 0,
    FTYPE_3DS_800,   // 3DS (NCSD at 0x800)
    FTYPE_3DS_000,   // 3DS (NCSD at 0x000)
    FTYPE_NCCH       // NCCH
} FileType;

void Log(const char* msg) {
    if (g_logConsole) { fprintf(g_logConsole, "%s\n", msg); fflush(g_logConsole); }
}

int ValidateTitleID(const char* tid) {
    if (strlen(tid) != 16) return 0;
    for (int i=0;i<16;i++) if (!isxdigit(tid[i])) return 0;
    return 1;
}

void HexToLE(const char* hexStr, unsigned char* out) {
    memset(out, 0, 8);
    for (int i=0;i<8;i++) {
        unsigned int b;
        sscanf(hexStr + i*2, "%02x", &b);
        out[7-i] = b;
    }
}

void LEToHex(const unsigned char* buf, char* out) {
    memset(out, 0, 17);
    for (int i=0;i<8;i++)
        sprintf(out + i*2, "%02X", buf[7-i]);
}

// 万能识别所有 3DS/NCCH 格式
FileType DetectFileType(FILE* fp) {
    char magic[4];

    // 1. 检查 NCCH (0x0)
    fseek(fp, 0, SEEK_SET);
    if (fread(magic, 1, 4, fp) == 4 && memcmp(magic, "NCCH", 4) == 0)
        return FTYPE_NCCH;

    // 2. 检查 3DS (NCSD at 0x0)
    fseek(fp, 0, SEEK_SET);
    if (fread(magic, 1, 4, fp) == 4 && memcmp(magic, "NCSD", 4) == 0)
        return FTYPE_3DS_000;

    // 3. 检查 3DS (NCSD at 0x800)
    fseek(fp, 0x800, SEEK_SET);
    if (fread(magic, 1, 4, fp) == 4 && memcmp(magic, "NCSD", 4) == 0)
        return FTYPE_3DS_800;

    return FTYPE_UNKNOWN;
}

// 获取正确偏移
long GetTidOffset(FileType type) {
    if (type == FTYPE_3DS_800) return 0x800 + 0x118;
    if (type == FTYPE_3DS_000) return 0x118;
    if (type == FTYPE_NCCH)    return 0x118;
    return 0;
}

long GetNameOffset(FileType type) {
    if (type == FTYPE_3DS_800) return 0x800 + 0x200;
    if (type == FTYPE_3DS_000) return 0x200;
    if (type == FTYPE_NCCH)    return 0x200;
    return 0;
}

int ReadTitleID(const char* path, char* outTID) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return -1;

    FileType type = DetectFileType(fp);
    if (type == FTYPE_UNKNOWN) {
        Log("Error: Unknown file type");
        fclose(fp);
        return -2;
    }

    Log("File detected successfully");
    long offset = GetTidOffset(type);
    unsigned char buf[8];
    fseek(fp, offset, SEEK_SET);
    fread(buf, 1, 8, fp);
    LEToHex(buf, outTID);
    fclose(fp);
    return 0;
}

int ReadGameName(const char* path, char* outName, int maxLen) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return -1;

    FileType type = DetectFileType(fp);
    if (type == FTYPE_UNKNOWN) { fclose(fp); return -2; }

    memset(outName, 0, maxLen);
    long offset = GetNameOffset(type);
    fseek(fp, offset, SEEK_SET);
    fread(outName, 1, maxLen - 1, fp);
    fclose(fp);
    return 0;
}

int CreateNewFileWithMods(const char* srcPath, const char* newTID, const char* newName) {
    char newPath[512];
    snprintf(newPath, sizeof(newPath), "%s_modified", srcPath);

    FILE* in = fopen(srcPath, "rb");
    FILE* out = fopen(newPath, "wb");
    if (!in || !out) {
        if (in) fclose(in);
        if (out) fclose(out);
        return 0;
    }

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, 4096, in)) > 0)
        fwrite(buf, 1, n, out);

    fclose(in);
    fclose(out);

    FILE* fp = fopen(newPath, "r+b");
    if (!fp) return 0;

    FileType type = DetectFileType(fp);
    long tidOff = GetTidOffset(type);
    long nameOff = GetNameOffset(type);

    unsigned char tidBytes[8];
    HexToLE(newTID, tidBytes);
    fseek(fp, tidOff, SEEK_SET);
    fwrite(tidBytes, 1, 8, fp);

    char nameBuf[0x40] = {0};
    strncpy(nameBuf, newName, sizeof(nameBuf)-1);
    fseek(fp, nameOff, SEEK_SET);
    fwrite(nameBuf, 1, sizeof(nameBuf), fp);

    fclose(fp);
    Log("New file created:");
    Log(newPath);
    return 1;
}

void BrowseFile(HWND hEditPath, HWND hEditTid, HWND hEditName) {
    char szFile[512] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = 512;
    ofn.lpstrFilter = "3DS/NCCH Files\0*.3ds;*.ncch\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        strcpy(g_filePath, szFile);
        SetWindowTextA(hEditPath, szFile);

        ReadTitleID(g_filePath, g_origTid);
        ReadGameName(g_filePath, g_origGameName, sizeof(g_origGameName));

        SetWindowTextA(hEditTid, g_origTid);
        SetWindowTextA(hEditName, g_origGameName);

        char tmp[128];
        snprintf(tmp, sizeof(tmp), "TitleID: %s", g_origTid);
        Log(tmp);
        snprintf(tmp, sizeof(tmp), "GameName: %s", g_origGameName);
        Log(tmp);
    }
}

void DoModify(HWND hEditTid, HWND hEditName) {
    if (!*g_filePath) {
        MessageBoxA(NULL, "Select file first", "Warning", MB_ICONWARNING);
        return;
    }
    char newID[32], newName[64];
    GetWindowTextA(hEditTid, newID, 32);
    GetWindowTextA(hEditName, newName, 64);

    if (!ValidateTitleID(newID)) {
        MessageBoxA(NULL, "TitleID must be 16 hex chars", "Error", MB_ICONERROR);
        return;
    }

    if (CreateNewFileWithMods(g_filePath, newID, newName)) {
        MessageBoxA(NULL, "Success!", "OK", MB_OK);
    } else {
        MessageBoxA(NULL, "Failed", "Error", MB_ICONERROR);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE:
            CreateWindowA("STATIC", "File:", WS_CHILD|WS_VISIBLE,20,20,50,20,hWnd,NULL,NULL,NULL);
            CreateWindowA("EDIT", "", WS_CHILD|WS_VISIBLE|WS_BORDER,80,18,330,24,hWnd,(HMENU)IDC_EDIT_PATH,NULL,NULL);
            CreateWindowA("BUTTON", "Browse", WS_CHILD|WS_VISIBLE,425,18,80,26,hWnd,(HMENU)IDC_BTN_BROWSE,NULL,NULL);

            CreateWindowA("STATIC", "TitleID:", WS_CHILD|WS_VISIBLE,20,70,60,20,hWnd,NULL,NULL,NULL);
            CreateWindowA("EDIT", "", WS_CHILD|WS_VISIBLE|WS_BORDER,80,68,200,24,hWnd,(HMENU)IDC_EDIT_TID,NULL,NULL);

            CreateWindowA("STATIC", "Game Name:", WS_CHILD|WS_VISIBLE,20,120,60,20,hWnd,NULL,NULL,NULL);
            CreateWindowA("EDIT", "", WS_CHILD|WS_VISIBLE|WS_BORDER,80,118,320,24,hWnd,(HMENU)IDC_EDIT_GAMENAME,NULL,NULL);

            CreateWindowA("BUTTON", "Copy Orig", WS_CHILD|WS_VISIBLE,290,65,90,28,hWnd,(HMENU)IDC_BTN_COPY_ORIG,NULL,NULL);
            CreateWindowA("BUTTON", "Clear", WS_CHILD|WS_VISIBLE,390,65,70,28,hWnd,(HMENU)IDC_BTN_CLEAR,NULL,NULL);

            CreateWindowA("BUTTON", "CREATE NEW FILE", WS_CHILD|WS_VISIBLE,130,160,300,40,hWnd,(HMENU)IDC_BTN_MODIFY,NULL,NULL);
            return 0;

        case WM_COMMAND: {
            HWND ePath = GetDlgItem(hWnd, IDC_EDIT_PATH);
            HWND eTid  = GetDlgItem(hWnd, IDC_EDIT_TID);
            HWND eName = GetDlgItem(hWnd, IDC_EDIT_GAMENAME);

            switch(LOWORD(wParam)) {
                case IDC_BTN_BROWSE: BrowseFile(ePath, eTid, eName); break;
                case IDC_BTN_MODIFY: DoModify(eTid, eName); break;
                case IDC_BTN_COPY_ORIG:
                    SetWindowTextA(eTid, g_origTid);
                    SetWindowTextA(eName, g_origGameName);
                    break;
                case IDC_BTN_CLEAR:
                    SetWindowTextA(eTid, "");
                    SetWindowTextA(eName, "");
                    break;
            }
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    AllocConsole();
    g_logConsole = freopen("CONOUT$", "w", stdout);
    Log("===== 3DS Tool - All Format Supported =====");

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "3DSTool";
    RegisterClassA(&wc);

    HWND hWnd = CreateWindowExA(0, wc.lpszClassName, "3DS Editor",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 540, 240,
        NULL, NULL, hInst, NULL);

    ShowWindow(hWnd, nShow);
    UpdateWindow(hWnd);

    MSG m;
    while(GetMessage(&m,0,0,0)) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    fclose(g_logConsole);
    FreeConsole();
    return 0;
}
