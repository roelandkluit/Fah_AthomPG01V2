#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include "WifiManagerParamHelper.h"

#ifdef ESP32
#include <dummy.h>
#include <WiFi.h>
#include <WiFiClient.h>
//#include <WebServer.h>
#else
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
//#include <ESP8266WebServer.h>
#else
#error "Platform not supported"
#endif
#endif

#define RELAY_CONTACT_GPIO12 12
#define BUTTON_GPIO05 5

#include "FreeAtHomeESPapi.h"
#include "FahESPDevice.h"
#include "FahESPSwitchDevice.h"
#include "ButtonManager.h"

String Text = "Init";
String header = "";
FreeAtHomeESPapi freeAtHomeESPapi;
ButtonManager DeviceButton(BUTTON_GPIO05, true);
FahESPSwitchDevice* espDev = NULL;
String deviceID;

WiFiManager wm;
WifiManagerParamHelper wm_helper(wm);

constexpr size_t CUSTOM_FIELD_LEN = 40;
constexpr std::array<ParamEntry, 3> PARAMS = { {
    {
      "Ap",
      "Sap",
      CUSTOM_FIELD_LEN
    },
    {
      "uGd",
      "User Guid",
      CUSTOM_FIELD_LEN
    },
    {
      "uPd",
      "Password",
      CUSTOM_FIELD_LEN
    }
} };

void FahCallBack(FAHESPAPI_EVENT Event, uint64_t FAHID, const char* ptrChannel, const char* ptrDataPoint, void* ptrValue)
{
    if (Event == FAHESPAPI_EVENT::FAHESPAPI_ON_DEVICE_EVENT)
    {
        String t;
        bool val = ((bool)ptrValue);
        freeAtHomeESPapi.U64toStringDev(FAHID, t);

        if (val)
        {
            digitalWrite(RELAY_CONTACT_GPIO12, HIGH);
        }
        else
        {
            digitalWrite(RELAY_CONTACT_GPIO12, LOW);
        }

        Text = "Dev: " + t + ", Event: " + ptrChannel + "-" + ptrDataPoint + " = " + val;        
    }
}

void OnButtonPress(bool LongPress)
{
    if (LongPress)
    {
        Text = "Reset";
        /*if (wm.startConfigPortal())
        {
            ESP.reset();
        }*/
    }
    else
    {
        if (espDev != NULL)
        {
            espDev->SetState(!espDev->GetState());
            Text = "Toggle: " + String(espDev->GetState());
        }
    }
}

void setup()
{
    deviceID = String(F("AthomPG01V2_")) + String(WIFI_getChipId(), HEX);
    WiFi.mode(WIFI_AP_STA); // explicitly set mode, esp defaults to STA+AP
    wm.setDebugOutput(false);
    wm_helper.Init(0xABB, PARAMS.data(), PARAMS.size());

    pinMode(RELAY_CONTACT_GPIO12, OUTPUT);
    digitalWrite(RELAY_CONTACT_GPIO12, LOW);

    DeviceButton.OnButtonPressEvent(&OnButtonPress);

    bool res = wm.autoConnect(deviceID.c_str()); // password protected ap

    if (!res)
    {
        //Serial.println(F("Failed to connect"));
        ESP.restart();
    }
    else 
    {
        wm.startWebPortal();
        //if you get here you have connected to the WiFi
        //Serial.println(F("connected"));
        WiFi.mode(WIFI_STA);
    }
}

void loop()
{
    DeviceButton.process();
    wm.process();

    if (Text.length() != 0)
    {
        header = "Status: " + Text + "<br>";
        wm.setCustomHeadElement(header.c_str());
        Text = "";
    }

    if (!freeAtHomeESPapi.process())
    {
        /*
        Serial.println(String("SysAp: ") + wm_helper.GetSetting(0));
        Serial.println(String("User: ") + wm_helper.GetSetting(1));
        Serial.println(String("Pwd: ") + wm_helper.GetSetting(2));*/
        if ((strlen(wm_helper.GetSetting(0)) > 0) && (strlen(wm_helper.GetSetting(1)) > 0) && (strlen(wm_helper.GetSetting(2))> 0))
        {
            //Serial.println(F("Connecting WebSocket"));
            if (!freeAtHomeESPapi.ConnectToSysAP(wm_helper.GetSetting(0), wm_helper.GetSetting(1), wm_helper.GetSetting(2), false))
            {
                Text = "SysAp connect err";               
                for (uint16 t = 0; t < 5000; t++)
                {
                    wm.process();
                    DeviceButton.process();
                    delay(1);
                }
            }
        }
        else
        {
            Text = "NoConf";
        }
    }
    else
    {
        if (espDev == NULL)
        {
            //Serial.println(F("Create Switch Device"));
            String deviceName = String(F("Athom PG01 ")) + String(WIFI_getChipId(), HEX);
            espDev = freeAtHomeESPapi.CreateSwitchDevice(deviceID.c_str(), deviceName.c_str(), 300);
            if (espDev != NULL)
            {
                String t;
                espDev->AddCallback(FahCallBack);
                //freeAtHomeESPapi.U64toStringDev(espDev->GetFahDeviceID(), t);
                //Serial.print(t);
                //Serial.println(F(": Succes!"));
                Text = "Dev Created";
            }
            else
            {
                Text = "Dev Error";
                //Serial.println(F("Failed to create Virtual device, check user authorizations"));
                delay(5000);
            }
        }
    }
}