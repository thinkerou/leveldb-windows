#pragma once
#include <list>
#include <windows.h>

typedef std::list<WIN32_FIND_DATAW> EnumResultList;
#define ED_FILE         1
#define ED_DIRECTORY    2

struct CodePage
{
    CodePage() : dwCodePage(CP_UTF8) {}
    CodePage(DWORD c, int) : dwCodePage(c) {}
    ~CodePage() {}

    bool operator == (const CodePage &cp) const
    {
        return dwCodePage == cp.dwCodePage;
    }

    bool operator != (const CodePage &cp) const
    {
        return dwCodePage != cp.dwCodePage;
    }

    DWORD dwCodePage;
};

const int g_iAcp_DONOT_USE_THIS = CP_ACP;
const int g_iUtf8_DONOT_USE_THIS = CP_UTF8;
const int g_iUtf7_DONOT_USE_THIS = CP_UTF7;

#define CP_Utf8     CodePage(g_iUtf8_DONOT_USE_THIS, 0)
#define CP_Utf7     CodePage(g_iUtf7_DONOT_USE_THIS, 0)
#define CP_Ansi     CodePage(g_iAcp_DONOT_USE_THIS, 0)
#define CP_GBK      CodePage(936, 0)

std::wstring AToW(__in LPCSTR lpszSrc, __in int iSrcLen = -1, __in CodePage codePage = CP_Utf8);
std::wstring AToW(__in const std::string &strSrc, __in CodePage codePage = CP_Utf8);
LPCWSTR AToW(__in LPCSTR lpszSrc, __in const int iSrcLen, __out std::wstring& strDest, __in CodePage codePage = CP_Utf8);

std::string WToA(__in LPCWSTR lpszSrc, __in const int iSrcLen = -1, __in CodePage codePage = CP_Utf8);
std::string WToA(__in const std::wstring &strSrc, __in CodePage codePage = CP_Utf8);
LPCSTR WToA(__in LPCWSTR lpszSrc, __in const int iSrcLen, __out std::string& strDest, __in CodePage codePage = CP_Utf8);

namespace FileMisc
{
    bool IsFileExist(LPCWSTR lpszFile, bool bIncludeDirectory = false/* 是否连目录也算 */);
    bool IsDirectory(LPCWSTR lpszPath);
    bool CreateDirectory(LPCWSTR lpszDir);
    bool RemoveDirectory(LPCWSTR lpszDir);
    void EnumDirectory(LPCWSTR lpszEnumStr, EnumResultList& result, DWORD dwEnumFlag = ED_FILE | ED_DIRECTORY);

    std::wstring GetSpecialPath(int nFolder);

    bool GetFileTime(LPCWSTR lpszFileName, LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime, LPFILETIME lpLastWriteTime);
    bool SetFileTime(LPCWSTR lpszFileName, LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime, LPFILETIME lpLastWriteTime);

    UINT GetInvalidCharPosInTitle(const std::wstring &wstr);

    LPSTR GetFileContent(LPCWSTR lpszFilePath, DWORD *pdwSize = NULL);
    bool SetFileContent(LPCWSTR lpszFilePath, const void *lpData, int iSize);
}
