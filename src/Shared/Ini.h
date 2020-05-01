//Small Ini class to make handling settings a bit less painful, see Ini.cpp for details

#pragma once

#include <string>

typedef struct ini_t ini_t;

class Ini
{
    private:
        std::wstring m_WFileName;
        ini_t* m_IniPtr;

    public:
        Ini(const std::wstring& filename);
        Ini(const Ini&) = delete;
        ~Ini();

        bool Save();
        bool Save(const std::wstring& filename);

        std::string ReadString(const char* section, const char* key, const char* default_value = "");
        int ReadInt(const char* section, const char* key, int default_value = -1);
        bool ReadBool(const char* section, const char* key, bool default_value = false);
        void WriteString(const char* section, const char* key, const char* value);
        void WriteInt(const char* section, const char* key, int value);
        void WriteBool(const char* section, const char* key, bool value);

        bool SectionExists(const char* section);
        bool KeyExists(const char* section, const char* key);
        void RemoveSection(const char* section);
        void RemoveKey(const char* section, const char* key);
};