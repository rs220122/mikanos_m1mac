#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "console.hpp"
#include "font.hpp"
#include "frame_buffer_config.hpp"
#include "graphics.hpp"

// cstdintは、uintX_tという整数型(Xはビット数)
// short,
// intは、ビット数が決まっていないため、ビット数を固定したいときに用いる。

void* operator new(size_t size, void* buf) { return buf; }
void operator delete(void* obj) noexcept {}

// デスクトップカラーの設定
const PixelColor kDesktopBGColor{45, 118, 237};
const PixelColor kDesktopFGColor{255, 255, 255};

// マウスカーソルの設定
const int kMouseCursorWidth = 15;
const int kMouseCursorHeight = 24;
const char mouse_cursor_shape[kMouseCursorHeight][kMouseCursorWidth + 1] = {
    "@              ",  //
    "@@             ",  //
    "@.@            ",  //
    "@..@           ",  //
    "@...@          ",  //
    "@....@         ",  //
    "@.....@        ",  //
    "@......@       ",  //
    "@.......@      ",  //
    "@........@     ",  //
    "@.........@    ",  //
    "@..........@   ",  //
    "@...........@  ",  //
    "@............@ ",  //
    "@......@@@@@@@@",  //
    "@......@       ",  //
    "@....@@.@      ",  //
    "@...@ @.@      ",  //
    "@..@   @.@     ",  //
    "@.@    @.@     ",  //
    "@@      @.@    ",  //
    "@       @.@    ",  //
    "         @.@   ",  //
    "         @@@   ",  //
};

char console_buf[sizeof(Console)];
Console* console;
char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* pixel_writer;

int printk(const char* format, ...) {
    // 可変超引数を取る
    va_list ap;
    int result;
    char s[1024];

    va_start(ap, format);
    // sprintfの親戚で、可変超引数の代わりにva_list型の変数を受け取ることができる。
    // printkの内部で1024バイトの固定配列をとっているため、最大文字数に制限がありますが、それ以外はprintfと同様
    result = vsprintf(s, format, ap);
    va_end(ap);

    console->PutString(s);
    return result;
}

extern "C" void KernelMain(const FrameBufferConfig& frame_buffer_config) {
    // 一般的なnew演算子は、new <クラス名> なので、引数を取らない
    // 一般のnewは指定したクラスのインスタンスをヒープ領域(関数の実行が終了しても破棄されない。)に生成する。
    // mallocとnewの違いは、クラスのコンストラクタが呼び出されるかどうか。
    // ヒープ領域に確保するのは、OSがメモリ管理できる様になって初めて可能
    // メモリ管理ができない時でもクラスのインスタンスを作りたいときに登場するのが、"配置new"
    // 配置newでは、メモリ領域の確保を行わない。
    // 配置newを使うには、<new>を淫クルーづするか自分で定義する必要がある。
    // C++では、operatorキーワードを使うことで演算子を定義できる。
    switch (frame_buffer_config.pixel_format) {
        case kPixelRGBResv8BitPerColor:
            pixel_writer = new (pixel_writer_buf)
                RGBResv8BitPerColorPixelWriter{frame_buffer_config};
            break;
        case kPixelBGRResv8BitPerColor:
            pixel_writer = new (pixel_writer_buf)
                BGRResv8BitPerColorPixelWriter{frame_buffer_config};
            break;
    }

    const int kFrameWidth = frame_buffer_config.horizontal_resolution;
    const int kFrameHeight = frame_buffer_config.vertical_resolution;

    // デスクトップ本体内部を塗りつぶす
    FillRectangle(*pixel_writer, {0, 0}, {kFrameWidth, kFrameHeight - 50},
                  kDesktopBGColor);
    FillRectangle(*pixel_writer, {0, kFrameHeight - 50}, {kFrameWidth, 50},
                  {1, 8, 17});
    FillRectangle(*pixel_writer, {0, kFrameHeight - 50}, {kFrameWidth / 5, 50},
                  {80, 80, 80});
    DrawRectangle(*pixel_writer, {10, kFrameHeight - 40}, {30, 30},
                  {160, 160, 160});

    console = new (console_buf)
        Console{*pixel_writer, kDesktopFGColor, kDesktopBGColor};
    printk("Welcome to MikanOS!\n");

    for (int dy = 0; dy < kMouseCursorHeight; dy++) {
        for (int dx = 0; dx < kMouseCursorWidth; dx++) {
            if (mouse_cursor_shape[dy][dx] == '@') {
                pixel_writer->Write(200 + dx, 100 + dy, {0, 0, 0});
            } else if (mouse_cursor_shape[dy][dx] == '.') {
                pixel_writer->Write(200 + dx, 100 + dy, {255, 255, 255});
            }
        }
    }
    // WriteAscii(*pixel_writer, 50, 50, 'A', {0, 0, 0});
    // WriteAscii(*pixel_writer, 58, 50, 'A', {0, 0, 0});
    while (1) __asm__("hlt");
}
