#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define EXHEADER_SIZE    0x800
#define TITLEID_OFFSET   0x100

#define IDC_BTN_BROWSE    101
#define IDC_EDIT_PATH     102
#define IDC_EDIT_TID      103
#define IDC_BTN_MODIFY    104
#define IDC_BTN_COPY_ORIG 105
#define IDC_BTN_CLEAR     106

char g_filePath[512] = {0};
char g_origTid[17]   = {0};
FILE* g_logConsole   = NULL;

void Log(const char* msg) {
    if (g_logConsole) {
        fprintf(g_logConsole, "%s\n", msg);
        fflush(g_logConsole);
    }
}

int ValidateTitleID(const char* tid) {
    if (strlen(tid) != 16) return 0;
    for (int i=0;i<16;i++) if (!isxdigit(tid[i])) return 0;
    return 1;
}

void HexToLE(const char* hexStr, unsigned char* out) {
    memset(out, 0, 8);
    for (int i=0;i<8;i++) {
        unsigned char b;
        sscanf(&hexStr[i*2], "%2hhx", &b);
        out[7-i] = b;
    }
}

void LEToHex(const unsigned char* buf, char* out) {
    memset(out, 0, 17);
    for (int i=0;i<8;i++)
        sprintf(&out[i*2], "%02X", buf[7-i]);
}

int BackupFile(const char* path) {
    char bakPath[512];
    snprintf(bakPath, sizeof(bakPath), "%s.bak", path);
    FILE* fpSrc = fopen(path, "rb");
    FILE* fpDst = fopen(bakPath, "wb");
    if (!fpSrc || !fpDst) {
        if (fpSrc) fclose(fpSrc);
        if (fpDst) fclose(fpDst);
        return 0;
    }
    unsigned char buf[EXHEADER_SIZE];
    fread(buf, 1, EXHEADER_SIZE, fpSrc);
    fwrite(buf, 1, EXHEADER_SIZE, fpDst);
    fclose(fpSrc);
    fclose(fpDst);
    Log("已备份原文件");
    return 1;
}

int ReadExHeaderTID(const char* path, char* outTID) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return -1;
    fseek(fp, 0, SEEK_END);
    if (ftell(fp) != EXHEADER_SIZE) { fclose(fp); return -2; }
    fseek(fp, TITLEID_OFFSET, SEEK_SET);
    unsigned char buf[8];
    fread(buf,1,8,fp);
    fclose(fp);
    LEToHex(buf, outTID);
    return 0;
}

int WriteExHeaderTID(const char* path, const char* newTID) {
    FILE* fp = fopen(path, "r+b");
    if (!fp) return 0;
    unsigned char buf[8];
    HexToLE(newTID, buf);
    fseek(fp, TITLEID_OFFSET, SEEK_SET);
    fwrite(buf,1,8,fp);
    fclose(fp);
    return 1;
}

void BrowseFile(HWND hEditPath) {
    char szFile[512] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = 512;
    ofn.lpstrFilter = "ExHeader 文件\0*.bin;*.header\0所有文件\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        strcpy(g_filePath, szFile);
        SetWindowTextA(hEditPath, szFile);
        Log("已选择文件");
        
        memset(g_origTid, 0, sizeof(g_origTid));
        int ret = ReadExHeaderTID(g_filePath, g_origTid);
        if (ret == -1) Log("错误：无法打开文件");
        else if (ret == -2) Log("错误：无效的ExHeader文件");
        else {
            char tmp[64];
            snprintf(tmp, sizeof(tmp), "原始TitleID: %s", g_origTid);
            Log(tmp);
        }
    }
}

void DoModify(HWND hEditTID) {
    if (!*g_filePath) {
        MessageBoxA(NULL, "请先选择ExHeader文件", "提示", MB_ICONWARNING);
        return;
    }
    char newTID[32];
    GetWindowTextA(hEditTID, newTID, 32);
    if (!ValidateTitleID(newTID)) {
        MessageBoxA(NULL, "必须是16位十六进制字符", "错误", MB_ICONERROR);
        Log("错误：ID格式不正确");
        return;
    }
    Log("开始修改...");
    BackupFile(g_filePath);
    
    if (!WriteExHeaderTID(g_filePath, newTID)) {
        Log("错误：写入失败");
        MessageBoxA(NULL, "写入失败", "错误", MB_ICONERROR);
        return;
    }
    char finalID[17];
    ReadExHeaderTID(g_filePath, finalID);
    char msg[128];
    snprintf(msg, sizeof(msg), "修改成功！新ID: %s", finalID);
    Log(msg);
    MessageBoxA(NULL, "修改成功！已自动备份", "完成", MB_OK);
}

void CopyOrigToEdit(HWND hEditTid) {
    if (strlen(g_origTid)!=16) {
        MessageBoxA(NULL, "请先加载有效文件", "提示", MB_ICONINFORMATION);
        return;
    }
    SetWindowTextA(hEditTid, g_origTid);
    Log("已复制原始ID到输入框");
}

void ClearEdit(HWND hEditTid) {
    SetWindowTextA(hEditTid, "");
    Log("已清空输入框");
}

// 【修复】这里是最后一个错误！！！LPARAM 不是 LPSTR！！！
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE:
            CreateWindowA("STATIC", "ExHeader路径:", WS_CHILD|WS_VISIBLE,20,20,100,20,hWnd,NULL,NULL,NULL);
            CreateWindowA("EDIT", "", WS_CHILD|WS_VISIBLE|WS_BORDER,120,18,350,24,hWnd,(HMENU)IDC_EDIT_PATH,NULL,NULL);
            CreateWindowA("BUTTON", "浏览...", WS_CHILD|WS_VISIBLE,480,18,80,26,hWnd,(HMENU)IDC_BTN_BROWSE,NULL,NULL);
            
            CreateWindowA("STATIC", "新16位TitleID:", WS_CHILD|WS_VISIBLE,20,70,100,20,hWnd,NULL,NULL,NULL);
            CreateWindowA("EDIT", "", WS_CHILD|WS_VISIBLE|WS_BORDER,120,68,200,24,hWnd,(HMENU)IDC_EDIT_TID,NULL,NULL);
            
            CreateWindowA("BUTTON", "复制原始ID", WS_CHILD|WS_VISIBLE,330,65,100,30,hWnd,(HMENU)IDC_BTN_COPY_ORIG,NULL,NULL);
            CreateWindowA("BUTTON", "清空", WS_CHILD|WS_VISIBLE,440,65,60,30,hWnd,(HMENU)IDC_BTN_CLEAR,NULL,NULL);
            
            CreateWindowA("BUTTON", "一键修改", WS_CHILD|WS_VISIBLE,180,120,200,40,hWnd,(HMENU)IDC_BTN_MODIFY,NULL,NULL);
            return 0;
        case WM_COMMAND: {
            HWND path = GetDlgItem(hWnd, IDC_EDIT_PATH);
            HWND tid = GetDlgItem(hWnd, IDC_EDIT_TID);
            switch(LOWORD(wParam)) {
                case IDC_BTN_BROWSE: BrowseFile(path); break;
                case IDC_BTN_MODIFY: DoModify(tid); break;
                case IDC_BTN_COPY_ORIG: CopyOrigToEdit(tid); break;
                case IDC_BTN_CLEAR: ClearEdit(tid); break;
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
    system("chcp 936");
    g_logConsole = freopen("CONOUT$", "w", stdout);

    Log("===== 3DS ExHeader TitleID 修改工具 =====");
    Log("中文GUI + 中文控制台 | 无乱码版");
    Log("自动备份 | 复制ID | 一键修改\n");

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "ExhTool";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    HWND hWnd = CreateWindowExA(0, wc.lpszClassName, "3DS ExHeader 修改工具",
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
