/*************************************************************************************************************
*
* Title			    : FreeAtHome_AthomPG01V2
* Description:      : Implements the Busch-Jeager / ABB Free@Home API for Athom PG01 Version 2 Socket.
* Version		    : v 0.2
* Last updated      : 2023.10.20
* Target		    : Athom Smart Plug PG01 v2
* Author            : Roeland Kluit
* Web               : https://github.com/roelandkluit/Fah_AthomPG01V2
* License           : GPL-3.0 license
*
**************************************************************************************************************/

#include "WiFiManager.h" // original from https://github.com/tzapu/WiFiManager
#include "WifiManagerParamHelper.h"

// Version 0.1
//
// Use MINIMAL_UPLOAD to create a small WifiManager only image for upload to small OTA partition as stepping stone
//#define MINIMAL_UPLOAD

/* Compile using:
* *********************** *********************** *********************** *********************** **********************
Generic ESP8285 Module, 2MB Flash, FS: 128k, OTA: ~960K
* *********************** *********************** *********************** *********************** **********************
*/

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

#ifndef MINIMAL_UPLOAD
#include "FreeAtHomeESPapi.h"
#include "FahESPDevice.h"
#include "FahESPSwitchDevice.h"

FreeAtHomeESPapi freeAtHomeESPapi;
FahESPSwitchDevice* espDev = NULL;
#endif

#include "ButtonManager.h"
ButtonManager DeviceButton(BUTTON_GPIO05, true);
String deviceID;
String menuHtml;

WiFiManager wm;
WifiManagerParamHelper wm_helper(wm);
uint16_t registrationDelay = 2000;
uint16_t regCount = 0;
uint16_t regCountFail = 0;

//ntp
/*#include <NTPClient.h>
#include <WiFiUdp.h>
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0);
unsigned long boot_unixtimestamp = 0;
*/

constexpr size_t CUSTOM_FIELD_LEN = 40;
constexpr std::array<ParamEntry, 3> PARAMS = { {
    {
      "Ap",
      "SysAp",
      CUSTOM_FIELD_LEN,
      ""
    },
    {
      "uG",
      "User Guid",
      CUSTOM_FIELD_LEN,
      ""
    },
    {
      "pw",
      "Password",
      CUSTOM_FIELD_LEN,
      "type=\"password\""
    }
} };

#ifndef MINIMAL_UPLOAD
void FahCallBack(FAHESPAPI_EVENT Event, uint64_t FAHID, const char* ptrChannel, const char* ptrDataPoint, void* ptrValue)
{
    if (Event == FAHESPAPI_EVENT::FAHESPAPI_ON_DEVICE_EVENT)
    {
        bool val = ((bool)ptrValue);
        String FahID = freeAtHomeESPapi.U64toString(FAHID);

        if (val)
        {
            digitalWrite(RELAY_CONTACT_GPIO12, HIGH);
        }
        else
        {
            digitalWrite(RELAY_CONTACT_GPIO12, LOW);
        }

        SetCustomMenu(String(F("Dev: ")) + FahID + String(F(", Event: ")) + ptrChannel + "-" + ptrDataPoint + " = " + val);
    }
}
#endif

void handleDeviceState(bool state)
{
    #ifndef MINIMAL_UPLOAD
    if (espDev != NULL)
    {
        espDev->SetState(state);
    }
    #endif
    wm.server->sendHeader(String(F("Location")), "/", true);
    wm.server->send(302, String(F("text/plain")), "");
}

void handleDeviceOn()
{
    handleDeviceState(true);
}

void handleDeviceOff()
{
    handleDeviceState(false);
}

void handleDevice()
{
    /*
    String Date = GetDateTime(boot_unixtimestamp);
    #ifdef ESP32
        String Text = "Timestamp: " + Date + " > " + String(boot_unixtimestamp) + "\r\nHeap: " + String(ESP.getFreeHeap()) + "\r\nMaxHeap: " + String(ESP.getMaxAllocHeap()) + "\r\n";
    #else //ESP8266
        String Text = "Timestamp: " + Date + " > " + String(boot_unixtimestamp) + "\r\nHeap: " + String(ESP.getFreeHeap()) + "\r\nMaxHeap: " + String(ESP.getMaxFreeBlockSize()) + "\r\nFragemented:" + String(ESP.getHeapFragmentation()) + "\r\n";
    #endif // ESP32
    */
    #ifdef ESP32
        String Text = "Heap: " + String(ESP.getFreeHeap()) + "\r\nMaxHeap: " + String(ESP.getMaxAllocHeap()) + "\r\n";q
    #else //ESP8266
        String Text = String(F("Heap: ")) + String(ESP.getFreeHeap()) + String(F("\r\nMaxHeap: ")) + String(ESP.getMaxFreeBlockSize()) + String(F("\r\nFragemented:")) + String(ESP.getHeapFragmentation()) + String(F("\r\nFAHESP:")) + freeAtHomeESPapi.Version() + String(F("\r\nConnectCount:")) + String(regCount) + String(F("\r\nConnectFail:")) + String(regCountFail);
    #endif // ESP32

    #ifndef MINIMAL_UPLOAD
    if (espDev != NULL)
    {
        bool isOn = espDev->GetState();
        if (isOn)
        {
            Text += String(F("\r\nIsON: True"));
        }
        else
        {
            Text += String(F("\r\nIsON: False"));
        }
    }
    #endif
    wm.server->send(200, String(F("text/plain")), Text.c_str());
}

void OnButtonPress(bool LongPress)
{
    if (LongPress)
    {
        String(F("Reset"));
        delay(5000);
        ESP.reset();
    }
    else
    {
        #ifndef MINIMAL_UPLOAD
        if (espDev != NULL)
        {
            espDev->SetState(!espDev->GetState());
            SetCustomMenu(String(F("ButtonPress: ")) + String(espDev->GetState()));
        }
        #endif  
    }
}

void setup()
{
    deviceID = String(F("AthomPG01V2_")) + String(WIFI_getChipId(), HEX);
    WiFi.mode(WIFI_AP_STA); // explicitly set mode, esp defaults to STA+AP
    wm.setDebugOutput(false);
    wm_helper.Init(0xABB, PARAMS.data(), PARAMS.size());
    wm.setHostname(deviceID);

    //Serial.begin(115200);

    pinMode(RELAY_CONTACT_GPIO12, OUTPUT);
    digitalWrite(RELAY_CONTACT_GPIO12, LOW);

    DeviceButton.OnButtonPressEvent(&OnButtonPress);

    bool res = wm.autoConnect(deviceID.c_str()); // Non password protected AP

    if (!res)
    {
        //Serial.println(F("Failed to connect"));
        ESP.restart();
    }
    else 
    {
        //if you get here you have connected to the WiFi
        //Serial.println(F("connected"));
        WiFi.mode(WIFI_STA);
        wm.startWebPortal();
        wm.setShowInfoUpdate(true);
        wm.server->on("/fah", handleDevice);
        wm.server->on("/on", handleDeviceOn);
        wm.server->on("/off", handleDeviceOff);
        SetCustomMenu(String(F("Initializing")));
        std::vector<const char*> _menuIdsUpdate = {"custom", "sep", "wifi","param","info","update" };
        wm.setMenu(_menuIdsUpdate);
    }
    //timeClient.begin();
}

void SetCustomMenu(String StatusText)
{
    String State = String(F("Unknown"));
    String Button = "";
    #ifndef MINIMAL_UPLOAD
    if (espDev != NULL)
    {
        Button = String(F("<form action='/o{1}' method='get'><button>Turn O{1}</button></form><br/>"));
        if (espDev->GetState())
        {
            State = String(F("On"));
            Button.replace(T_1, F("ff"));
        }
        else
        {
            State = String(F("Off"));
            Button.replace(T_1, F("n"));
        }        
    }
    #endif

    //menuHtml = "Relay is: " + State + "<br/>" + StatusText + "<hr/><br/>" + Button + "<form action='/fah' method='get'><button>Free@Home Status</button></form><br/>\n";
    menuHtml = String(F("Relay is: {1}<br/>{2}<hr/><br/>{3}<form action='/fah' method='get'><button>Free@Home Status</button></form><br/><meta http-equiv='refresh' content='10'>\n"));
    menuHtml.replace(T_1, State);
    menuHtml.replace(T_2, StatusText);
    menuHtml.replace(T_3, Button);

    wm.setCustomMenuHTML(menuHtml.c_str());
}

/*String GetDateTime(uint32_t t) {
    // http://howardhinnant.github.io/date_algorithms.html#civil_from_days
    //t += _gmt * 3600ul;
    uint8_t second = t % 60ul;
    t /= 60ul;
    uint8_t minute = t % 60ul;
    t /= 60ul;
    uint8_t hour = t % 24ul;
    t /= 24ul;
    //dayOfWeek = (t + 4) % 7;
    //if (!dayOfWeek) dayOfWeek = 7;
    uint32_t z = t + 719468;
    uint8_t era = z / 146097ul;
    uint16_t doe = z - era * 146097ul;
    uint16_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    uint16_t y = yoe + era * 400;
    uint16_t doy = doe - (yoe * 365 + yoe / 4 - yoe / 100);
    uint16_t mp = (doy * 5 + 2) / 153;
    uint8_t day = doy - (mp * 153 + 2) / 5 + 1;
    uint8_t month = mp + (mp < 10 ? 3 : -9);
    y += (month <= 2);
    uint16_t year = y;

    String Date = String(year) + "-" + String(month) + "-" + String(day) + " " + String(hour) + ":" + String(minute) + "." + String(second);
    return Date;
}*/

void loop()
{
    /*if (boot_unixtimestamp == 0)
    {
        timeClient.update();
        if (timeClient.isTimeSet())
        {
            boot_unixtimestamp = timeClient.getEpochTime();
            timeClient.end();
        }
    }*/

    /*
    if (Serial.available())
    {
        while (Serial.available())
        {
            Serial.read();
        }
    }*/

    DeviceButton.process();
    wm.process();

    if (registrationDelay > 0)
    {
        registrationDelay--;
        delay(1);
    }
    else
    {
        #ifndef MINIMAL_UPLOAD
        if (!freeAtHomeESPapi.process())
        {
            /*
            Serial.println(String("SysAp: ") + wm_helper.GetSetting(0));
            Serial.println(String("User: ") + wm_helper.GetSetting(1));
            Serial.println(String("Pwd: ") + wm_helper.GetSetting(2));*/
            if ((strlen(wm_helper.GetSetting(0)) > 0) && (strlen(wm_helper.GetSetting(1)) > 0) && (strlen(wm_helper.GetSetting(2)) > 0))
            {
                //Serial.println(F("Connecting WebSocket"));
                if (!freeAtHomeESPapi.ConnectToSysAP(wm_helper.GetSetting(0), wm_helper.GetSetting(1), wm_helper.GetSetting(2), false))
                {
                    SetCustomMenu(String(F("SysAp connect error")));
                    //Prevent to many retries
                    registrationDelay = 10000;
                    regCountFail++;
                }
                else
                {
                    regCount++;
                }
            }
            else
            {
                SetCustomMenu(String(F("No SysAp configuration")));
                registrationDelay = 1000;
            }
        }
        else
        {
            if (espDev == NULL)
            {
                //Clear string for as high as possible heap placement
                wm.setCustomMenuHTML(NULL);
                menuHtml = "";

                //Serial.println(F("Create Switch Device"));
                String deviceName = String(F("Athom PG01 ")) + String(WIFI_getChipId(), HEX);
                espDev = freeAtHomeESPapi.CreateSwitchDevice(deviceID.c_str(), deviceName.c_str(), 300);
                if (espDev != NULL)
                {
                    espDev->AddCallback(FahCallBack);
                    String FahID = freeAtHomeESPapi.U64toString(espDev->GetFahDeviceID());
                    //Serial.print(t);
                    //Serial.println(F(": Succes!"));
                    SetCustomMenu(String(F("Device Registered: ")) + FahID);
                }
                else
                {
                    SetCustomMenu(String(F("Device Registration Error")));
                    //Serial.println(F("Failed to create Virtual device, check user authorizations"));
                    registrationDelay = 30000;
                }
            }
        }
        #endif
    }
}