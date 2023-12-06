/*************************************************************************************************************
*
* Title			    : FreeAtHome_AthomPG01V2
* Description:      : Implements the Busch-Jeager / ABB Free@Home API for Athom PG01 Version 2 Socket.
* Version		    : v 0.5
* Last updated      : 2023.11.19
* Target		    : Athom Smart Plug PG01 v2
* Author            : Roeland Kluit
* Web               : https://github.com/roelandkluit/Fah_AthomPG01V2
* License           : GPL-3.0 license
*
**************************************************************************************************************/

#include "WiFiManager.h" // original from https://github.com/tzapu/WiFiManager
#include "WifiManagerParamHelper.h"

// Version 0.5

/* Compile using:
* *********************** *********************** *********************** *********************** **********************
Generic ESP8285 Module, 2MB Flash, FS: 128k, OTA: ~960K
* *********************** *********************** *********************** *********************** **********************
*/

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#else
#error "Platform not supported"
#endif

#define RELAY_CONTACT_GPIO12 12
#define BUTTON_GPIO05 5
#define CSE7766_RXPIN_GPIO3 3

#include "CSE7766.h"
#include "FreeAtHomeESPapi.h"
#include "FahESPDevice.h"
#include "FahESPSwitchDevice.h"

FreeAtHomeESPapi freeAtHomeESPapi;
FahESPSwitchDevice* espDev = NULL;

#include "ButtonManager.h"
ButtonManager DeviceButton(BUTTON_GPIO05, true);
String deviceID;
String menuHtml;

WiFiManager wm;
WifiManagerParamHelper wm_helper(wm);
uint16_t registrationDelay = 2000;
uint16_t regCount = 0;
uint16_t regCountFail = 0;

CSE7766 oCSE7766;

#define DEBUG

#ifdef DEBUG
#define DEBUG_PL Serial.println
#define DEBUG_P Serial.print
#define DEBUG_F Serial.printf
#else
#define DEBUG_PL(MSG)
#define DEBUG_P(MSG)
#define DEBUG_F(MSG)
#endif

constexpr size_t CUSTOM_FIELD_LEN = 40;
constexpr std::array<ParamEntry, 4> PARAMS = { {
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
    },
    {
      "dn",
      "Name",
      CUSTOM_FIELD_LEN,
      ""
    }
} };

void FahCallBack(FAHESPAPI_EVENT Event, uint64_t FAHID, const char* ptrChannel, const char* ptrDataPoint, void* ptrValue)
{
    /*if (Event == FAHESPAPI_EVENT::FAHESPAPI_ON_DISPLAYNAME)
    {
        char* val = ((char*)ptrValue);
        wm_helper.setSetting(3, val, strlen(val));
        
    }
    else*/if (Event == FAHESPAPI_EVENT::FAHESPAPI_ON_DEVICE_EVENT)
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

        SetCustomMenu(String(F("Device: ")) + FahID + String(F(", Event: ")) + ptrChannel + "." + ptrDataPoint + " = " + val);
    }
}

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

void handleDbgSys()
{
    String Text = String(F("Heap: ")) + String(ESP.getFreeHeap()) + String(F("\r\nMaxHeap: ")) + String(ESP.getMaxFreeBlockSize()) + String(F("\r\nFragemented:")) + String(ESP.getHeapFragmentation()) + String(F("\r\nFAHESP:")) + freeAtHomeESPapi.Version() + String(F("\r\nConnectCount:")) + String(regCount) + String(F("\r\nConnectFail:")) + String(regCountFail);    

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
    
    Text += String(F("\r\nVoltage: ")) + String(oCSE7766.getVoltage());
    Text += String(F("\r\nCurrent: ")) + String(oCSE7766.getCurrent());
    Text += String(F("\r\nPower: ")) + String(oCSE7766.getActivePower());
    /*DEBUG_MSG("Current %.4f A\n", oCSE7766.getCurrent());
    DEBUG_MSG("ActivePower %.4f W\n", oCSE7766.getActivePower());
    DEBUG_MSG("ApparentPower %.4f VA\n", oCSE7766.getApparentPower());
    DEBUG_MSG("ReactivePower %.4f VAR\n", oCSE7766.getReactivePower());
    DEBUG_MSG("PowerFactor %.4f %\n", oCSE7766.getPowerFactor());
    DEBUG_MSG("Energy %.4f Ws\n", oCSE7766.getEnergy());*/

    wm.server->send(200, String(F("text/plain")), Text.c_str());
}

void OnButtonPress(bool LongPress)
{
    if (LongPress)
    {
        DEBUG_PL(F("Reset"));
        delay(5000);
        ESP.reset();
    }
    else
    {
        if (espDev != NULL)
        {
            espDev->SetState(!espDev->GetState());
            SetCustomMenu(String(F("ButtonPress: ")) + String(espDev->GetState()));
        }
    }
}

void setup()
{
    #ifdef DEBUG
        Serial.begin(115200);
        delay(500);
        DEBUG_PL(F("Starting"));
    #endif // DEBUG

    deviceID = String(F("AthomPG01V2_")) + String(WIFI_getChipId(), HEX);
    WiFi.mode(WIFI_AP_STA); // explicitly set mode, esp defaults to STA+AP
    wm.setDebugOutput(false);
    wm_helper.Init(0x1ABB, PARAMS.data(), PARAMS.size());
    wm.setHostname(deviceID);

    pinMode(RELAY_CONTACT_GPIO12, OUTPUT);
    //On or OFF state is now requested from SysAP
    //digitalWrite(RELAY_CONTACT_GPIO12, LOW);

    DeviceButton.OnButtonPressEvent(&OnButtonPress);

    bool res = wm.autoConnect(deviceID.c_str()); // Non password protected AP

    if (!res)
    {
        DEBUG_PL(F("Failed to connect"));
        ESP.restart();
    }
    else 
    {
        //if you get here you have connected to the WiFi
        DEBUG_PL(F("connected"));
        WiFi.mode(WIFI_STA);
        wm.startWebPortal();
        wm.setShowInfoUpdate(true);
        wm.server->on("/dbgs", handleDbgSys);
        wm.server->on("/on", handleDeviceOn);
        wm.server->on("/off", handleDeviceOff);
        SetCustomMenu(String(F("Initializing")));
        std::vector<const char*> _menuIdsUpdate = {"custom", "sep", "wifi","param","info","update" };
        wm.setMenu(_menuIdsUpdate);
    }

    //Power and voltage monitor chip
    oCSE7766.setRX(CSE7766_RXPIN_GPIO3);
    oCSE7766.begin(); // will initialize serial to 4800 bps
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

    //menuHtml = "Relay is: " + State + "<br/>" + StatusText + "<hr/><br/>" + Button + "<form action='/fah' method='get'><button>Debug Status</button></form><br/>\n";
    menuHtml = String(F("Relay is: {1}<br/>{2}<hr/><br/>{3}<form action='/dbgs' method='get'><button>Debug status</button></form><br/><meta http-equiv='refresh' content='10'>\n"));
    menuHtml.replace(T_1, State);
    menuHtml.replace(T_2, StatusText);
    menuHtml.replace(T_3, Button);

    wm.setCustomMenuHTML(menuHtml.c_str());
}

void loop()
{
    DeviceButton.process();
    wm.process();    

    if (registrationDelay > 0)
    {
        registrationDelay--;
        delay(1);
    }
    else
    {
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
                    SetCustomMenu(String(F("SysAp connected")));
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

                DEBUG_PL(F("Create Switch Device"));
                String deviceName = String(F("Athom PG01 ")) + String(WIFI_getChipId(), HEX);
                /*const char* val = wm_helper.GetSetting(3);
                if (strlen(val) > 0)
                {
                    deviceName = String(val);
                }*/
                DEBUG_P(F("Using:"));
                DEBUG_PL(deviceName);
                espDev = freeAtHomeESPapi.CreateSwitchDevice(deviceID.c_str(), deviceName.c_str(), 300);
                //Todo Add callback!
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
            else
            {
                oCSE7766.handle();
            }
        }
    }
}