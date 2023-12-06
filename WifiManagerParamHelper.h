/*************************************************************************************************************
*
* Title			    : FreeAtHome_AthomPG01V2
* Description:      : Implements the Busch-Jeager / ABB Free@Home API for Athom PG01 Version 2 Socket.
* Version		    : v 0.7
* Last updated      : 2023.12.06
* Target		    : Athom Smart Plug PG01 v2
* Author            : Roeland Kluit
* Web               : https://github.com/roelandkluit/Fah_AthomPG01V2
* License           : GPL-3.0 license
*
**************************************************************************************************************/

#pragma once

#include <vector>
#include <cmath>
#include <EEPROM.h>

struct ParamEntry
{
    const char* id;
    const char* label;
    int max_len;
    const char* customHtml;
};

class WifiManagerParamHelper
{
public:
    WifiManagerParamHelper(WiFiManager& wm) : wm_(wm) {}

    void Init(uint16_t preamble, const ParamEntry* entries, size_t entries_len)
    {
        uint16_t read_buffer = 0;

        _data_size = 0;
        for (size_t i = 0; i < entries_len; i++)
        {
            _data_size += entries[i].max_len;
        }

        // Could predetermine, actual size, but this is simpler.
        // This logic should only dirty the EEPROM when new parameters are added or fist use.
        EEPROM.begin(HEADER_SIZE + _data_size);

        // Presize to avoid needing to relocate members.
        parameters_.clear();
        parameters_.reserve(entries_len);

        wm_.setParamsPage(true);
        wm_.setSaveParamsCallback(std::bind(&WifiManagerParamHelper::OnParamCallback, this));

        bool valid = EEPROM.get(0, read_buffer) == preamble;
        uint16_t eeprom_length = (valid) ? EEPROM.get(2, read_buffer) : 0;

        uint16_t current_size = HEADER_SIZE;

        for (size_t i = 0; i < entries_len; i++)
        {
            if (eeprom_length >= current_size + entries[i].max_len)
            {
                const char* loaded_data = reinterpret_cast<const char*>(EEPROM.getConstDataPtr() + current_size);
                parameters_.emplace_back(entries[i].id, entries[i].label, loaded_data, entries[i].max_len, entries[i].customHtml);
                //Serial.println(String("Loading: ") + entries[i].id + ": " + loaded_data);
            }
            else
            {
                EEPROM.write(current_size, 0);
                parameters_.emplace_back(entries[i].id, entries[i].label, "", entries[i].max_len, entries[i].customHtml);
                //Serial.println(String("Creating: ") + entries[i].id);
            }
            current_size += entries[i].max_len;
            wm_.addParameter(&parameters_.back());
        }
        EEPROM.put(2, current_size);
        EEPROM.put(0, preamble);
        // This is smart enough to only write if the values have been modified.
        EEPROM.end();
    }

    const char* GetSetting(size_t idx) {
        if (idx < parameters_.size())
        {
            return parameters_[idx].getValue();
        }
        return nullptr;
    }

    void setSetting(size_t idx, const char* value, const int &length)
    {
        if (idx < parameters_.size())
        {
            parameters_[idx].setValue(value, length);
            OnParamCallback();
        }
    }


private:
    static constexpr size_t HEADER_SIZE = sizeof(uint16_t) * 2;
    size_t _data_size = 0;
    WiFiManager& wm_;
    std::vector<WiFiManagerParameter> parameters_;

    void OnParamCallback()
    {
        EEPROM.begin(HEADER_SIZE + _data_size);
        uint16_t current_size = HEADER_SIZE;
        for (const auto& param : parameters_)
        {
            // Do this check to avoid dirtying EEPROM buffer if nothing changed.
            //if ((strlen(param.getCustomHTML()) > 0) && (strstr(param.getCustomHTML(), "password") != NULL) && (strcmp(param.getValue(), "****") == 0))
            //{
                //Password not changed
                //Serial.println("PWDNC");
            //}
            //else
            if (strncmp(reinterpret_cast<const char*>(EEPROM.getConstDataPtr() + current_size), param.getValue(), param.getValueLength()) != 0)
            {
                strncpy(reinterpret_cast<char*>(EEPROM.getDataPtr() + current_size), param.getValue(), param.getValueLength());
                //Serial.println(String("Updating: ") + param.getID());
            }
            current_size += param.getValueLength();
        }
        EEPROM.end();
    }
};