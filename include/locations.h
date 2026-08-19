//locations holds the timezone/weather information for each location. WiFi is now
//a separate concept (see WifiNetwork below): you pick a network once at startup
//and the location only drives timezone + weather, no longer the connection. That
//means you can sit in any location's settings without being on its WiFi.
#ifndef LOCATIONS_H
#define LOCATIONS_H

#include "secrets.h"

// ---- WiFi networks ---------------------------------------------------------
// Chosen once on the startup selection screen and saved; every WiFi operation
// uses the saved pick. Add networks here and bump WIFI_COUNT.
struct WifiNetwork {
    const char* name;
    const char* ssid;
    const char* password;
};

const WifiNetwork WIFI_NETWORKS[] = {
    { "Waimea",  H_WIFI_SSID, H_WIFI_PASSWORD },
    { "Boulder", B_WIFI_SSID, B_WIFI_PASSWORD },
    { "Florian's House", F_WIFI_SSID, F_WIFI_PASSWORD }
};


const int WIFI_COUNT = 3;

// ---- Locations -------------------------------------------------------------
struct Location {
    const char* name;
    const char* posixTz;
    const char* lat;
    const char* lon;
    const char* timezone;
};

//add locations as needed, just make sure to update LOCATION_COUNT
const Location LOCATIONS[] = {
    {
        "Waimea",
        "HST10",
        "20.01",
        "-155.7",
        "Pacific%2FHonolulu"
    },
    {
        "Boulder",
        "MST7MDT,M3.2.0,M11.1.0", //timezone, utc offset, dst start rule, dst end rule
        "40.00006",
        "-105.2613",
        "America%2FDenver"
    },
    { 
        "Minneapolis",
        "CST6CDT,M3.2.0,M11.1.0",
        "45.104565",
        "-93.29215",
        "America%2FChicago"
    }
};

const int LOCATION_COUNT = 3;
#endif