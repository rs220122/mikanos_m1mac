# MikanOS for M1 mac
M1 macで[ゼロからのOS自作入門](https://zero.osdev.jp/)を写経するレポジトリ

## 実行環境
```
Apple M1
macOS Ventura 13.5
Homebrew clang version 17.0.6
Target: arm64-apple-darwin22.6.0
```

## setup
m1 mac用の環境を[こちら](https://qiita.com/yamoridon/items/4905765cc6e4f320c9b5)からセットアップする。  
`env_setup.sh`の中の`OSBOOK_DIR`と`EDK2_DIR`を適宜自分の環境に合わせる。


## ソースコードのビルド
- kernelのビルド
```
make kernel
```

-  ローダーのビルド

```
make loader
```

- QEMUシミュレータの起動
```
make qemu
```

# 参考
https://qiita.com/yamoridon/items/4905765cc6e4f320c9b5
