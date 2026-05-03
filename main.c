#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define EXHEADER_SIZE    0x800
#define TITLEID_OFFSET   0x100
#define TITLEID_BYTES    8

#define IDC_BTN_BROWSE    101
#define IDC_EDIT_PATH     102
#define IDC_EDIT_TID      103
#define IDC_BTN_MODIFY    104
#define IDC_BTN_COPY_ORIG 105
#define IDC_BTN_CLEAR     106

char g_filePath[512] = {0};
char g_origTid[17]   = {0};
FILE* g_logConsole   = NULL;

void Log(const char* msg)
{
    if (g_logConsole)
    {
        fprintf(g_logConsole, "%s\n", msg);
        fflush(g_logConsole);
    }
}

int ValidateTitleID(const char* tid)
{
    if (strlen(tid) != 16) return 0;
    for (int i = 0; i < 16; i++)
        if (!isxdigit(tid[i])) return 0;
    return 1;
}

void HexToLE(const char* hexStr, unsigned char* out)
{
    memset(out, 0, 8);
    for (int i = 0; i < 8; i++)
    {
        unsigned char b;
        sscanf(&hexStr[i*2], "%2hhx", &b);
        out[7 - i] = b;
    }
}

void LEToHex(const unsigned char* buf, char* out)
{
    memset(out, 0, 17);
    for (int i = 0; i < 8; i++)
        sprintf(&out[i*2], "%02X", buf[7 - i]);
}

int BackupFile(const char* path)
{
    char bakPath[512];
    snprintf(bakPath, sizeof(bakPath), "%s.bak", path);

    FILE* fpSrc = fopen(path, "rb");
    FILE* fpDst = fopen(bakPath, "wb");
    if (!fpSrc || !fpDst)
    {
        if (fpSrc) fclose(fpSrc);
        if (fpDst) fclose(fpDst);
        return 0;
    }

    unsigned char buf[EXHEADER_SIZE];
    fread(buf, 1, EXHEADER_SIZE, fpSrc);
    fwrite(buf, 1, EXHEADER_SIZE, fpDst);

    fclose(fpSrc);
    fclose(fpDst);

    char log[512];
    snprintf(log, sizeof(log), "已备份原文件到: %s", bakPath);
    Log(log);
    return 1;
}

int ReadExHeaderTID(const char* path, char* outTID)
{
    FILE* fp = fopen(path, "rb");
    if (!fp) return -1;

    fseek(fp, 0, SEEK_END);
    if (ftell(fp) != EXHEADER_SIZE)
    {
        fclose(fp);
        return -2;
    }

    fseek(fp, TITLEID_OFFSET, SEEK_SET);
    unsigned char buf[8];
    fread(buf, 1, 8, fp);
    fclose(fp);

    LEToHex(buf, outTID);
    return 0;
}

int WriteExHeaderTID(const char* path, const char* newTID)
{
    FILE* fp = fopen(path, "r+b");
    if (!fp) return 0;

    unsigned char buf[8];
    HexToLE(newTID, buf);

    fseek(fp, TITLEID_OFFSET, SEEK_SET);
    fwrite(buf, 1, 8, fp);
    fclose(fp);
    return 1;
}

void BrowseFile(HWND hEditPath, HWND hEditTid)
{
    OPENFILENAME ofn = {0};
    char szFile[512] = {0};

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hEditPath;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "ExHeader 文件\0*.bin;*.header\0所有文件\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn))
    {
        strcpy(g_filePath, szFile);
        SetWindowText(hEditPath, szFile);

        char log[512];
        snprintf(log, sizeof(log), "已选择文件: %s", szFile);
        Log(log);

        memset(g_origTid, 0, sizeof(g_origTid));
        int ret = ReadExHeaderTID(szFile, g_origTid);
        if (ret == -1)
            Log("错误：无法打开文件");
        else if (ret == -2)
            Log("错误：不是标准2048字节 ExHeader");
        else
        {
            char tidLog[64];
            snprintf(tidLog, sizeof(tidLog), "原始 TitleID: %s", g_origTid);
            Log(tidLog);
        }
    }
}

void DoModify(HWND hEditTID)
{
    if (strlen(g_filePath) == 0)
    {
        MessageBox(NULL, "请先选择 ExHeader 文件", "提示", MB_ICONWARNING);
        return;
    }

    char newTID[32];
    GetWindowText(hEditTID, newTID, sizeof(newTID));

    if (!ValidateTitleID(newTID))
    {
        MessageBox(NULL, "TitleID 必须是16位十六进制字符", "格式错误", MB_ICONERROR);
        Log("错误：TitleID 格式非法，需16位0-9 A-F");
        return;
    }

    Log("开始备份并修改 TitleID...");
    if (!BackupFile(g_filePath))
        Log("警告：备份失败");

    if (!WriteExHeaderTID(g_filePath, newTID))
    {
        Log("错误：写入失败");
        MessageBox(NULL, "写入失败", "错误", MB_ICONERROR);
        return;
    }

    char finalTID[17];
    ReadExHeaderTID(g_filePath, finalTID);

    char log[128];
    snprintf(log, sizeof(log), "修改成功！新 TitleID: %s", finalTID);
    Log(log);
    MessageBox(NULL, "修改成功！已自动备份原文件", "完成", MB_OK);
}

void CopyOrigToEdit(HWND hEditTid)
{
    if (strlen(g_origTid) != 16)
    {
        MessageBox(NULL, "请先选择并读取ExHeader文件", "提示", MB_ICONINFORMATION);
        return;
    }
    SetWindowText(hEditTid, g_origTid);
    Log("已将原始TitleID复制到输入框，可直接修改后几位");
}

void ClearEdit(HWND hEditTid)
{
    SetWindowText(hEditTid, "");
    Log("已清空TitleID输入框");
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        CreateWindowA("STATIC", "ExHeader 文件路径：", WS_CHILD | WS_VISIBLE,
            20, 20, 120, 20, hWnd, NULL, NULL, NULL);

        CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
            150, 18, 320, 24, hWnd, (HMENU)IDC_EDIT_PATH, NULL, NULL);

        CreateWindowA("BUTTON", "浏览...", WS_CHILD | WS_VISIBLE,
            480, 18, 80, 26, hWnd, (HMENU)IDC_BTN_BROWSE, NULL, NULL);

        CreateWindowA("STATIC", "输入新16位TitleID：", WS_CHILD | WS_VISIBLE,
            20, 70, 140, 20, hWnd, NULL, NULL, NULL);

        CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
            170, 68, 200, 24, hWnd, (HMENU)IDC_EDIT_TID, NULL, NULL);

        CreateWindowA("BUTTON", "复制原始ID到输入框", WS_CHILD | WS_VISIBLE,
            380, 65, 120, 30, hWnd, (HMENU)IDC_BTN_COPY_ORIG, NULL, NULL);

        CreateWindowA("BUTTON", "清空输入", WS_CHILD | WS_VISIBLE,
            510, 65, 80, 30, hWnd, (HMENU)IDC_BTN_CLEAR, NULL, NULL);

        CreateWindowA("BUTTON", "一键修改 TitleID", WS_CHILD | WS_VISIBLE,
            200, 120, 180, 35, hWnd, (HMENU)IDC_BTN_MODIFY, NULL, NULL);
        return 0;
    }
    case WM_COMMAND:
    {
        HWND hEditPath = GetDlgItem(hWnd, IDC_EDIT_PATH);
        HWND hEditTid   = GetDlgItem(hWnd, IDC_EDIT_TID);

        switch (LOWORD(wParam))
        {
        case IDC_BTN_BROWSE:
            BrowseFile(hEditPath, hEditTid);
            break;
        case IDC_BTN_MODIFY:
            DoModify(hEditTid);
            break;
        case IDC_BTN_COPY_ORIG:
            CopyOrigToEdit(hEditTid);
            break;
        case IDC_BTN_CLEAR:
            ClearEdit(hEditTid);
            break;
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    AllocConsole();
    g_logConsole = freopen("CONOUT$", "w", stdout);
    Log("===== 3DS ExHeader TitleID 修改工具 GUI v1.2 =====");
    Log("日志窗口：所有操作、原始ID、备份、错误都会在此显示");
    Log("功能：浏览文件、复制原ID、清空、一键修改\n");

    const char CLASS_NAME[] = "ExhTIDToolGUI";

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassA(&wc);

    HWND hWnd = CreateWindowExA(
        0, CLASS_NAME, "3DS ExHeader TitleID 修改工具 v1.2",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 620, 220,
        NULL, NULL, hInst, NULL
    );

    ShowWindow(hWnd, nShow);
    UpdateWindow(hWnd);

    MSG m;
    while (GetMessage(&m, NULL, 0, 0))
    {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }

    if (g_logConsole) fclose(g_logConsole);
    FreeConsole();
    return 0;
}
