/*************************************************************************************************************
*
* Title			    : FreeAtHome_AthomPG01V2
* Description:      : Implements the Busch-Jeager / ABB Free@Home API for Athom PG01 Version 2 Socket.
* Version		    : v 0.12
* Last updated      : 2024.04.15
* Target		    : Athom Smart Plug PG01 v2
* Author            : Roeland Kluit
* Web               : https://github.com/roelandkluit/Fah_AthomPG01V2
* License           : GPL-3.0 license
*
**************************************************************************************************************/

#include "WiFiManager.h" // original from https://github.com/tzapu/WiFiManager
#include "WifiManagerParamHelper.h"

// Version 0.12

/* Compile using:
* *********************** *********************** *********************** *********************** **********************
Generic ESP8285 Module, 2MB Flash, FS: 128k, OTA: ~960K
* *********************** *********************** *********************** *********************** **********************
*/

//#define DEBUG

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#else
#error "Platform not supported"
#endif

#define RELAY_CONTACT_GPIO12 12
#define BUTTON_GPIO05 5

#ifdef DEBUG
    //Use serial 2 in case debugger is connected to serial 0
    #define CSE7766_RX_SERIAL Serial1
#else
    #define CSE7766_RX_SERIAL Serial
#endif // DEBUG

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
bool hasLoad = false;
unsigned long previousIntervalMillis = 0;

WiFiManager wm;
WifiManagerParamHelper wm_helper(wm);
uint16_t registrationDelay = 2000;
uint16_t regCount = 0;
uint16_t regCountFail = 0;
uint16 milliAmps = 0;
uint8_t NoLoadTurnOffCounter = 0;
#define TIME_OFF_COUNTER 6

CSE7766 oCSE7766(&CSE7766_RX_SERIAL);

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
constexpr std::array<ParamEntry, 5> PARAMS = { {
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
    },
    {
      "mc",
      "Minimal Current mA",
      CUSTOM_FIELD_LEN,
      ""
    }
} };

void FahCallBack(FAHESPAPI_EVENT Event, uint64_t FAHID, const char* ptrChannel, const char* ptrDataPoint, void* ptrValue)
{
    if (Event == FAHESPAPI_EVENT::FAHESPAPI_ON_DISPLAYNAME)
    {
        DEBUG_P(F("PARM_DISPLAYNAME "));
        const char* val = ((char*)ptrValue);
        DEBUG_PL(val);
        wm_helper.setSetting(3, val, strlen(val));
    }
    else if (Event == FAHESPAPI_EVENT::FAHESPAPI_ON_DEVICE_EVENT)
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

        SetCustomMenu("Device: " + FahID + ", Event: " + ptrChannel + "." + ptrDataPoint + " = " + val);
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
    wm.server->sendHeader("Location", "/", true);
    wm.server->send(302, "text/plain", "");
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
    String Text = "Heap: " + String(ESP.getFreeHeap()) + "\r\nMaxHeap: " + String(ESP.getMaxFreeBlockSize()) + "\r\nFragemented: " + String(ESP.getHeapFragmentation()) + "\r\nFAHESP: " + freeAtHomeESPapi.Version() + "\r\nConnectCount: " + String(regCount) + "\r\nConnectFail: " + String(regCountFail);

    if (espDev != NULL)
    {
        bool isOn = espDev->GetState();
        if (isOn)
        {
            Text += "\r\nIsON: True";
        }
        else
        {
            Text += "\r\nIsON: False";
        }
    }

    Text += "\r\nVoltage: " + String(oCSE7766.getVoltage());
    Text += "\r\nCurrent: " + String(oCSE7766.getCurrent());
    Text += "\r\nPower: " + String(oCSE7766.getActivePower());
    Text += "\r\nApperentPower: " + String(oCSE7766.getApparentPower());
    Text += "\r\nReactivePower: " + String(oCSE7766.getReactivePower());
    Text += "\r\nPowerFactor: " + String(oCSE7766.getPowerFactor());
    Text += "\r\nEnergy: " + String(oCSE7766.getEnergy());
    Text += "\r\nLoad: " + String(hasLoad);

    wm.server->send(200, "text/plain", Text.c_str());
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
            SetCustomMenu("ButtonPress: " + espDev->GetState());
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

    pinMode(RELAY_CONTACT_GPIO12, OUTPUT);

    deviceID ="AthomPG01V2_" + String(WIFI_getChipId(), HEX);
    WiFi.mode(WIFI_AP_STA); // explicitly set mode, esp defaults to STA+AP
    #ifdef DEBUG
        wm.setDebugOutput(true);
    #else
        wm.setDebugOutput(false);
    #endif
    wm_helper.Init(0xABB0, PARAMS.data(), PARAMS.size());
    wm.setHostname(deviceID);
    
    //On or OFF state is now requested from SysAP
    //digitalWrite(RELAY_CONTACT_GPIO12, LOW);

    wm.setConfigPortalTimeout(300);
    wm.setConnectTimeout(20);
    DeviceButton.OnButtonPressEvent(&OnButtonPress);
    //wm.setDebugOutput(true);

    bool res = wm.autoConnect(deviceID.c_str()); // start autoconnect or Non password protected AP

    if (!res)
    {
        DEBUG_PL(F("Failed to connect"));
        ESP.restart();
    }
    else 
    {
        //if you get here you have connected to the WiFi
        DEBUG_PL(F("connected"));
        wm.setConfigPortalTimeout(0);
        WiFi.mode(WIFI_STA);
        wm.startWebPortal();
        wm.setShowInfoUpdate(true);
        wm.server->on("/dbgs", handleDbgSys);
        wm.server->on("/on", handleDeviceOn);
        wm.server->on("/off", handleDeviceOff);
        SetCustomMenu("Initializing");
        std::vector<const char*> _menuIdsUpdate = {"custom", "sep", "wifi","param","info","update" };
        wm.setMenu(_menuIdsUpdate);
    }

    //Power and voltage monitor chip
    oCSE7766.begin(); // will initialize serial to 4800 bps
}

void SetCustomMenu(String StatusText)
{
    DEBUG_PL(StatusText);
    String State = "Unknown";
    String Button = "";
    #ifndef MINIMAL_UPLOAD
    if (espDev != NULL)
    {
        Button = "<form action='/o{1}' method='get'><button>Turn O{1}</button></form><br/>";
        if (espDev->GetState())
        {
            State = "On";
            Button.replace(T_1, "ff");
        }
        else
        {
            State = "Off";
            Button.replace(T_1, "n");
        }        
    }
    #endif

    //menuHtml = "Relay is: " + State + "<br/>" + StatusText + "<hr/><br/>" + Button + "<form action='/fah' method='get'><button>Debug Status</button></form><br/>\n";
    menuHtml = "Name: {n}<br/>Relay: {1}<br/>HasLoad: {l}</br/>{2}<hr/><br/>{3}<form action='/dbgs' method='get'><button>Debug status</button></form><br/><meta http-equiv='refresh' content='10'>\n";
    menuHtml.replace(T_n, wm_helper.GetSetting(3));
    menuHtml.replace(T_1, State);
    menuHtml.replace(T_2, StatusText);
    menuHtml.replace(T_3, Button);
    menuHtml.replace(T_l, String(hasLoad));

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
        if (!WiFi.isConnected())
        {
            DEBUG_PL(F("Re-connecting??"));
            WiFi.reconnect();
            registrationDelay = 30000;
        }
        else if (!freeAtHomeESPapi.process())
        {
            DEBUG_PL(String("SysAp: ") + wm_helper.GetSetting(0));
            DEBUG_PL(String("User: ") + wm_helper.GetSetting(1));
            DEBUG_PL(String("Pwd: ") + wm_helper.GetSetting(2));
            DEBUG_PL(String("Name: ") + wm_helper.GetSetting(3));
            milliAmps = atoi(wm_helper.GetSetting(4));
            DEBUG_PL(String("Ma: ") + String(milliAmps));
            if ((strlen(wm_helper.GetSetting(0)) > 0) && (strlen(wm_helper.GetSetting(1)) > 0) && (strlen(wm_helper.GetSetting(2)) > 0))
            {
                //DEBUG_PL("Connecting WebSocket");
                if (!freeAtHomeESPapi.ConnectToSysAP(wm_helper.GetSetting(0), wm_helper.GetSetting(1), wm_helper.GetSetting(2), false))
                {
                    SetCustomMenu("SysAp connect error");
                    //Prevent to many retries
                    registrationDelay = 10000;
                    regCountFail++;
                }
                else
                {
                    SetCustomMenu("SysAp connected");
                    regCount++;
                }
            }
            else
            {
                SetCustomMenu("No SysAp configuration");
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

                DEBUG_PL("Create Switch Device");
                String deviceName = "Athom PG01 " + String(WIFI_getChipId(), HEX);
                const char* val = wm_helper.GetSetting(3);
                if (strlen(val) > 0)
                {
                    deviceName = String(val);
                }
                DEBUG_P("Using: ");
                DEBUG_PL(deviceName);
                espDev = freeAtHomeESPapi.CreateSwitchDevice(deviceID.c_str(), deviceName.c_str(), 300);
                
                if (espDev != NULL)
                {
                    espDev->AddCallback(FahCallBack);
                    String FahID = freeAtHomeESPapi.U64toString(espDev->GetFahDeviceID());
                    //DEBUG_PL(t);
                    //DEBUG_PL(": Succes!");
                    SetCustomMenu("Device Registered: " + FahID);
                }
                else
                {
                    SetCustomMenu("Device Registration Error");
                    //DEBUG_PL("Failed to create Virtual device, check user authorizations");
                    registrationDelay = 30000;
                }
            }
            else
            {
                if (millis() - previousIntervalMillis >= (10000))
                {
                    previousIntervalMillis = millis();
                    DEBUG_PL("upu");
                    oCSE7766.handle();
                    double current = oCSE7766.getCurrent();
                    if (milliAmps > 0)
                    {
                        if ((current * 1000) >= milliAmps)
                        {
                            NoLoadTurnOffCounter = 0;
                            if (hasLoad != true)
                            {
                                DEBUG_PL("Load");
                                hasLoad = true;
                                SetCustomMenu("Load current over tresshold");
                            }

                        }
                        else
                        {
                            if (espDev->GetState())
                            {
                                NoLoadTurnOffCounter++;
                                if (hasLoad != false)
                                {
                                    DEBUG_PL("NoLoad");
                                    hasLoad = false;
                                    SetCustomMenu("Load current below tresshold");
                                }
                                if (NoLoadTurnOffCounter > TIME_OFF_COUNTER)
                                {
                                    handleDeviceState(false);
                                    SetCustomMenu("Load current triggered off");
                                }
                            }
                            else
                            {
                                NoLoadTurnOffCounter = 0;
                            }
                        }
                    }
                }
            }
        }
    }
}