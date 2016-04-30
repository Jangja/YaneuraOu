
・大合神(だいごうしん)クジラちゃんsdt4a_wcsc25とは？

大合神(だいごうしん)クジラちゃんsdt4a_wcsc25はやねうら王classic-tceをベースにApery WCSC25 versionの評価関数を読み込んだ思考エンジンです。(R3300程度？)

ちなみに、sdt4aとはshogi denou tournament 4th alphaの略です。

・やねうら王classic-tceとは？

やねうら王classic-tceはやねうら王classicにPonderの処理、思考時間制御(秒読み、フィッシャールール対応)などを追加した思考エンジンです。(R3300程度)

ちなみに、tceとはtime control enabledの略です。

・大合神(だいごうしん)クジラちゃんsdt4a_wcsc25の遊び方

    将棋所かShogiGUIから思考エンジンとして登録して使ってください。
    実行ファイルはWindows 64bit版、CPUはSSE42用にコンパイルされています。(AVX2用は各自お願いします)
    （PGOは各自お願いします）

    以下のファイルを同じフォルダに配置します。

    god_whale_sdt4a.exe      : 大合神(だいごうしん)クジラちゃんsdt4a本体
    YaneuraOu-classic-tce_ja.txt   : これを用いると思考エンジンの設定項目の表示が日本語化される。(無くても良い)
    standard_book.db   : 基本定跡

// 以下、書きかけ。

    KKP_synthesized.bin        : 3駒関係の評価関数で用いるファイル(Apery WCSC25 version のKKP)
    KPP_synthesized.bin        : 3駒関係の評価関数で用いるファイル(Apery WCSC25 version のKPP)
    KK_synthesized.bin         : 3駒関係の評価関数で用いるファイル(Apery WCSC25 version のKK)
        ※　「大樹の枝」の評価関数バイナリ(上記の3ファイル)は、
            http://hiraokatakuya.github.io/apery/のサイトからダウンロードできます。
            
　　　　　　http://hiraokatakuya.github.io/apery/の
            4 Previous Versions
　　　　　　WCSC25 version (for Windows 64bit)
            リンク先からダウンロードできます。
　　　　　　Aperyの開発者様には深く感謝いたします。

    ・入玉宣言勝ちに対応しています。
    ・Ponder(相手番で思考する)に対応しています。
    ・秒読み、フィッシャールールに対応しています。
    ・最小思考時間設定に対応しています。
    ・スレッド数は思考エンジン設定で選べます。
    ・定跡の指し手がランダムに選択されます。
    ・置換表サイズは、思考エンジン設定のところで設定した値に従います。
