#pragma once
#include <Windows.h>
#include <string>

std::string utf16_to_utf8(const wchar_t* utf16);
std::wstring utf8_to_utf16(const char* utf8);
const char* OpenFileDialog(const wchar_t* filter);

struct MappedFile {
    HANDLE fileMap = NULL;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    size_t fileSize = 0;
    void* fileMapView = NULL;

    MappedFile(const wchar_t* fn)
    {
        hFile = CreateFileW(fn, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            return;
        }
        fileSize = GetFileSize(hFile, NULL);
        fileMap = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, fileSize, NULL);
        if (fileMap == NULL) {
            CloseHandle(hFile);
            return;
        }
        fileMapView = MapViewOfFile(fileMap, FILE_MAP_READ, 0, 0, fileSize);
        if (!fileMapView) {
            CloseHandle(hFile);
            CloseHandle(fileMap);
            return;
        }
    }
    ~MappedFile()
    {
        UnmapViewOfFile(fileMapView);
        CloseHandle(fileMap);
        CloseHandle(hFile);
    }
};