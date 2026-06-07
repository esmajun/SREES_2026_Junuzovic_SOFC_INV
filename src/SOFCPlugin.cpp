#include "SOFCPlugin.h"
#include <sc/IPlugin.h>
#include "WindowPlugin.h"
#include <td/StringUtils.h>
#include <dense/Matrix.h>
#include <mu/ScopedCLocale.h>
#include <cmath>
#include <thread>
#include <atomic>
#include <chrono>

// ============================================================
// Plugin klasa
// ============================================================
class Plugin : public sc::IPlugin
{
    MemoryArchiveContainer _outArchives;
    WindowPlugin* _pWnd = nullptr;
public:
    Plugin()
    {
        for (size_t i = 0; i < size_t(ArchType::NA); ++i)
            _outArchives[i] = nullptr;
    }

    void show(gui::Window* parentWnd, MemoryArchiveContainer& archives, 
              td::UINT4 wndID, const sc::IPlugin::Cleaner& cleaner, 
              const sc::IPlugin::CallBack& onComplete) override final
    {
        for (size_t i = 0; i < size_t(ArchType::NA); ++i)
            _outArchives[i] = archives[i];

        if (_pWnd)
            _pWnd->setFocus();
        else
        {
            _pWnd = new WindowPlugin(parentWnd, this, onComplete, cleaner, wndID);
            _pWnd->open();
        }
    }

    td::String getMenuName() const override final
    {
        return "SOFC + Inverter Converter";
    }

    arch::MemoryOut* getArchive(sc::IPlugin::ArchType type) override final
    {
        auto iType = size_t(type);
        if (iType >= getMaxSupportedArchiveParts())
            return nullptr;
        return _outArchives[size_t(type)];
    }

    MemoryArchiveContainer& getArchives() override final
    {
        return _outArchives;
    }

    td::String getOutFileName() const override final
    {
        assert(_pWnd);
        return _pWnd->getOutFileName();
    }

    size_t getMaxSupportedArchiveParts() const override final
    {
        return size_t(ArchType::NA);
    }

    ModelType getModelType() const override final
    {
        return ModelType::DAE;
    }

    void onClosedPluginWindow()
    {
        _pWnd = nullptr;
    }
};

static Plugin s_plugin;

void onClosedPluginWindow()
{
    s_plugin.onClosedPluginWindow();
}

extern "C"
{
PLUGIN_API sc::IPlugin* getPluginInterface()
{
    return &s_plugin;
}
}

// ============================================================
// SOFC matematicki model
// ============================================================

// Konstante
static const double R  = 8.314;    // univerzalna plinska konstanta [J/mol/K]
static const double F  = 96485.0;  // Faradayeva konstanta [C/mol]
static const double E0 = 1.229;    // standardni reversibilni napon [V]

// Izracunaj Nernst napon
static double calcNernstVoltage(const SOFCParams& p)
{
    double lnTerm = std::log(p.pressureH2 * std::sqrt(p.pressureO2) / p.pressureH2O);
    return E0 + (R * p.temperature / (2.0 * F)) * lnTerm;
}

// Izracunaj napon SOFC celije pri datoj struji
static double calcCellVoltage(const SOFCParams& p, double current)
{
    double vNernst = calcNernstVoltage(p);
    double vOhm    = current * p.internalRes;
    // Pojednostavljen aktivacijski gubitak
    double vAct    = 0.1 * std::log(1.0 + current / 0.01);
    return vNernst - vOhm - vAct;
}

// Izracunaj DC snagu SOFC
static double calcSOFCPower(const SOFCParams& p, double current)
{
    double vCell = calcCellVoltage(p, current);
    if (vCell < 0.0) vCell = 0.0;
    return vCell * current; // [W] po celiji
}

// ============================================================
// IEEE Test case podaci (Case 9 - ugradjeni)
// ============================================================
struct BusData
{
    int id;
    double pLoad; // [MW]
    double qLoad; // [MVAr]
    double pGen;  // [MW]
    double vMag;  // [p.u.]
};

static BusData case9Buses[] = {
    {1, 0.0,   0.0,   0.0,   1.04},
    {2, 0.0,   0.0,   163.0, 1.025},
    {3, 0.0,   0.0,   85.0,  1.025},
    {4, 0.0,   0.0,   0.0,   1.0},
    {5, 125.0, 50.0,  0.0,   1.0},
    {6, 90.0,  30.0,  0.0,   1.0},
    {7, 0.0,   0.0,   0.0,   1.0},
    {8, 100.0, 35.0,  0.0,   1.0},
    {9, 0.0,   0.0,   0.0,   1.0}
};

static int case9BusCount = 9;

// ============================================================
// Glavni createModel - konverzija u jedan thread
// Progress update u drugi thread
// ============================================================
bool createModel(const td::String& inputFileName,
                 const td::String& outFileName,
                 sc::IPlugin* pIPlugin,
                 const Options& options,
                 gui::LineEdit& status)
{
    mu::ScopedCLocale scopedLocale;

    // Atomicni progress (thread-safe)
    std::atomic<int> progress(0);
    std::atomic<bool> done(false);

    // Thread 2 - progress indikator
    std::thread progressThread([&progress, &done]() {
        while (!done.load())
        {
            int p = progress.load();
            // gui::thread::asyncExecInMainThread([p]() {
                // progress bar update - implementirat ce se kroz callback
            // });
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // Thread 1 - konverzija
    bool success = false;
    std::thread convThread([&]() {
        
        progress = 10;
        
        // Odabir broja busova prema case-u
        int nBus = case9BusCount;
        BusData* buses = case9Buses;
        
        if (options.caseNumber == 9)
        {
            nBus = 9;
            buses = case9Buses;
        }
        // Case 30, 118, 300 - dodati podatke naknadno
        
        progress = 30;
        
        // SOFC proracun za svaki bus
        const auto& sofc = options.sofcParams;
        double nomCurrent = sofc.ratedPower * 1000.0 / 
                           calcCellVoltage(sofc, 100.0); // nominalna struja
        if (nomCurrent <= 0.0) nomCurrent = 100.0;
        
        double vSOFC  = calcCellVoltage(sofc, nomCurrent);
        double pSOFC  = calcSOFCPower(sofc, nomCurrent);
        double pAC    = pSOFC * sofc.efficiency; // snaga nakon invertora
        
        progress = 60;
        
        // Kreiranje output modela
        auto pDigitModel = pIPlugin->getArchive(sc::IPlugin::ArchType::DigitalModel);
        auto& memOut = *pDigitModel;
        td::MutableString mStr;
        mStr.reserve(4096);
        
        // Header
        mStr.appendFormat(
            "Header:\n"
            "\tmaxIter = %d\n"
            "\treport = Solver\n"
            "\tstartTime = 0\n"
            "\tdTime = %.4f\n"
            "\tendTime = %.2f\n"
            "end\n\n",
            options.maxIter, options.dTime, options.endTime);
        memOut.put(mStr.c_str(), mStr.length());
        mStr.reset();
        
        // Model info
        mStr.appendFormat(
            "// SOFC + Inverter Model - IEEE Case %d\n"
            "// Generated by SOFC_INV Plugin\n"
            "// Nernst voltage: %.4f V\n"
            "// SOFC DC power:  %.2f W\n"
            "// AC power out:   %.2f W\n\n",
            options.caseNumber,
            calcNernstVoltage(sofc),
            pSOFC,
            pAC);
        memOut.put(mStr.c_str(), mStr.length());
        mStr.reset();
        
        // Model DAE
        mStr.appendFormat("Model [type=DAE domain=real name=\"%s\"]:\n", 
                         options.modelName.c_str());
        memOut.put(mStr.c_str(), mStr.length());
        mStr.reset();
        
        // Varijable - napon i kut za svaki bus
        memOut.put("Vars [out=true]:\n\t");
        for (int i = 0; i < nBus; ++i)
        {
            mStr.appendFormat("v_%d; theta_%d; ", buses[i].id, buses[i].id);
        }
        mStr.append("v_sofc; i_sofc; p_sofc; p_ac\n");
        memOut.put(mStr.c_str(), mStr.length());
        mStr.reset();
        
        // Parametri
        memOut.put("Params:\n");
        mStr.appendFormat(
            "\tT_sofc = %.2f\t// temperatura [K]\n"
            "\tpH2   = %.4f\t// pritisak H2 [atm]\n"
            "\tpO2   = %.4f\t// pritisak O2 [atm]\n"
            "\tpH2O  = %.4f\t// pritisak H2O [atm]\n"
            "\tRint  = %.4f\t// unutarnji otpor [Ohm]\n"
            "\teta   = %.4f\t// efikasnost invertora\n"
            "\tE0    = %.4f\t// standardni napon [V]\n",
            sofc.temperature, sofc.pressureH2, sofc.pressureO2,
            sofc.pressureH2O, sofc.internalRes, sofc.efficiency, E0);
        memOut.put(mStr.c_str(), mStr.length());
        mStr.reset();
        
        // Bus parametri
        for (int i = 0; i < nBus; ++i)
        {
            mStr.appendFormat("\tPL_%d = %.2f; QL_%d = %.2f; PG_%d = %.2f\n",
                buses[i].id, buses[i].pLoad,
                buses[i].id, buses[i].qLoad,
                buses[i].id, buses[i].pGen);
            memOut.put(mStr.c_str(), mStr.length());
            mStr.reset();
        }
        
        // Algebarske jednadžbe
        memOut.put("AEs:\n");
        mStr.appendFormat(
            "\t// Nernst napon SOFC\n"
            "\tv_sofc = E0 + (%.6f * T_sofc / %.1f) * ln(pH2 * sqrt(pO2) / pH2O)"
            " - i_sofc * Rint - 0.1 * ln(1 + i_sofc / 0.01)\n"
            "\t// Snaga SOFC i invertora\n"
            "\tp_sofc = v_sofc * i_sofc\n"
            "\tp_ac   = eta * p_sofc\n",
            R, 2.0 * F);
        memOut.put(mStr.c_str(), mStr.length());
        mStr.reset();
        
        progress = 90;
        
        memOut.put("end\n");
        
        // Zapis u fajl
        std::ofstream fOut;
        if (!fo::createTextFile(fOut, outFileName))
        {
            status = "ERROR! Cannot create output file!";
            success = false;
            done = true;
            return;
        }
        memOut.writeToFile(fOut);
        fOut.close();
        
        progress = 100;
        success = true;
        done = true;
    });

    convThread.join();
    progressThread.join();

    if (success)
        status = "OK! Model successfully created.";

    return success;
}