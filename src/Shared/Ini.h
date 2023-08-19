//Small Ini class to make handling settings a bit less painful, see Ini.cpp for details

#pragma once

#include <string>
#include <vector>

typedef struct ini_t ini_t;

class Ini
{
    private:
        std::wstring m_WFileName;
        ini_t* m_IniPtr;

    public:
        Ini(const std::wstring& filename, bool replace_contents = false);
        Ini(const Ini&) = delete;
        ~Ini();

        bool Save();
        bool Save(const std::wstring& filename);

        std::string ReadString(const char* section, const char* key, const char* default_value = "") const;
        int ReadInt(const char* section, const char* key, int default_value = -1) const;
        bool ReadBool(const char* section, const char* key, bool default_value = false) const;
        void WriteString(const char* section, const char* key, const char* value);
        void WriteInt(const char* section, const char* key, int value);
        void WriteBool(const char* section, const char* key, bool value);

        bool SectionExists(const char* section) const;
        bool KeyExists(const char* section, const char* key) const;
        void RemoveSection(const char* section);
        void RemoveKey(const char* section, const char* key);

        std::vector<std::string> GetSectionList();
};