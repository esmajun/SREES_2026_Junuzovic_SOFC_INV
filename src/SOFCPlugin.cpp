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
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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
static const double R = 8.314;
static const double F = 96485.0;
static const double E0 = 1.229;

static double calcNernstVoltage(const SOFCParams& p)
{
    double lnTerm = std::log(p.pressureH2 * std::sqrt(p.pressureO2) / p.pressureH2O);
    return E0 + (R * p.temperature / (2.0 * F)) * lnTerm;
}

static double calcCellVoltage(const SOFCParams& p, double current)
{
    double vNernst = calcNernstVoltage(p);
    double vOhm = current * p.internalRes;
    double vAct = 0.1 * std::log(1.0 + current / 0.01);
    return vNernst - vOhm - vAct;
}

static double calcSOFCPower(const SOFCParams& p, double current)
{
    double vCell = calcCellVoltage(p, current);
    if (vCell < 0.0) vCell = 0.0;
    return vCell * current;
}

// ============================================================
// IEEE Test case podaci
// ============================================================
struct BusData
{
    int id;
    double pLoad;
    double qLoad;
    double pGen;
    double vMag;
    bool hasSOFC;
};

// Case 9
static BusData case9Buses[] = {
    {1, 0.0,   0.0,   0.0,   1.04,  false},
    {2, 0.0,   0.0,   163.0, 1.025, false},
    {3, 0.0,   0.0,   85.0,  1.025, false},
    {4, 0.0,   0.0,   0.0,   1.0,   false},
    {5, 125.0, 50.0,  0.0,   1.0,   false},
    {6, 90.0,  30.0,  0.0,   1.0,   false},
    {7, 0.0,   0.0,   0.0,   1.0,   false},
    {8, 100.0, 35.0,  0.0,   1.0,   false},
    {9, 0.0,   0.0,   0.0,   1.0,   false}
};

// Case 30
static BusData case30Buses[] = {
    {1,  0.0,   0.0,   0.0,   1.06,  false},
    {2,  21.7,  12.7,  40.0,  1.045, false},
    {3,  2.4,   1.2,   0.0,   1.0,   false},
    {4,  7.6,   1.6,   0.0,   1.0,   false},
    {5,  94.2,  19.0,  0.0,   1.01,  false},
    {6,  0.0,   0.0,   0.0,   1.0,   false},
    {7,  22.8,  10.9,  0.0,   1.0,   false},
    {8,  30.0,  30.0,  0.0,   1.01,  false},
    {9,  0.0,   0.0,   0.0,   1.0,   false},
    {10, 5.8,   2.0,   0.0,   1.0,   false},
    {11, 0.0,   0.0,   0.0,   1.082, false},
    {12, 11.2,  7.5,   0.0,   1.0,   false},
    {13, 0.0,   0.0,   0.0,   1.071, false},
    {14, 6.2,   1.6,   0.0,   1.0,   false},
    {15, 8.2,   2.5,   0.0,   1.0,   false},
    {16, 3.5,   1.8,   0.0,   1.0,   false},
    {17, 9.0,   5.8,   0.0,   1.0,   false},
    {18, 3.2,   0.9,   0.0,   1.0,   false},
    {19, 9.5,   3.4,   0.0,   1.0,   false},
    {20, 2.2,   0.7,   0.0,   1.0,   false},
    {21, 17.5,  11.2,  0.0,   1.0,   false},
    {22, 0.0,   0.0,   0.0,   1.0,   false},
    {23, 3.2,   1.6,   0.0,   1.0,   false},
    {24, 8.7,   6.7,   0.0,   1.0,   false},
    {25, 0.0,   0.0,   0.0,   1.0,   false},
    {26, 3.5,   2.3,   0.0,   1.0,   false},
    {27, 0.0,   0.0,   0.0,   1.0,   false},
    {28, 0.0,   0.0,   0.0,   1.0,   false},
    {29, 2.4,   0.9,   0.0,   1.0,   false},
    {30, 10.6,  1.9,   0.0,   1.0,   false}
};

// Case 118
static BusData case118Buses[] = {
    {1,   51.0,  27.0,  0.0,   0.955, false},
    {2,   20.0,  9.0,   0.0,   0.971, false},
    {3,   39.0,  10.0,  0.0,   0.968, false},
    {4,   39.0,  12.0,  0.0,   0.998, false},
    {5,   0.0,   0.0,   0.0,   1.002, false},
    {6,   52.0,  22.0,  0.0,   0.990, false},
    {7,   19.0,  2.0,   0.0,   0.989, false},
    {8,   0.0,   0.0,   0.0,   1.015, false},
    {9,   0.0,   0.0,   0.0,   1.043, false},
    {10,  0.0,   0.0,   0.0,   1.050, false},
    {11,  70.0,  23.0,  0.0,   0.985, false},
    {12,  47.0,  10.0,  0.0,   0.990, false},
    {13,  34.0,  16.0,  0.0,   0.968, false},
    {14,  14.0,  1.0,   0.0,   0.984, false},
    {15,  90.0,  30.0,  0.0,   0.970, false},
    {16,  25.0,  10.0,  0.0,   0.984, false},
    {17,  11.0,  3.0,   0.0,   0.995, false},
    {18,  60.0,  34.0,  0.0,   0.973, false},
    {19,  45.0,  25.0,  0.0,   0.963, false},
    {20,  18.0,  3.0,   0.0,   0.958, false},
    {21,  14.0,  8.0,   0.0,   0.959, false},
    {22,  10.0,  5.0,   0.0,   0.970, false},
    {23,  7.0,   3.0,   0.0,   1.000, false},
    {24,  13.0,  0.0,   0.0,   0.992, false},
    {25,  0.0,   0.0,   0.0,   1.050, false},
    {26,  0.0,   0.0,   0.0,   1.015, false},
    {27,  71.0,  13.0,  0.0,   0.968, false},
    {28,  17.0,  7.0,   0.0,   0.962, false},
    {29,  24.0,  4.0,   0.0,   0.963, false},
    {30,  0.0,   0.0,   0.0,   0.968, false},
    {31,  43.0,  27.0,  0.0,   0.967, false},
    {32,  59.0,  23.0,  0.0,   0.964, false},
    {33,  23.0,  9.0,   0.0,   0.972, false},
    {34,  59.0,  26.0,  0.0,   0.986, false},
    {35,  33.0,  9.0,   0.0,   0.981, false},
    {36,  31.0,  17.0,  0.0,   0.980, false},
    {37,  0.0,   0.0,   0.0,   0.992, false},
    {38,  0.0,   0.0,   0.0,   0.962, false},
    {39,  27.0,  11.0,  0.0,   0.970, false},
    {40,  66.0,  23.0,  0.0,   0.970, false},
    {41,  37.0,  10.0,  0.0,   0.967, false},
    {42,  96.0,  23.0,  0.0,   0.985, false},
    {43,  18.0,  7.0,   0.0,   0.978, false},
    {44,  16.0,  8.0,   0.0,   0.985, false},
    {45,  53.0,  22.0,  0.0,   0.987, false},
    {46,  28.0,  10.0,  0.0,   1.005, false},
    {47,  34.0,  0.0,   0.0,   1.017, false},
    {48,  20.0,  11.0,  0.0,   1.021, false},
    {49,  87.0,  30.0,  0.0,   1.025, false},
    {50,  17.0,  4.0,   0.0,   1.001, false},
    {51,  17.0,  8.0,   0.0,   0.967, false},
    {52,  18.0,  5.0,   0.0,   0.957, false},
    {53,  23.0,  11.0,  0.0,   0.946, false},
    {54,  113.0, 32.0,  0.0,   0.955, false},
    {55,  63.0,  22.0,  0.0,   0.952, false},
    {56,  84.0,  18.0,  0.0,   0.954, false},
    {57,  12.0,  3.0,   0.0,   0.971, false},
    {58,  12.0,  3.0,   0.0,   0.959, false},
    {59,  277.0, 113.0, 0.0,   0.985, false},
    {60,  78.0,  3.0,   0.0,   0.993, false},
    {61,  0.0,   0.0,   0.0,   0.995, false},
    {62,  77.0,  14.0,  0.0,   0.998, false},
    {63,  0.0,   0.0,   0.0,   0.969, false},
    {64,  0.0,   0.0,   0.0,   0.984, false},
    {65,  0.0,   0.0,   0.0,   1.005, false},
    {66,  39.0,  18.0,  0.0,   1.050, false},
    {67,  28.0,  7.0,   0.0,   1.020, false},
    {68,  0.0,   0.0,   0.0,   1.003, false},
    {69,  0.0,   0.0,   0.0,   1.035, false},
    {70,  66.0,  20.0,  0.0,   0.984, false},
    {71,  0.0,   0.0,   0.0,   0.987, false},
    {72,  12.0,  0.0,   0.0,   0.980, false},
    {73,  6.0,   0.0,   0.0,   0.991, false},
    {74,  68.0,  27.0,  0.0,   0.958, false},
    {75,  47.0,  11.0,  0.0,   0.967, false},
    {76,  68.0,  36.0,  0.0,   0.943, false},
    {77,  61.0,  28.0,  0.0,   1.006, false},
    {78,  71.0,  26.0,  0.0,   1.003, false},
    {79,  39.0,  32.0,  0.0,   1.009, false},
    {80,  130.0, 26.0,  0.0,   1.040, false},
    {81,  0.0,   0.0,   0.0,   0.997, false},
    {82,  54.0,  27.0,  0.0,   0.989, false},
    {83,  20.0,  10.0,  0.0,   0.985, false},
    {84,  11.0,  7.0,   0.0,   0.980, false},
    {85,  24.0,  15.0,  0.0,   0.985, false},
    {86,  21.0,  10.0,  0.0,   0.987, false},
    {87,  0.0,   0.0,   0.0,   1.015, false},
    {88,  48.0,  10.0,  0.0,   0.987, false},
    {89,  0.0,   0.0,   0.0,   1.005, false},
    {90,  163.0, 42.0,  0.0,   0.985, false},
    {91,  10.0,  0.0,   0.0,   0.980, false},
    {92,  65.0,  10.0,  0.0,   0.993, false},
    {93,  12.0,  7.0,   0.0,   0.987, false},
    {94,  30.0,  16.0,  0.0,   0.991, false},
    {95,  42.0,  31.0,  0.0,   0.981, false},
    {96,  38.0,  15.0,  0.0,   0.993, false},
    {97,  15.0,  9.0,   0.0,   1.011, false},
    {98,  34.0,  8.0,   0.0,   0.990, false},
    {99,  42.0,  0.0,   0.0,   1.010, false},
    {100, 37.0,  18.0,  0.0,   1.017, false},
    {101, 22.0,  15.0,  0.0,   0.993, false},
    {102, 5.0,   3.0,   0.0,   0.991, false},
    {103, 23.0,  16.0,  0.0,   1.001, false},
    {104, 38.0,  25.0,  0.0,   0.971, false},
    {105, 31.0,  26.0,  0.0,   0.965, false},
    {106, 43.0,  16.0,  0.0,   0.962, false},
    {107, 28.0,  12.0,  0.0,   0.952, false},
    {108, 2.0,   1.0,   0.0,   0.967, false},
    {109, 8.0,   3.0,   0.0,   0.967, false},
    {110, 39.0,  30.0,  0.0,   0.973, false},
    {111, 0.0,   0.0,   0.0,   0.980, false},
    {112, 68.0,  13.0,  0.0,   0.975, false},
    {113, 6.0,   0.0,   0.0,   0.993, false},
    {114, 8.0,   3.0,   0.0,   0.960, false},
    {115, 22.0,  7.0,   0.0,   0.960, false},
    {116, 184.0, 0.0,   0.0,   1.005, false},
    {117, 20.0,  8.0,   0.0,   0.974, false},
    {118, 33.0,  15.0,  0.0,   0.986, false}
};

// ============================================================
// XML Parser - cita konfiguraciju
// ============================================================
struct XmlConfig
{
    int caseNumber;
    std::vector<int> sofcBusIds;
    bool valid;
};

static XmlConfig parseXmlConfig(const td::String& fileName)
{
    XmlConfig cfg;
    cfg.caseNumber = 9;
    cfg.valid = false;

    std::ifstream f(fileName.c_str());
    if (!f.is_open())
        return cfg;

    std::string line;
    while (std::getline(f, line))
    {
        // Trazi <Case number="9"/>
        auto posCase = line.find("Case");
        if (posCase != std::string::npos)
        {
            auto posNum = line.find("number=\"");
            if (posNum != std::string::npos)
            {
                posNum += 8; // preskoci number="
                auto posEnd = line.find("\"", posNum);
                if (posEnd != std::string::npos)
                {
                    std::string numStr = line.substr(posNum, posEnd - posNum);
                    cfg.caseNumber = std::atoi(numStr.c_str());
                }
            }
        }

        // Trazi <SOFC busId="3"/>
        auto posSOFC = line.find("SOFC");
        if (posSOFC != std::string::npos)
        {
            auto posBus = line.find("busId=\"");
            if (posBus != std::string::npos)
            {
                posBus += 7; // preskoci busId="
                auto posEnd = line.find("\"", posBus);
                if (posEnd != std::string::npos)
                {
                    std::string busStr = line.substr(posBus, posEnd - posBus);
                    cfg.sofcBusIds.push_back(std::atoi(busStr.c_str()));
                }
            }
        }
    }

    f.close();
    cfg.valid = true;
    return cfg;
}

// ============================================================
// createModel
// ============================================================
bool createModel(const td::String& inputFileName,
    const td::String& outFileName,
    sc::IPlugin* pIPlugin,
    const Options& options,
    gui::LineEdit& status)
{
    mu::ScopedCLocale scopedLocale;

    std::atomic<int> progress(0);
    std::atomic<bool> done(false);

    // Thread 2 - progress indikator
    std::thread progressThread([&progress, &done]() {
        while (!done.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        });

    bool success = false;

    // Thread 1 - konverzija
    std::thread convThread([&]() {

        progress = 10;

        // Ucitaj XML konfiguraciju
        XmlConfig xmlCfg = parseXmlConfig(inputFileName);
        if (!xmlCfg.valid)
        {
            status = "ERROR! Cannot parse XML config file!";
            success = false;
            done = true;
            return;
        }

        // Odabir case-a — XML ima prioritet, opcija je fallback
        int caseNum = xmlCfg.caseNumber > 0 ? xmlCfg.caseNumber : options.caseNumber;

        progress = 20;

        // Odabir bus podataka
        BusData* buses = case9Buses;
        int nBus = 9;

        if (caseNum == 9)
        {
            buses = case9Buses;
            nBus = 9;
        }
        else if (caseNum == 30)
        {
            buses = case30Buses;
            nBus = 30;
        }
        else if (caseNum == 118)
        {
            buses = case118Buses;
            nBus = 118;
        }
		else  //postavi kao default case 9 ako nije poznat broj case-a
        {
            buses = case9Buses;
            nBus = 9;
            caseNum = 9;
        }

        // Oznaci SOFC busove iz XML-a
        for (int i = 0; i < nBus; ++i)
            buses[i].hasSOFC = false;

        for (int sofcBusId : xmlCfg.sofcBusIds)
        {
            for (int i = 0; i < nBus; ++i)
            {
                if (buses[i].id == sofcBusId)
                {
                    buses[i].hasSOFC = true;
                    break;
                }
            }
        }

        // Ako nema SOFC busova u XML-u, stavi na prvi bus
        if (xmlCfg.sofcBusIds.empty())
            buses[0].hasSOFC = true;

        progress = 40;

        // SOFC proracun
        const auto& sofc = options.sofcParams;
        double nomCurrent = sofc.ratedPower * 1000.0 /
            calcCellVoltage(sofc, 100.0);
        if (nomCurrent <= 0.0) nomCurrent = 100.0;

        double vSOFC = calcCellVoltage(sofc, nomCurrent);
        double pSOFC = calcSOFCPower(sofc, nomCurrent);
        double pAC = pSOFC * sofc.efficiency;

        progress = 60;

        // Zapis u fajl
        std::ofstream fOut;
        if (!fo::createTextFile(fOut, outFileName))
        {
            status = "ERROR! Cannot create output file!";
            success = false;
            done = true;
            return;
        }

        fOut << "Header:\n";
        fOut << "\tmaxIter = " << options.maxIter << "\n";
        fOut << "\treport = Solver\n";
        fOut << "\tmaxReps = -1\n";
        fOut << "\toutToTxt = false\n";
        fOut << "\ttxtFile = \"\"\n";
        fOut << "\tstartTime = 0\n";
        fOut << "\tdTime = " << options.dTime << "\n";
        fOut << "\tendTime = " << options.endTime << "\n";
        fOut << "end\n\n";

        fOut << "// SOFC + Inverter Model - IEEE Case " << caseNum << "\n";
        fOut << "// Nernst voltage: " << calcNernstVoltage(sofc) << " V\n";
        fOut << "// SOFC DC power:  " << pSOFC << " W\n";
        fOut << "// AC power out:   " << pAC << " W\n\n";

        fOut << "Model [type=DAE domain=real method=RK2 name=\""
            << options.modelName.c_str() << "\"]:\n";

        fOut << "Vars [out=true]:\n\t";
        for (int i = 0; i < nBus; ++i)
            fOut << "v_" << buses[i].id << "; theta_" << buses[i].id << "; ";
        fOut << "i_sofc\n";

        fOut << "Params:\n";
        fOut << "\tT_sofc = " << sofc.temperature << "\n";
        fOut << "\tpH2 = " << sofc.pressureH2 << "\n";
        fOut << "\tpO2 = " << sofc.pressureO2 << "\n";
        fOut << "\tpH2O = " << sofc.pressureH2O << "\n";
        fOut << "\tRint = " << sofc.internalRes << "\n";
        fOut << "\teta = " << sofc.efficiency << "\n";
        fOut << "\tE0 = " << E0 << "\n";
        fOut << "\tv_sofc = 0\t[out=true]\n";
        fOut << "\tp_sofc = 0\t[out=true]\n";
        fOut << "\tp_ac = 0\t[out=true]\n";

        for (int i = 0; i < nBus; ++i)
            fOut << "\tPL_" << buses[i].id << " = " << buses[i].pLoad << "; "
            << "QL_" << buses[i].id << " = " << buses[i].qLoad << "; "
            << "PG_" << buses[i].id << " = " << buses[i].pGen << "\n";

        fOut << "ODEs:\n";
        for (int i = 0; i < nBus; ++i)
        {
            fOut << "\tv_" << buses[i].id << "\' = 0\n";
            fOut << "\ttheta_" << buses[i].id << "\' = 0\n";
        }
        fOut << "\ti_sofc\' = (v_sofc - i_sofc * Rint) / 0.001\n";

        fOut << "PostProc:\n";
        fOut << "\tv_sofc = E0 + (" << R << " * T_sofc / " << 2.0 * F
            << ") * ln(pH2 * sqrt(pO2) / pH2O)"
            << " - i_sofc * Rint - 0.1 * ln(1 + i_sofc / 0.01)\n";
        fOut << "\tp_sofc = v_sofc * i_sofc\n";
        fOut << "\tp_ac = eta * p_sofc\n";
        fOut << "end\n";

        fOut.close();

        progress = 80;

        // Vizualni model
        td::String strVisualFileName = fo::replaceFileExtension<false>(outFileName, ".vmodl");
        std::ofstream fVis;
        if (fo::createTextFile(fVis, strVisualFileName))
        {
            fVis << "Header:\n";
            fVis << "\tnewTab = false\n";
            fVis << "\tdrawPlots = true\n";
            fVis << "end\n";
            fVis << "Model [name=\"SOFC + Inverter Results - Case "
                << caseNum << "\"]:\n";
            fVis << "Plots [backColor=auto]:\n";
            fVis << "\tlinePlot [xLabel=\"Time [s]\" yLabel=\"Current [A]\""
                << " name=\"SOFC Current\" legend=true]:\n";
            fVis << "\t\t@x << t\n";
            fVis << "\t\t@y << i_sofc [colorL=black colorD=red width=2 name=\"i_sofc\"]\n";
            fVis << "\tend\n";
            fVis << "\tlinePlot [xLabel=\"Time [s]\" yLabel=\"Power [W]\""
                << " name=\"SOFC Power\" legend=true]:\n";
            fVis << "\t\t@x << t\n";
            fVis << "\t\t@y << p_sofc [colorL=darkBlue colorD=cyan width=2 name=\"p_sofc\"]\n";
            fVis << "\t\t@y << p_ac [colorL=darkGreen colorD=green width=2 name=\"p_ac\"]\n";
            fVis << "\tend\n";
            fVis << "end\n";
            fVis.close();
        }

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