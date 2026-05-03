#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define EXHEADER_SIZE    0x800
#define TITLEID_OFFSET   0x100
#define TITLEID_LEN      8

int is_hex_char(char c) { return isxdigit(c); }

int validate_titleid(const char* tid) {
    if (strlen(tid) != 16) return 0;
    for (int i = 0; i < 16; i++)
        if (!is_hex_char(tid[i])) return 0;
    return 1;
}

void hex_to_le_bytes(const char* hex, unsigned char* out) {
    for (int i = 0; i < 8; i++)
        sscanf(&hex[i*2], "%2hhx", &out[7-i]);
}

void le_bytes_to_hex(const unsigned char* bytes, char* out) {
    for (int i = 0; i < 8; i++)
        sprintf(&out[i*2], "%02X", bytes[7-i]);
    out[16] = 0;
}

void backup_file(const char* path) {
    char bak[512];
    snprintf(bak, sizeof(bak), "%s.bak", path);
    FILE* src = fopen(path, "rb");
    FILE* dst = fopen(bak, "wb");
    if (!src || !dst) {
        printf("[!] 备份失败\n");
        if (src) fclose(src); if (dst) fclose(dst);
        return;
    }
    unsigned char buf[EXHEADER_SIZE];
    fread(buf,1,EXHEADER_SIZE,src);
    fwrite(buf,1,EXHEADER_SIZE,dst);
    fclose(src); fclose(dst);
    printf("[+] 已备份: %s\n", bak);
}

int read_titleid(const char* path, char* out_tid) {
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f,0,SEEK_END);
    if (ftell(f) != EXHEADER_SIZE) { fclose(f); return -2; }
    unsigned char b[8];
    fseek(f,TITLEID_OFFSET,SEEK_SET);
    fread(b,1,8,f);
    le_bytes_to_hex(b, out_tid);
    fclose(f);
    return 0;
}

int write_titleid(const char* path, const char* new_tid) {
    FILE* f = fopen(path, "r+b");
    if (!f) return -1;
    unsigned char b[8];
    hex_to_le_bytes(new_tid, b);
    fseek(f,TITLEID_OFFSET,SEEK_SET);
    fwrite(b,1,8,f);
    fclose(f);
    return 0;
}

int main() {
    printf("===== 3DS ExHeader TitleID 修改工具 =====\n");
    char path[512];
    printf("> 拖拽 ExHeader 文件到窗口并回车: ");
    fgets(path, sizeof(path), stdin);
    size_t l = strlen(path);
    if (l>0 && path[l-1]=='\n') path[l-1]=0;
    if (path[0]=='"') path[strlen(path)-1]=0;

    char orig[17];
    int r = read_titleid(path, orig);
    if (r==-1) { printf("[!] 打不开文件\n"); system("pause"); return 1; }
    if (r==-2) { printf("[!] 不是2048字节 ExHeader\n"); system("pause"); return 1; }
    printf("[*] 原始 TitleID: %s\n", orig);

    char newtid[17];
    printf("> 输入新16位十六进制 TitleID: ");
    scanf("%16s", newtid);
    if (!validate_titleid(newtid)) {
        printf("[!] 必须是16位十六进制\n");
        system("pause");
        return 1;
    }

    backup_file(path);
    if (write_titleid(path, newtid)) {
        printf("[!] 写入失败\n");
        system("pause");
        return 1;
    }

    char finaltid[17];
    read_titleid(path, finaltid);
    printf("[*] 修改成功！新 TitleID: %s\n", finaltid);
    system("pause");
    return 0;
}
