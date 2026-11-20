"""訳を集めて dist/WWJP/wwjp.tsv（EUC-JP、鍵<TAB>訳）を作る。--install でゲームのフォルダにも置く。
- translation/ja/b*.tsv（部分の .pN.tsv は除く）
- batches/dedup.tsv で同じ英文の鍵にも同じ訳
- 話題の語: N0:i（全体の辞書）と N3:n:i（人物ごとの表）は kw_ja.tsv から
"""
import os, sys, re, glob, shutil
HERE = os.path.dirname(os.path.abspath(__file__))
W = os.path.join(HERE, '..')
TR = os.path.join(W, 'translation')
GAME = os.path.join(W, '..')

def tsv(p):
    for l in open(p, encoding='utf-8'):
        l = l.rstrip('\r\n')
        if not l or l.startswith('#') or '\t' not in l:
            continue
        yield l.split('\t', 1)

def main():
    ja = {}
    for p in sorted(glob.glob(os.path.join(TR, 'ja', 'b*.tsv'))):
        if re.search(r'\.p\d+\.tsv$', p):
            continue
        for k, v in tsv(p):
            ja[k] = v
    fix = os.path.join(TR, 'ja_fix.tsv')
    if os.path.exists(fix):
        for k, v in tsv(fix):
            ja[k] = v
    for k, r in tsv(os.path.join(TR, 'batches', 'dedup.tsv')):
        if r in ja and k not in ja:
            ja[k] = ja[r]
    kw = dict(tsv(os.path.join(TR, 'kw_ja.tsv')))
    for k, e in tsv(os.path.join(TR, 'en', 'npcvar.tsv')):
        ja[k] = kw[e]
    for k, e in tsv(os.path.join(TR, 'en', 'npckw.tsv')):
        ja[k] = kw[e]
    gv = os.path.join(TR, 'en', 'gmvar.tsv')
    if os.path.exists(gv):
        for k, e in tsv(gv):
            ja[k] = kw.get(e, e)
    for k, v in tsv(os.path.join(TR, 'gm_names.tsv')):
        ja[k] = v
    # 英語で打った話題の語 → 日本語の語（小文字の FNV-1a を鍵に）
    def fnv(t):
        h = 2166136261
        for c in t.lower().encode('cp1252'):
            h = ((h ^ c) * 16777619) & 0xFFFFFFFF
        return h
    nk = 0
    for f in ('kw_ja.tsv', 'kw_extra.tsv'):
        for e, j in tsv(os.path.join(TR, f)):
            k = 'KWH:%08x' % fnv(e)
            if k not in ja:
                ja[k] = j; nk += 1
    print('英語の話題の語', nk)
    # 打ち込みでしか言えない語（会話の文に ~語~ として一度も出ない語）の全体の辞書の番号。
    # 人物と話すとき、その人物の表にあれば一覧に出す。日本語が同じ語は一つだけ
    import re as _re
    hl = set()
    for f in ('npc.tsv', 'gm.tsv', 'narr.tsv'):
        for k, e in tsv(os.path.join(TR, 'en', f)):
            for m in _re.findall(r'~([^~]*)~', e):
                hl.add(m.lower())
    kwl = {e.lower(): j for e, j in kw.items()}
    for e, j in tsv(os.path.join(TR, 'kw_extra.tsv')):
        kwl.setdefault(e.lower(), j)
    hl_ja = set(kwl[h] for h in hl if h in kwl)
    dic = list(tsv(os.path.join(TR, 'en', 'npcvar.tsv')))
    used = set(); typed = []
    for i, (k, e) in enumerate(dic):
        j = kw[e]
        if e in ('Hail', 'Goodbye', 'Trade', '$BLANK$') or e.lower() in hl or j in hl_ja or j in used:
            continue
        used.add(j); typed.append(i)
    ja['TYPED'] = ','.join(map(str, typed))
    print('打ち込み専用の語', len(typed))
    # 英文の全鍵に対して、どれだけ訳があるか
    total = 0; have = 0
    for p in glob.glob(os.path.join(TR, 'en', '*.tsv')):
        for k, e in tsv(p):
            total += 1
            if k in ja: have += 1
    rp = os.path.join(TR, 'replace.tsv')
    if os.path.exists(rp):
        pairs = list(tsv(rp))
        for k in ja:
            for a, b in pairs:
                ja[k] = ja[k].replace(a, b)
    out = []
    bad = 0
    for k in sorted(ja):
        v = ja[k]
        try:
            v.encode('euc_jp')
        except UnicodeEncodeError as x:
            print('EUC にできない', k, repr(v[x.start:x.end])); bad += 1; continue
        out.append(k + '\t' + v)
    os.makedirs(os.path.join(W, 'dist', 'WWJP'), exist_ok=True)
    dst = os.path.join(W, 'dist', 'WWJP', 'wwjp.tsv')
    with open(dst, 'wb') as f:
        f.write(('\n'.join(out) + '\n').encode('euc_jp'))
    print('訳 %d / %d 件（%.1f%%）、書けない %d → %s' % (have, total, 100.0 * have / total, bad, dst))
    if '--install' in sys.argv:
        os.makedirs(os.path.join(GAME, 'WWJP'), exist_ok=True)
        shutil.copy(dst, os.path.join(GAME, 'WWJP', 'wwjp.tsv'))
        shutil.copy(os.path.join(W, 'dll', 'dinput.dll'), os.path.join(GAME, 'dinput.dll'))
        print('ゲームのフォルダに置いた')

if __name__ == '__main__':
    main()
