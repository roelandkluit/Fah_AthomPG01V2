#include "WiFiManager.h" // https://github.com/tzapu/WiFiManager
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

FreeAtHomeESPapi freeAtHomeESPapi;
ButtonManager DeviceButton(BUTTON_GPIO05, true);
FahESPSwitchDevice* espDev = NULL;
String deviceID;
String menuHtml;

WiFiManager wm;
WifiManagerParamHelper wm_helper(wm);
uint16 registrationDelay = 0;

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

        SetCustomMenu(String(F("Dev: ")) + t + String(F(", Event: ")) + ptrChannel + "-" + ptrDataPoint + " = " + val);
    }
}

void handleDeviceState(bool state)
{
    if (espDev != NULL)
    {
        espDev->SetState(state);
    }
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
        String Text = "Heap: " + String(ESP.getFreeHeap()) + "\r\nMaxHeap: " + String(ESP.getMaxAllocHeap()) + "\r\n";
    #else //ESP8266
        String Text = "Heap: " + String(ESP.getFreeHeap()) + "\r\nMaxHeap: " + String(ESP.getMaxFreeBlockSize()) + "\r\nFragemented:" + String(ESP.getHeapFragmentation()) + "\r\n";
    #endif // ESP32

    if (espDev != NULL)
    {
        bool isOn = espDev->GetState();
        if (isOn)
        {
            Text += "IsON: True";
        }
        else
        {
            Text += "IsON: False";
        }
    }
    wm.server->send(200, "text/plain", Text.c_str());
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
        if (espDev != NULL)
        {
            espDev->SetState(!espDev->GetState());
            SetCustomMenu(String(F("ButtonPress: ")) + String(espDev->GetState()));
        }
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
    String State = "Unknown";
    String Button = "";
    if (espDev != NULL)
    {
        Button = String(F("<form action='/o{1}' method='get'><button>Turn O{1}</button></form><br/>"));
        if (espDev->GetState())
        {
            State = "On";
            Button.replace(T_1, F("ff"));
        }
        else
        {
            State = "Off";
            Button.replace(T_1, F("n"));
        }        
    }

    //menuHtml = "Relay is: " + State + "<br/>" + StatusText + "<hr/><br/>" + Button + "<form action='/fah' method='get'><button>Free@Home Status</button></form><br/>\n";
    menuHtml = "Relay is: {1}<br/>{2}<hr/><br/>{3}<form action='/fah' method='get'><button>Free@Home Status</button></form><br/>\n";
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
                    registrationDelay = 5000;
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
                    SetCustomMenu(String(F("Device Registered")));
                }
                else
                {
                    SetCustomMenu(String(F("Device Registration Error")));
                    //Serial.println(F("Failed to create Virtual device, check user authorizations"));
                    registrationDelay = 5000;
                }
            }
        }
    }
}