"""原本から英文を全部取り出して translation/en/<組>.tsv に書く。
1 行: 鍵<TAB>英文（\\n などは書かない。文中のタブ・改行は無い前提で検査する）
組: ui(D6STRING) / narr(TEXTPAK) / npc(NPCDATA の台詞) / npckw(話題の語) / npcvar /
    gm(GMDATA の台詞) / gmkw / names(固定長の名前)
"""
import sys, os, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from wwfmt import *

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'translation', 'en')

def dec(b):
    return b.decode('cp1252')

def main():
    os.makedirs(OUT, exist_ok=True)
    groups = collections.OrderedDict((k, []) for k in
        ['ui', 'narr', 'npc', 'npckw', 'npcvar', 'gm', 'gmkw', 'gmvar', 'names'])
    d = open(ORIG + '/D6STRING.DAT', 'rb').read()
    for id_, s in read_d6string(d):
        groups['ui'].append(('S:%d' % id_, s))
    d = open(ORIG + '/TEXTPAK.000', 'rb').read()
    for id_, s in read_textpak(d):
        groups['narr'].append(('T:%d' % id_, s))
    for fn, pre, g2, g3, g0 in [('NPCDATA.PAK', 'N', 'npc', 'npckw', 'npcvar'),
                                ('GMDATA.PAK', 'G', 'gm', 'gmkw', 'gmvar')]:
        d = open(ORIG + '/' + fn, 'rb').read()
        N, E, P = read_pak(d)
        for i, s in vartab_strings(P[0]):
            groups[g0].append(('%s0:%d' % (pre, i), s))
        for n in range(N):
            t = read_strtab(P[(2, n)])
            for i, s in enumerate(t or []):
                groups[g2].append(('%s2:%d:%d' % (pre, n, i), s))
            if pre == 'N':
                t = read_strtab(P[(3, n)])
                for i, s in enumerate(t or []):
                    groups[g3].append(('%s3:%d:%d' % (pre, n, i), s))
    for fn in RECS:
        d = open(ORIG + '/' + fn, 'rb').read()
        for k, s, fl in read_recs(fn, d):
            groups['names'].append((k, s))
    tot = 0
    for g, rows in groups.items():
        rows = [(k, s) for k, s in rows if s.strip()]
        for k, s in rows:
            assert b'\t' not in s and b'\n' not in s and b'\r' not in s, (k, s)
        with open(os.path.join(OUT, g + '.tsv'), 'w', encoding='utf-8', newline='\n') as f:
            for k, s in rows:
                f.write(k + '\t' + dec(s) + '\n')
        uniq = len(set(s for _, s in rows))
        ch = sum(len(s) for s in set(s for _, s in rows))
        tot += ch
        print('%-7s %6d 件  重複なし %6d 件  %7d 字' % (g, len(rows), uniq, ch))
    print('合計（組ごとの重複なし）', tot, '字')

if __name__ == '__main__':
    main()
