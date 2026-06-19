#pragma once
#include <compiler/Definitions.h>
#include <sc/IPlugin.h>
#include <gui/LineEdit.h>

#ifdef MU_WINDOWS
    #ifdef PLUGIN_EXPORTS
    #define PLUGIN_API __declspec(dllexport)
    #else
    #define PLUGIN_API __declspec(dllimport)
    #endif
#else
    #ifdef PLUGIN_EXPORTS
    #define PLUGIN_API __attribute__((visibility("default")))
    #else
    #define PLUGIN_API
    #endif
#endif

// Tip čvora u mreži
enum class BusType { Slack = 0, PQ, PV };

// Jedan čvor mreže
using Bus = struct _Bus
{
    int id;
    BusType type;
    double vMag;      // napon [p.u.]
    double vAng;      // kut [rad]
    double pLoad;     // aktivna snaga opterećenja [MW]
    double qLoad;     // reaktivna snaga opterećenja [MVAr]
    double pGen;      // generisana aktivna snaga [MW]
    double qGen;      // generisana reaktivna snaga [MVAr]
    bool hasSOFC;     // da li čvor ima SOFC
};

// SOFC parametri
using SOFCParams = struct _SOFCParams
{
    double ratedPower;      // nominalna snaga [kW]
    double temperature;     // radna temperatura [K]
    double pressureH2;      // pritisak vodika [atm]
    double pressureO2;      // pritisak kisika [atm]
    double pressureH2O;     // pritisak vodene pare [atm]
    double internalRes;     // unutarnji otpor [Ohm]
    double efficiency;      // efikasnost invertora [0-1]
    // Novi termički parametri
    double thermalMass;     // termalna masa [J/K]
    double heatLoss;        // koeficijent gubitka topline [W/K]
    double fuelUtil;        // fuel utilization faktor [0-1]
};

// Opcije konverzije
using Options = struct _Options
{
    td::String modelName;
    td::String xmlConfigFile;
    int caseNumber;         // 9, 30, 118 ili 300
    int maxIter;
    double dTime;
    double endTime;
    SOFCParams sofcParams;
};

void onClosedPluginWindow();

bool createModel(const td::String& inputFileName,
                 const td::String& outFileName,
                 sc::IPlugin* pIPlugin,
                 const Options& options,
                 gui::LineEdit& status);