//Small Ini class to make handling settings a bit less painful
//Win32 API used in particular since it's non-destructive to order and comments already present as well as to avoid additional depedencies
//Even though it's pretty crusty

//This turned out to be not a terribly great idea in hindsight.
//It works fine, but in anticipation of more dynamic tree/object like stuff in the config files, it would make sense to switch to XML or JSON in the future

#pragma once

#include <string>
#define NOMINMAX
#include <windows.h>

class Ini
{
    private:
        std::string m_filename;
    public:
        Ini(const std::string& filename) : m_filename(filename) { }

        bool Exists()
        {
            char buffer[3]; //Function still writes 2 NULs and maybe part of the real output, so give it something to work with
            ::GetPrivateProfileStringA(nullptr, nullptr, "", buffer, 3, m_filename.c_str());    

            return (GetLastError() != 0x2); //"File not found"
        }

        std::string ReadString(const char* category, const char* key, const char* default_value = "")
        {
            int buffer_size = 1024;
            DWORD read_length;
            char* buffer;

            while (true)
            {
                buffer = new char[buffer_size];

                read_length = ::GetPrivateProfileStringA(category, key, default_value, buffer, buffer_size, m_filename.c_str());

                if (read_length == buffer_size - 1) //Yes, this will do another loop if the buffer was perfectly sized, nothing can be done about that
                {
                    delete[] buffer;
                    buffer_size += 1024;
                }
                else
                {
                    break;
                }
            }

            std::string value(buffer);
            delete[] buffer;

            return value;
        }

        void WriteString(const char* category, const char* key, const char* value)
        {
            ::WritePrivateProfileStringA(category, key, value, m_filename.c_str());
        }

        int ReadInt(const char* category, const char* key, int default_value = -1)
        {
            std::string str_value = ReadString(category, key, "NAN");

            if (str_value != "NAN")
                return atoi(str_value.c_str());
            else
                return default_value;
        }

        bool ReadBool(const char* category, const char* key, bool default_value = false)
        {
            std::string str_value = ReadString(category, key, "NAN");

			//Allow these, because why not
			if (str_value == "true")
				return true;
			else if (str_value == "false")
				return false;

            if (str_value != "NAN")
                return (atoi(str_value.c_str()) != 0);
            else
                return default_value;
        }

        void WriteInt(const char* category, const char* key, int value)
        {
            WriteString(category, key, std::to_string(value).c_str());
        }

        void WriteBool(const char* category, const char* key, bool value)
        {
            if (value)
                WriteString(category, key, "true");
            else
                WriteString(category, key, "false");
        }
};