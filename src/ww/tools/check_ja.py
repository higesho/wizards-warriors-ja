"""訳の検査: python -I tools/check_ja.py b08 [b09 ...]   （引数なしで全部）
translation/batches/bNN.tsv（英文）と translation/ja/bNN.tsv（訳）を比べる。
誤り（ERR）は必ず直す。注意（WARN）は見直す。"""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
TR = os.path.join(HERE, '..', 'translation')

def tsv(p):
    out = []
    for l in open(p, encoding='utf-8'):
        l = l.rstrip('\n').rstrip('\r')
        if not l or l.startswith('#'):
            continue
        if '\t' not in l:
            out.append((None, l)); continue
        k, v = l.split('\t', 1)
        out.append((k, v))
    return out

KW = {}
for f in ('kw_ja.tsv', 'kw_extra.tsv'):
    for k, v in tsv(os.path.join(TR, f)):
        KW.setdefault(k.lower(), v)
KWJA = set(KW.values())
FMT = re.compile(r'%[-+ #0]*\d*(?:\.\d+)?[sdcxXuif%]')

def fmts(s):
    return FMT.findall(s)

def check(b):
    en = [(k, v) for k, v in tsv(os.path.join(TR, 'batches', b + '.tsv'))]
    jp_path = os.path.join(TR, 'ja', b + '.tsv')
    if not os.path.exists(jp_path):
        print(b, 'ERR 訳のファイルが無い', jp_path); return 1
    ja = tsv(jp_path)
    errs = 0; warns = 0
    def err(k, m):
        nonlocal errs; errs += 1; print('%s ERR %s: %s' % (b, k, m))
    def warn(k, m):
        nonlocal warns; warns += 1; print('%s WARN %s: %s' % (b, k, m))
    jd = {}
    for k, v in ja:
        if k is None:
            err('?', 'タブの無い行: ' + v[:40]); continue
        if k in jd:
            err(k, '鍵が重複'); continue
        jd[k] = v
    ek = set(k for k, _ in en)
    for k in jd:
        if k not in ek:
            err(k, '英文に無い鍵')
    for k, e in en:
        if k not in jd:
            err(k, '訳が無い'); continue
        j = jd[k]
        if not j.strip():
            err(k, '訳が空'); continue
        try:
            j.encode('euc_jp')
        except UnicodeEncodeError as x:
            err(k, 'EUC-JP で書けない字: %r' % j[x.start:x.end])
        if re.search('[｡-ﾟ]', j):
            err(k, '半角カナ')
        if '〜' in j or '～' in j:
            err(k, '波線（〜/～）は使わない')
        if fmts(e) != fmts(j):
            err(k, '差し込みが違う 英%s 訳%s' % (fmts(e), fmts(j)))
        if e.count('$') != j.count('$') and '$BLANK$' not in e:
            err(k, '$ の数が違う')
        if e.count('@') != j.count('@'):
            err(k, '@ の数が違う（英 %d 訳 %d）' % (e.count('@'), j.count('@')))
        kind = k.split(':')[0]
        if kind == 'S':
            if e.count('~') != j.count('~'):
                err(k, '~（改行）の数が違う')
        elif kind in ('T', 'N2', 'G2', 'N0', 'G0'):
            em = re.findall(r'~([^~]*)~', e); jm = re.findall(r'~([^~]*)~', j)
            if e.count('~') != j.count('~') or len(em) != len(jm):
                err(k, '~語~ の数が違う（英 %d 訳 %d）' % (len(em), len(jm)))
            else:
                for a, c in zip(em, jm):
                    want = KW.get(a.lower())
                    if want is None:
                        if c not in KWJA:
                            warn(k, '~%s~ は辞書に無い語（訳 ~%s~）' % (a, c))
                    elif c != want:
                        err(k, '~%s~ は ~%s~ にする（訳は ~%s~）' % (a, want, c))
        if kind in ('MONS', 'NPC', 'PROP', 'TRL'):
            if len(j.encode('euc_jp', 'replace')) > 19:
                err(k, '名前が長い（全角9字まで）: %s' % j)
        if kind in ('ITEM', 'ITEMU'):
            if len(j.encode('euc_jp', 'replace')) > 21:
                err(k, '名前が長い（全角10字まで）: %s' % j)
        if '\t' in j:
            err(k, 'タブがある')
        if e.strip() in ('$BLANK$', '***UNUSED***', '<empty>') and j != e:
            err(k, '訳さずそのまま')
        rest = FMT.sub('', j)
        words = [w for w in re.findall(r'[A-Za-z]{3,}', rest)
                 if w not in ('BLANK', 'UNUSED', 'empty', 'TCP', 'IPX', 'ROM', 'Lbs')]
        if words and e.strip() not in ('$BLANK$', '***UNUSED***', '<empty>'):
            warn(k, '英字が残っている: %s' % ' '.join(words[:4]))
        if '*' in re.sub(r'\*{3}UNUSED\*{3}', '', j):
            warn(k, '半角の * がある（＊にする）')
        if ' ' in FMT.sub('', j).strip() and kind not in ('S',):
            warn(k, '半角の空白がある')
    print('%s: %d 件, 誤り %d, 注意 %d' % (b, len(en), errs, warns))
    return errs

def main():
    bs = sys.argv[1:] or sorted(f[:-4] for f in os.listdir(os.path.join(TR, 'batches')) if re.match(r'b\d+\.tsv$', f))
    tot = 0
    for b in bs:
        tot += check(b)
    sys.exit(1 if tot else 0)

if __name__ == '__main__':
    main()
