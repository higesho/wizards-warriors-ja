/* Wizards & Warriors 日本語化 dinput.dll
 *  - 本物の dinput.dll へ DirectInputCreate* を渡す
 *  - deep6.exe（GOG 版）の文字列を描く・幅を測る・入る字数を数える関数を EUC-JP 対応のものに置き換える
 *  - 文章のファイル 9 つを開くとき、原本と WWJP\wwjp.tsv から日本語版を組み立てて一時フォルダに書き、そちらを開かせる
 *  試験用の仕組み（記録、入力の差し込み、窓表示）は WWJP_TEST を付けた試験版（build.bat test）にだけ入る
 */
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "jfont.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef short i16;

/* ---- 本物の dinput.dll ---- */
static HMODULE real_dinput;
static FARPROC get_real(const char *name)
{
    if (!real_dinput) {
        char path[MAX_PATH];
        GetSystemDirectoryA(path, MAX_PATH);
        strcat(path, "\\dinput.dll");
        real_dinput = LoadLibraryA(path);
        if (!real_dinput) return NULL;
    }
    return GetProcAddress(real_dinput, name);
}
typedef HRESULT (WINAPI *DICreateA)(HINSTANCE, DWORD, void **, void *);
typedef HRESULT (WINAPI *DICreateEx)(HINSTANCE, DWORD, const void *, void **, void *);
typedef HRESULT (WINAPI *DIGetClass)(const void *, const void *, void **);
typedef HRESULT (WINAPI *DICanUnload)(void);
#ifdef WWJP_TEST
static void di_wrap(void *di);
#endif
HRESULT WINAPI DirectInputCreateA(HINSTANCE h, DWORD v, void **o, void *u)
{
    DICreateA f = (DICreateA)get_real("DirectInputCreateA");
    HRESULT r = f ? f(h, v, o, u) : E_FAIL;
#ifdef WWJP_TEST
    if (r == S_OK && o && *o) di_wrap(*o);
#endif
    return r;
}
HRESULT WINAPI DirectInputCreateW(HINSTANCE h, DWORD v, void **o, void *u)
{ DICreateA f = (DICreateA)get_real("DirectInputCreateW"); return f ? f(h, v, o, u) : E_FAIL; }
HRESULT WINAPI DirectInputCreateEx(HINSTANCE h, DWORD v, const void *r, void **o, void *u)
{ DICreateEx f = (DICreateEx)get_real("DirectInputCreateEx"); return f ? f(h, v, r, o, u) : E_FAIL; }
HRESULT WINAPI DllGetClassObject(const void *a, const void *b, void **c)
{ DIGetClass f = (DIGetClass)get_real("DllGetClassObject"); return f ? f(a, b, c) : E_FAIL; }
HRESULT WINAPI DllCanUnloadNow(void)
{ DICanUnload f = (DICanUnload)get_real("DllCanUnloadNow"); return f ? f() : S_FALSE; }

/* ---- 記録（試験版で WWJP_LOG のときだけ。配布版では何もしない） ---- */
static char gamedir[MAX_PATH], jpdir[MAX_PATH], tmpdir[MAX_PATH];
#ifdef WWJP_TEST
static FILE *logf;
static void logp(const char *fmt, ...)
{
    va_list ap;
    if (!logf) return;
    va_start(ap, fmt);
    vfprintf(logf, fmt, ap);
    va_end(ap);
    fflush(logf);
}
#else
#define logp(...) ((void)0)
#endif

/* ---- 訳の表（鍵 → EUC-JP の文） ---- */
#define HSIZE 65536
static char *hkey[HSIZE];
static char *hval[HSIZE];
static int nent;
static unsigned hstr(const char *s)
{
    unsigned h = 2166136261u;
    while (*s) { h ^= (u8)*s++; h *= 16777619u; }
    return h;
}
static void hput(char *k, char *v)
{
    unsigned i = hstr(k) & (HSIZE - 1);
    while (hkey[i]) {
        if (!strcmp(hkey[i], k)) { hval[i] = v; return; }
        i = (i + 1) & (HSIZE - 1);
    }
    hkey[i] = k; hval[i] = v; nent++;
}
static const char *hget(const char *k)
{
    unsigned i = hstr(k) & (HSIZE - 1);
    while (hkey[i]) {
        if (!strcmp(hkey[i], k)) return hval[i];
        i = (i + 1) & (HSIZE - 1);
    }
    return NULL;
}
static u8 *read_all(const char *path, int *len);
static void load_table(void)
{
    char path[MAX_PATH];
    int len, i;
    char *buf, *p, *e;
    sprintf(path, "%s\\wwjp.tsv", jpdir);
    buf = (char *)read_all(path, &len);
    if (!buf) { logp("wwjp.tsv が無い\n"); return; }
    p = buf; e = buf + len;
    while (p < e) {
        char *line = p, *tab, *nl;
        nl = memchr(p, '\n', e - p);
        if (!nl) nl = e;
        *nl = 0;
        if (nl > line && nl[-1] == '\r') nl[-1] = 0;
        p = nl + 1;
        if (line[0] == '#' || !line[0]) continue;
        tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = 0;
        hput(line, tab + 1);
    }
    (void)i;
    logp("訳 %d 件\n", nent);
}

/* ---- ファイル ---- */
typedef HANDLE (WINAPI *CreateFileA_t)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static CreateFileA_t real_CreateFileA;
static u8 *read_all(const char *path, int *len)
{
    HANDLE h = real_CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD sz, got;
    u8 *buf;
    if (h == INVALID_HANDLE_VALUE) return NULL;
    sz = GetFileSize(h, NULL);
    buf = (u8 *)malloc(sz + 1);
    if (!buf || !ReadFile(h, buf, sz, &got, NULL) || got != sz) { CloseHandle(h); free(buf); return NULL; }
    CloseHandle(h);
    buf[sz] = 0;
    *len = (int)sz;
    return buf;
}
static int write_all(const char *path, const u8 *d, int len)
{
    HANDLE h = real_CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD put;
    int ok;
    if (h == INVALID_HANDLE_VALUE) return 0;
    ok = WriteFile(h, d, len, &put, NULL) && (int)put == len;
    CloseHandle(h);
    return ok;
}

/* 伸びるバイト列 */
typedef struct { u8 *p; int n, cap; } Buf;
static void bput(Buf *b, const void *d, int n)
{
    if (b->n + n > b->cap) {
        b->cap = (b->n + n) * 2 + 1024;
        b->p = (u8 *)realloc(b->p, b->cap);
    }
    memcpy(b->p + b->n, d, n);
    b->n += n;
}
static void bput32(Buf *b, unsigned v) { bput(b, &v, 4); }
static void bset32(Buf *b, int at, unsigned v) { memcpy(b->p + at, &v, 4); }
static unsigned rd32(const u8 *p) { unsigned v; memcpy(&v, p, 4); return v; }

static const char *tr_or(const char *key, const char *orig)
{
    const char *t = hget(key);
    return t ? t : orig;
}

/* D6STRING.DAT: 数, (番号, 位置)×数, 文字列 */
static int build_d6string(const u8 *d, int len, Buf *o)
{
    unsigned n = rd32(d), i, base;
    char key[32];
    Buf body = {0};
    if (4 + 8 * n > (unsigned)len) return 0;
    bput32(o, n);
    base = 4 + 8 * n;
    for (i = 0; i < n; i++) {
        unsigned id = rd32(d + 4 + 8 * i), off = rd32(d + 8 + 8 * i);
        const char *s;
        if (off >= (unsigned)len) return 0;
        sprintf(key, "S:%u", id);
        s = tr_or(key, (const char *)d + off);
        bput32(o, id);
        bput32(o, base + body.n);
        bput(&body, s, (int)strlen(s) + 1);
    }
    bput(o, body.p, body.n);
    free(body.p);
    return 1;
}

/* TEXTPAK.000: 数, (番号, 位置, 長さ[NUL まで])×数, 文字列 */
static int build_textpak(const u8 *d, int len, Buf *o)
{
    unsigned n = rd32(d), i, base;
    char key[32];
    Buf body = {0};
    if (4 + 12 * n > (unsigned)len) return 0;
    bput32(o, n);
    base = 4 + 12 * n;
    for (i = 0; i < n; i++) {
        unsigned id = rd32(d + 4 + 12 * i), off = rd32(d + 8 + 12 * i);
        const char *s;
        int sl;
        if (off >= (unsigned)len) return 0;
        sprintf(key, "T:%u", id);
        s = tr_or(key, (const char *)d + off);
        sl = (int)strlen(s);
        bput32(o, id);
        bput32(o, base + body.n);
        bput32(o, sl + 1);
        bput(&body, s, sl + 1);
    }
    bput(o, body.p, body.n);
    free(body.p);
    return 1;
}

/* 文字列の表: 数, 位置×数, 文字列 */
static void build_strtab(const u8 *r, int rl, const char *pre, int npc, Buf *o)
{
    unsigned c = rd32(r), i, base;
    char key[48];
    Buf body = {0};
    int start = o->n;
    bput32(o, c);
    for (i = 0; i < c; i++) bput32(o, 0);
    base = 4 + 4 * c;
    for (i = 0; i < c; i++) {
        unsigned off = rd32(r + 4 + 4 * i);
        const char *s = off < (unsigned)rl ? (const char *)r + off : "";
        sprintf(key, "%s:%d:%u", pre, npc, i);
        s = tr_or(key, s);
        bset32(o, start + 4 + 4 * i, base + body.n);
        bput(&body, s, (int)strlen(s) + 1);
    }
    bput(o, body.p, body.n);
    free(body.p);
}

/* 変数の表: 数, (値4 区分1)×数, 文字列。値は全部文字列の位置 */
static void build_vartab(const u8 *r, int rl, const char *pre, Buf *o)
{
    unsigned c = rd32(r), i, first = 0xFFFFFFFF, last = 0;
    char key[32];
    int start = o->n;
    if (4 + 5 * c > (unsigned)rl) { bput(o, r, rl); return; }
    for (i = 0; i < c; i++) {
        unsigned v = rd32(r + 4 + 5 * i);
        if (v < first) first = v;
        if (v > last) last = v;
    }
    if (!c || first != 4 + 5 * c || last >= (unsigned)rl) { bput(o, r, rl); return; }
    bput(o, r, first);
    for (i = 0; i < c; i++) {
        unsigned off = rd32(r + 4 + 5 * i);
        const char *s = (const char *)r + off;
        sprintf(key, "%s:%u", pre, i);
        s = tr_or(key, s);
        bset32(o, start + 4 + 5 * i, o->n - start);
        bput(o, s, (int)strlen(s) + 1);
    }
}

/* NPCDATA.PAK / GMDATA.PAK: (位置, 大きさ)×(1+3N)。部分 0 は変数の表、部分 2 は台詞、部分 3 は話題の語（NPC のみ） */
static int build_pak(const u8 *d, int len, int isnpc, Buf *o)
{
    unsigned off0 = rd32(d), nent_ = off0 / 8, N, i, k;
    unsigned *order;
    const char *pre = isnpc ? "N" : "G";
    if (off0 < 8 || off0 > (unsigned)len) return 0;
    N = (nent_ - 1) / 3;
    order = (unsigned *)malloc(nent_ * sizeof(unsigned));
    for (i = 0; i < nent_; i++) order[i] = i;
    /* 元の位置の順に並べる（空は後ろ） */
    for (i = 1; i < nent_; i++) {
        unsigned x = order[i], j = i;
        unsigned kx = rd32(d + 12 + 8 * 0) , ox = rd32(d + 8 * x), sx = rd32(d + 8 * x + 4);
        (void)kx;
        while (j > 0) {
            unsigned y = order[j - 1], oy = rd32(d + 8 * y), sy = rd32(d + 8 * y + 4);
            unsigned long long ky = sy ? oy : 0xFFFFFFFFull + y, kxx = sx ? ox : 0xFFFFFFFFull + x;
            if (ky <= kxx) break;
            order[j] = y; j--;
        }
        order[j] = x;
    }
    for (i = 0; i < nent_; i++) { bput32(o, 0); bput32(o, 0); }
    for (k = 0; k < nent_; k++) {
        unsigned e = order[k], eo = rd32(d + 8 * e), es = rd32(d + 8 * e + 4);
        int at = o->n;
        char pre2[8];
        if (!es) { bset32(o, 8 * e, eo); continue; }
        if (eo + es > (unsigned)len) { free(order); return 0; }
        if (e == 0) {
            sprintf(pre2, "%s0", pre);
            build_vartab(d + eo, es, pre2, o);
        } else {
            unsigned p = (e - 1) / N + 1, n = (e - 1) % N;
            if (p == 2) { sprintf(pre2, "%s2", pre); build_strtab(d + eo, es, pre2, n, o); }
            else if (p == 3 && isnpc) { sprintf(pre2, "%s3", pre); build_strtab(d + eo, es, pre2, n, o); }
            else bput(o, d + eo, es);
        }
        bset32(o, 8 * e, at);
        bset32(o, 8 * e + 4, o->n - at);
    }
    free(order);
    return 1;
}

/* 固定長の記録（記録 0 は頭）。名前の欄に訳を書く（欄の長さを超えない） */
typedef struct { const char *file; int rs; int nf; struct { int off, len; const char *tag; } f[2]; } RecDef;
static const RecDef recdefs[] = {
    {"d6item.dat", 284, 2, {{0, 22, "ITEM"}, {22, 22, "ITEMU"}}},
    {"d6mons.dat", 340, 1, {{0, 20, "MONS"}}},
    {"d6npc.dat", 36, 1, {{0, 20, "NPC"}}},
    {"d6prop.dat", 56, 1, {{0, 20, "PROP"}}},
    {"d6trlist.dat", 60, 1, {{0, 20, "TRL"}}},
};
static int build_recs(const RecDef *rd, const u8 *d, int len, Buf *o)
{
    unsigned n = rd32(d), r;
    int j;
    char key[32];
    bput(o, d, len);
    for (r = 1; r <= n; r++) {
        if ((int)((r + 1) * rd->rs) > len) break;
        for (j = 0; j < rd->nf; j++) {
            const char *t;
            int at = r * rd->rs + rd->f[j].off, tl;
            sprintf(key, "%s:%u", rd->f[j].tag, r);
            t = hget(key);
            if (!t) continue;
            tl = (int)strlen(t);
            if (tl > rd->f[j].len - 1) tl = rd->f[j].len - 1;
            /* 元の文の長さ（+NUL）か訳の長さ（+NUL）の大きい方だけ消して書く */
            {
                int ol = (int)strnlen((const char *)d + at, rd->f[j].len);
                int clr = (ol > tl ? ol : tl) + 1;
                if (clr > rd->f[j].len) clr = rd->f[j].len;
                memset(o->p + at, 0, clr);
                memcpy(o->p + at, t, tl);
            }
        }
    }
    return 1;
}

static const char *targets[] = {"d6string.dat", "textpak.000", "npcdata.pak", "gmdata.pak",
    "d6item.dat", "d6mons.dat", "d6npc.dat", "d6prop.dat", "d6trlist.dat", NULL};
static char built[16][MAX_PATH];
static int built_state[16];   /* 0 未, 1 済, -1 失敗 */

static const char *jp_path_for(const char *name)
{
    const char *b = name, *p;
    char low[64];
    int i, len;
    for (p = name; *p; p++) if (*p == '\\' || *p == '/' || *p == ':') b = p + 1;
    if (strlen(b) >= sizeof(low)) return NULL;
    for (i = 0; b[i]; i++) low[i] = (char)((b[i] >= 'A' && b[i] <= 'Z') ? b[i] + 32 : b[i]);
    low[i] = 0;
    for (i = 0; targets[i]; i++) if (!strcmp(low, targets[i])) break;
    if (!targets[i] || !nent) return NULL;
    if (built_state[i] == 1) return built[i];
    if (built_state[i] == -1) return NULL;
    built_state[i] = -1;
    {
        u8 *d = read_all(name, &len);
        Buf o = {0};
        int ok = 0, k;
        if (!d) { logp("%s を読めない\n", name); return NULL; }
        if (i == 0) ok = build_d6string(d, len, &o);
        else if (i == 1) ok = build_textpak(d, len, &o);
        else if (i == 2) ok = build_pak(d, len, 1, &o);
        else if (i == 3) ok = build_pak(d, len, 0, &o);
        else {
            for (k = 0; k < (int)(sizeof(recdefs) / sizeof(recdefs[0])); k++)
                if (!strcmp(recdefs[k].file, low)) ok = build_recs(&recdefs[k], d, len, &o);
        }
        free(d);
        if (!ok) { logp("%s を組み立てられない\n", name); free(o.p); return NULL; }
        sprintf(built[i], "%s\\%s", tmpdir, low);
        if (!write_all(built[i], o.p, o.n)) { logp("%s を書けない\n", built[i]); free(o.p); return NULL; }
        logp("%s → %s（%d バイト）\n", name, built[i], o.n);
        free(o.p);
    }
    built_state[i] = 1;
    return built[i];
}

static HANDLE WINAPI J_CreateFileA(LPCSTR name, DWORD acc, DWORD share, LPSECURITY_ATTRIBUTES sa,
                                   DWORD disp, DWORD flags, HANDLE tmpl)
{
    if (name && (acc & GENERIC_READ) && !(acc & GENERIC_WRITE) && disp == OPEN_EXISTING) {
        const char *p = jp_path_for(name);
        if (p) return real_CreateFileA(p, acc, share, sa, disp, flags, tmpl);
    }
    return real_CreateFileA(name, acc, share, sa, disp, flags, tmpl);
}

/* ---- 字 ---- */
#define FONTTAB ((u8 **)0x6B7418)
#define SURF (*(u8 **)0x6B7460)
#define SX (*(int *)(SURF + 0x1C))
#define ADDR_DRAWCHAR 0x424BA0

static void call_drawchar(u8 *font, int code)
{
    __asm {
        mov eax, font
        mov edx, code
        mov ecx, ADDR_DRAWCHAR
        call ecx
    }
}

typedef struct {
    u8 *data;            /* 字の絵（見分けの鍵） */
    int h, S, ol, olv, y0, sp, dark;
    u8 ink[64];
} FInfo;
static FInfo finfo[64];
static int nfinfo;

static int f_h(u8 *f) { return *(i16 *)(f + 2); }
static int f_sp(u8 *f) { return *(i16 *)(f + 8); }
static int f_cs(u8 *f) { return *(i16 *)(f + 0xC); }
static u8 *f_data(u8 *f) { return *(u8 **)(f + 0xE); }
static int glyph_w(u8 *f, int ch) { return *(i16 *)(f_data(f) + ch * f_cs(f)); }

static int E_[256], I_[256];
static int rowI[64][256], rowA[64][256];
static int csat(u16 c)
{
    int r = ((c >> 11) & 31) * 255 / 31, g = ((c >> 5) & 63) * 255 / 63, b = (c & 31) * 255 / 31;
    int mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    return mx - mn;
}
static FInfo *font_info(u8 *f)
{
    FInfo *fi;
    int i, ch, x, y, h, capt = 99, capb = -1, lowb = -1, anyt = 99, anyb = -1;
    int o = -1, te = 0, nd = 0, innerg = 0, ng = 0, band, capH;
    u8 *data = f_data(f);
    for (i = 0; i < nfinfo; i++) if (finfo[i].data == data) return &finfo[i];
    if (nfinfo >= 64) return &finfo[63];
    fi = &finfo[nfinfo++];
    memset(fi, 0, sizeof(*fi));
    fi->data = data;
    h = f_h(f); if (h > 64) h = 64; if (h < 1) h = 1;
    fi->h = h;
    fi->sp = f_sp(f);
    memset(E_, 0, sizeof(E_)); memset(I_, 0, sizeof(I_));
    memset(rowI, 0, sizeof(rowI)); memset(rowA, 0, sizeof(rowA));
    for (ch = 33; ch < 127; ch++) {
        int gw = glyph_w(f, ch);
        const u8 *px = data + ch * f_cs(f) + 2;
        if (gw <= 0 || gw > 64 || 2 + gw * f_h(f) > f_cs(f)) continue;
        for (y = 0; y < h; y++) for (x = 0; x < gw; x++) {
            int v = px[y * gw + x], edge;
            if (!v) continue;
            edge = x == 0 || x == gw - 1 || y == 0 || y == h - 1 ||
                   !px[y * gw + x - 1] || !px[y * gw + x + 1] || !px[(y - 1) * gw + x] || !px[(y + 1) * gw + x];
            if (edge) E_[v]++; else I_[v]++;
            if (ch >= 'A' && ch <= 'Z') { if (y < capt) capt = y; if (y > capb) capb = y; }
            if (ch >= 'a' && ch <= 'z') { if (y > lowb) lowb = y; }
            if (y < anyt) anyt = y; if (y > anyb) anyb = y;
        }
    }
    for (i = 1; i < 256; i++) {
        te += E_[i];
        if (E_[i] || I_[i]) nd++;
        if (E_[i] && (o < 0 || E_[i] > E_[o])) o = i;
    }
    /* 縁取りの色: 縁にいちばん多く、中にはあまり無く、ほかの色が字の中にある */
    if (o > 0 && nd > 1 && E_[o] * 100 > te * 55 && I_[o] * 2 < E_[o] + I_[o]) {
        for (ch = 'A'; ch <= 'Z'; ch++) {
            int gw = glyph_w(f, ch), has = 0;
            const u8 *px = data + ch * f_cs(f) + 2;
            if (gw <= 0 || gw > 64) continue;
            ng++;
            for (i = 0; i < gw * h; i++) if (px[i] && px[i] != o) { has = 1; break; }
            innerg += has;
        }
        if (ng && innerg * 2 >= ng) { fi->ol = 1; fi->olv = o; }
    }
    /* 段ごとの色 */
    for (ch = 33; ch < 127; ch++) {
        int gw = glyph_w(f, ch);
        const u8 *px = data + ch * f_cs(f) + 2;
        if (gw <= 0 || gw > 64 || 2 + gw * f_h(f) > f_cs(f)) continue;
        for (y = 0; y < h; y++) for (x = 0; x < gw; x++) {
            int v = px[y * gw + x], edge;
            if (!v || (fi->ol && v == fi->olv)) continue;
            edge = x == 0 || x == gw - 1 || y == 0 || y == h - 1 ||
                   !px[y * gw + x - 1] || !px[y * gw + x + 1] || !px[(y - 1) * gw + x] || !px[(y + 1) * gw + x];
            rowA[y][v]++;
            if (!edge) rowI[y][v]++;
        }
    }
    /* 段ごとの色: その段によく出る色（最多の 2 割以上）のうち、いちばん明るいもの。
       ただし字体でいちばん多い色が黒に近い（明るい地に黒い字）ときは、いちばん暗いもの */
    {
        const u16 *pal = *(const u16 **)(f + 0x12);
        int type = *(i16 *)(f + 4), dark = 0, sat = 0;
        if (type == 1 && pal && !IsBadReadPtr(pal, 32 * 256 * 2)) {
            int tot[256], m = 0;
            memset(tot, 0, sizeof(tot));
            for (y = 0; y < h; y++) for (i = 1; i < 256; i++) tot[i] += (fi->ol ? rowI : rowA)[y][i];
            for (i = 1; i < 256; i++) if (tot[i] > tot[m]) m = i;
            if (m) {
                u16 c = pal[31 * 256 + m];
                dark = ((c >> 11) & 31) * 77 + ((c >> 5) & 63) * 75 + (c & 31) * 29 < 600;
                sat = csat(c) >= 60 ? csat(c) / 3 : 0;   /* 色味のある字（青など）: 灰色の縁の色は選ばない */
            }
        }
        fi->dark = dark;
        for (y = 0; y < h; y++) {
            int (*row)[256] = (fi->ol ? rowI : rowA);
            int mx = 0, best = 0, bl = -1;
            for (i = 1; i < 256; i++) if (row[y][i] > mx) mx = row[y][i];
            if (!mx) { row = rowA; for (i = 1; i < 256; i++) if (row[y][i] > mx) mx = row[y][i]; }
            for (i = 1; i < 256; i++) {
                int l;
                if (!mx || row[y][i] * 5 < mx) continue;
                if (type == 1 && pal && !IsBadReadPtr(pal, 32 * 256 * 2)) {
                    u16 c = pal[31 * 256 + i];
                    l = ((c >> 11) & 31) * 77 + ((c >> 5) & 63) * 75 + (c & 31) * 29;
                    if (sat && csat(c) < sat) continue;
                } else l = row[y][i];
                if (dark) l = 100000 - l;
                if (l > bl) { bl = l; best = i; }
            }
            fi->ink[y] = (u8)best;
        }
    }
    for (y = 0; y < h; y++) if (!fi->ink[y]) {
        int d;
        for (d = 1; d < h; d++) {
            if (y - d >= 0 && fi->ink[y - d]) { fi->ink[y] = fi->ink[y - d]; break; }
            if (y + d < h && fi->ink[y + d]) { fi->ink[y] = fi->ink[y + d]; break; }
        }
        if (!fi->ink[y]) fi->ink[y] = 1;
    }
    /* 大きさと置く段 */
    if (capb < 0) { capt = anyt; capb = anyb; }
    if (capb < 0) { capt = 0; capb = h - 1; }
    band = (lowb > capb ? lowb : capb) - capt + 1;
    fi->S = 8;
    if (16 + 2 * fi->ol <= h && 16 <= band) fi->S = 16;
    else if (14 + 2 * fi->ol <= h && 14 <= band) fi->S = 14;
    else if (12 + 2 * fi->ol <= h && 12 <= band) fi->S = 12;
    capH = capb - capt + 1;
    fi->y0 = capt + (capH - (fi->S + 2 * fi->ol)) / 2;
    if (fi->y0 > h - fi->S - 2 * fi->ol) fi->y0 = h - fi->S - 2 * fi->ol;
    if (fi->y0 < 0) fi->y0 = 0;
    logp("字体 %p: 高さ %d 間 %d → 日本語 %d 段 %d 縁 %d(%d) 暗 %d\n", data, h, fi->sp, fi->S, fi->y0, fi->ol, fi->olv, fi->dark);
    return fi;
}

static const u16 *jrows(int S, int c1, int c2)
{
    int idx = (c1 - 0xA1) * 94 + (c2 - 0xA1);
    if (idx < 0 || idx >= 94 * 94) idx = 0;
    switch (S) {
    case 16: return jf16[idx];
    case 14: return jf14[idx];
    case 12: return jf12[idx];
    default: return jf8[idx];
    }
}
static int jwidth(FInfo *fi) { return fi->S + 2 * fi->ol; }

static int is_jp(const u8 *s) { return s[0] >= 0xA1 && s[0] != 0xFF && s[1] >= 0xA1 && s[1] != 0xFF; }
static int char_w(u8 *f, FInfo *fi, const u8 *s, int *adv)
{
    if (is_jp(s)) { *adv = 2; return jwidth(fi) + fi->sp; }
    *adv = 1;
    if (s[0] >= 0x80) return 0;
    return glyph_w(f, s[0]) + fi->sp;
}

static u8 gbuf[2 + 40 * 64];
static u8 fake[0x20];
static int draw_jp(u8 *f, FInfo *fi, int c1, int c2)
{
    const u16 *rows = jrows(fi->S, c1, c2);
    int W = jwidth(fi), H = fi->h, x, y;
    u8 *px = gbuf + 2;
    if (SX + W > 639) return 0;
    memset(gbuf, 0, sizeof(gbuf));
    *(i16 *)gbuf = (i16)W;
    for (y = 0; y < fi->S; y++) {
        int yy = fi->y0 + fi->ol + y;
        if (yy < 0 || yy >= H) continue;
        for (x = 0; x < fi->S; x++)
            if (rows[y] & (0x8000 >> x)) px[yy * W + x + fi->ol] = fi->ink[yy];
    }
    if (fi->ol) {
        static u8 m[40 * 64];
        memcpy(m, px, W * H);
        for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
            if (m[y * W + x]) continue;
            if ((x > 0 && m[y * W + x - 1] && m[y * W + x - 1] != fi->olv) ||
                (x < W - 1 && m[y * W + x + 1]) || (y > 0 && m[(y - 1) * W + x]) || (y < H - 1 && m[(y + 1) * W + x]))
                px[y * W + x] = (u8)fi->olv;
        }
    }
    memcpy(fake, f, 0x1A);
    *(i16 *)(fake + 0xC) = (i16)(2 + W * H);
    *(u8 **)(fake + 0xE) = gbuf;
    {
        int x0 = SX;
        call_drawchar(fake, 0);
        SX = x0 + W + fi->sp;     /* 1 字の関数は幅の 2 倍進めてしまうので直す */
    }
    return 1;
}

/* 訳の無い英文を記録する（試験版で WWJP_LOG のとき） */
#ifdef WWJP_TEST
static unsigned seen[8192];
static void log_untranslated(const u8 *s)
{
    unsigned h, i;
    int letters = 0;
    const u8 *p;
    if (!logf) return;
    for (p = s; *p; p++) { if (*p >= 0x80) return; if ((*p | 32) >= 'a' && (*p | 32) <= 'z') letters++; }
    if (letters < 3) return;
    h = hstr((const char *)s) | 1;
    i = h & 8191;
    while (seen[i]) { if (seen[i] == h) return; i = (i + 1) & 8191; }
    seen[i] = h;
    logp("英: %s\n", s);
}
#endif

static void c_drawstring(u8 *f, const u8 *s)
{
    FInfo *fi = font_info(f);
#ifdef WWJP_TEST
    log_untranslated(s);
#endif
    while (*s) {
        if (is_jp(s)) {
            if (!draw_jp(f, fi, s[0], s[1])) return;
            s += 2;
        } else {
            int c = *s++;
            if (c >= 0x80) continue;
            {
                int x0 = SX, gw = glyph_w(f, c);
                if (x0 + gw > 639) return;
                call_drawchar(f, c);
                SX = x0 + gw + fi->sp;
            }
        }
    }
}
static int c_width(int idx, const u8 *s)
{
    u8 *f = FONTTAB[idx];
    FInfo *fi = font_info(f);
    int w = 0, adv;
    while (*s) { w += char_w(f, fi, s, &adv); s += adv; }
    return w;
}
static int in_list(const u16 *l, const u8 *s)
{
    u16 c;
    if (!is_jp(s)) return 0;
    c = (u16)(s[0] << 8 | s[1]);
    for (; *l; l++) if (*l == c) return 1;
    return 0;
}
static int c_fit(int idx, const u8 *s, int maxw)
{
    u8 *f = FONTTAB[idx];
    FInfo *fi = font_info(f);
    int w = 0, n = 0, adv, prev = -1, prev2 = -1;
    while (s[n]) {
        int cw = char_w(f, fi, s + n, &adv);
        if (w + cw > maxw) break;
        w += cw;
        prev2 = prev; prev = n;
        n += adv;
    }
    if (s[n] && n > 0) {
        /* 行頭に来てはいけない字が次に来るなら、前の 1 字を次の行へ送る */
        if (in_list(nostart, s + n) && prev > 0) n = prev;
        /* 行末に来てはいけない字で終わるなら、それを次の行へ送る */
        else if (prev >= 0 && in_list(noend, s + prev) && prev > 0) n = prev;
    }
    (void)prev2;
    return n;
}
static int c_fitr(int idx, const u8 *s, int maxw)
{
    u8 *f = FONTTAB[idx];
    FInfo *fi = font_info(f);
    static int pos[4096];
    int n = 0, i = 0, adv, w = 0, k;
    while (s[i] && n < 4096) { pos[n++] = i; char_w(f, fi, s + i, &adv); i += adv; }
    for (k = n - 1; k >= 0; k--) {
        w += char_w(f, fi, s + pos[k], &adv);
        if (w > maxw) return k + 1 < n ? pos[k + 1] : i;
    }
    return 0;
}

/* 話題の語を全体の辞書から引く（0x4A6550 の置き換え）。英語で打った語は KWH:ハッシュ で日本語の語に直して引く */
static unsigned hlower(const char *s)
{
    unsigned h = 2166136261u;
    for (; *s; s++) { u8 c = (u8)*s; if (c >= 'A' && c <= 'Z') c += 32; h ^= c; h *= 16777619u; }
    return h;
}
static int kw_search(const char *s)
{
    int n = *(int *)0x5D9130, i;
    const u8 *d = *(const u8 **)0x908268;
    if (!d || n <= 0) return -1;
    for (i = 0; i < n; i++) {
        unsigned off = rd32(d + 4 + 5 * i);
        if (!_stricmp((const char *)d + off, s)) return i;
    }
    return -1;
}
static int c_findkw(const char *s)
{
    int r = kw_search(s);
    if (r < 0 && s && *s) {
        const u8 *p;
        for (p = (const u8 *)s; *p && *p < 0x80; p++);
        if (!*p) {
            char key[24];
            const char *j;
            sprintf(key, "KWH:%08x", hlower(s));
            j = hget(key);
            if (j) r = kw_search(j);
            logp("語「%s」→ %s → %d\n", s, j ? j : "(無し)", r);
        }
    }
    return r;
}

/* Watcom の呼び方（eax, edx, ebx）から C へ。eax 以外は保つ */
__declspec(naked) static void H_DrawString(void)
{
    __asm {
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        push edx
        push eax
        call c_drawstring
        add esp, 8
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
    }
}
__declspec(naked) static void H_Width(void)
{
    __asm {
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        push edx
        push eax
        call c_width
        add esp, 8
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
    }
}
__declspec(naked) static void H_FindKw(void)
{
    __asm {
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        push eax
        call c_findkw
        add esp, 4
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
    }
}
__declspec(naked) static void H_Fit(void)
{
    __asm {
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        push ebx
        push edx
        push eax
        call c_fit
        add esp, 12
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
    }
}
__declspec(naked) static void H_FitR(void)
{
    __asm {
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        push ebx
        push edx
        push eax
        call c_fitr
        add esp, 12
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        ret
    }
}

#ifdef WWJP_TEST
/* ---- 試験の入力（WWJP_TEST のとき）: 共有メモリ "WWJP_IN" からマウスとキーを混ぜる ----
 * マウス: GOG 版は 0x5F11E0 の関数（POINT* を埋める）で位置を得て、0x5E903C/3D/3E がボタン。
 *         その関数を差し替え、指定の位置とボタンを返す。
 * キー: キーボードはバッファ読み（GetDeviceData）。押す・離すの出来事を足す。 */
typedef struct {
    volatile LONG seq, done;
    volatile LONG x, y;         /* マウスの位置（ゲームの座標）。x<0 で指定しない */
    volatile LONG btn;          /* bit0 左, bit1 右 */
    volatile LONG btn_n;        /* ボタンを押しておく回数（位置の問い合わせの数） */
    volatile LONG key;          /* DIK 番号 */
    volatile LONG busy;         /* まだ処理中なら 1 */
    volatile LONG calls_pos, calls_k;
} TestIn;
static TestIn *tin;
static void *dev_kbd;
static int test_on;
static LONG cur_x = 320, cur_y = 240, btn_bits, btn_n, key_state;   /* key_state: 0 無, 1 押す, 2 離す */
static LONG key_code;
static DWORD btn_until, key_until;
typedef HRESULT (WINAPI *CreateDevice_t)(void *, const GUID *, void **, void *);
typedef HRESULT (WINAPI *GetData_t)(void *, DWORD, void *, DWORD *, DWORD);
typedef BOOL (WINAPI *GetPos_t)(POINT *);
static CreateDevice_t real_CreateDevice;
static GetData_t real_GetData;
typedef HRESULT (WINAPI *GetState_t)(void *, DWORD, void *);
static GetState_t real_GetState;
static HRESULT WINAPI W_GetState(void *dev, DWORD sz, void *data)
{
    HRESULT r = real_GetState(dev, sz, data);
    if (tin && dev == dev_kbd && data && sz >= 256 && key_code && (key_state == 1 || (LONG)(GetTickCount() - key_until) < 0)) {
        ((u8 *)data)[key_code & 255] |= 0x80;
        if (r != 0) r = 0;
    }
    return r;
}
static GetPos_t real_GetPos;
static const GUID G_SysKeyboard = {0x6F1D2B61, 0xD5A0, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
static void poll_test(void)
{
    if (!tin) return;
    if (tin->seq != tin->done) {
        if (tin->x >= 0) { cur_x = tin->x; cur_y = tin->y; }
        btn_bits = tin->btn; btn_n = tin->btn_n;
        if (tin->key) { key_code = tin->key; key_state = 1; key_until = GetTickCount() + (btn_n > 0 ? 50 * btn_n : 120); btn_n = 0; }
        tin->done = tin->seq;
    }
    tin->busy = (btn_n > 0 || btn_until || key_state) ? 1 : 0;
}
static BOOL WINAPI T_GetPos(POINT *pt)
{
    BOOL r = real_GetPos ? real_GetPos(pt) : TRUE;
    poll_test();
    if (tin) tin->calls_pos++;
    /* 試験のときは本物のマウスを使わない（窓の外の座標で落ちるため）。常に画面の中に収める */
    pt->x = cur_x < 0 ? 0 : cur_x > 639 ? 639 : cur_x;
    pt->y = cur_y < 0 ? 0 : cur_y > 479 ? 479 : cur_y;
    return r;
}
/* ボタンは GetAsyncKeyState(VK_LBUTTON/VK_RBUTTON) で読まれる。押す指示から btn_n×50ms の間だけ押す */
typedef SHORT (WINAPI *GAKS_t)(int);
static GAKS_t real_GAKS;
static SHORT WINAPI T_GetAsyncKeyState(int vk)
{
    SHORT r = real_GAKS(vk);
    poll_test();
    if (btn_n > 0) { btn_until = GetTickCount() + 50 * btn_n; btn_n = 0; }
    if (btn_until && (LONG)(GetTickCount() - btn_until) < 0) {
        if ((vk == VK_LBUTTON && (btn_bits & 1)) || (vk == VK_RBUTTON && (btn_bits & 2))) r |= (SHORT)0x8001;
    } else btn_until = 0;
    if (tin) tin->busy = (btn_until || key_state) ? 1 : 0;
    return r;
}
static void hook_pos(void)
{
    void **pp = (void **)0x5F11E0;
    if (*pp && *pp != (void *)T_GetPos) {
        real_GetPos = (GetPos_t)*pp;
        *pp = (void *)T_GetPos;
        logp("マウスの位置の関数を差し替えた\n");
    }
}
static HRESULT WINAPI W_GetData(void *dev, DWORD sz, void *od, DWORD *n, DWORD fl)
{
    DWORD room = n ? *n : 0;
    HRESULT r = real_GetData(dev, sz, od, n, fl);
    static int nlog;
    if (!tin || dev != dev_kbd) return r;
    if (nlog < 8) { nlog++; logp("GetDeviceData sz=%lu od=%p room=%lu got=%lu fl=%lu r=%08lx" "%c", sz, od, room, n ? *n : 0, fl, (unsigned long)r, 10); }
    hook_pos();
    poll_test();
    tin->calls_k++;
    if (key_state && od && n && !(fl & 1) && *n < room && sz >= 16 && (r == 0 || r == 1) &&
        (key_state == 1 || (LONG)(GetTickCount() - key_until) >= 0)) {
        DWORD *e = (DWORD *)((u8 *)od + *n * sz);
        memset(e, 0, sz);
        e[0] = (DWORD)key_code;
        e[1] = key_state == 1 ? 0x80 : 0;
        e[2] = GetTickCount();
        e[3] = 0;
        (*n)++;
        key_state = key_state == 1 ? 2 : 0;
        if (!key_state) key_code = 0;
    }
    return r;
}
static void patch_vt(void **vt, int i, void *fn, void **save)
{
    DWORD old;
    if (vt[i] == fn) return;
    if (save && !*save) *save = vt[i];
    VirtualProtect(&vt[i], 4, PAGE_READWRITE, &old);
    vt[i] = fn;
    VirtualProtect(&vt[i], 4, old, &old);
}
static HRESULT WINAPI W_CreateDevice(void *di, const GUID *g, void **dev, void *u)
{
    HRESULT r = real_CreateDevice(di, g, dev, u);
    if (r == S_OK && dev && *dev && !memcmp(g, &G_SysKeyboard, sizeof(GUID))) {
        dev_kbd = *dev;
        patch_vt(*(void ***)*dev, 10, (void *)W_GetData, (void **)&real_GetData);
        patch_vt(*(void ***)*dev, 9, (void *)W_GetState, (void **)&real_GetState);
        logp("キーボードの読み取りに割り込んだ\n");
    }
    return r;
}
static void di_wrap(void *di)
{
    if (!test_on) return;
    patch_vt(*(void ***)di, 3, (void *)W_CreateDevice, (void **)&real_CreateDevice);
}
static void test_init(void)
{
    HANDLE m;
    test_on = 1;
    /* 試験のあいだ画面が省電力で消えると描画の装置が失われるので、点けておく（このプロセスの間だけ） */
    SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
    m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, "WWJP_IN");
    if (m) tin = (TestIn *)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, 4096);
}

/* sprintf（0x5AB186）に入る書式を記録する（WWJP_FMT のとき） */
static void c_logfmt(DWORD *sp)
{
    const char *f = (const char *)sp[2];
    static const char *seenf[512];
    static int nseen;
    int i, k, ns = 0;
    char line[512];
    if (IsBadReadPtr(f, 4) || !strstr(f, "%s")) return;
    for (i = 0; i < nseen; i++) if (seenf[i] == f) return;
    if (nseen < 512) seenf[nseen++] = f;
    k = sprintf(line, "書式 %p「%.60s」", f, f);
    for (i = 3; i < 7 && ns < 4; i++) {
        const char *a = (const char *)sp[i];
        if ((DWORD)a > 0x10000 && !IsBadReadPtr(a, 1) && a[0] && (u8)a[0] != 0xCC) {
            k += sprintf(line + k, " [%.30s]", a);
            ns++;
        }
    }
    logp("%s%c", line, 10);
}
__declspec(naked) static void H_Sprintf(void)
{
    __asm {
        pushad
        lea eax, [esp + 32]
        push eax
        call c_logfmt
        add esp, 4
        popad
        push ebx
        push ecx
        push edx
        sub esp, 4
        push 0x5AB18C
        ret
    }
}
#endif /* WWJP_TEST */

/* ---- 書き換え ---- */
static void write_jmp(DWORD at, void *to)
{
    DWORD old;
    u8 *p = (u8 *)at;
    VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
    p[0] = 0xE9;
    *(DWORD *)(p + 1) = (DWORD)to - (at + 5);
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
}
static int check(DWORD at, const char *hex)
{
    const u8 *p = (const u8 *)at;
    int i = 0;
    while (hex[0] && hex[1]) {
        unsigned v;
        sscanf(hex, "%2x", &v);
        if (p[i++] != (u8)v) return 0;
        hex += 2;
    }
    return 1;
}
static void set_iat(DWORD at, void *fn, void *expect)
{
    DWORD old;
    void **p = (void **)at;
    if (*p != expect) { logp("IAT %08lx が違う\n", at); return; }
    VirtualProtect(p, 4, PAGE_READWRITE, &old);
    *p = fn;
    VirtualProtect(p, 4, old, &old);
}

#ifdef WWJP_TEST
/* ---- 試験のときだけ: GOG の ddraw.dll が作る Direct3D 9 の画面を窓表示にする（前面でなくても最小化されない） ---- */
typedef void *(WINAPI *D3DCreate9_t)(UINT);
typedef HRESULT (WINAPI *D3DCreateDev_t)(void *, UINT, int, HWND, DWORD, DWORD *, void **);
typedef HRESULT (WINAPI *D3DReset_t)(void *, DWORD *);
static D3DCreate9_t real_D3DCreate9;
static D3DCreateDev_t real_D3DCreateDev;
static D3DReset_t real_D3DReset;
static void force_windowed(DWORD *pp)
{
    pp[8] = 1;      /* Windowed */
    pp[12] = 0;     /* FullScreen_RefreshRateInHz */
    pp[2] = 0;      /* BackBufferFormat: 画面に合わせる */
}
static HRESULT WINAPI W_D3DReset(void *dev, DWORD *pp)
{
    HRESULT r;
    force_windowed(pp);
    r = real_D3DReset(dev, pp);
    logp("Reset %08lx%c", (unsigned long)r, 10);
    return r;
}
static DWORD saved_pp[14];
static int saved_pp_ok;
typedef HRESULT (WINAPI *Present_t)(void *, const void *, const void *, HWND, const void *);
static Present_t real_Present;
static HRESULT WINAPI W_Present(void *dev, const void *a, const void *b, HWND w, const void *c)
{
    HRESULT r = real_Present(dev, a, b, w, c);
    if (r == (HRESULT)0x88760868) r = S_OK;
    return r;
}
typedef HRESULT (WINAPI *TCL_t)(void *);
static TCL_t real_TCL;
static HRESULT WINAPI W_TCL(void *dev)
{
    HRESULT r = real_TCL(dev);
    static HRESULT last = 1;
    if (r == (HRESULT)0x88760869 && saved_pp_ok) {
        DWORD pp[14];
        HRESULT rr;
        memcpy(pp, saved_pp, sizeof(pp));
        rr = real_D3DReset(dev, pp);
        logp("代わりに Reset %08lx%c", (unsigned long)rr, 10);
        r = real_TCL(dev);
    }
    if (r != last) { logp("TestCooperativeLevel %08lx%c", (unsigned long)r, 10); last = r; }
    if (r == (HRESULT)0x88760868) r = S_OK;   /* 失われている間も続けさせる */
    return r;
}
static HRESULT WINAPI W_D3DCreateDev(void *d3d, UINT ad, int type, HWND hw, DWORD fl, DWORD *pp, void **out)
{
    HRESULT r;
    force_windowed(pp);
    r = real_D3DCreateDev(d3d, ad, type, hw, fl, pp, out);
    if (r == S_OK) { memcpy(saved_pp, pp, sizeof(saved_pp)); saved_pp_ok = 1; }
    logp("Direct3D の画面を窓で作った（%08lx）\n", (unsigned long)r);
    if (r == S_OK && out && *out) {
        patch_vt(*(void ***)*out, 16, (void *)W_D3DReset, (void **)&real_D3DReset);
        patch_vt(*(void ***)*out, 3, (void *)W_TCL, (void **)&real_TCL);
        patch_vt(*(void ***)*out, 17, (void *)W_Present, (void **)&real_Present);
    }
    return r;
}
static void *WINAPI W_D3DCreate9(UINT v)
{
    void *d3d = real_D3DCreate9(v);
    if (d3d) patch_vt(*(void ***)d3d, 16, (void *)W_D3DCreateDev, (void **)&real_D3DCreateDev);
    return d3d;
}
static HWND game_hwnd(void)
{
    HWND h = FindWindowA(NULL, "Deep6");
    DWORD pid = 0;
    if (h) GetWindowThreadProcessId(h, &pid);
    return (h && pid == GetCurrentProcessId()) ? h : NULL;
}
static HWND WINAPI T_GetActiveWindow(void) { HWND h = game_hwnd(); return h ? h : GetActiveWindow(); }
static BOOL WINAPI T_IsIconic(HWND h) { (void)h; return FALSE; }
static void ddraw_iat(const char *dll, const char *fn, void *to)
{
    HMODULE m = GetModuleHandleA("ddraw.dll");
    IMAGE_NT_HEADERS *nh;
    IMAGE_IMPORT_DESCRIPTOR *id;
    if (!m) return;
    nh = (IMAGE_NT_HEADERS *)((u8 *)m + ((IMAGE_DOS_HEADER *)m)->e_lfanew);
    id = (IMAGE_IMPORT_DESCRIPTOR *)((u8 *)m + nh->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; id->Name; id++) {
        IMAGE_THUNK_DATA *ot, *ft;
        if (_stricmp((char *)m + id->Name, dll)) continue;
        ot = (IMAGE_THUNK_DATA *)((u8 *)m + id->OriginalFirstThunk);
        ft = (IMAGE_THUNK_DATA *)((u8 *)m + id->FirstThunk);
        for (; ot->u1.AddressOfData; ot++, ft++) {
            DWORD old;
            if (ot->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            if (strcmp((char *)((IMAGE_IMPORT_BY_NAME *)((u8 *)m + ot->u1.AddressOfData))->Name, fn)) continue;
            VirtualProtect(&ft->u1.Function, 4, PAGE_READWRITE, &old);
            ft->u1.Function = (DWORD)to;
            VirtualProtect(&ft->u1.Function, 4, old, &old);
            logp("ddraw.dll の %s を差し替えた%c", fn, 10);
        }
    }
}

static void hook_ddraw_d3d(void)
{
    HMODULE m = GetModuleHandleA("ddraw.dll");
    IMAGE_DOS_HEADER *dh;
    IMAGE_NT_HEADERS *nh;
    IMAGE_IMPORT_DESCRIPTOR *id;
    if (!m) { logp("ddraw.dll がまだ無い\n"); return; }
    dh = (IMAGE_DOS_HEADER *)m;
    nh = (IMAGE_NT_HEADERS *)((u8 *)m + dh->e_lfanew);
    id = (IMAGE_IMPORT_DESCRIPTOR *)((u8 *)m + nh->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; id->Name; id++) {
        IMAGE_THUNK_DATA *ot, *ft;
        if (_stricmp((char *)m + id->Name, "d3d9.dll")) continue;
        ot = (IMAGE_THUNK_DATA *)((u8 *)m + id->OriginalFirstThunk);
        ft = (IMAGE_THUNK_DATA *)((u8 *)m + id->FirstThunk);
        for (; ot->u1.AddressOfData; ot++, ft++) {
            IMAGE_IMPORT_BY_NAME *ibn;
            DWORD old;
            if (ot->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            ibn = (IMAGE_IMPORT_BY_NAME *)((u8 *)m + ot->u1.AddressOfData);
            if (strcmp((char *)ibn->Name, "Direct3DCreate9")) continue;
            real_D3DCreate9 = (D3DCreate9_t)ft->u1.Function;
            VirtualProtect(&ft->u1.Function, 4, PAGE_READWRITE, &old);
            ft->u1.Function = (DWORD)W_D3DCreate9;
            VirtualProtect(&ft->u1.Function, 4, old, &old);
            logp("ddraw.dll の Direct3DCreate9 に割り込んだ\n");
        }
    }
}

/* 試験のときだけ: 画面の Lock が「描画中」以外で失敗したら、少し待ってやり直す（窓表示で画面が失われても落ちないように） */
static const DWORD lock_sites[] = {0x423525, 0x423D90, 0x424336, 0x424680, 0x42479C, 0x424899, 0x425308, 0x425412,
                                   0x425546, 0x427164, 0x4272A9, 0x42785F, 0x4279F1, 0x427A96, 0x427B3B, 0x427C07, 0x427CA3};
static volatile LONG lock_tries;
static void patch_locks(void)
{
    u8 *mem = (u8 *)VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    u8 *q = mem;
    DWORD sl = (DWORD)GetProcAddress(GetModuleHandleA("kernel32.dll"), "Sleep");
    int k, n = 0;
    for (k = 0; k < (int)(sizeof(lock_sites) / sizeof(lock_sites[0])); k++) {
        u8 *a = (u8 *)lock_sites[k], *b = q;
        DWORD back, next = lock_sites[k] + 7, old;
        if (a[0] != 0x3D || *(DWORD *)(a + 1) != 0x8876021C || a[5] != 0x74) continue;
        back = next + (signed char)a[6];
        /* cmp eax, 8876021Ch / je back */
        *q++ = 0x3D; *(DWORD *)q = 0x8876021C; q += 4;
        *q++ = 0x0F; *q++ = 0x84; *(DWORD *)q = back - ((DWORD)q + 4); q += 4;
        /* test eax, eax / jnz FAIL */
        *q++ = 0x85; *q++ = 0xC0;
        *q++ = 0x75; *q++ = 0x0F;
        /* 成功: mov dword [lock_tries], 0 / jmp next */
        *q++ = 0xC7; *q++ = 0x05; *(DWORD *)q = (DWORD)&lock_tries; q += 4; *(DWORD *)q = 0; q += 4;
        *q++ = 0xE9; *(DWORD *)q = next - ((DWORD)q + 4); q += 4;
        /* FAIL: inc dword [lock_tries] / cmp dword [lock_tries], 20 / jae 成功の所（0 に戻して先へ） */
        *q++ = 0xFF; *q++ = 0x05; *(DWORD *)q = (DWORD)&lock_tries; q += 4;
        *q++ = 0x83; *q++ = 0x3D; *(DWORD *)q = (DWORD)&lock_tries; q += 4; *q++ = 20;
        if (lock_sites[k] == 0x425546) {
            /* カーソル: 諦めたら描かずに関数の終わり（0x42560E）へ */
            *q++ = 0x72; *q++ = 0x0F;                    /* jb 待つ */
            *q++ = 0xC7; *q++ = 0x05; *(DWORD *)q = (DWORD)&lock_tries; q += 4; *(DWORD *)q = 0; q += 4;
            *q++ = 0xE9; *(DWORD *)q = 0x42560E - ((DWORD)q + 4); q += 4;
        } else
        *q++ = 0x73, *q++ = (u8)(0x100 - (13 + 15 + 2));   /* 成功の所（mov dword..）へ戻る */
        /* pushad / push 50 / mov eax, Sleep / call eax / popad / jmp back */
        *q++ = 0x60; *q++ = 0x6A; *q++ = 50; *q++ = 0xB8; *(DWORD *)q = sl; q += 4; *q++ = 0xFF; *q++ = 0xD0; *q++ = 0x61;
        *q++ = 0xE9; *(DWORD *)q = back - ((DWORD)q + 4); q += 4;
        VirtualProtect(a, 7, PAGE_EXECUTE_READWRITE, &old);
        a[0] = 0xE9; *(DWORD *)(a + 1) = (DWORD)b - (lock_sites[k] + 5); a[5] = 0x90; a[6] = 0x90;
        VirtualProtect(a, 7, old, &old);
        n++;
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logp("Lock の確かめ %d か所%c", n, 10);
}
#endif /* WWJP_TEST */

/* 折り返しの「空白まで戻る」比べ（cmp byte ptr [..],20h / je）を、
 * 「空白か、EUC の字（0xA1 以上）なら区切れる」に置き換える */
static const DWORD brk_sites[] = {0x491E52, 0x4E5E21, 0x4E6578, 0x506016, 0x55540D, 0x56C4DE,
                                  0x588D5D, 0x5890E1, 0x58967D, 0x59D090};
static void patch_breaks(void)
{
    u8 *mem = (u8 *)VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    u8 *q = mem;
    int k;
    for (k = 0; k < (int)(sizeof(brk_sites) / sizeof(brk_sites[0])); k++) {
        u8 *a = (u8 *)brk_sites[k], *b;
        int modrm, mod, rm, len = 2, sib = -1, esp_base = 0, disp_sz = 0, cmp_len, tot;
        long disp = 0;
        DWORD after, target, old;
        if (a[0] != 0x80) { logp("区切り %08lx が違う\n", brk_sites[k]); continue; }
        modrm = a[1]; mod = modrm >> 6; rm = modrm & 7;
        if (((modrm >> 3) & 7) != 7) continue;
        if (rm == 4) { sib = a[2]; len++; if ((sib & 7) == 4) esp_base = 1; }
        if (mod == 1) { disp = (signed char)a[len]; disp_sz = 1; }
        else if (mod == 2 || (mod == 0 && rm == 5)) { memcpy(&disp, a + len, 4); disp_sz = 4; }
        else if (mod != 0 || (sib >= 0 && (sib & 7) == 5)) continue;
        len += disp_sz;
        if (a[len] != 0x20) continue;
        cmp_len = len + 1;
        if (a[cmp_len] != 0x74) continue;               /* je だけ */
        tot = cmp_len + 2;
        after = brk_sites[k] + tot;
        target = after + (signed char)a[cmp_len + 1];
        if (esp_base) disp += 4;
        b = q;
        *q++ = 0x50;                                     /* push eax */
        *q++ = 0x8A;                                     /* mov al, r/m8 */
        if (esp_base && mod == 0) { *q++ = (u8)((modrm & 0xC7) | 0x40); }
        else *q++ = (u8)(modrm & 0xC7);
        if (sib >= 0) *q++ = (u8)sib;
        if (disp_sz == 1 || (esp_base && mod == 0)) *q++ = (u8)disp;
        else if (disp_sz == 4) { memcpy(q, &disp, 4); q += 4; }
        *q++ = 0x3C; *q++ = 0x20;                        /* cmp al, 20h */
        *q++ = 0x74; *q++ = 0x0A;                        /* je YES */
        *q++ = 0x3C; *q++ = 0xA1;                        /* cmp al, A1h */
        *q++ = 0x73; *q++ = 0x06;                        /* jae YES */
        *q++ = 0x58; *q++ = 0xE9; *(DWORD *)q = after - ((DWORD)q + 4); q += 4;   /* NO: pop; jmp after */
        *q++ = 0x58; *q++ = 0xE9; *(DWORD *)q = target - ((DWORD)q + 4); q += 4;  /* YES: pop; jmp target */
        VirtualProtect(a, tot, PAGE_EXECUTE_READWRITE, &old);
        a[0] = 0xE9; *(DWORD *)(a + 1) = (DWORD)b - (brk_sites[k] + 5);
        memset(a + 5, 0x90, tot - 5);
        VirtualProtect(a, tot, old, &old);
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logp("区切りの比べ %d か所\n", (int)(sizeof(brk_sites) / sizeof(brk_sites[0])));
}

/* 町の依頼主の名前の表（exe の中、26 バイトずつ）を訳に書き換える。鍵 GMN:番号 */
static void patch_gm_names(void)
{
    int i;
    char key[16];
    for (i = 0; i < 21; i++) {
        u8 *p = (u8 *)(0x5ED456 + 26 * i);
        const char *t;
        DWORD old;
        sprintf(key, "GMN:%d", i);
        t = hget(key);
        if (!t || strlen(t) > 25) continue;
        VirtualProtect(p, 26, PAGE_READWRITE, &old);
        memset(p, 0, 26);
        memcpy(p, t, strlen(t));
        VirtualProtect(p, 26, old, &old);
    }
}

/* 会話の一覧: 人物の表の語のうち、打ち込みでしか言えない語（訳の表の TYPED）は、
 * その人物と話すときだけ「聞いた」扱い（状態 1）にして一覧に出す。0x4A4C22 の cmp eax,-1 / je を差し替え */
static u8 typed_kw[1024];
/* 一覧（0x904E7C の 1 番から、数は 0x5D9294。負の値は新しく聞いた語）はキャラが聞いた語のうち
   状態 2 のものだけ。打ち込み専用の語は聞いた語に入らないので、状態 2 のものを先頭に足す
   （巻物は最後の 8 行の組から見せるので、新しく聞いた語が後ろに残るように） */
static void __cdecl c_addtyped(void)
{
    int *cnt = (int *)0x5D9294, *list = (int *)0x904E7C, *st = (int *)0x9059C0;
    int add[64], na = 0, i, j, n = *(int *)0x5D9130;
    for (i = 0; i < 1024 && i < n && na < 64; i++) {
        if (!typed_kw[i] || st[i] != 2) continue;
        for (j = 1; j <= *cnt; j++) if (list[j] == i || list[j] == -i) break;
        if (j <= *cnt) continue;
        add[na++] = i;
    }
    if (!na || *cnt + na > 700) return;
    for (j = *cnt; j >= 1; j--) list[j + na] = list[j];
    for (i = 0; i < na; i++) list[1 + i] = add[i];
    *cnt += na;
}
__declspec(naked) static void H_AddTyped(void)
{
    __asm {
        pushad
        call c_addtyped
        popad
        cmp dword ptr ds:[0x5D9294], 0
        push 0x4A4CD3
        ret
    }
}

static void patch_typed(void)
{
    const char *t = hget("TYPED");
    u8 *q, *a = (u8 *)0x4A4C22;
    DWORD old;
    int n = 0;
    if (!t || !check(0x4A4C22, "83F8FF7412")) return;
    while (*t) {
        int v = atoi(t);
        if (v >= 0 && v < 1024) { typed_kw[v] = 1; n++; }
        while (*t && *t != ',') t++;
        if (*t == ',') t++;
    }
    q = (u8 *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    {
        u8 *b = q;
        *q++ = 0x83; *q++ = 0xF8; *q++ = 0xFF;                                   /* cmp eax, -1 */
        *q++ = 0x0F; *q++ = 0x84; *(DWORD *)q = 0x4A4C39 - ((DWORD)q + 4); q += 4; /* je 4A4C39 */
        *q++ = 0x3D; *(DWORD *)q = 1024; q += 4;                                 /* cmp eax, 1024 */
        *q++ = 0x73; *q++ = 0x1E;                                                /* jae cont（7+2+8+2+11） */
        *q++ = 0x80; *q++ = 0xB8; *(DWORD *)q = (DWORD)typed_kw; q += 4; *q++ = 0;  /* cmp byte [eax+typed], 0 */
        *q++ = 0x74; *q++ = 0x15;                                                /* je cont */
        *q++ = 0x83; *q++ = 0x3C; *q++ = 0x85; *(DWORD *)q = 0x9059C0; q += 4; *q++ = 0;   /* cmp dword [eax*4+9059C0], 0 */
        *q++ = 0x75; *q++ = 0x0B;                                                /* jne cont */
        *q++ = 0xC7; *q++ = 0x04; *q++ = 0x85; *(DWORD *)q = 0x9059C0; q += 4; *(DWORD *)q = 1; q += 4; /* mov dword [..], 1 */
        *q++ = 0xE9; *(DWORD *)q = 0x4A4C27 - ((DWORD)q + 4); q += 4;              /* cont: jmp 4A4C27 */
        VirtualProtect(a, 5, PAGE_EXECUTE_READWRITE, &old);
        a[0] = 0xE9; *(DWORD *)(a + 1) = (DWORD)b - (0x4A4C22 + 5);
        VirtualProtect(a, 5, old, &old);
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    if (check(0x4A4CCC, "833D94925D0000")) write_jmp(0x4A4CCC, (void *)H_AddTyped);
    logp("打ち込み専用の語 %d%c", n, 10);
}

static void install(void)
{
    char *p;
    char exe[MAX_PATH];
    GetModuleFileNameA(NULL, exe, MAX_PATH);
    p = strrchr(exe, '\\');
    if (!p || _stricmp(p + 1, "deep6.exe")) return;   /* 本体のときだけ */
    *p = 0;
    strcpy(gamedir, exe);
    sprintf(jpdir, "%s\\WWJP", gamedir);
#ifdef WWJP_TEST
    if (GetEnvironmentVariableA("WWJP_LOG", NULL, 0)) {
        char lp[MAX_PATH];
        sprintf(lp, "%s\\log.txt", jpdir);
        logf = fopen(lp, "w");
    }
#endif
    /* deep6.exe（GOG 版）か確かめる */
    if (!check(0x424C94, "5351565755") || !check(0x424DCC, "53515657") ||
        !check(0x424E0C, "5156575583EC08") || !check(0x424E6C, "5156575583EC10") ||
        !check(0x424BA0, "5351565783EC04")) {
        logp("deep6.exe の版が違うので何もしない\n");
        return;
    }
    real_CreateFileA = (CreateFileA_t)GetProcAddress(GetModuleHandleA("kernel32.dll"), "CreateFileA");
    GetTempPathA(MAX_PATH, tmpdir);
    strcat(tmpdir, "WWJP");
    CreateDirectoryA(tmpdir, NULL);
    load_table();
#ifdef WWJP_TEST
    if (GetEnvironmentVariableA("WWJP_TEST", NULL, 0)) {
        test_init();
        real_GAKS = (GAKS_t)GetProcAddress(GetModuleHandleA("user32.dll"), "GetAsyncKeyState");
        set_iat(0xD8032C, (void *)T_GetAsyncKeyState, (void *)real_GAKS);
        if (GetEnvironmentVariableA("WWJP_WINDOW", NULL, 0)) {
            hook_ddraw_d3d();
            ddraw_iat("user32.dll", "GetActiveWindow", (void *)T_GetActiveWindow);
            ddraw_iat("user32.dll", "IsIconic", (void *)T_IsIconic);
            patch_locks();
        }
    }
    if (GetEnvironmentVariableA("WWJP_NOHOOK", NULL, 0)) return;
#endif
    set_iat(0xD801EC, (void *)J_CreateFileA, (void *)real_CreateFileA);
    set_iat(0xD805A4, (void *)J_CreateFileA, (void *)real_CreateFileA);
    write_jmp(0x424C94, (void *)H_DrawString);
    write_jmp(0x424DCC, (void *)H_Width);
    write_jmp(0x424E0C, (void *)H_Fit);
    write_jmp(0x424E6C, (void *)H_FitR);
    patch_breaks();
    patch_typed();
    /* 人物との会話で、話した言葉の英数字以外を空白にする処理（0x4A6678）が日本語の字を消すので、
       「a〜z なら残す」の上限 0x7A を 0xFF にして 0x80 以上の字も残す */
    if (check(0x4A66C3, "80FD7A")) {
        DWORD old;
        VirtualProtect((void *)0x4A66C5, 1, PAGE_EXECUTE_READWRITE, &old);
        *(u8 *)0x4A66C5 = 0xFF;
        VirtualProtect((void *)0x4A66C5, 1, old, &old);
    }
#ifdef WWJP_TEST
    if (logf && GetEnvironmentVariableA("WWJP_FMT", NULL, 0) && check(0x5AB186, "53515283EC04")) write_jmp(0x5AB186, (void *)H_Sprintf);
#endif
    if (check(0x4A6550, "535152565755")) write_jmp(0x4A6550, (void *)H_FindKw);
    if (check(0x5ED456, "53697220456C676172")) patch_gm_names();   /* "Sir Elgar" */
    logp("準備できた\n");
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        install();
    }
    return TRUE;
}
