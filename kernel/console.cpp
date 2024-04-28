#include "console.hpp"

#include <cstring>
#include "font.hpp"

Console::Console(PixelWriter& writer, 
    const PixelColor& fg_color, const PixelColor& bg_color) 
    : writer_{writer}, fg_color_{fg_color}, bg_color_{bg_color},
    buffer_{}, cursor_row_{0}, cursor_column_{0} {
}
// buffer_は、ヌル文字で初期化しておく

void Console::PutString(const char* s) {
    // 与えられた文字列を先頭から1文字ずつ描画する。
    while (*s) {
        if (*s == '\n') {
            Newline();
        } else if (cursor_column_ < kColumns - 1) {
            WriteAscii(writer_, 8*cursor_column_, 16*cursor_row_, *s,  fg_color_);
            buffer_[cursor_row_][cursor_column_] = *s;
            ++cursor_column_;
        }
        ++s;
    }
}


void Console::Newline() {
    // 改行文字に対する処理
    // buffer_の次の行に移動する。
    cursor_column_ = 0;
    if (cursor_row_ < kRows - 1) {
        ++cursor_row_;
    } else {
        // 一旦、バックグラウンドカラーで塗りつぶす
        for (int y = 0; y < 16 * kRows; y++) {
            for (int x = 0; x < 8 * kColumns; x++) {
                writer_.Write(x, y, bg_color_);
            }
        }
        for(int row = 0; row < kRows -1; row++) {
            // memcpy(dest, src, size)
            // 2行目の内容を1行目に持ってくる
            memcpy(buffer_[row], buffer_[row+1], kColumns+1);
            WriteString(writer_, 0, 16 * row, buffer_[row], fg_color_);
        }
        memset(buffer_[kRows-1], 0, kColumns + 1);
    }
}