// Copyright (c) 2011 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

using namespace std;

// Don't use stderr for errors because VS has large buffers on them, leading
// to confusing error output.
static void Fatal(const char* msg)
{
    fprintf(stdout, "supalink fatal error: %s\n", msg);
    exit(1);
}

static string ErrorMessageToString(DWORD err)
{
    LPTSTR msgBuf = NULL;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&msgBuf, 0, NULL);
    string ret(msgBuf);
    LocalFree(msgBuf);
    return ret;
}


static void Fallback(const char* msg = 0)
{
    if (msg)
    {
        fprintf(stdout, "supalink failed (%s), trying to fallback to standard link.\n", msg);
        fprintf(stdout, "Original command line: %s\n", GetCommandLine());
        fflush(stdout);
    }

    STARTUPINFO startupInfo = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION processInfo;
    DWORD exitCode;

    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    string origCmd(GetCommandLine());

    const char* searchFor[] = {
        "link.exe ",
        "LINK.EXE ",
        "link ",
        "LINK ",
        "link.exe\" ",
        "LINK.EXE\" ",
        "link\" ",
        "LINK\" ",
    };
    string cmd;
    string replaceWith = "link.exe.supalink_orig.exe";
    for (size_t i = 0; i < sizeof(searchFor) / sizeof(searchFor[0]); ++i)
    {
        string linkexe = searchFor[i];
        string::size_type at = origCmd.find(linkexe, 0);
        if (at == string::npos)
            continue;
        if (linkexe[linkexe.size() - 2] == '"')
            replaceWith += "\" ";
        else
            replaceWith += " ";
        cmd = origCmd.replace(at, linkexe.size(), replaceWith);
        break;
    }
    if (cmd == "")
    {
        fprintf(stdout, "Original run '%s'\n", origCmd.c_str());
        Fatal("Couldn't find link.exe (or similar) in command line");
    }
    fprintf(stdout, "supalink running '%s'\n", cmd.c_str());
    fflush(stdout);
    if (!CreateProcess(NULL, (LPSTR)cmd.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &startupInfo, &processInfo))
    {
        string error = ErrorMessageToString(GetLastError());
        Fatal(error.c_str());
    }
    WaitForSingleObject(processInfo.hProcess, INFINITE);
    GetExitCodeProcess(processInfo.hProcess, &exitCode);
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
    exit(exitCode);
}

wstring SlurpFile(const char* path)
{
    FILE* f = fopen(path, "rb, ccs=UNICODE");
    if (!f) Fallback("couldn't read file");
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    wchar_t* data = (wchar_t*)malloc(len);
    fread(data, 1, len, f);
    fclose(f);
    wstring ret(data, len/sizeof(wchar_t));
    free(data);
    return ret;
}

void DumpFile(const char* path, wstring& contents)
{
    FILE* f = fopen(path, "wb, ccs=UTF-16LE");
    if (!f) Fallback("couldn't write file");

    fwrite(contents.c_str(), sizeof(wchar_t), contents.size(), f);
    if (ferror(f)) Fatal("failed during response rewrite");
    fclose(f);
}

// Input command line is assumed to be of the form:
//
// link.exe @C:\src\chromium\src\chrome\Debug\obj\chrome_dll\RSP00003045884740.rsp /NOLOGO /ERRORREPORT:PROMPT
//
// Specifically, we parse & hack the contents of argv[1] and pass the rest
// onwards.
//
// %REALLINK%.supalink_orig.exe
int main(int argc, char** argv)
{
    //fprintf(stderr, "GetCommandLine(): '%s'\n", GetCommandLine());
    //fflush(stderr);

    int rspFileIndex = -1;

    if (argc < 2)
        Fallback("too few commmand line args");

    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] == '@')
        {
            rspFileIndex = i;
            break;
        }
    }

    if (rspFileIndex == -1)
        Fallback("couldn't find a response file in argv");

    wstring rsp = SlurpFile(&argv[rspFileIndex][1]);

    // The first line of this file is all we try to fix. It's a bunch of
    // quoted space separated items. Simplest thing seems to be replacing " "
    // with "\n". So, just slurp the file, replace, spit it out to the same
    // file and continue on our way.

    wstring search = L"\" \"";
    wstring replace = L"\"\r\n\"";
    wstring::size_type next;
    for (next = rsp.find(search); next != string::npos; next = rsp.find(search, next))
    {
        rsp.replace(next, search.length(), replace);
        next += replace.length();
    }

    DumpFile(&argv[rspFileIndex][1], rsp);

    string backupCopy(&argv[rspFileIndex][1]);
    backupCopy += ".copy";
    DumpFile(backupCopy.c_str(), rsp);

    Fallback();
}
