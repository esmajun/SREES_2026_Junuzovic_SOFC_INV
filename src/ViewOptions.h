#pragma once
#include <gui/View.h>
#include <gui/Label.h>
#include <gui/LineEdit.h>
#include <gui/NumericEdit.h>
#include <gui/GridLayout.h>
#include <gui/GridComposer.h>
#include "SOFCPlugin.h"

class ViewOptions : public gui::View
{
protected:
    // Model
    gui::Label _lblName;
    gui::LineEdit _editName;

    // Case
    gui::Label _lblCase;
    gui::LineEdit _editCase;

    // Solver
    gui::Label _lblMaxIter;
    gui::NumericEdit _neMaxIter;
    gui::Label _lbldT;
    gui::NumericEdit _neDeltaTime;
    gui::Label _lblEndT;
    gui::NumericEdit _neEndTime;

    // SOFC parametri
    gui::Label _lblTemp;
    gui::NumericEdit _neTemp;
    gui::Label _lblPressH2;
    gui::NumericEdit _nePressH2;
    gui::Label _lblPressO2;
    gui::NumericEdit _nePressO2;
    gui::Label _lblPressH2O;
    gui::NumericEdit _nePressH2O;
    gui::Label _lblIntRes;
    gui::NumericEdit _neIntRes;
    gui::Label _lblEffInv;
    gui::NumericEdit _neEffInv;
    gui::Label _lblRatedPwr;
    gui::NumericEdit _neRatedPwr;

    // Termicki parametri
    gui::Label _lblThermalMass;
    gui::NumericEdit _neThermalMass;
    gui::Label _lblHeatLoss;
    gui::NumericEdit _neHeatLoss;
    gui::Label _lblFuelUtil;
    gui::NumericEdit _neFuelUtil;

    gui::GridLayout _gl;
    Options _options;

public:
    ViewOptions()
        : _lblName(tr("Model name:"))
        , _lblCase(tr("Case (9/30/118/300):"))
        , _lblMaxIter(tr("Max iters:"))
        , _lbldT(tr("dT [s]:"))
        , _lblEndT(tr("End time [s]:"))
        , _lblTemp(tr("Temperature [K]:"))
        , _lblPressH2(tr("Pressure H2 [atm]:"))
        , _lblPressO2(tr("Pressure O2 [atm]:"))
        , _lblPressH2O(tr("Pressure H2O [atm]:"))
        , _lblIntRes(tr("Internal resistance [Ohm]:"))
        , _lblEffInv(tr("Inverter efficiency [0-1]:"))
        , _lblRatedPwr(tr("Rated power [kW]:"))
        , _lblThermalMass(tr("Thermal mass [J/K]:"))
        , _lblHeatLoss(tr("Heat loss coeff [W/K]:"))
        , _lblFuelUtil(tr("Fuel utilization [0-1]:"))
        , _neMaxIter(td::int4)
        , _neDeltaTime(td::real8)
        , _neEndTime(td::real8)
        , _neTemp(td::real8)
        , _nePressH2(td::real8)
        , _nePressO2(td::real8)
        , _nePressH2O(td::real8)
        , _neIntRes(td::real8)
        , _neEffInv(td::real8)
        , _neRatedPwr(td::real8)
        , _neThermalMass(td::real8)
        , _neHeatLoss(td::real8)
        , _neFuelUtil(td::real8)
        , _gl(17, 2)
    {
        // Defaultne vrijednosti
        _editName = "SOFC Power System Model";
        _editCase = "9";
        _neMaxIter.setValue(td::INT4(50));
        _neDeltaTime.setValue(0.01);
        _neEndTime.setValue(10.0);
        _neTemp.setValue(1073.15);
        _nePressH2.setValue(1.0);
        _nePressO2.setValue(0.21);
        _nePressH2O.setValue(0.1);
        _neIntRes.setValue(0.01);
        _neEffInv.setValue(0.96);
        _neRatedPwr.setValue(100.0);
        _neThermalMass.setValue(1000.0);  // J/K
        _neHeatLoss.setValue(5.0);         // W/K
        _neFuelUtil.setValue(0.85);        // 85%

        gui::GridComposer gc(_gl);
        gc.appendRow(_lblName);         gc.appendCol(_editName);
        gc.appendRow(_lblCase);         gc.appendCol(_editCase);
        gc.appendRow(_lblMaxIter) << _neMaxIter;
        gc.appendRow(_lbldT) << _neDeltaTime;
        gc.appendRow(_lblEndT) << _neEndTime;
        gc.appendRow(_lblTemp) << _neTemp;
        gc.appendRow(_lblPressH2) << _nePressH2;
        gc.appendRow(_lblPressO2) << _nePressO2;
        gc.appendRow(_lblPressH2O) << _nePressH2O;
        gc.appendRow(_lblIntRes) << _neIntRes;
        gc.appendRow(_lblEffInv) << _neEffInv;
        gc.appendRow(_lblRatedPwr) << _neRatedPwr;
        gc.appendRow(_lblThermalMass) << _neThermalMass;
        gc.appendRow(_lblHeatLoss) << _neHeatLoss;
        gc.appendRow(_lblFuelUtil) << _neFuelUtil;

        setLayout(&_gl);
    }

    const Options& getOptions()
    {
        _options.modelName = _editName.getText();
        _options.caseNumber = std::atoi(_editCase.getText().c_str());
        _options.maxIter = _neMaxIter.getValue().i4Val();
        _options.dTime = _neDeltaTime.getValue().r8Val();
        _options.endTime = _neEndTime.getValue().r8Val();
        _options.sofcParams.temperature = _neTemp.getValue().r8Val();
        _options.sofcParams.pressureH2 = _nePressH2.getValue().r8Val();
        _options.sofcParams.pressureO2 = _nePressO2.getValue().r8Val();
        _options.sofcParams.pressureH2O = _nePressH2O.getValue().r8Val();
        _options.sofcParams.internalRes = _neIntRes.getValue().r8Val();
        _options.sofcParams.efficiency = _neEffInv.getValue().r8Val();
        _options.sofcParams.ratedPower = _neRatedPwr.getValue().r8Val();
        _options.sofcParams.thermalMass = _neThermalMass.getValue().r8Val();
        _options.sofcParams.heatLoss = _neHeatLoss.getValue().r8Val();
        _options.sofcParams.fuelUtil = _neFuelUtil.getValue().r8Val();
        return _options;
    }
};