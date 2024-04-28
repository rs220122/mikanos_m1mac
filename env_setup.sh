
# 環境変数のセットアップ
if [ -z "$OSBOOK_DIR" ]
then 
    export OSBOOK_DIR=~/Documents/osbook
fi 

if [ -z "$EDK2_DIR" ]
then
    export EDK2_DIR=~/Documents/osbook/edk2
fi 

# devenvディレクトリは、$OSBOOK_DIRの直下にあることを想定する。
# mikanos用のカスタムライブラリを事前にインクルードしておくために必要
# kernelをコンパイルするときに必要になる。
source $OSBOOK_DIR/devenv/buildenv.sh

# edk2ディレクトリにシンボリックリンクを作成する。
# ローダーはこれを毎回ビルドして使用する。
if [ ! -L "$EDK2_DIR/MikanLoaderPkg" ]
then 
    echo "create alias"
    echo "$PWD/MikanLoaderPkg"
    ln -s $PWD/MikanLoaderPkg $EDK2_DIR
fi