#include "file_misc.h"
#include <vector>
#include <ShlObj.h>


std::wstring AToW(__in LPCSTR lpszSrc, __in int iSrcCount, __in CodePage codePage)
{
    if (NULL == lpszSrc)
        return std::wstring();

    std::wstring strDest;
    LPCWSTR lpsz = AToW(lpszSrc, iSrcCount, strDest, codePage);
    if (NULL != lpsz)
        return strDest;
    else
        return std::wstring();
}

std::wstring AToW(__in const std::string &strSrc, __in CodePage codePage)
{
    if (strSrc.empty())
        return std::wstring();

    std::wstring strDest;
    LPCWSTR lpsz = AToW(strSrc.c_str(), strSrc.length(), strDest, codePage);
    if (NULL != lpsz)
        return strDest;
    else
        return std::wstring();
}

LPCWSTR AToW(__in LPCSTR lpszSrc,
    __in const int iSrcCount,
    __out std::wstring& strDest,
    __in CodePage codePage)
{
    if (NULL == lpszSrc)
        return NULL;

    wchar_t szBuf[512] = { 0 };
    int iConvert = ::MultiByteToWideChar(codePage.dwCodePage, 0, lpszSrc, iSrcCount, szBuf, _countof(szBuf) - 1);
    if (iConvert > 0)
    {
        szBuf[iConvert] = 0;
        strDest = szBuf;
        return strDest.c_str();
    }
    else
    {
        // 为了运行速度，牺牲一点点空间了
        int iDestCount = (iSrcCount == -1 ? strlen(lpszSrc) : iSrcCount) + 1;
        wchar_t *lpszBuff = new wchar_t[iDestCount + 1];
        int iConvert = ::MultiByteToWideChar(codePage.dwCodePage, 0, lpszSrc, iSrcCount, lpszBuff, iDestCount);
        lpszBuff[iConvert] = 0;
        strDest = iConvert > 0 ? lpszBuff : L"";

        delete[] lpszBuff;
        return strDest.c_str();
    }

    return NULL;
}


std::string WToA(__in LPCWSTR lpszSrc, __in const int iSrcCount, __in CodePage codePage)
{
    if (NULL == lpszSrc)
        return std::string();

    std::string str;
    LPCSTR lpsz = WToA(lpszSrc, iSrcCount, str, codePage);
    if (NULL != lpsz)
        return str;
    else
        return std::string();
}

std::string WToA(__in const std::wstring &strSrc, __in CodePage codePage)
{
    if (strSrc.empty())
        return std::string();

    std::string str;
    LPCSTR lpsz = WToA(strSrc.c_str(), strSrc.length(), str, codePage);
    if (NULL != lpsz)
        return str;
    else
        return std::string();
}

LPCSTR WToA(__in LPCWSTR lpszSrc,
    __in const int iSrcCount,
    __out std::string& strDest,
    __in CodePage codePage)
{
    if (NULL == lpszSrc || 0 == iSrcCount)
        return NULL;

    char szBuff[512] = { 0 };
    int iConvert = ::WideCharToMultiByte(codePage.dwCodePage, 0, lpszSrc, iSrcCount, szBuff, _countof(szBuff) - 1, NULL, FALSE);
    if (iConvert > 0)
    {
        szBuff[iConvert] = 0;
        strDest = szBuff;
        return strDest.c_str();
    }
    else if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
        int iDestCount = ::WideCharToMultiByte(codePage.dwCodePage, 0, lpszSrc, iSrcCount, 0, 0, 0, 0);
        iDestCount += 1;

        char *lpszBuff = new char[iDestCount];
        int iConvert = ::WideCharToMultiByte(codePage.dwCodePage, 0, lpszSrc, iSrcCount, lpszBuff, iDestCount, NULL, FALSE);
        lpszBuff[iConvert] = 0;
        strDest = iConvert > 0 ? lpszBuff : "";

        delete[] lpszBuff;
        return strDest.c_str();
    }

    return NULL;
}

bool FileMisc::GetFileTime(LPCWSTR lpszFileName, LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime, LPFILETIME lpLastWriteTime)
{
    HANDLE hFile = ::CreateFile(lpszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        ::GetFileTime(hFile, lpCreationTime, lpLastAccessTime, lpLastWriteTime);
        ::CloseHandle(hFile);
        return true;
    }
    return false;
}

bool FileMisc::SetFileTime(LPCWSTR lpszFileName, LPFILETIME lpCreationTime, LPFILETIME lpLastAccessTime, LPFILETIME lpLastWriteTime)
{
    HANDLE hFile = ::CreateFile(lpszFileName, FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        ::SetFileTime(hFile, lpCreationTime, lpLastAccessTime, lpLastWriteTime);
        ::CloseHandle(hFile);
        return true;
    }
    return false;
}

bool FileMisc::IsFileExist(LPCWSTR lpszFile, bool bIncludeDirectory)
{
    // 之所以自己写一个，是因为使用shell api的PathFileExist总是会出问题
    WIN32_FIND_DATA FindData;
    HANDLE hFindFile = ::FindFirstFile(lpszFile, &FindData);
    if (hFindFile != INVALID_HANDLE_VALUE)
    {
        ::FindClose(hFindFile);
        return (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 || bIncludeDirectory;
    }
    return false;
}

bool FileMisc::IsDirectory(LPCWSTR lpszPath)
{
    std::wstring sb = lpszPath;
    if (sb.length() && lpszPath[sb.length() - 1] == '\\')
    sb.resize(sb.length() - 1);

    WIN32_FIND_DATA FindData;
    HANDLE hFindFile = ::FindFirstFile(sb.c_str(), &FindData);
    // 这个方法比GetFileAttributes好，因为Xp中GetFileAttributes需要touch文件，而FindFirstFile只需要读目录信息，看文件是否存在
    if (hFindFile != INVALID_HANDLE_VALUE)
    {
        ::FindClose(hFindFile);
        return (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    return false;
}

std::wstring NormalizePath(LPCWSTR lpszDir)
{
    std::wstring strDir = lpszDir;
    std::vector<std::wstring> vecParts;

    size_t iLast = 0;
    size_t iCur = 0;
    while (*lpszDir != L'\0')
    {
        if (*lpszDir == L'\\' || *lpszDir == L'/')
        {
            if (iLast != iCur)
            {
                vecParts.push_back(strDir.substr(iLast, iCur - iLast));
            }

            ++lpszDir;
            ++iCur;
            iLast = iCur;
            continue;
        }

        if (*lpszDir == L':')
        {
            if (iLast != iCur)
            {
                std::wstring strPart = strDir.substr(iLast, iCur - iLast);
                strPart.append(L":");
                vecParts.push_back(strPart);
            }

            ++lpszDir;
            ++iCur;
            iLast = iCur;
            continue;
        }

        if (*lpszDir == L'.')
        {
            if (*(lpszDir + 1) != L'\0' && (*(lpszDir + 1) == L'\\' || *(lpszDir + 1) == L'/'))
            {
                ++lpszDir;
                ++iCur;
                iLast = iCur;
            }
            else if (*(lpszDir + 1) != L'\0' && *(lpszDir + 1) == L'.')
            {
                if (*(lpszDir + 2) != L'\0' && (*(lpszDir + 2) == L'\\' || *(lpszDir + 2) == L'/'))
                {
                    if (vecParts.size() > 0)
                    {
                        vecParts.pop_back();
                    }

                    lpszDir += 3;
                    iCur += 3;
                    iLast = iCur;
                    continue;
                }
            }

            ++lpszDir;
            ++iCur;
            continue;
        }

        ++lpszDir;
        ++iCur;
    }

    if (iCur != iLast)
    {
        vecParts.push_back(strDir.substr(iLast, iCur - iLast));
    }

    strDir.clear();
    for (std::vector<std::wstring>::iterator iter = vecParts.begin(); iter != vecParts.end(); ++iter)
    {
        strDir.append(*iter);
        strDir.append(L"\\");
    }
    return strDir;
}

bool FileMisc::CreateDirectory(LPCWSTR lpszDir)
{
    // 如果已存在就不需要创建了
    if (IsFileExist(lpszDir, true))
        return true;

    std::wstring strPath = NormalizePath(lpszDir);
    //std::wstring strPath = lpszDir;
    if (strPath.length() < 2)
        return false;

    size_t nPos = std::wstring::npos;
    if (strPath[0] == '\\' && strPath[1] == '\\')
    {
        // \\dtbuild\abc
        nPos = 2;
    }
    else if ((_wcsnicmp(strPath.c_str() + 1, L":\\", 2) == 0)
            && (strPath[0] >= 'a' && strPath[0] <= 'z' || strPath[0] >= 'A' && strPath[0] <= 'Z'))
    {
        // C:\windows\system32
        nPos = 3;
    }
    else
    {
        // a\b\c
        nPos = 0;
    }

    for (; ;)
    {
        nPos = strPath.find('\\', nPos);
        if (nPos == std::wstring::npos)
        {
            if (!::CreateDirectory(strPath.c_str(), NULL) && ERROR_ALREADY_EXISTS != GetLastError())
                return false;
            else
                break;
        }

        strPath[nPos] = 0;
        if (!::CreateDirectory(strPath.c_str(), NULL) && ERROR_ALREADY_EXISTS != ::GetLastError())
            return false;
        strPath[nPos] = '\\';
        nPos++;
    }

    return true;
}

void FileMisc::EnumDirectory(LPCWSTR lpszEnumStr, EnumResultList& result, DWORD dwEnumFlag)
{
    WIN32_FIND_DATAW wd;
    HANDLE hFile = ::FindFirstFileW(lpszEnumStr, &wd);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (wcscmp(wd.cFileName, L".") && wcscmp(wd.cFileName, L".."))
            {
                if ((wd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && dwEnumFlag & ED_FILE || (wd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 && dwEnumFlag & ED_DIRECTORY)
                {
                    result.push_back(wd);
                }
            }
        }while(::FindNextFileW(hFile, &wd));

        ::FindClose(hFile);
    }
}

bool FileMisc::RemoveDirectory(LPCWSTR lpszDir)
{
    //std::wstring strPath = NormalizePath(lpszDir);
    std::wstring strPath = lpszDir;
    EnumResultList lsFiles;
    EnumDirectory((strPath + L"\\*").c_str(), lsFiles);

    for (EnumResultList::iterator it = lsFiles.begin(); it != lsFiles.end(); it++)
    {
        std::wstring strFile = strPath + L"\\" + it->cFileName;
        WCHAR lpszShortName[MAX_PATH] = { 0 };

        int iShortNameLen = GetShortPathName(strFile.c_str(), lpszShortName, MAX_PATH);
        if (iShortNameLen > 0 && iShortNameLen < MAX_PATH)
            strFile = lpszShortName;

        if (it->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (!RemoveDirectory(strFile.c_str()))
                return false;
        }
        else
        {
            ::DeleteFile(strFile.c_str());
        }
    }

    return ::RemoveDirectory(lpszDir) != FALSE;
}

std::wstring FileMisc::GetSpecialPath(int nFolder)
{
    LPITEMIDLIST pidl = NULL;
    WCHAR sz[1024] = { 0 };
    if (S_OK == ::SHGetFolderLocation(NULL, nFolder, NULL, NULL, &pidl))
    {
        ::SHGetPathFromIDList(pidl, sz);
        ::ILFree(pidl);
    }
    else
    {
        ::SHGetFolderPath(NULL, nFolder, NULL, 0, sz);
    }
    return sz;
}


UINT FileMisc::GetInvalidCharPosInTitle(const std::wstring& wstr)
{
        return wstr.find_first_of(L"/\\*?\"<>|:");
}

LPSTR FileMisc::GetFileContent(LPCWSTR lpszFilePath, DWORD *pdwSize)
{
    HANDLE hFile = ::CreateFile(lpszFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD dwSize = ::GetFileSize(hFile, NULL);
        if (dwSize >= 1024 * 1024 * 100)
        {
                ::CloseHandle(hFile);
                return NULL;    // 如果文件大于100M了，那么就失败吧，一下开那么多内存也不是玩的。
        }

        char *pData = new char[dwSize + 10];
        if (NULL == pData)
        {
                ::CloseHandle(hFile);
                return NULL;
        }

        ::ReadFile(hFile, pData, dwSize, &dwSize, NULL);
        ::CloseHandle(hFile);
        pData[dwSize] = 0;
        if (pdwSize)
                *pdwSize = dwSize;
        return pData;
    }
    return NULL;
}

bool FileMisc::SetFileContent(LPCWSTR lpszFilePath, const void *lpData, int iSize)
{
    HANDLE hFile = ::CreateFile(lpszFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD dwBytesWritten;
        ::WriteFile(hFile, lpData, iSize > 0 ? iSize : strlen((char*)lpData), &dwBytesWritten, NULL);
        ::CloseHandle(hFile);
        return true;
    }
    return false;
}
