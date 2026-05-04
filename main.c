#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define IDC_BTN_BROWSE    101
#define IDC_EDIT_PATH     102
#define IDC_EDIT_TID      103
#define IDC_BTN_MODIFY    104
#define IDC_BTN_COPY_ORIG 105
#define IDC_BTN_CLEAR     106

char g_filePath[512] = {0};
char g_origTid[17] = {0};
FILE* g_logConsole = NULL;

typedef enum {
    FILE_TYPE_UNKNOWN = 0,
    FILE_TYPE_3DS,     // NCSD (0x800 offset)
    FILE_TYPE_NCCH
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

FileType DetectFileType(FILE* fp) {
    char magic[4];

    // Check for NCCH (at 0x0)
    fseek(fp, 0, SEEK_SET);
    if (fread(magic, 1, 4, fp) == 4 && memcmp(magic, "NCCH", 4) == 0) {
        return FILE_TYPE_NCCH;
    }

    // Check for 3DS NCSD (at 0x800)
    fseek(fp, 0x800, SEEK_SET);
    if (fread(magic, 1, 4, fp) == 4 && memcmp(magic, "NCSD", 4) == 0) {
        return FILE_TYPE_3DS;
    }

    return FILE_TYPE_UNKNOWN;
}

int ReadTitleID(const char* path, char* outTID) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return -1;

    FileType type = DetectFileType(fp);
    unsigned char buf[8];
    long offset;

    if (type == FILE_TYPE_3DS) {
        Log("Detected: 3DS ROM (NCSD)");
        offset = 0x800 + 0x118;
    } else if (type == FILE_TYPE_NCCH) {
        Log("Detected: NCCH");
        offset = 0x118;
    } else {
        Log("Error: Unknown file");
        fclose(fp);
        return -2;
    }

    fseek(fp, offset, SEEK_SET);
    if (fread(buf, 1, 8, fp) != 8) {
        fclose(fp);
        return -3;
    }
    LEToHex(buf, outTID);
    fclose(fp);
    return 0;
}

int CreateNewFileWithNewTID(const char* srcPath, const char* newTID) {
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
    while ((n = fread(buf, 1, 4096, in)) > 0) {
        fwrite(buf, 1, n, out);
    }
    fclose(in);
    fclose(out);

    FILE* fp = fopen(newPath, "r+b");
    if (!fp) return 0;

    FileType type = DetectFileType(fp);
    long offset;
    if (type == FILE_TYPE_3DS) offset = 0x800 + 0x118;
    else offset = 0x118;

    unsigned char tidBytes[8];
    HexToLE(newTID, tidBytes);
    fseek(fp, offset, SEEK_SET);
    fwrite(tidBytes, 1, 8, fp);
    fclose(fp);

    Log("Created new file:");
    Log(newPath);
    return 1;
}

void BrowseFile(HWND hEditPath) {
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
        memset(g_origTid, 0, sizeof(g_origTid));
        ReadTitleID(g_filePath, g_origTid);

        char tmp[64];
        snprintf(tmp, sizeof(tmp), "TitleID: %s", g_origTid);
        Log(tmp);
    }
}

void DoModify(HWND hEdit) {
    if (!*g_filePath) {
        MessageBoxA(NULL, "Select file first", "Warning", MB_ICONWARNING);
        return;
    }
    char newID[32];
    GetWindowTextA(hEdit, newID, 32);
    if (!ValidateTitleID(newID)) {
        MessageBoxA(NULL, "16 hex required", "Error", MB_ICONERROR);
        return;
    }
    if (CreateNewFileWithNewTID(g_filePath, newID)) {
        MessageBoxA(NULL, "Success!", "OK", MB_OK);
        Log("Done!");
    } else {
        MessageBoxA(NULL, "Fail", "Error", MB_ICONERROR);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE:
            CreateWindowA("STATIC", "File:", WS_CHILD|WS_VISIBLE,20,20,50,20,hWnd,NULL,NULL,NULL);
            CreateWindowA("EDIT", "", WS_CHILD|WS_VISIBLE|WS_BORDER,80,18,380,24,hWnd,(HMENU)IDC_EDIT_PATH,NULL,NULL);
            CreateWindowA("BUTTON", "Browse", WS_CHILD|WS_VISIBLE,470,18,90,26,hWnd,(HMENU)IDC_BTN_BROWSE,NULL,NULL);

            CreateWindowA("STATIC", "TitleID:", WS_CHILD|WS_VISIBLE,20,70,60,20,hWnd,NULL,NULL,NULL);
            CreateWindowA("EDIT", "", WS_CHILD|WS_VISIBLE|WS_BORDER,80,68,220,24,hWnd,(HMENU)IDC_EDIT_TID,NULL,NULL);
            CreateWindowA("BUTTON", "Copy Orig", WS_CHILD|WS_VISIBLE,310,65,90,30,hWnd,(HMENU)IDC_BTN_COPY_ORIG,NULL,NULL);
            CreateWindowA("BUTTON", "Clear", WS_CHILD|WS_VISIBLE,410,65,70,30,hWnd,(HMENU)IDC_BTN_CLEAR,NULL,NULL);

            CreateWindowA("BUTTON", "CREATE NEW FILE", WS_CHILD|WS_VISIBLE,160,120,240,40,hWnd,(HMENU)IDC_BTN_MODIFY,NULL,NULL);
            return 0;
        case WM_COMMAND:
            switch(LOWORD(wParam)) {
                case IDC_BTN_BROWSE: BrowseFile(GetDlgItem(hWnd, IDC_EDIT_PATH)); break;
                case IDC_BTN_MODIFY: DoModify(GetDlgItem(hWnd, IDC_EDIT_TID)); break;
                case IDC_BTN_COPY_ORIG: SetWindowTextA(GetDlgItem(hWnd, IDC_EDIT_TID), g_origTid); break;
                case IDC_BTN_CLEAR: SetWindowTextA(GetDlgItem(hWnd, IDC_EDIT_TID), ""); break;
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    AllocConsole();
    g_logConsole = freopen("CONOUT$", "w", stdout);
    Log("===== 3DS TitleID Editor (FINAL FIX) =====");

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "TIDTool";
    RegisterClassA(&wc);

    HWND hWnd = CreateWindowExA(0, wc.lpszClassName, "3DS TitleID Editor",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 220,
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
