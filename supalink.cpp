// Copyright 2011 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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

static void Fallback(const char* msg = 0)
{
    if (msg)
    {
        fprintf(stdout, "supalink failed (%s), trying to fallback to standard link.\n", msg);
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
    };
    string cmd;
    for (size_t i = 0; i < sizeof(searchFor) / sizeof(searchFor[0]); ++i)
    {
        string linkexe = searchFor[i];
        string::size_type at = origCmd.find(linkexe, 0);
        if (at == string::npos)
            continue;
        cmd = origCmd.replace(at, linkexe.size(), "link.exe.supalink_orig.exe ");
    }
    if (cmd == "")
        Fatal("Couldn't find link.exe (or similar) in command line");
    fprintf(stdout, "supalink running '%s'\n", cmd.c_str());
    fflush(stdout);
    if (!CreateProcess(NULL, (LPSTR)cmd.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &startupInfo, &processInfo))
        Fatal("CreateProcess");
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
