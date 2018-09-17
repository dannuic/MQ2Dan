#pragma once

#include <string>

#include "../MQ2Plugin.h"

namespace MQ2DanIPC {
    // specify more than just global to make it harder to have duplicated naming
    const char szMemPrefix[] = "Global\\MQ2DanFileMap_";
    const char szMutexPrefix[] = "Global\\MQ2DanMutex_";

    template<class T> class FileMapping {
    public:
        class locking_ptr {
        public:
            locking_ptr(T* ptr, HANDLE lock);
            ~locking_ptr();

            locking_ptr(locking_ptr&& ptr) : _ptr(ptr._ptr), _mutex(ptr._mutex) {
                ptr._ptr = nullptr;
                ptr._mutex = nullptr;
            }

            T* operator->() { return _ptr; }
            T* const operator->() const { return _ptr; }
            operator T* () { return _ptr; }

        private:
            T* _ptr;
            const HANDLE _mutex;

            // prevent copy/assignment 
            locking_ptr(locking_ptr const& ptr) {}
            locking_ptr& operator=(locking_ptr const& ptr) { return *this; }
        };

    protected:
        // names for the memory map and semaphores
        const std::string memName;
        const std::string mutexName;

        // handle for the file mapping itself
        HANDLE memHandle;
        T* memBuf;

        // handle for the mutex
        HANDLE mutexHandle;

    public:
        FileMapping(const std::string sName);
        ~FileMapping();

        T* operator->() { return locking_ptr(memBuf, mutexHandle); }
        T* const operator->() const { return locking_ptr(memBuf, mutexHandle); }
        operator T* () { return locking_ptr(memBuf, mutexHandle); }
    };

    // GENERIC TEMPLATES NEED TO BE PORTABLE
    template<class T>
    inline FileMapping<T>::locking_ptr::locking_ptr(T * ptr, HANDLE lock) :
        _ptr(ptr),
        _mutex(lock) {
        DWORD waitResult = WaitForSingleObject(
            _mutex,
            INFINITE
        );

        if (WAIT_OBJECT_0 != waitResult) {
            DebugSpewAlways("[ERROR] MQ2DanIPC: Failed to get valid mutex with error %d", GetLastError());
            throw new std::runtime_error("Could not create locking pointer.");
        }
    }

    template<class T>
    inline FileMapping<T>::locking_ptr::~locking_ptr() {
        // TODO: This can potentially fail, do we need to handle that error?
        ReleaseMutex(_mutex);
    }

    template<class T>
    FileMapping<T>::FileMapping(const std::string sName) :
        memName(szMemPrefix + sName),
        mutexName(szMutexPrefix + sName) {
        // Get the mutex handle
        mutexHandle = CreateMutex(
            NULL,
            FALSE,
            mutexName.c_str()
        );

        // don't worry about ERROR_ALREADY_EXISTS because we get the handle we care about with all the perms we care about
        // do care about access denied though, we need to open it in that case
        if (ERROR_ACCESS_DENIED == GetLastError()) {
            mutexHandle = OpenMutex(
                MUTEX_ALL_ACCESS,
                FALSE,
                mutexName.c_str()
            );
        }

        if (NULL == mutexHandle) {
            DebugSpewAlways("[ERROR] MQ2DanIPC: Failed to create/open mutex handle with name %s with error %d", sName.c_str(), GetLastError());
            throw new std::runtime_error("Failed to create mutex.");
        }

        // Create or open our local shared memory instance
        memHandle = CreateFileMapping(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE,
            0,
            sizeof(T),
            memName.c_str()
        );

        if (NULL == memHandle) {
            DebugSpewAlways("[ERROR] MQ2DanIPC: Failed to create/open memory handle with name %s with error %d", sName.c_str(), GetLastError());
            CloseHandle(mutexHandle);
            throw new std::runtime_error("Failed to create memory.");
        } else if (ERROR_ALREADY_EXISTS == GetLastError()) {
            // this is expected to happen for memory spaces on other clients that we need to access (Create is more like CreateOrOpen),
            // so let's go ahead and use it if it already exists, but we need to check size
            DebugSpewAlways("[WARN] MQ2DanIPC: Named memory for %s already exists.", sName.c_str());

            FILE_STANDARD_INFO fileInfo;
            if (GetFileInformationByHandleEx(
                memHandle,
                FileStandardInfo,
                &fileInfo,
                sizeof(FILE_STANDARD_INFO)
            ) != 0) {
                DebugSpewAlways("[WARN] MQ2DanIPC: Existing structure size: %d vs expected structure size: %d.", fileInfo.AllocationSize.LowPart, sizeof(T));
                if (fileInfo.AllocationSize.LowPart != sizeof(T)) {
                    CloseHandle(memHandle);
                    CloseHandle(mutexHandle);
                    throw new std::runtime_error("Unmatched structure sizes.");
                }
            }
        }

        memBuf = (T*)MapViewOfFile(
            memHandle,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            sizeof(T)
        );

        if (NULL == memBuf) {
            DebugSpewAlways("[ERROR] MQ2DanIPC: Failed to open handle for %s with error %d.", sName.c_str(), GetLastError());
            CloseHandle(memHandle);
            CloseHandle(mutexHandle);
            throw new std::runtime_error("Failed to open memory handle.");
        }
    }

    template<class T>
    MQ2DanIPC::FileMapping<T>::~FileMapping() {
        UnmapViewOfFile(memBuf);
        CloseHandle(memHandle);
        CloseHandle(mutexHandle);
    }
}