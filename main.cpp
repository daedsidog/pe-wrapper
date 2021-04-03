#include <filesystem>
#include <sstream>
#include <iostream>
#include <string>
#include <list>
#include <windows.h>

#define BUFSIZE      1024
#define AUX_APPENDIX "_core"
#define BIN_APPENDIX ".exe"

int main(int argc, char **argv) {
    STARTUPINFO           start_info = {sizeof(start_info)};
    PROCESS_INFORMATION   process_info;
    std::filesystem::path p(argv[0]);
    std::string exe_name = std::string(p.stem().string()) + AUX_APPENDIX;
    std::string dll_name = exe_name;
#ifdef DEBUG
    dll_name = dll_name + "_d";
#endif
    dll_name = dll_name + ".dll";
    exe_name = exe_name + BIN_APPENDIX;

    // Format passed down to loader to pass to process.
    std::stringstream args;
    args << exe_name << " " << dll_name << " -args";
    for (int i = 1; i < argc; ++i) {
        args << " " << argv[i];
    }

    // Launch the target process.
    // &std::string[0] is equivalent to char*.
    if (CreateProcess(nullptr, &(args.str())[0], nullptr, nullptr, FALSE,
                      CREATE_SUSPENDED, nullptr, nullptr, &start_info,
                      &process_info)) {
        LPVOID page = VirtualAllocEx(process_info.hProcess, nullptr, BUFSIZE,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (page == nullptr) {
            std::cerr << "VirtualAllocEx error: " << GetLastError()
                      << std::endl;
            return -1;
        }
        if (WriteProcessMemory(process_info.hProcess, page, dll_name.c_str(),
                               dll_name.length(), nullptr) == 0) {
            std::cerr << "WriteProcessMemory error: " << GetLastError()
                      << std::endl;
            return -1;
        }
        HANDLE thread = CreateRemoteThread(process_info.hProcess, nullptr, 0,
                                           (LPTHREAD_START_ROUTINE)LoadLibraryA,
                                           page, 0, nullptr);
        if (thread == nullptr) {
            std::cerr << "CreateRemoteThread error: " << GetLastError()
                      << std::endl;
            return -1;
        }
        if (WaitForSingleObject(thread, INFINITE) == WAIT_FAILED) {
            std::cerr << "WaitForSingleObject error: " << GetLastError()
                      << std::endl;
            return -1;
        }
        CloseHandle(thread);
        std::cout << "Loaded " << dll_name << std::endl;
        if (ResumeThread(process_info.hThread) == -1) {
            std::cerr << "ResumeThread error: " << GetLastError() << std::endl;
            return -1;
        }
        CloseHandle(process_info.hProcess);
        if (!VirtualFreeEx(process_info.hProcess, page, BUFSIZE, MEM_RELEASE)) {
            std::cerr << "VirtualFreeEx error: " << GetLastError() << std::endl;
            return -1;
        }
        return 0;
    }
    std::cerr << "CreateProcess error: " << GetLastError() << std::endl;
    return -1;
}
