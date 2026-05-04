#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define IDC_BTN_BROWSE     101
#define IDC_EDIT_PATH      102
#define IDC_EDIT_TID       103
#define IDC_BTN_WRITE      104
#define IDC_BTN_COPY       105

char g_srcPath[512] = {0};
FILE* g_log = NULL;

void Log(const char* s) {
    if (g_log) { fprintf(g_log, "%s\n", s); fflush(g_log); }
}

int IsValidTID(const char* s) {
    if (strlen(s) != 16) return 0;
    for (int i = 0; i < 16; i++)
        if (!isxdigit((unsigned char)s[i])) return 0;
    return 1;
}

// 8字节小端 → 16进制字符串
void TIDBytesToHex(const unsigned char* bytes, char* out) {
    memset(out, 0, 17);
    for (int i = 0; i < 8; i++)
        sprintf(out + i*2, "%02X", bytes[7 - i]);
}

// 16进制字符串 → 8字节小端
void HexToTIDBytes(const char* hex, unsigned char* out) {
    memset(out, 0, 8);
    for (int i = 0; i < 8; i++) {
        char buf[3] = { hex[14-2*i], hex[15-2*i], 0 };
        out[i] = (unsigned char)strtoul(buf, NULL, 16);
    }
}

// 读取官方标准位置的 TitleID
int ReadRealTitleID(const char* path, char* out_tid) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return -1;

    unsigned char tid[8];
    // 读取 NCCH Header @ 0x118
    fseek(fp, 0x118, SEEK_SET);
    fread(tid, 1, 8, fp);
    TIDBytesToHex(tid, out_tid);

    Log("Read TitleID from NCCH Header @ 0x118");
    fclose(fp);
    return 0;
}

// 按官方文档写入 3 个位置
int WriteTitleIDToFile(const char* src, const char* newtid) {
    char dst[512];
    snprintf(dst, sizeof(dst), "%s_patched", src);

    FILE* in = fopen(src, "rb");
    FILE* out = fopen(dst, "wb");
    if (!in || !out) { Log("File error"); return 0; }

    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, 4096, in)) > 0)
        fwrite(buf, 1, n, out);
    fclose(in);

    out = freopen(dst, "r+b", out);
    unsigned char tid_bytes[8];
    HexToTIDBytes(newtid, tid_bytes);

    Log("Writing NCCH Header   @ 0x118    (Official)");
    fseek(out, 0x118, SEEK_SET);
    fwrite(tid_bytes, 1, 8, out);

    Log("Writing ExHeader      @ 0x200+0x000 (Official)");
    fseek(out, 0x200 + 0x00, SEEK_SET);
    fwrite(tid_bytes, 1, 8, out);

    Log("Writing ExHeader      @ 0x200+0x008 (Official)");
    fseek(out, 0x200 + 0x08, SEEK_SET);
    fwrite(tid_bytes, 1, 8, out);

    fclose(out);
    Log("Success!");
    Log(dst);
    return 1;
}

// GUI
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HWND hPath, hTID;

    switch (msg) {
        case WM_CREATE:
            CreateWindowA("STATIC", "File:", WS_VISIBLE|WS_CHILD, 20,20,50,22,hWnd,0,0,0);
            hPath = CreateWindowA("EDIT", "", WS_VISIBLE|WS_CHILD|WS_BORDER,70,18,380,24,hWnd,(HMENU)IDC_EDIT_PATH,0,0);
            CreateWindowA("BUTTON", "Browse", WS_VISIBLE|WS_CHILD,460,18,90,26,hWnd,(HMENU)IDC_BTN_BROWSE,0,0);

            CreateWindowA("STATIC", "Title ID:", WS_VISIBLE|WS_CHILD,20,70,70,22,hWnd,0,0,0);
            hTID = CreateWindowA("EDIT", "", WS_VISIBLE|WS_CHILD|WS_BORDER,90,68,280,24,hWnd,(HMENU)IDC_EDIT_TID,0,0);

            CreateWindowA("BUTTON", "PATCH & SAVE", WS_VISIBLE|WS_CHILD,100,120,350,40,hWnd,(HMENU)IDC_BTN_WRITE,0,0);
            return 0;

        case WM_COMMAND:
            if (LOWORD(wp) == IDC_BTN_BROWSE) {
                char fn[512] = {0};
                OPENFILENAMEA ofn = {sizeof(ofn)};
                ofn.lpstrFile = fn;
                ofn.nMaxFile = 512;
                ofn.lpstrFilter = "Decrypted 3DS/NCCH/CXI\0*.3ds;*.cxi;*.ncch\0All\0*.*\0";
                if (GetOpenFileNameA(&ofn)) {
                    strcpy(g_srcPath, fn);
                    SetWindowTextA(hPath, fn);
                    Log("File opened: %s", fn);

                    char tid[17];
                    ReadRealTitleID(fn, tid);
                    SetWindowTextA(hTID, tid);
                    Log("Current TitleID: %s", tid);
                }
            }

            if (LOWORD(wp) == IDC_BTN_WRITE) {
                if (!*g_srcPath) { MessageBoxA(hWnd,"Select file first","",0); return 0; }
                char tid[32];
                GetWindowTextA(hTID, tid, 32);
                if (!IsValidTID(tid)) { MessageBoxA(hWnd,"16 hex required","",0); return 0; }
                WriteTitleIDToFile(g_srcPath, tid);
                MessageBoxA(hWnd,"Patched successfully!","Done",0);
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hWnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int nShow) {
    AllocConsole();
    g_log = freopen("CONOUT$", "w", stdout);
    Log("===== 3DS TitleID Patcher (Official NCCH/ExHeader Spec) =====");

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "TIDTool";
    wc.hCursor = LoadCursorA(0, IDC_ARROW);
    RegisterClassA(&wc);

    HWND hWnd = CreateWindowExA(0, "TIDTool", "3DS TitleID Tool (NCCH + ExHeader)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        200,200,580,190, NULL,NULL,hInst,NULL);

    ShowWindow(hWnd, nShow);
    UpdateWindow(hWnd);

    MSG m;
    while (GetMessageA(&m,0,0,0)) { TranslateMessage(&m); DispatchMessageA(&m); }
    fclose(g_log);
    FreeConsole();
    return 0;
}
