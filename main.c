#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define IDC_BTN_BROWSE     101
#define IDC_EDIT_PATH      102
#define IDC_EDIT_TID       103
#define IDC_EDIT_GAMENAME  104
#define IDC_BTN_MODIFY     105
#define IDC_BTN_COPY_ORIG  106
#define IDC_BTN_CLEAR      107

char g_filePath[512] = {0};
char g_origTid[17] = {0};
char g_origGameName[128] = {0};
FILE* g_logConsole = NULL;

typedef enum {
    FTYPE_UNKNOWN = 0,
    FTYPE_VALID
} FileType;

void Log(const char* msg) {
    if (g_logConsole) {
        fprintf(g_logConsole, "%s\n", msg);
        fflush(g_logConsole);
    }
}

int utf16le_to_utf8(const unsigned short* utf16, char* utf8, int max_out) {
    int i = 0, o = 0;
    while (utf16[i] && o < max_out - 1) {
        unsigned int c = utf16[i++];
        if (c < 0x80) {
            utf8[o++] = (char)c;
        } else if (c < 0x800) {
            utf8[o++] = 0xC0 | (c >> 6);
            utf8[o++] = 0x80 | (c & 0x3F);
        } else {
            utf8[o++] = 0xE0 | (c >> 12);
            utf8[o++] = 0x80 | ((c >> 6) & 0x3F);
            utf8[o++] = 0x80 | (c & 0x3F);
        }
    }
    utf8[o] = 0;
    return o;
}

long FindNCCH(FILE* fp) {
    unsigned char sig[4];
    for (long off = 0; off < 0x8000; off += 0x100) {
        fseek(fp, off, SEEK_SET);
        if (fread(sig, 1, 4, fp) == 4 && memcmp(sig, "NCCH", 4) == 0)
            return off;
    }
    return -1;
}

long FindSMDH(FILE* fp) {
    unsigned char sig[4];
    for (long off = 0; off < 0x40000; off += 0x200) {
        fseek(fp, off, SEEK_SET);
        if (fread(sig, 1, 4, fp) == 4 && memcmp(sig, "SMDH", 4) == 0)
            return off;
    }
    return -1;
}

int ReadRealTitleID(const char* path, char* out) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return -1;

    long ncch = FindNCCH(fp);
    if (ncch < 0) {
        fclose(fp);
        Log("NCCH not found");
        return -2;
    }
    Log("NCCH found");

    unsigned char tid[8];
    fseek(fp, ncch + 0x118, SEEK_SET);
    fread(tid, 1, 8, fp);
    fclose(fp);

    for (int i = 0; i < 8; i++)
        sprintf(out + i*2, "%02X", tid[7-i]);
    return 0;
}

int ReadRealGameName(const char* path, char* out, int max) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return -1;

    long smdh = FindSMDH(fp);
    if (smdh < 0) {
        fclose(fp);
        Log("SMDH not found");
        return -2;
    }
    Log("SMDH found");

    unsigned short name[128] = {0};
    fseek(fp, smdh + 0x2008, SEEK_SET);
    fread(name, 2, 127, fp);
    fclose(fp);

    utf16le_to_utf8(name, out, max);
    return 0;
}

int ValidateTID(const char* s) {
    if (strlen(s) != 16) return 0;
    for (int i=0;i<16;i++)
        if (!isxdigit((unsigned char)s[i])) return 0;
    return 1;
}

int CreateNewFile(const char* src, const char* newtid, const char* newname) {
    char dst[512];
    snprintf(dst, sizeof(dst), "%s_mod", src);

    FILE* in = fopen(src, "rb");
    FILE* out = fopen(dst, "wb");
    if (!in || !out) {
        if (in) fclose(in);
        if (out) fclose(out);
        return 0;
    }

    char buf[4096];
    size_t n;
    while ((n = fread(buf,1,4096,in))>0)
        fwrite(buf,1,n,out);
    fclose(in);
    fclose(out);

    FILE* fp = fopen(dst, "r+b");
    if (!fp) return 0;

    long ncch = FindNCCH(fp);
    if (ncch >= 0) {
        unsigned char tid[8];
        for (int i=0;i<8;i++) {
            char b[3];
            b[0] = newtid[14 - i*2];
            b[1] = newtid[15 - i*2];
            b[2] = 0;
            tid[i] = (unsigned char)strtoul(b, NULL, 16);
        }
        fseek(fp, ncch + 0x118, SEEK_SET);
        fwrite(tid,1,8,fp);
    }

    long smdh = FindSMDH(fp);
    if (smdh >=0) {
        unsigned short utf16[128] = {0};
        int len = strlen(newname);
        for (int i=0;i<len && i<127;i++)
            utf16[i] = (unsigned char)newname[i];
        fseek(fp, smdh + 0x2008, SEEK_SET);
        fwrite(utf16,2,128,fp);
    }

    fclose(fp);
    Log("New file:");
    Log(dst);
    return 1;
}

void Browse(HWND hPath, HWND hTID, HWND hName) {
    char fn[512] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = fn;
    ofn.nMaxFile = 512;
    ofn.lpstrFilter = "3DS/CXI/NCCH\0*.3ds;*.cxi;*.ncch\0All\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        strcpy(g_filePath, fn);
        SetWindowTextA(hPath, fn);

        ReadRealTitleID(fn, g_origTid);
        ReadRealGameName(fn, g_origGameName, sizeof(g_origGameName));

        SetWindowTextA(hTID, g_origTid);
        SetWindowTextA(hName, g_origGameName);

        Log("----------------");
        Log("TitleID: %s", g_origTid);
        Log("Name: %s", g_origGameName);
    }
}

void Modify(HWND hTID, HWND hName) {
    if (!*g_filePath) {
        MessageBoxA(NULL,"Select file first","Warning",MB_ICONWARNING);
        return;
    }
    char tid[32], name[128];
    GetWindowTextA(hTID, tid, 32);
    GetWindowTextA(hName, name, 128);

    if (!ValidateTID(tid)) {
        MessageBoxA(NULL,"TitleID must be 16 hex","Error",MB_ICONERROR);
        return;
    }

    if (CreateNewFile(g_filePath, tid, name))
        MessageBoxA(NULL,"Success! New file created.","OK",MB_OK);
    else
        MessageBoxA(NULL,"Failed","Error",MB_ICONERROR);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch(msg) {
        case WM_CREATE:
            CreateWindowA("STATIC","File:",WS_VISIBLE|WS_CHILD,20,20,40,20,hWnd,0,0,0);
            CreateWindowA("EDIT","",WS_VISIBLE|WS_CHILD|WS_BORDER,70,18,380,24,hWnd,(HMENU)IDC_EDIT_PATH,0,0);
            CreateWindowA("BUTTON","Browse",WS_VISIBLE|WS_CHILD,460,18,80,26,hWnd,(HMENU)IDC_BTN_BROWSE,0,0);

            CreateWindowA("STATIC","TitleID:",WS_VISIBLE|WS_CHILD,20,70,50,20,hWnd,0,0,0);
            CreateWindowA("EDIT","",WS_VISIBLE|WS_CHILD|WS_BORDER,70,68,240,24,hWnd,(HMENU)IDC_EDIT_TID,0,0);

            CreateWindowA("STATIC","Game Name:",WS_VISIBLE|WS_CHILD,20,120,60,20,hWnd,0,0,0);
            CreateWindowA("EDIT","",WS_VISIBLE|WS_CHILD|WS_BORDER,70,118,380,24,hWnd,(HMENU)IDC_EDIT_GAMENAME,0,0);

            CreateWindowA("BUTTON","Copy Orig",WS_VISIBLE|WS_CHILD,320,65,90,28,hWnd,(HMENU)IDC_BTN_COPY_ORIG,0,0);
            CreateWindowA("BUTTON","Clear",WS_VISIBLE|WS_CHILD,420,65,70,28,hWnd,(HMENU)IDC_BTN_CLEAR,0,0);
            CreateWindowA("BUTTON","CREATE NEW FILE",WS_VISIBLE|WS_CHILD,140,160,320,40,hWnd,(HMENU)IDC_BTN_MODIFY,0,0);
            return 0;

        case WM_COMMAND: {
            HWND t = GetDlgItem(hWnd,IDC_EDIT_TID);
            HWND n = GetDlgItem(hWnd,IDC_EDIT_GAMENAME);
            HWND p = GetDlgItem(hWnd,IDC_EDIT_PATH);

            switch(LOWORD(wp)) {
                case IDC_BTN_BROWSE: Browse(p,t,n); break;
                case IDC_BTN_MODIFY: Modify(t,n); break;
                case IDC_BTN_COPY_ORIG:
                    SetWindowTextA(t,g_origTid);
                    SetWindowTextA(n,g_origGameName);
                    break;
                case IDC_BTN_CLEAR:
                    SetWindowTextA(t,"");
                    SetWindowTextA(n,"");
                    break;
            }
            return 0;
        }
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hWnd,msg,wp,lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int nShow) {
    AllocConsole();
    g_logConsole = freopen("CONOUT$","w",stdout);
    Log("===== 3DS Tool (Fixed NCCH/SMDH/UTF16) =====");

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "3DSTOOL";
    wc.hCursor = LoadCursorA(NULL,IDC_ARROW);
    RegisterClassA(&wc);

    HWND hWnd = CreateWindowExA(0, wc.lpszClassName, "3DS Editor",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        200,200,570,250, NULL,NULL,hInst,NULL);

    ShowWindow(hWnd,nShow);
    UpdateWindow(hWnd);

    MSG m;
    while(GetMessageA(&m,NULL,0,0)) {
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }

    fclose(g_logConsole);
    FreeConsole();
    return 0;
}
