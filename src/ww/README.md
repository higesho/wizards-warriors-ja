# ウィザーズ＆ウォリアーズ 日本語化 ソース

GOG 版 Wizards & Warriors 用の `dinput.dll`（ゲームのフォルダに置くと文を日本語にして描く）と、訳の表を作る道具です。

- `dll/` … dinput.dll の本体。先に `tools/mkjfont.py` で字の形の表 `dll/jfont.h` を作り、`build.bat`（Visual Studio の x86 の cl）で組み立てます。`build.bat test` は試験用の仕組み（`WWJP_TEST`）を入れた `dll/test/dinput.dll` を作ります（配布しません）
- `tools/wwfmt.py` / `tools/extract.py` … ゲームのデータを読み、英文を `translation/en/` に抜き出す
- `tools/check_ja.py` … 訳を検査する
- `tools/build_text.py` … 訳から `dist/WWJP/wwjp.tsv` を作る
- `tools/make_dist.py` … 配布用の zip を作る

必要なもの: Python 3、Visual Studio。
ゲーム本体（GOG 版）、訳のファイル、美咲フォント・東雲フォント（BDF、`font/` に置く）は各自で用意してください。
