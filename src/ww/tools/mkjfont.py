"""日本語の字の表 dll/jfont.h を作る。
- jf8  … 美咲ゴシック第2（8x8、Unicode の BDF を JIS 区点に並べ直す）
- jf12 / jf14 / jf16 … 東雲ゴシック（パブリックドメイン、JIS の BDF）
1 字は 1 行 16 ビット（左詰め）× 高さ。94×94 を EUC の (b1-0xA1)*94+(b2-0xA1) で引く。
禁則の表（行頭に来てはいけない字・行末に来てはいけない字）も EUC で作る。"""
import os
HERE = os.path.dirname(os.path.abspath(__file__))
FONT = os.path.join(HERE, '..', 'font')

def load_bdf(path, size, unicode=False):
    g = {}
    lines = open(path, encoding='latin1').read().split('\n')
    fbb = None
    i = 0
    while i < len(lines):
        l = lines[i]
        if l.startswith('FONTBOUNDINGBOX'):
            fbb = list(map(int, l.split()[1:]))
        if l.startswith('STARTCHAR'):
            enc = None; bbx = None; bm = []
            while not lines[i].startswith('ENDCHAR'):
                t = lines[i]
                if t.startswith('ENCODING'): enc = int(t.split()[1])
                elif t.startswith('BBX'): bbx = list(map(int, t.split()[1:]))
                elif t.startswith('BITMAP'):
                    i += 1
                    while not lines[i].startswith('ENDCHAR'):
                        bm.append(int(lines[i], 16)); i += 1
                    break
                i += 1
            w, h, ox, oy = bbx
            nbits = ((w + 7) // 8) * 8
            rows = [0] * size
            top = (fbb[1] + fbb[3]) - (h + oy)
            for r, v in enumerate(bm):
                y = top + r
                if 0 <= y < size:
                    rows[y] = ((v << (16 - nbits)) >> max(ox, 0)) & 0xFFFF
            if unicode:
                if enc is None or enc < 0x80:
                    i += 1; continue
                try:
                    e = chr(enc).encode('euc_jp')
                except UnicodeEncodeError:
                    i += 1; continue
                if len(e) != 2:
                    i += 1; continue
                code = ((e[0] - 0x80) << 8) | (e[1] - 0x80)
            else:
                code = enc
            g[code] = rows
        i += 1
    return g

def emit(name, g, size, out):
    out.append('static const unsigned short %s[94*94][%d] = {' % (name, size))
    n = 0
    for ku in range(94):
        row = []
        for ten in range(94):
            code = ((ku + 0x21) << 8) | (ten + 0x21)
            rows = g.get(code)
            if rows: n += 1
            row.append('{' + ','.join(str(x) for x in (rows or [0] * size)) + '}')
        out.append(','.join(row) + ',')
    out.append('};')
    return n

def euc_list(s):
    r = []
    for ch in s:
        b = ch.encode('euc_jp')
        if len(b) == 2: r.append(b[0] << 8 | b[1])
    return r

NOSTART = '、。，．・：；？！゛゜ヽヾゝゞ々ー）］｝」』〉》】〕’”…‥ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ'
NOEND = '（［｛「『〈《【〔‘“'

def main():
    out = ['/* mkjfont.py で作った。美咲ゴシック第2（jf8）、東雲フォント 0.9.11p1（パブリックドメイン、jf12/14/16）から */']
    g = load_bdf(os.path.join(FONT, 'misaki_gothic_2nd.bdf'), 8, unicode=True)
    print(8, emit('jf8', g, 8, out), 'glyphs')
    for size in (12, 14, 16):
        g = load_bdf(os.path.join(FONT, 'shnmk%d.bdf' % size), size)
        print(size, emit('jf%d' % size, g, size, out), 'glyphs')
    out.append('static const unsigned short nostart[] = {' + ','.join(map(str, euc_list(NOSTART))) + ',0};')
    out.append('static const unsigned short noend[] = {' + ','.join(map(str, euc_list(NOEND))) + ',0};')
    open(os.path.join(HERE, '..', 'dll', 'jfont.h'), 'w', encoding='utf-8').write('\n'.join(out) + '\n')

if __name__ == '__main__':
    main()
