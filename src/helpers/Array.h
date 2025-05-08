//
// Created by A.P.A. Slaa (a.p.a.slaa@projectsource.nl) on 2/21/24.
// Optimized version
//

#ifndef NODE_LXC_ARRAY_H
#define NODE_LXC_ARRAY_H

#include <napi.h>
#include <string>
#include <cstring>
#include <memory>
#include <stdexcept>

class Array {
public:
    /**
     * Convert a Napi::Array to a null-terminated array of C-style strings
     * @param jsArray JavaScript array to convert
     * @param length Output parameter that will contain the length of the array
     * @return Null-terminated array of C-style strings (must be freed with free())
     */
    static char** NapiToCharStarArray(const Napi::Array& jsArray, uint32_t& length) {
        // Handle null/undefined array
        if (jsArray.IsEmpty() || jsArray.IsUndefined() || jsArray.IsNull()) {
            length = 0;
            char** result = new char*[1];
            result[0] = nullptr;
            return result;
        }

        length = jsArray.Length();
        char** argv = new char*[length + 1]; // +1 for NULL terminator

        // Initialize all pointers to nullptr for safe cleanup in case of exception
        std::memset(argv, 0, sizeof(char*) * (length + 1));

        try {
            for (uint32_t i = 0; i < length; ++i) {
                Napi::Value item = jsArray.Get(i);

                // Skip null/undefined items
                if (item.IsNull() || item.IsUndefined()) {
                    argv[i] = new char[1];
                    argv[i][0] = '\0';
                    continue;
                }

                // Convert to string
                std::string str;
                try {
                    str = item.As<Napi::String>();
                } catch (const Napi::Error& e) {
                    // Convert non-string items to string representation
                    str = item.ToString();
                }

                // Allocate and copy string
                argv[i] = new char[str.length() + 1];
                std::strcpy(argv[i], str.c_str());
            }

            argv[length] = nullptr; // NULL terminate the array
            return argv;
        } catch (const std::exception& e) {
            // Clean up any allocated memory before re-throwing
            for (uint32_t i = 0; i < length; ++i) {
                delete[] argv[i];
            }
            delete[] argv;
            throw std::runtime_error(std::string("Failed to convert array: ") + e.what());
        }
    }

    /**
     * Convert a Napi::Array to a null-terminated array of C-style strings
     * @param jsArray JavaScript array to convert
     * @return Null-terminated array of C-style strings (must be freed with free())
     */
    static char** NapiToCharStarArray(const Napi::Array& jsArray) {
        uint32_t length;
        return NapiToCharStarArray(jsArray, length);
    }

    /**
     * Free a null-terminated array of C-style strings
     * @param argv Array to free
     * @param length Length of the array
     */
    static void free(char** argv, uint32_t length) {
        if (!argv) {
            return;
        }

        for (uint32_t i = 0; i < length; ++i) {
            delete[] argv[i];
        }
        delete[] argv;
    }

    /**
     * Free a null-terminated array of C-style strings without knowing its length
     * @param argv Null-terminated array to free
     */
    static void freeNullTerminated(char** argv) {
        if (!argv) {
            return;
        }

        for (int i = 0; argv[i] != nullptr; ++i) {
            delete[] argv[i];
        }
        delete[] argv;
    }
};

#endif // NODE_LXC_ARRAY_H