"""Wizards & Warriors の文章ファイルを読む・組み直す。
鍵（key）の形:
  S:<id>            D6STRING.DAT
  T:<id>            TEXTPAK.000
  N0:<i>            NPCDATA.PAK 全体の変数（文字列のもの）
  N2:<npc>:<i>      NPCDATA.PAK 人物ごとの台詞
  N3:<npc>:<i>      NPCDATA.PAK 人物ごとの話題の語
  G0:<i> / G2:<g>:<i> / G3:<g>:<i>   GMDATA.PAK（同じ作り）
  ITEM:<rec>:<off>  など固定長の記録（名前の欄）
"""
import struct, os

ORIG = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'orig')

def cstr(d, o):
    return d[o:d.index(0, o)]

# ---------- D6STRING.DAT ----------
def read_d6string(d):
    n = struct.unpack_from('<I', d, 0)[0]
    out = []
    for i in range(n):
        id_, o = struct.unpack_from('<II', d, 4 + 8 * i)
        out.append((id_, cstr(d, o)))
    return out

def build_d6string(ents):
    n = len(ents)
    head = bytearray(struct.pack('<I', n))
    body = bytearray()
    base = 4 + 8 * n
    for id_, s in ents:
        head += struct.pack('<II', id_, base + len(body))
        body += s + b'\0'
    return bytes(head + body)

# ---------- TEXTPAK.000 ----------
def read_textpak(d):
    n = struct.unpack_from('<I', d, 0)[0]
    out = []
    for i in range(n):
        id_, o, ln = struct.unpack_from('<III', d, 4 + 12 * i)
        s = d[o:o + ln]
        if s.endswith(bytes(1)):
            s = s[:-1]           # 長さは終わりの NUL まで数えている
        out.append((id_, s))
    return out

def build_textpak(ents):
    n = len(ents)
    head = bytearray(struct.pack('<I', n))
    body = bytearray()
    base = 4 + 12 * n
    for id_, s in ents:
        head += struct.pack('<III', id_, base + len(body), len(s) + 1)
        body += s + b'\0'
    return bytes(head + body)

# ---------- 文字列の表（数・位置・文字列） ----------
def read_strtab(r):
    if not r:
        return None
    c = struct.unpack_from('<I', r, 0)[0]
    offs = struct.unpack_from('<%dI' % c, r, 4)
    return [cstr(r, o) for o in offs]

def build_strtab(strs):
    c = len(strs)
    head = bytearray(struct.pack('<I', c))
    body = bytearray()
    base = 4 + 4 * c
    for s in strs:
        head += struct.pack('<I', base + len(body))
        body += s + b'\0'
    return bytes(head + body)

# ---------- 型つきの変数の表（数・[値4 型1]…・文字列）。型 3 は値が文字列の位置 ----------
def read_vartab(r):
    c = struct.unpack_from('<I', r, 0)[0]
    ents = []
    for i in range(c):
        v = struct.unpack_from('<I', r, 4 + 5 * i)[0]
        t = r[8 + 5 * i]
        ents.append([t, v])
    return c, ents

def vartab_strings(r):
    # 値は全部文字列の位置（型の値は別の区分）。表の形でなければ空
    c, ents = read_vartab(r)
    if not ents or min(v for t, v in ents) != 4 + 5 * c or max(v for t, v in ents) >= len(r):
        return []
    return [(i, cstr(r, v)) for i, (t, v) in enumerate(ents)]

def build_vartab(r, newstr):
    """型 3 の文字列だけ置き換え、残りはそのまま。newstr: {番号: bytes}"""
    c, ents = read_vartab(r)
    sidx = [i for i, (t, v) in enumerate(ents) if t == 3]
    if not sidx:
        return r
    first = min(ents[i][1] for i in sidx)
    assert first == 4 + 5 * c
    head = bytearray(r[:first])
    body = bytearray()
    for i in sidx:
        s = newstr.get(i, cstr(r, ents[i][1]))
        struct.pack_into('<I', head, 4 + 5 * i, len(head) + len(body))
        body += s + b'\0'
    return bytes(head + body)

# ---------- NPCDATA.PAK / GMDATA.PAK ----------
def read_pak(d):
    off0, sz0 = struct.unpack_from('<II', d, 0)
    nent = off0 // 8
    E = [struct.unpack_from('<II', d, 8 * i) for i in range(nent)]
    N = (nent - 1) // 3
    parts = {0: d[off0:off0 + sz0]}
    for p in (1, 2, 3):
        for n in range(N):
            o, s = E[1 + (p - 1) * N + n]
            parts[(p, n)] = d[o:o + s] if s else b''
    return N, E, parts

def build_pak(N, E, parts):
    """部分を置き換えて組み直す。並びは元の位置の順を守る。"""
    order = sorted(range(len(E)), key=lambda i: (E[i][0] if E[i][1] else 1 << 40, i))
    nent = len(E)
    out = bytearray(8 * nent)
    newE = [(0, 0)] * nent
    for i in order:
        o, s = E[i]
        if i == 0:
            blob = parts[0]
        else:
            p = (i - 1) // N + 1; n = (i - 1) % N
            blob = parts[(p, n)]
        if not s and not blob:
            newE[i] = (o, 0)  # 空はそのまま（元の値を残す）
            continue
        newE[i] = (len(out), len(blob))
        out += blob
    for i, (o, s) in enumerate(newE):
        struct.pack_into('<II', out, 8 * i, o, s)
    return bytes(out)

# ---------- 固定長の記録 ----------
RECS = {
    # ファイル: (記録の大きさ, [(欄の位置, 欄の長さ, 鍵の名)])
    'D6ITEM.DAT': (284, [(0, 22, 'ITEM'), (22, 22, 'ITEMU')]),
    'D6MONS.DAT': (340, [(0, 20, 'MONS')]),
    'D6NPC.DAT': (36, [(0, 24, 'NPC')]),
    'D6PROP.DAT': (56, [(0, 24, 'PROP')]),
    'D6TRLIST.DAT': (60, [(0, 20, 'TRL')]),
}

def read_recs(name, d):
    rs, fields = RECS[name]
    n = struct.unpack_from('<I', d, 0)[0]
    out = []
    for r in range(1, n + 1):
        for fo, fl, tag in fields:
            o = rs * r + fo
            s = d[o:o + fl].split(b'\0')[0]
            out.append(('%s:%d' % (tag, r), s, fl))
    return out
