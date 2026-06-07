#pragma once
#include <gui/View.h>
#include <gui/Label.h>
#include <gui/Button.h>
#include <gui/LineEdit.h>
#include <gui/TextEdit.h>
#include <gui/HorizontalLayout.h>
#include <gui/GridLayout.h>
#include <gui/GridComposer.h>
//  #include <gui/ProgressBar.h>
#include <fo/FileOperations.h>
#include <gui/FileDialog.h>
#include "ViewOptions.h"

class ViewConv : public gui::View
{
protected:
    sc::IPlugin* _pIPlugin;
    sc::IPlugin::CallBack _onComplete;
    ViewOptions* _pViewOptions = nullptr;

    gui::Label _lblFnIn;
    gui::LineEdit _editFnIn;
    gui::Label _lblFnOut;
    gui::LineEdit _editFnOut;
    gui::Label _lblStatus;
    gui::LineEdit _editStatus;
    gui::Button _btnSelectInFn;
    gui::Button _btnSelectOutFn;
    //  gui::Label _lblProgress;
    //  gui::ProgressBar _progressBar;
    gui::Button _btnConvert;
    gui::HorizontalLayout _hlButtons;
    gui::GridLayout _gl;
    td::UINT4 _wndID;

protected:
    void handleUserActions()
    {
        // Odabir input fajla (XML config)
        _btnSelectInFn.onClick([this]{
            gui::OpenFileDialog::show(this, tr("openXMLConfig"), "*.xml", _wndID + 1000, [this](gui::FileDialog* pDlg)
            {
                auto status = pDlg->getStatus();
                if (status == gui::FileDialog::Status::OK)
                {
                    td::String fileName = pDlg->getFileName();
                    if (fileName.isEmpty())
                        return;
                    _editFnIn = fileName;
                    _editFnIn.setFocus();
                }
            });
        });

        // Odabir output fajla
        _btnSelectOutFn.onClick([this]{
            gui::SaveFileDialog::show(this, tr("createDmodl"), "*.dmodl", _wndID + 2000, [this](gui::FileDialog* pDlg)
            {
                auto status = pDlg->getStatus();
                if (status == gui::FileDialog::Status::OK)
                {
                    td::String fileName = pDlg->getFileName();
                    if (fileName.isEmpty())
                        return;
                    _editFnOut = fileName;
                    _editFnOut.setFocus();
                }
            });
        });

        // Convert dugme
        _btnConvert.onClick([this]{
            td::String inputFileName = _editFnIn.getText();
            if (inputFileName.isEmpty())
            {
                _editStatus = "ERROR! Empty input file name";
                return;
            }
            if (!fo::fileExists(inputFileName))
            {
                _editStatus = "ERROR! Input file does not exist";
                return;
            }
            td::String outFileName = _editFnOut.getText();
            if (outFileName.isEmpty())
            {
                _editStatus = "ERROR! Empty output file name";
                return;
            }

            //  _progressBar.setValue(0);
            _editStatus = "Converting...";

            const auto& options = _pViewOptions->getOptions();
            if (!createModel(inputFileName, outFileName, _pIPlugin, options, _editStatus))
                return;

            //  _progressBar.setValue(100);
            _onComplete(_pIPlugin);

            gui::Window* pWnd = getParentWindow();
            pWnd->close();
            onClosedPluginWindow();
        });
    }

    ViewConv() = delete;

public:
    ViewConv(sc::IPlugin* pIPlugin, const sc::IPlugin::CallBack& onComplete)
    : _pIPlugin(pIPlugin)
    , _onComplete(onComplete)
    , _lblFnIn(tr("In File Name:"))
    , _lblFnOut(tr("Out File Name:"))
    , _lblStatus(tr("Status:"))
    //  , _lblProgress(tr("Progress:"))
    , _btnSelectInFn("…")
    , _btnSelectOutFn("…")
    , _btnConvert(tr("Convert"))
    , _hlButtons(2)
    , _gl(5, 3)  //promijenjeno sa _gl(6,3) na _gl(5,3)
    {
        assert(_pIPlugin);
        _editStatus.setAsReadOnly();

        gui::GridComposer gc(_gl);
        gc.appendRow(_lblFnIn)    << _editFnIn    << _btnSelectInFn;
        gc.appendRow(_lblFnOut)   << _editFnOut   << _btnSelectOutFn;
        gc.appendRow(_lblStatus);  gc.appendCol(_editStatus, 0);
        //  gc.appendRow(_lblProgress); gc.appendCol(_progressBar, 0);
        _hlButtons.appendSpacer() << _btnConvert;
        gc.appendRow(_hlButtons, 0);

        setLayout(&_gl);
        handleUserActions();
    }

    void setOptions(ViewOptions* pViewOptions)
    {
        _pViewOptions = pViewOptions;
    }

    td::String getOutFileName() const
    {
        return _editFnOut.getText();
    }
};