"""dist/WizardsWarriors_JP.zip を作る（dll/dinput.dll と dist/WWJP/wwjp.tsv を使う。先に build_text.py を走らせる）"""
import os, zipfile, hashlib
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
os.chdir(ROOT)
README = '日本語化MOD_はじめにお読みください.txt'
FILES = [('dll/dinput.dll', 'dinput.dll'),
         ('dist/WWJP/wwjp.tsv', 'WWJP/wwjp.tsv')]
zp = 'dist/WizardsWarriors_JP.zip'
if b'WWJP_TEST' in open('dll/dinput.dll', 'rb').read():
    raise SystemExit('dll/dinput.dll が試験版です。dll/build.bat（引数なし）で配布版を作ってください')
txt = open('dist_src/はじめにお読みください.txt', encoding='utf-8').read().replace('\r\n', '\n').replace('\n', '\r\n')
with zipfile.ZipFile(zp, 'w', zipfile.ZIP_DEFLATED) as z:
    for src, dst in FILES:
        z.write(src, dst)
    z.writestr(README, txt.encode('utf-8'))
for src, dst in FILES:
    print(dst, os.path.getsize(src), hashlib.md5(open(src, 'rb').read()).hexdigest())
print(zp, os.path.getsize(zp))
