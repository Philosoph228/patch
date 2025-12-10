#include "pch.h"
#include "CppUnitTest.h"

#include "../../src/patch.c"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTest1 {
// Path to the patcher executable. If you copy patcher.exe to the test output directory,
// you can leave this as "patcher.exe". Otherwise, set an absolute path.
static const char* kPatcherExe = "C:\\Users\\User\\source\\repos\\patch\\x64\\Release\\patch.exe";

static std::string NormalizeNewlines(std::string s) {
    // convert CRLF -> LF and also normalize lone CR -> LF just in case
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\r') {
            if (i + 1 < s.size() && s[i + 1] == '\n') {
                // skip the '\r', the '\n' will be copied on next iteration or we add it now
                continue;
            } else {
                out.push_back('\n');
            }
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

static std::string GetTempDir() {
    char buf[MAX_PATH];
    DWORD n = GetTempPathA(MAX_PATH, buf);
    if (n == 0 || n >= MAX_PATH)
        throw std::runtime_error("GetTempPathA failed");
    std::ostringstream ss;
    ss << buf << "patcher_tests_" << GetCurrentProcessId() << "_" << GetTickCount64();
    std::string dir = ss.str();
    CreateDirectoryA(dir.c_str(), NULL);
    return dir;
}

static void WriteFileUtf8(const std::string& path, const std::string& content) {
    std::ofstream os(path, std::ios::binary);
    if (!os)
        throw std::runtime_error("failed to open for write: " + path);
    os << content;
    os.close();
}

static std::string ReadFile(const std::string& path) {
    std::ifstream is(path, std::ios::binary);
    if (!is)
        throw std::runtime_error("failed to open for read: " + path);
    std::ostringstream ss;
    ss << is.rdbuf();
    return ss.str();
}

static bool RunPatcherAndWait(const std::string& patchPath, DWORD timeoutMs = 10000) {
    // Build command line: patcher.exe "<patchPath>"
    std::string cmd = std::string("\"") + kPatcherExe + "\" \"" + patchPath + "\"";

    // CreateProcess requires a mutable char buffer
    std::vector<char> cmdMutable(cmd.begin(), cmd.end());
    cmdMutable.push_back('\0');

    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};

    BOOL ok = CreateProcessA(
        NULL,
        cmdMutable.data(),
        NULL, NULL, FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (!ok) {
        return false;
    }

    DWORD wait = WaitForSingleObject(pi.hProcess, timeoutMs);
    if (wait == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode == 0;
}

TEST_CLASS(PatcherTests) {
public:
    TEST_METHOD(Test_SingleHunk) {
        std::string orig = "./tests/data/sample.cpp";
        std::string patch = "./tests/data/single_hunk.patch";
        std::string expected = "./tests/data/expected/sample.cpp";

        // Original file
        WriteFileUtf8(orig,
                      "Line 1\n"
                      "Line 2\n"
                      "Line 3\n"
                      "Line 4\n"
        );

        // Expected after patch
        WriteFileUtf8(expected,
                      "Line 1\n"
                      "Inserted A\n"
                      "Inserted B\n"
                      "Line 3\n"
                      "Line 4\n"
        );

        // Patch: replace lines 2..3 (start_old=2,len_old=2) with 2 inserted + keep Line3
        // Note: +++ header must point to the path of the file to be patched
        std::ostringstream p;
        p << "--- " << orig << "\n";
        p << "+++ " << orig << "\n";
        p << "@@ -2,2 +2,3 @@\n";
        p << "-Line 2\n";
        p << "-Line 3\n";
        p << "+Inserted A\n";
        p << "+Inserted B\n";
        p << "+Line 3\n";

        WriteFileUtf8(patch, p.str());

        // Run patcher
        Assert::IsTrue(RunPatcherAndWait(patch), L"patcher failed");

        // Compare
        std::string actual = ReadFile(orig);
        std::string expect = ReadFile(expected);
        Assert::AreEqual(NormalizeNewlines(expect), NormalizeNewlines(actual), L"Single-hunk result mismatch");
    }

    TEST_METHOD(Test_MultipleHunks_SameFile) {
        std::string tmp = GetTempDir();
        std::string orig = tmp + ".\\tests\\data\\graphbuilder.cpp";
        std::string patch = tmp + ".\\tests\\data\\diff.patch";
        std::string expected = ".\\tests\\data\\expected\\graphbuilder.cpp";

        // Original: 6 lines
        WriteFileUtf8(orig,
                      "Line1\n"
                      "Line2\n"
                      "Line3\n"
                      "Line4\n"
                      "Line5\n"
                      "Line6\n"
        );

        // Expected: make two separate modifications applied correctly
        WriteFileUtf8(expected,
                      "Line1\n"
                      "NewA\n"
                      "NewB\n"
                      "Line3\n"
                      "Line4\n"
                      "Line5_changed\n"
                      "Line6\n"
        );

        // Patch with two hunks:
        // - Hunk1 replaces original line 2 (start_old=2,len_old=1) with two lines
        // - Hunk2 replaces original line 5 (start_old=5,len_old=1) with changed content
        // Note: start_old values are relative to the original file (before any hunks are applied).
        std::ostringstream p;
        p << "--- " << orig << "\n";
        p << "+++ " << orig << "\n";
        p << "@@ -2,1 +2,2 @@\n";
        p << "-Line2\n";
        p << "+NewA\n";
        p << "+NewB\n";
        p << "@@ -5,1 +6,1 @@\n";
        p << "-Line5\n";
        p << "+Line5_changed\n";

        WriteFileUtf8(patch, p.str());

        // Run patcher
        Assert::IsTrue(RunPatcherAndWait(patch), L"patcher failed");

        // Compare
        std::string actual = ReadFile(orig);
        std::string expect = ReadFile(expected);
        Assert::AreEqual(NormalizeNewlines(expect), NormalizeNewlines(actual), L"Multi-hunk result mismatch");
    }

    TEST_METHOD(Test_AddDeleteAcrossHunks) {
        std::string tmp = GetTempDir();
        std::string orig = tmp + "\\test3.txt";
        std::string patch = tmp + "\\test3.patch";
        std::string expected = tmp + "\\test3.expected.txt";

        WriteFileUtf8(orig,
                      "A\n"
                      "B\n"
                      "C\n"
                      "D\n"
                      "E\n"
                      "F\n"
        );

        WriteFileUtf8(expected,
                      "X\n"
                      "Y\n"
                      "A\n"
                      "C\n"
                      "D\n"
                      "Z\n"
        );

        // Two hunks:
        // Hunk1 replaces lines 1..2 with X,Y,A (so -1,2 +1,3)
        // Hunk2 replaces lines 5..6 (original numbering) with Z (so -5,2 +6,1)
        std::ostringstream p;
        p << "--- " << orig << "\n";
        p << "+++ " << orig << "\n";
        p << "@@ -1,2 +1,3 @@\n";
        p << "-A\n";
        p << "-B\n";
        p << "+X\n";
        p << "+Y\n";
        p << "+A\n";
        p << "@@ -5,2 +6,1 @@\n";
        p << "-E\n";
        p << "-F\n";
        p << "+Z\n";

        WriteFileUtf8(patch, p.str());

        Assert::IsTrue(RunPatcherAndWait(patch), L"patcher failed");

        std::string actual = ReadFile(orig);
        std::string expect = ReadFile(expected);
        Assert::AreEqual(NormalizeNewlines(expect), NormalizeNewlines(actual), L"Add/Delete across hunks mismatch");
    }

}; // TEST_CLASS
} // namespace
