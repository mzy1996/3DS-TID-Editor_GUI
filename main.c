#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TITLEID_BYTES 8
#define NCCH_TITLEID_OFFSET 0x118
#define NCSD_HEADER_SIZE 0x200
#define NCSD_TITLEID_OFFSET (NCSD_HEADER_SIZE + NCCH_TITLEID_OFFSET)

#define IDC_BTN_BROWSE    101
#define IDC_EDIT_PATH     102
#define IDC_EDIT_TID      103
#define IDC_BTN_MODIFY    104
#define IDC_BTN_COPY_ORIG 105
#define IDC_BTN_CLEAR     106

char g_filePath[512] = {0};
char g_origTid[17] = {0};
FILE* g_logConsole = NULL;

typedef struct {
    unsigned char titleID[8];
} TitleIDEntry;

void Log(const char* msg) {
    if (g_logConsole) {
        fprintf(g_logConsole, "%s\n", msg);
        fflush(g_logConsole);
    }
}

int ValidateTitleID(const char* tid) {
    if (strlen(tid) != 16) return 0;
    for (int i = 0; i < 16; i++) if (!isxdigit(tid[i])) return 0;
    return 1;
}

void HexToLE(const char* hexStr, unsigned char* out) {
    memset(out, 0, 8);
    for (int i = 0; i < 8; i++) {
        unsigned int byte;
        sscanf(hexStr + (i * 2), "%02x", &byte);
        out[7 - i] = (unsigned char)byte;
    }
}

void LEToHex(const unsigned char* buf, char* out) {
    memset(out, 0, 17);
    for (int i = 0; i < 8; i++)
        sprintf(out + (i * 2), "%02X", buf[7 - i]);
}

int IsNcsdFile(FILE* fp) {
    fseek(fp, 0, SEEK_SET);
    char magic[4];
    if (fread(magic, 1, 4, fp) != 4) return 0;
    return (memcmp(magic, "NCSD", 4) == 0);
}

int ReadTitleIDFrom3DS(FILE* fp, char* out) {
    unsigned char buf[8];
    fseek(fp, NCSD_TITLEID_OFFSET, SEEK_SET);
    if (fread(buf, 1, 8, fp) != 8) return -1;
    LEToHex(buf, out);
    return 0;
}

int ReadTitleIDFromNCCH(FILE* fp, char* out) {
    unsigned char buf[8];
    fseek(fp, NCCH_TITLEID_OFFSET, SEEK_SET);
    if (fread(buf, 1, 8, fp) != 8) return -1;
    LEToHex(buf, out);
    return 0;
}

int ReadTitleID(const char* path, char* out) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return -1;

    if (IsNcsdFile(fp)) {
        Log("Detected: 3DS (NCSD)");
        ReadTitleIDFrom3DS(fp, out);
    } else {
        Log("Detected: NCCH");
        ReadTitleIDFromNCCH(fp, out);
    }

    fclose(fp);
    return 0;
}

int CreateModifiedFile(const char* src, const char* newID) {
    char dstPath[512];
    snprintf(dstPath, sizeof(dstPath), "%s_modified.bin", src);

    FILE* in = fopen(src, "rb");
    FILE* out = fopen(dstPath, "wb");
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

    FILE* fp = fopen(dstPath, "r+b");
    if (!fp) return 0;

    unsigned char tidBytes[8];
    HexToLE(newID, tidBytes);

    if (IsNcsdFile(fp))
        fseek(fp, NCSD_TITLEID_OFFSET, SEEK_SET);
    else
        fseek(fp, NCCH_TITLEID_OFFSET, SEEK_SET);

    fwrite(tidBytes, 1, 8, fp);
    fclose(fp);

    Log("Created:");
    Log(dstPath);
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
        ReadTitleID(g_filePath, g_origTid);

        char tmp[64];
        snprintf(tmp, sizeof(tmp), "TitleID: %s", g_origTid);
        Log(tmp);
    }
}

void DoModify(HWND hEdit) {
    if (!*g_filePath) {
        MessageBoxA(NULL, "Select a file first", "Warning", MB_ICONWARNING);
        return;
    }
    char newID[32];
    GetWindowTextA(hEdit, newID, 32);
    if (!ValidateTitleID(newID)) {
        MessageBoxA(NULL, "Must be 16 hex chars", "Error", MB_ICONERROR);
        return;
    }
    if (CreateModifiedFile(g_filePath, newID)) {
        MessageBoxA(NULL, "Success! New file created", "OK", MB_OK);
        Log("Done!");
    } else {
        MessageBoxA(NULL, "Failed", "Error", MB_ICONERROR);
    }
}

void CopyOrig(HWND hEdit) {
    SetWindowTextA(hEdit, g_origTid);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            CreateWindowA("STATIC", "File:", WS_CHILD | WS_VISIBLE, 20, 20, 50, 20, hWnd, NULL, NULL, NULL);
            CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER, 80, 18, 380, 24, hWnd, (HMENU)IDC_EDIT_PATH, NULL, NULL);
            CreateWindowA("BUTTON", "Browse", WS_CHILD | WS_VISIBLE, 470, 18, 90, 26, hWnd, (HMENU)IDC_BTN_BROWSE, NULL, NULL);

            CreateWindowA("STATIC", "TitleID:", WS_CHILD | WS_VISIBLE, 20, 70, 60, 20, hWnd, NULL, NULL, NULL);
            CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER, 80, 68, 220, 24, hWnd, (HMENU)IDC_EDIT_TID, NULL, NULL);
            CreateWindowA("BUTTON", "Copy Orig", WS_CHILD | WS_VISIBLE, 310, 65, 90, 30, hWnd, (HMENU)IDC_BTN_COPY_ORIG, NULL, NULL);
            CreateWindowA("BUTTON", "Clear", WS_CHILD | WS_VISIBLE, 410, 65, 70, 30, hWnd, (HMENU)IDC_BTN_CLEAR, NULL, NULL);

            CreateWindowA("BUTTON", "CREATE NEW FILE", WS_CHILD | WS_VISIBLE, 160, 120, 240, 40, hWnd, (HMENU)IDC_BTN_MODIFY, NULL, NULL);
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_BTN_BROWSE: BrowseFile(GetDlgItem(hWnd, IDC_EDIT_PATH)); break;
                case IDC_BTN_MODIFY: DoModify(GetDlgItem(hWnd, IDC_EDIT_TID)); break;
                case IDC_BTN_COPY_ORIG: CopyOrig(GetDlgItem(hWnd, IDC_EDIT_TID)); break;
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
    Log("===== 3DS TitleID Editor (FIXED) =====");

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "TIDTool";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    HWND hWnd = CreateWindowExA(0, wc.lpszClassName, "3DS TitleID Editor",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 220,
        NULL, NULL, hInst, NULL);

    ShowWindow(hWnd, nShow);
    UpdateWindow(hWnd);

    MSG m;
    while (GetMessage(&m, 0, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    fclose(g_logConsole);
    FreeConsole();
    return 0;
}
