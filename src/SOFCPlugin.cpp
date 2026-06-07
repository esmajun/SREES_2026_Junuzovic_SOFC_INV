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

static const double R  = 8.314;
static const double F  = 96485.0;
static const double E0 = 1.229;

static double calcNernstVoltage(const SOFCParams& p)
{
    double lnTerm = std::log(p.pressureH2 * std::sqrt(p.pressureO2) / p.pressureH2O);
    return E0 + (R * p.temperature / (2.0 * F)) * lnTerm;
}

static double calcCellVoltage(const SOFCParams& p, double current)
{
    double vNernst = calcNernstVoltage(p);
    double vOhm    = current * p.internalRes;
    double vAct    = 0.1 * std::log(1.0 + current / 0.01);
    return vNernst - vOhm - vAct;
}

static double calcSOFCPower(const SOFCParams& p, double current)
{
    double vCell = calcCellVoltage(p, current);
    if (vCell < 0.0) vCell = 0.0;
    return vCell * current;
}

// ============================================================
// IEEE Test case podaci (Case 9)
// ============================================================
struct BusData
{
    int id;
    double pLoad;
    double qLoad;
    double pGen;
    double vMag;
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

    // Thread 1 - konverzija
    bool success = false;
    std::thread convThread([&]() {

        progress = 10;

        int nBus = case9BusCount;
        BusData* buses = case9Buses;

        if (options.caseNumber == 9)
        {
            nBus = 9;
            buses = case9Buses;
        }

        progress = 30;

        const auto& sofc = options.sofcParams;
        double nomCurrent = sofc.ratedPower * 1000.0 /
                           calcCellVoltage(sofc, 100.0);
        if (nomCurrent <= 0.0) nomCurrent = 100.0;

        double vSOFC = calcCellVoltage(sofc, nomCurrent);
        double pSOFC = calcSOFCPower(sofc, nomCurrent);
        double pAC   = pSOFC * sofc.efficiency;

        progress = 60;

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

        fOut << "// SOFC + Inverter Model - IEEE Case " << options.caseNumber << "\n";
        fOut << "// Nernst voltage: " << calcNernstVoltage(sofc) << " V\n";
        fOut << "// SOFC DC power: " << pSOFC << " W\n";
        fOut << "// AC power out: " << pAC << " W\n\n";

        fOut << "Model [type=DAE domain=real method=RK2 name=\"" << options.modelName.c_str() << "\"]:\n";
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
// Napon busova - jednostavna dinamika (drže se na nominalnoj vrijednosti)
for (int i = 0; i < nBus; ++i)
{
    fOut << "\tv_" << buses[i].id << "\' = 0\n";
    fOut << "\ttheta_" << buses[i].id << "\' = 0\n";
}
fOut << "\ti_sofc\' = (v_sofc - i_sofc * Rint) / 0.001\n";

        fOut << "PostProc:\n";
        fOut << "\tv_sofc = E0 + (" << R << " * T_sofc / " << 2.0*F << ") * ln(pH2 * sqrt(pO2) / pH2O)";
        fOut << " - i_sofc * Rint - 0.1 * ln(1 + i_sofc / 0.01)\n";
        fOut << "\tp_sofc = v_sofc * i_sofc\n";
        fOut << "\tp_ac = eta * p_sofc\n";
        fOut << "end\n";

        fOut.close();

// Kreiraj vizualni model
td::String strVisualFileName = fo::replaceFileExtension<false>(outFileName, ".vmodl");
std::ofstream fVis;
if (fo::createTextFile(fVis, strVisualFileName))
{
    fVis << "Header:\n";
    fVis << "\tnewTab = false\n";
    fVis << "\tdrawPlots = true\n";
    fVis << "end\n";
    fVis << "Model [name=\"SOFC + Inverter Results\"]:\n";
    fVis << "Plots [backColor=auto]:\n";
    fVis << "\tlinePlot [xLabel=\"Time [s]\" yLabel=\"Current [A]\" name=\"SOFC Current\" legend=true]:\n";
    fVis << "\t\t@x << t\n";
    fVis << "\t\t@y << i_sofc [colorL=black colorD=red width=2 name=\"i_sofc\"]\n";
    fVis << "\tend\n";
    fVis << "\tlinePlot [xLabel=\"Time [s]\" yLabel=\"Power [W]\" name=\"SOFC Power\" legend=true]:\n";
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