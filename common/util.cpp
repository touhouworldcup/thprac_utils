#include <Windows.h>
#include <stdlib.h>
#include "util.h"
#include "window.h"

static void* _str_cvt_buffer(size_t size)
{
    static size_t bufferSize = 512;
    static void* bufferPtr = nullptr;
    if (!bufferPtr) {
        bufferPtr = malloc(bufferSize);
    }
    if (bufferSize < size) {
        for (; bufferSize < size; bufferSize *= 2)
            ;
        if (bufferPtr) {
            free(bufferPtr);
        }
        bufferPtr = malloc(size);
    }
    return bufferPtr;
}
std::string utf16_to_utf8(const wchar_t* utf16)
{
    int utf8Length = WideCharToMultiByte(CP_UTF8, 0, utf16, -1, nullptr, 0, NULL, NULL);
    char* utf8 = (char*)_str_cvt_buffer(utf8Length);
    WideCharToMultiByte(CP_UTF8, 0, utf16, -1, utf8, utf8Length, NULL, NULL);
    return std::string(utf8);
}
std::wstring utf8_to_utf16(const char* utf8)
{
    int utf16Length = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    wchar_t* utf16 = (wchar_t*)_str_cvt_buffer(utf16Length);
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, utf16, utf16Length);
    return std::wstring(utf16);
}

const char* OpenFileDialog(const wchar_t* filter) {
    OPENFILENAMEW ofn = {};
    wchar_t fn[MAX_PATH] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = fn;
    ofn.nMaxFile = sizeof(fn);
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    ofn.lpstrFilter = filter;
    ofn.hwndOwner = GuiGetWindow();
    if (!GetOpenFileNameW(&ofn))
        return NULL;
    return _strdup(utf16_to_utf8(fn).c_str());
}
