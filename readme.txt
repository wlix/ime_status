【ソフト名】 IME Status for Win10
【著作権者】 Copyright (C) 2020 wlix
【種　　別】 TTBase プラグイン
【動作環境】 Windows 10 32-bit/64-bit
【開発環境】 Visual Studio Community 2019


【概要】
ウィンドウ移動時にウィンドウを半透明にします。
TTBase もしくは 互換プログラムのプラグインとして動作します。


【依存関係】
VC++16 の ランタイムが必要です。
お持ちでない場合は、Microsoft社 のページからダウンロードをお願い致します。

[Microsoft Visual C++ 2015, 2017, 2019 再頒布可能パッケージ]
(https://support.microsoft.com/ja-jp/help/2977003/the-latest-supported-visual-c-downloads)
（デッドリンクの際はご容赦下さい。）


【インストール】
本体となるプログラムによりますが、peach、TTBaseCpp等の場合はtransparent_on_move_x64.dllおよびtransparent_on_move_x64.iniを本体のインストールフォルダ以下の場所に配置してください。
hako の場合は、32bit/64bitプラグインに対応しているため、全てのファイルを本体のインストールフォルダ以下の場所に配置してください。


【アンインストール】
DLLファイル 及び .iniファイルを削除してください。
常駐型のプラグインのため、本体を一旦終了させる必要があります。


【使用方法】
透明度および移動時に透明化させたくないプログラムはiniファイルを編集して指定してください。


【ライセンス】
NYSL Version 0.9982 です。このプラグラムを使用したことによる一切の影響は関知しません。
http://www.kmonos.net/nysl/


【謝辞】
TTBase を生み出された K2 さん、hakoやTTBase Plugin Template を 公開されている tapetums さん、
peach を 公開されている U さん、TTBaseCpp の作者さん、TransparentOnMove を公開されたconさんを始め、
数多くのプラグインを作られたそれぞれの作者さんたちに深い敬意と感謝を表します。
https://osdn.jp/projects/ttbase/
https://github.com/tapetums
http://seesaawiki.jp/ttbase/d/
http://white2.php.xdomain.jp/?page_id=27
http://ttbase.coresv.com/uploader/


【更新履歴】
2020.09.21 v0.0.0.4
 ・32bit版が正しく動作していない点を修正

2020.09.20 v0.0.0.3
 ・32bit版と64bit版で設定を共有するように対応（現状どちらを優先するかは決定していないため、あまり現行ではあまり意味はない）

2020.09.15 v0.0.0.2
 ・Windowsイベントフックでは正常に動作しなくなるため、通常のWindowsフックに変更

2020.09.11 v0.0.0.1
 ・初版作成


【文責】
wlix
http://wlix.wp.xdomain.jp/
