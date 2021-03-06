/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#include "route/routeexport.h"

#include "gui/mainwindow.h"
#include "common/constants.h"
#include "common/aircrafttrack.h"
#include "fs/perf/aircraftperf.h"
#include "route/route.h"
#include "route/routecontroller.h"
#include "io/fileroller.h"
#include "options/optiondata.h"
#include "gui/dialog.h"
#include "ui_mainwindow.h"
#include "fs/pln/flightplanio.h"
#include "navapp.h"
#include "atools.h"
#include "routestring/routestringwriter.h"
#include "gui/errorhandler.h"
#include "options/optiondata.h"
#include "gui/dialog.h"
#include "route/routeexportdata.h"
#include "route/routealtitude.h"
#include "ui_mainwindow.h"
#include "fs/pln/flightplanio.h"
#include "exception.h"

#include <QBitArray>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>
#include <QXmlStreamReader>

RouteExport::RouteExport(MainWindow *parent)
  : mainWindow(parent)
{
  documentsLocation = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation).first();
  dialog = new atools::gui::Dialog(mainWindow);
  flightplanIO = new atools::fs::pln::FlightplanIO;
}

RouteExport::~RouteExport()
{
  delete dialog;
  delete flightplanIO;
}

/* Called from menu or toolbar by action */
bool RouteExport::routeExportRxpGns()
{
  qDebug() << Q_FUNC_INFO;

  // Save flight plan as FPL file usable by the GNS 530W/430W V2 - XML format
  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString path;

#ifdef Q_OS_WIN32
    QString gnsPath(qgetenv("GNSAPPDATA"));
    path = gnsPath.isEmpty() ? "C:\\ProgramData\\Garmin\\GNS Trainer Data\\GNS\\FPL" : gnsPath + "\\FPL";
#elif DEBUG_INFORMATION
    path = atools::buildPath({documentsLocation, "Garmin", "GNS Trainer Data", "GNS", "FPL"});
#else
    path = documentsLocation;
#endif

    bool mkdir = QDir(path).mkpath(path);
    qInfo() << "mkdir" << path << "result" << mkdir;

    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan as FPL for Reality XP GNS"),
      tr("FPL Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_FPL),
      "fpl", "Route/RxpGns", path, buildDefaultFilenameShort("", ".fpl"),
      false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

    if(!routeFile.isEmpty())
    {
      if(exportFlighplanAsRxpGns(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved as FPL."));
        return true;
      }
    }
  }
  return false;
}

/* Called from menu or toolbar by action */
bool RouteExport::routeExportRxpGtn()
{
  qDebug() << Q_FUNC_INFO;

  // Save flight plan as GFP file usable by the Reality XP GTN 750/650 Touch
  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString path;

    // Location depends on trainer version - this is all above 6.41
#ifdef Q_OS_WIN32
    QString gtnPath(qgetenv("GTNSIMDATA"));
    path = gtnPath.isEmpty() ? "C:\\ProgramData\\Garmin\\Trainers\\Databases\\FPLN" : gtnPath + "\\Databases\\FPLN";
#elif DEBUG_INFORMATION
    path = atools::buildPath({documentsLocation, "Garmin", "Trainers", "GTN", "FPLN"});
#else
    path = documentsLocation;
#endif

    bool mkdir = QDir(path).mkpath(path);
    qInfo() << "mkdir" << path << "result" << mkdir;

    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan as GFP for Reality XP GTN"),
      tr("Garmin GFP Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_GFP),
      "gfp", "Route/RxpGfp", path,
      buildDefaultFilenameShort("_", ".gfp"), false, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

    if(!routeFile.isEmpty())
    {
      if(exportFlighplanAsRxpGtn(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved as GFP."));
        return true;
      }
    }
  }
  return false;
}

/* Called from menu or toolbar by action */
bool RouteExport::routeExportGfp()
{
  qDebug() << Q_FUNC_INFO;

  // <FSX/P3D>/F1GTN/FPL.
  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan as Garmin GFP Format"),
      tr("Garmin GFP Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_GFP),
      "gfp", "Route/Gfp",
      NavApp::getCurrentSimulatorBasePath() + QDir::separator() + "F1GTN" + QDir::separator() + "FPL",
      buildDefaultFilenameShort("-", ".gfp"),
      false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

    if(!routeFile.isEmpty())
    {
      if(exportFlighplanAsGfp(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved as GFP."));
        return true;
      }
    }
  }
  return false;
}

/* Called from menu or toolbar by action */
bool RouteExport::routeExportTxt()
{
  qDebug() << Q_FUNC_INFO;

  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan as TXT Format"),
      tr("Text Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_TXT), "txt", "Route/Txt",
      NavApp::getCurrentSimulatorBasePath() + QDir::separator() + "Aircraft",
      buildDefaultFilenameShort(QString(), ".txt"),
      false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

    if(!routeFile.isEmpty())
    {
      if(exportFlighplanAsTxt(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved as TXT."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportRte()
{
  qDebug() << Q_FUNC_INFO;

  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan as PMDG RTE Format"),
      tr("RTE Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_RTE),
      "rte", "Route/Rte",
      NavApp::getCurrentSimulatorBasePath() + QDir::separator() + "PMDG" + QDir::separator() + "FLIGHTPLANS",
      buildDefaultFilenameShort(QString(), ".rte"),
      false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

    if(!routeFile.isEmpty())
    {
      using namespace std::placeholders;
      if(exportFlighplan(routeFile, std::bind(&atools::fs::pln::FlightplanIO::saveRte, flightplanIO, _1, _2)))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved as RTE."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportFpr()
{
  qDebug() << Q_FUNC_INFO;

  // <FSX/P3D>/SimObjects/Airplanes/mjc8q400/nav/routes.

  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan as Majestic Dash FPR"),
      tr("FPR Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_FPR),
      "fpr", "Route/Fpr",
      NavApp::getCurrentSimulatorBasePath() +
      QDir::separator() + "SimObjects" +
      QDir::separator() + "Airplanes" +
      QDir::separator() + "mjc8q400" +
      QDir::separator() + "nav" +
      QDir::separator() + "routes",
      buildDefaultFilenameShort(QString(), ".fpr"));

    if(!routeFile.isEmpty())
    {
      using namespace std::placeholders;
      if(exportFlighplan(routeFile, std::bind(&atools::fs::pln::FlightplanIO::saveFpr, flightplanIO, _1, _2)))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved as FPR."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportFpl()
{
  qDebug() << Q_FUNC_INFO;

  // \X-Plane 11\Aircraft\X-Aviation\IXEG 737 Classic\coroutes
  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan as IXEG FPL Format"),
      tr("FPL Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_FPL),
      "fpl", "Route/Fpl",
      NavApp::getCurrentSimulatorBasePath() +
      QDir::separator() + "Aircraft" +
      QDir::separator() + "X-Aviation" +
      QDir::separator() + "IXEG 737 Classic" +
      QDir::separator() + "coroutes",
      buildDefaultFilenameShort(QString(), ".fpl"),
      false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

    if(!routeFile.isEmpty())
    {
      // Same format as txt
      if(exportFlighplanAsTxt(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved as FPL."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportCorteIn()
{
  qDebug() << Q_FUNC_INFO;

  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan to corte.in for Flight Factor Airbus"),
      tr("corte.in Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_CORTEIN),
      ".in", "Route/CorteIn",
      NavApp::getCurrentSimulatorBasePath() + QDir::separator() + "Aircraft", "corte.in",
      true /* dont confirm overwrite */);

    if(!routeFile.isEmpty())
    {
      if(exportFlighplanAsCorteIn(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan added to corte.in."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportFltplan()
{
  qDebug() << Q_FUNC_INFO;
  // Default directory for the iFly stored Flight plans is Prepar3D/iFly/737NG/navdata/FLTPLAN
  // <FSX/P3D>/iFly/737NG/navdata/FLTPLAN.
  // YSSYYMML.FLTPLAN

  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan as FLTPLAN for iFly"),
      tr("iFly FLTPLAN Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_FLTPLAN), "fltplan", "Route/Fltplan",
      NavApp::getCurrentSimulatorBasePath() +
      QDir::separator() + "iFly" +
      QDir::separator() + "737NG" +
      QDir::separator() + "navdata" +
      QDir::separator() + "FLTPLAN",
      buildDefaultFilenameShort(QString(), ".fltplan"),
      false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

    if(!routeFile.isEmpty())
    {
      using namespace std::placeholders;
      if(exportFlighplan(routeFile, std::bind(&atools::fs::pln::FlightplanIO::saveFltplan, flightplanIO, _1, _2)))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved as FLTPLAN for iFly."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportXFmc()
{
  qDebug() << Q_FUNC_INFO;
  // The plans have an extension of *.FPL and are saved in for following Folder : -
  // Path to Root X-Plane 11 Folder\Resources\plugins\XFMC\FlightPlans
  // LFLLEHAM.FPL
  // Same as TXT but FPL

  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan as FPL for X-FMC"),
      tr("X-FMC Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_FPL), "fpl", "Route/XFmc",
      NavApp::getCurrentSimulatorBasePath() +
      QDir::separator() + "Resources" +
      QDir::separator() + "plugins" +
      QDir::separator() + "XFMC" +
      QDir::separator() + "FlightPlans",
      buildDefaultFilenameShort(QString(), ".fpl"),
      false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

    if(!routeFile.isEmpty())
    {
      if(exportFlighplanAsTxt(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved as FPL for X-FMC."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportUFmc()
{
  qDebug() << Q_FUNC_INFO;
  // EDDHLIRF.ufmc
  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan for UFMC"),
      tr("UFMC Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_UFMC), "ufmc", "Route/UFmc",
      NavApp::getCurrentSimulatorBasePath(),
      buildDefaultFilenameShort(QString(), ".ufmc"),
      false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

    if(!routeFile.isEmpty())
    {
      if(exportFlighplanAsUFmc(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved for UFMC."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportProSim()
{
  // companyroutes.xml
  qDebug() << Q_FUNC_INFO;

  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan to companyroutes.xml for ProSim"),
      tr("companyroutes.xml Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_COMPANYROUTES_XML),
      ".xml", "Route/CompanyRoutesXml",
      documentsLocation, "companyroutes.xml",
      true /* dont confirm overwrite */);

    if(!routeFile.isEmpty())
    {
      if(exportFlighplanAsProSim(routeFile))
      {
        mainWindow->setStatusMessage(tr("Flight plan added to companyroutes.xml."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportBbs()
{
  qDebug() << Q_FUNC_INFO;
  // P3D Root Folder \BlackBox Simulation\Airbus A330
  // <FSX/P3D>/Blackbox Simulation/Company Routes.
  // Uses FS9 PLN format.   EDDHLIRF.pln

  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan for BBS Airbus"),
      tr("PLN Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_BBS_PLN), "pln", "Route/BbsPln",
      NavApp::getCurrentSimulatorBasePath() +
      QDir::separator() + "Blackbox Simulation" +
      QDir::separator() + "Company Routes",
      buildDefaultFilenameShort(QString(), ".pln"),
      false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

    if(!routeFile.isEmpty())
    {
      using namespace std::placeholders;
      if(exportFlighplan(routeFile, std::bind(&atools::fs::pln::FlightplanIO::saveBbsPln, flightplanIO, _1, _2)))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved for BBS."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportFeelthereFpl()
{
  qDebug() << Q_FUNC_INFO;
  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan for FeelThere Aircraft"),
      tr("FPL Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_FPL), "fpl", "Route/FeelThereFpl",
      NavApp::getCurrentSimulatorBasePath(),
      buildDefaultFilenameShort("_", ".fpl"),
      false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

    if(!routeFile.isEmpty())
    {
      int groundSpeed = atools::roundToInt(NavApp::getAltitudeLegs().getAverageGroundSpeed());
      if(groundSpeed < 5)
        groundSpeed = atools::roundToInt(NavApp::getAircraftPerformance().getCruiseSpeed());

      using namespace std::placeholders;
      if(exportFlighplan(routeFile,
                         std::bind(&atools::fs::pln::FlightplanIO::saveFeelthereFpl, flightplanIO,
                                   _1, _2, groundSpeed)))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved for FeelThere."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportLeveldRte()
{
  qDebug() << Q_FUNC_INFO;
  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan for Level-D Aircraft"),
      tr("RTE Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_RTE), "rte", "Route/LeveldRte",
      NavApp::getCurrentSimulatorBasePath() +
      QDir::separator() + "Level-D Simulations" +
      QDir::separator() + "navdata" +
      QDir::separator() + "Flightplans",
      buildDefaultFilenameShort("_", ".rte"),
      false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

    if(!routeFile.isEmpty())
    {
      using namespace std::placeholders;
      if(exportFlighplan(routeFile, std::bind(&atools::fs::pln::FlightplanIO::saveLeveldRte, flightplanIO, _1, _2)))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved for Level-D."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportEfbr()
{
  qDebug() << Q_FUNC_INFO;
  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan for the AivlaSoft EFB"),
      tr("EFBR Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_EFBR), "efbr", "Route/Efb",
      documentsLocation,
      buildDefaultFilenameShort("_", ".efbr"),
      false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

    if(!routeFile.isEmpty())
    {
      QString route = RouteStringWriter().createStringForRoute(NavApp::getRouteConst(), 0.f, rs::NONE);
      QString cycle = NavApp::getDatabaseAiracCycleNav();
      using namespace std::placeholders;
      if(exportFlighplan(routeFile,
                         std::bind(&atools::fs::pln::FlightplanIO::saveEfbr, flightplanIO, _1, _2,
                                   route, cycle, QString(), QString())))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved for EFB."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportQwRte()
{
  qDebug() << Q_FUNC_INFO;
  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan for QualityWings Aircraft"),
      tr("RTE Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_RTE), "rte", "Route/QwRte",
      NavApp::getCurrentSimulatorBasePath(),
      buildDefaultFilenameShort(QString(), ".rte"),
      false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

    if(!routeFile.isEmpty())
    {
      using namespace std::placeholders;
      if(exportFlighplan(routeFile, std::bind(&atools::fs::pln::FlightplanIO::saveQwRte, flightplanIO, _1, _2)))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved for QualityWings."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportMdr()
{
  qDebug() << Q_FUNC_INFO;
  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan for Maddog X Aircraft"),
      tr("MDR Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_MDR), "mdr", "Route/Mdx",
      NavApp::getCurrentSimulatorBasePath(),
      buildDefaultFilenameShort(QString(), ".mdr"),
      false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

    if(!routeFile.isEmpty())
    {
      using namespace std::placeholders;
      if(exportFlighplan(routeFile, std::bind(&atools::fs::pln::FlightplanIO::saveMdr, flightplanIO, _1, _2)))
      {
        mainWindow->setStatusMessage(tr("Flight plan saved for Maddog X."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportTfdi()
{
  // {Simulator}\SimObjects\Airplanes\TFDi_Design_717\Documents\Company Routes/

  qDebug() << Q_FUNC_INFO;
  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    QString routeFile = dialog->saveFileDialog(
      tr("Save Flight Plan for TFDi Design 717"),
      tr("XML Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_TFDI), "xml", "Route/Tfdi",
      NavApp::getCurrentSimulatorBasePath() +
      QDir::separator() + "SimObjects" +
      QDir::separator() + "Airplanes" +
      QDir::separator() + "TFDi_Design_717" +
      QDir::separator() + "Documents" +
      QDir::separator() + "Company Routes",
      buildDefaultFilenameShort(QString(), ".xml"),
      false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

    if(!routeFile.isEmpty())
    {
      Route route = routeAdjustedToProcedureOptions(true /* replace custom procedure waypoints*/,
                                                    true /* remove alternates */);
      try
      {
        flightplanIO->saveTfdi(route.getFlightplan(), routeFile, route.getJetAirwayFlags());
      }
      catch(atools::Exception& e)
      {
        atools::gui::ErrorHandler(mainWindow).handleException(e);
        return false;
      }
      catch(...)
      {
        atools::gui::ErrorHandler(mainWindow).handleUnknownException();
        return false;
      }

      mainWindow->setStatusMessage(tr("Flight plan saved for vPilot."));
      return true;
    }
  }
  return false;
}

bool RouteExport::routeExportVfp()
{
  qDebug() << Q_FUNC_INFO;
  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    RouteExportData exportData = createRouteExportData(re::VFP);
    if(routeExportDialog(exportData, re::VFP))
    {
      QString routeFile = dialog->saveFileDialog(
        tr("Export Flight Plan as vPilot VFP"),
        tr("VFP Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_VFP), "vfp", "Route/Vfp",
        documentsLocation,
        buildDefaultFilenameShort(QString(), ".vfp"),
        false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

      if(!routeFile.isEmpty())
      {
        if(exportFlighplanAsVfp(exportData, routeFile))
        {
          mainWindow->setStatusMessage(tr("Flight plan saved for vPilot."));
          return true;
        }
      }
    }
  }
  return false;
}

bool RouteExport::routeExportXIvap()
{
  return routeExportIvapInternal(re::XIVAP);
}

bool RouteExport::routeExportIvap()
{
  return routeExportIvapInternal(re::IVAP);
}

bool RouteExport::routeExportIvapInternal(re::RouteExportType type)
{
  qDebug() << Q_FUNC_INFO;

  if(routeValidate(false /* validate parking */, true /* validate departure and destination */))
  {
    RouteExportData exportData = createRouteExportData(type);
    if(routeExportDialog(exportData, type))
    {
      QString typeStr = RouteExportDialog::getRouteTypeAsDisplayString(type);
      QString routeFile = dialog->saveFileDialog(
        tr("Export Flight Plan as %1 FPL").arg(typeStr),
        tr("FPL Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_FPL), "fpl",
        "Route/" + RouteExportDialog::getRouteTypeAsString(type),
        documentsLocation,
        buildDefaultFilenameShort(QString(), ".fpl"),
        false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

      if(!routeFile.isEmpty())
      {
        if(exportFlighplanAsIvap(exportData, routeFile, type))
        {
          mainWindow->setStatusMessage(tr("Flight plan saved for %1.").arg(typeStr));
          return true;
        }
      }
    }
  }
  return false;
}

RouteExportData RouteExport::createRouteExportData(re::RouteExportType routeExportType)
{
  RouteExportData exportData;

  const Route& route = NavApp::getRouteConst();
  exportData.setRoute(RouteStringWriter().createStringForRoute(route, 0.f, rs::SID_STAR));
  exportData.setDeparture(route.getFlightplan().getDepartureIdent());
  exportData.setDestination(route.getFlightplan().getDestinationIdent());
  exportData.setDepartureTime(QDateTime::currentDateTimeUtc().time());
  exportData.setDepartureTimeActual(QDateTime::currentDateTimeUtc().time());
  exportData.setCruiseAltitude(atools::roundToInt(route.getCruisingAltitudeFeet()));

  QStringList alternates = route.getAlternateIdents();
  if(alternates.size() > 0)
    exportData.setAlternate(alternates.at(0));
  if(alternates.size() > 1)
    exportData.setAlternate2(alternates.at(1));

  atools::fs::pln::FlightplanType flightplanType = route.getFlightplan().getFlightplanType();
  switch(routeExportType)
  {
    case re::UNKNOWN:
      break;

    case re::VFP:
      // <?xml version="1.0" encoding="utf-8"?>
      // <FlightPlan xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema"
      // FlightType="IFR"
      exportData.setFlightRules(flightplanType == atools::fs::pln::IFR ? "IFR" : "VFR");
      break;

    case re::IVAP:
    case re::XIVAP:

      // [FLIGHTPLAN]
      // FLIGHTTYPE=N
      // RULES=I
      exportData.setFlightRules(flightplanType == atools::fs::pln::IFR ? "I" : "V");
      break;

  }

  const RouteAltitude& routeAlt = NavApp::getAltitudeLegs();
  exportData.setAircraftType(NavApp::getAircraftPerformance().getAircraftType());
  exportData.setSpeed(atools::roundToInt(NavApp::getRouteCruiseSpeedKts()));
  exportData.setEnrouteMinutes(atools::roundToInt(routeAlt.getTravelTimeHours() * 60.f));
  exportData.setEnduranceMinutes(atools::roundToInt(routeAlt.getTravelTimeHours() * 60.f) + 60);

  return exportData;
}

bool RouteExport::routeExportDialog(RouteExportData& exportData, re::RouteExportType flightplanType)
{
  RouteExportDialog exportDialog(mainWindow, flightplanType);
  exportDialog.setExportData(exportData);
  if(exportDialog.exec() == QDialog::Accepted)
  {
    exportData = exportDialog.getExportData();
    return true;
  }
  return false;
}

bool RouteExport::routeExportGpx()
{
  qDebug() << Q_FUNC_INFO;

  if(routeValidate(false /* validate parking */, false /* validate departure and destination */))
  {
    QString title = NavApp::getAircraftTrack().isEmpty() ?
                    tr("Save Flight Plan as GPX Format") : tr("Save Flightplan and Track as GPX Format");

    QString routeFile = dialog->saveFileDialog(
      title,
      tr("GPX Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_GPX),
      "gpx", "Route/Gpx", documentsLocation, buildDefaultFilename(QString(), ".gpx"),
      false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

    if(!routeFile.isEmpty())
    {
      if(exportFlightplanAsGpx(routeFile))
      {
        if(NavApp::getAircraftTrack().isEmpty())
          mainWindow->setStatusMessage(tr("Flight plan saved as GPX."));
        else
          mainWindow->setStatusMessage(tr("Flight plan and track saved as GPX."));
        return true;
      }
    }
  }
  return false;
}

bool RouteExport::routeExportHtml()
{
  qDebug() << Q_FUNC_INFO;

  QString routeFile = dialog->saveFileDialog(
    tr("Save Flight Plan as HTML Page"),
    tr("HTML Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_HTML),
    "html", "Route/Html", documentsLocation, buildDefaultFilename(QString(), ".html"),
    false /* confirm overwrite */, OptionData::instance().getFlags2() & opts2::PROPOSE_FILENAME);

  if(!routeFile.isEmpty())
  {
    QFile file(routeFile);
    if(file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
      QTextStream stream(&file);
      stream.setCodec("UTF-8");
      stream << NavApp::getRouteController()->getFlightplanTableAsHtmlDoc(24);
      mainWindow->setStatusMessage(tr("Flight plan saved as HTML."));
      return true;
    }
    else
      atools::gui::ErrorHandler(mainWindow).handleIOError(file);
  }
  return false;
}

/* Check if route has valid departure  and destination and departure parking.
 *  @return true if route can be saved anyway */
bool RouteExport::routeValidate(bool validateParking, bool validateDepartureAndDestination)
{
  if(!NavApp::getRouteConst().hasValidDeparture() || !NavApp::getRouteConst().hasValidDestination())
  {
    if(validateDepartureAndDestination)
    {
      NavApp::deleteSplashScreen();
      const static atools::gui::DialogButtonList BUTTONS({
        {QString(), QMessageBox::Cancel},
        {tr("Select Start &Position"), QMessageBox::Yes},
        {QString(), QMessageBox::Save}
      });
      int result = dialog->showQuestionMsgBox(lnm::ACTIONS_SHOWROUTE_WARNING,
                                              tr("Flight Plan must have a valid airport as "
                                                 "start and destination and "
                                                 "will not be usable by the Simulator."),
                                              tr("Do not show this dialog again and"
                                                 " save Flight Plan in the future."),
                                              QMessageBox::Cancel | QMessageBox::Save,
                                              QMessageBox::Cancel, QMessageBox::Save);

      if(result == QMessageBox::Save)
        // Save anyway
        return true;
      else if(result == QMessageBox::Cancel)
        return false;
    }
  }
  else
  {
    if(validateParking)
    {
      if(!NavApp::getRouteConst().hasValidParking())
      {
        NavApp::deleteSplashScreen();

        // Airport has parking but no one is selected
        const static atools::gui::DialogButtonList BUTTONS({
          {QString(), QMessageBox::Cancel},
          {tr("Select Start &Position"), QMessageBox::Yes},
          {tr("Show &Departure on Map"), QMessageBox::YesToAll},
          {QString(), QMessageBox::Save}
        });

        int result = dialog->showQuestionMsgBox(
          lnm::ACTIONS_SHOWROUTE_PARKING_WARNING,
          tr("The start airport has parking spots but no parking was selected for this Flight Plan"),
          tr("Do not show this dialog again and save Flight Plan in the future."),
          BUTTONS, QMessageBox::Yes, QMessageBox::Save);

        if(result == QMessageBox::Yes)
          // saving depends if user selects parking or cancels out of the dialog
          emit selectDepartureParking();
        else if(result == QMessageBox::YesToAll)
        {
          // Zoom to airport and cancel out
          emit showRect(NavApp::getRouteConst().getDepartureAirportLeg().getAirport().bounding, false);
          return false;
        }
        else if(result == QMessageBox::Save)
          // Save right away
          return true;
        else if(result == QMessageBox::Cancel)
          return false;
      }
    }
  }
  return true;
}

QString RouteExport::buildDefaultFilename(const QString& sep, const QString& suffix, const QString& extension) const
{
  return (OptionData::instance().getFlags2() & opts2::ROUTE_SAVE_SHORT_NAME) ?
         buildDefaultFilenameShort(sep, suffix) : buildDefaultFilenameLong(extension, suffix);
}

QString RouteExport::buildDefaultFilenameLong(const QString& extension, const QString& suffix)
{
  QString filename;

  const Route& route = NavApp::getRouteConst();
  const atools::fs::pln::Flightplan& flightplan = route.getFlightplan();

  if(flightplan.getFlightplanType() == atools::fs::pln::IFR)
    filename = "IFR ";
  else if(flightplan.getFlightplanType() == atools::fs::pln::VFR)
    filename = "VFR ";

  if(flightplan.getDepartureAiportName().isEmpty())
    filename += flightplan.getEntries().at(route.getDepartureAirportLegIndex()).getIcaoIdent();
  else
    filename += flightplan.getDepartureAiportName() + " (" + flightplan.getDepartureIdent() + ")";

  filename += " to ";

  if(flightplan.getDestinationAiportName().isEmpty())
    filename += flightplan.getEntries().at(route.getDestinationAirportLegIndex()).getIcaoIdent();
  else
    filename += flightplan.getDestinationAiportName() + " (" + flightplan.getDestinationIdent() + ")";

  filename += extension;
  filename += suffix;

  // Remove characters that are note allowed in most filesystems
  filename = atools::cleanFilename(filename);
  return filename;
}

QString RouteExport::buildDefaultFilenameShort(const QString& sep, const QString& suffix)
{
  QString filename;

  const Route& route = NavApp::getRouteConst();
  const atools::fs::pln::Flightplan& flightplan = route.getFlightplan();

  filename += flightplan.getEntries().at(route.getDepartureAirportLegIndex()).getIcaoIdent();
  filename += sep;

  filename += flightplan.getEntries().at(route.getDestinationAirportLegIndex()).getIcaoIdent();
  filename += suffix;

  // Remove characters that are note allowed in most filesystems
  filename = atools::cleanFilename(filename);
  return filename;
}

bool RouteExport::exportFlighplanAsGfp(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;
  QString gfp = RouteStringWriter().createGfpStringForRoute(
    routeAdjustedToProcedureOptions(true /* replace custom procedure waypoints*/,
                                    true /* remove alternates */), false /* procedures */,
    OptionData::instance().getFlags() & opts::ROUTE_GARMIN_USER_WPT);

  QFile file(filename);
  if(file.open(QFile::WriteOnly | QIODevice::Text))
  {
    QByteArray utf8 = gfp.toUtf8();
    file.write(utf8.data(), utf8.size());
    file.close();
    return true;
  }
  else
  {
    atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("While saving GFP file:"));
    return false;
  }
}

bool RouteExport::exportFlighplanAsTxt(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;
  QString txt = RouteStringWriter().createStringForRoute(
    routeAdjustedToProcedureOptions(true /* replace custom procedure waypoints*/,
                                    true /* remove alternates */),
    0.f, rs::DCT | rs::START_AND_DEST | rs::SID_STAR_GENERIC);

  QFile file(filename);
  if(file.open(QFile::WriteOnly | QIODevice::Text))
  {
    QByteArray utf8 = txt.toUtf8();
    file.write(utf8.data(), utf8.size());
    file.close();
    return true;
  }
  else
  {
    atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("While saving TXT or FPL file:"));
    return false;
  }
}

bool RouteExport::exportFlighplanAsUFmc(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;
  QStringList list = RouteStringWriter().createStringForRouteList(
    routeAdjustedToProcedureOptions(true /* replace custom procedure waypoints*/,
                                    true /* remove alternates */), 0.f,
    rs::DCT | rs::START_AND_DEST);

  // Remove last DCT
  if(list.size() - 2 >= 0 && list.at(list.size() - 2) == "DCT")
    list.removeAt(list.size() - 2);

  // Replace DCT with DIRECT
  list.replaceInStrings(QRegularExpression("^DCT$"), "DIRECT");
  // KJFK
  // CYYZ
  // DIRECT
  // GAYEL
  // Q818
  // WOZEE
  // 99
  QFile file(filename);
  if(file.open(QFile::WriteOnly | QIODevice::Text))
  {
    QTextStream stream(&file);
    // Save start and destination
    stream << list.first() << endl << list.last() << endl;

    // Waypoints and airways
    for(int i = 1; i < list.size() - 1; i++)
      stream << list.at(i) << endl;

    // File end
    stream << "99" << endl;

    file.close();
    return true;
  }
  else
  {
    atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("While saving UFMC file:"));
    return false;
  }
}

bool RouteExport::exportFlighplanAsRxpGns(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;

  try
  {
    atools::fs::pln::SaveOptions options = atools::fs::pln::SAVE_NO_OPTIONS;

    if(OptionData::instance().getFlags() & opts::ROUTE_GARMIN_USER_WPT)
      options |= atools::fs::pln::SAVE_GNS_USER_WAYPOINTS;

    // Regions are required for the export
    NavApp::getRoute().updateAirportRegions();
    atools::fs::pln::FlightplanIO().saveGarminGns(
      routeAdjustedToProcedureOptions(true /* replace custom procedure waypoints*/,
                                      true /* remove alternates */).getFlightplan(), filename, options);
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    return false;
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    return false;
  }
  return true;
}

bool RouteExport::exportFlighplanAsRxpGtn(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;
  QString gfp = RouteStringWriter().createGfpStringForRoute(
    routeAdjustedToProcedureOptions(true /* replace custom procedure waypoints*/,
                                    true /* remove alternates */), true /* procedures */,
    OptionData::instance().getFlags() & opts::ROUTE_GARMIN_USER_WPT);

  QFile file(filename);
  if(file.open(QFile::WriteOnly | QIODevice::Text))
  {
    QByteArray utf8 = gfp.toUtf8();
    file.write(utf8.data(), utf8.size());
    file.close();
    return true;
  }
  else
  {
    atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("While saving GFP file:"));
    return false;
  }
}

bool RouteExport::exportFlighplanAsVfp(const RouteExportData& exportData, const QString& filename)
{
  QFile file(filename);
  if(file.open(QFile::WriteOnly | QIODevice::Text))
  {
    // <?xml version="1.0" encoding="utf-8"?>
    // <FlightPlan xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    // FlightType="IFR"
    // Equipment="/L"
    // CruiseAltitude="23000"
    // CruiseSpeed="275"
    // DepartureAirport="KBZN"
    // DestinationAirport="KJAC"
    // AlternateAirport="KSLC"
    // Route="DCT 4529N11116W 4527N11114W 4524N11112W 4522N11110W DCT 4520N11110W 4519N11111W/N0276F220 4517N11113W 4516N11115W 4514N11115W/N0275F230 4509N11114W 4505N11113W 4504N11111W 4502N11107W 4500N11105W 4458N11104W 4452N11103W 4450N11105W/N0276F220 4449N11108W 4449N11111W 4449N11113W 4450N11116W 4451N11119W 4450N11119W/N0275F230 4448N11117W 4446N11116W DCT KWYS DCT 4440N11104W 4440N11059W 4439N11055W 4439N11052W 4435N11050W 4430N11050W 4428N11050W 4426N11044W 4427N11041W 4425N11035W 4429N11032W 4428N11031W 4429N11027W 4429N11025W 4432N11024W 4432N11022W 4432N11018W 4428N11017W 4424N11017W 4415N11027W/N0276F220 DCT 4409N11040W 4403N11043W DCT 4352N11039W DCT"
    // Remarks="PBN/D2 DOF/181102 REG/N012SB PER/B RMK/TCAS SIMBRIEF"
    // IsHeavy="false"
    // EquipmentPrefix=""
    // EquipmentSuffix="L"
    // DepartureTime="2035"
    // DepartureTimeAct="0"
    // EnrouteHours="0"
    // EnrouteMinutes="53"
    // FuelHours="2"
    // FuelMinutes="44"
    // VoiceType="Full" />
    QXmlStreamWriter writer(&file);
    writer.setCodec("UTF-8");
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(2);

    writer.writeStartDocument("1.0");
    writer.writeStartElement("FlightPlan");

    writer.writeAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    writer.writeAttribute("xmlns:xsd", "http://www.w3.org/2001/XMLSchema");

    writer.writeAttribute("FlightType", exportData.getFlightRules());
    writer.writeAttribute("Equipment", exportData.getEquipment());
    writer.writeAttribute("CruiseAltitude", QString::number(exportData.getCruiseAltitude()));
    writer.writeAttribute("CruiseSpeed", QString::number(exportData.getSpeed()));
    writer.writeAttribute("DepartureAirport", exportData.getDeparture());
    writer.writeAttribute("DestinationAirport", exportData.getDestination());
    writer.writeAttribute("AlternateAirport", exportData.getAlternate());
    writer.writeAttribute("Route", exportData.getRoute());
    writer.writeAttribute("Remarks", exportData.getRemarks());
    writer.writeAttribute("IsHeavy", exportData.isHeavy() ? "true" : "false");
    writer.writeAttribute("EquipmentPrefix", exportData.getEquipmentPrefix());
    writer.writeAttribute("EquipmentSuffix", exportData.getEquipmentSuffix());

    writer.writeAttribute("DepartureTime", exportData.getDepartureTime().toString("HHmm"));
    writer.writeAttribute("DepartureTimeAct", exportData.getDepartureTimeActual().isNull() ?
                          "0" : exportData.getDepartureTimeActual().toString("HHmm"));
    int enrouteHours = exportData.getEnrouteMinutes() / 60;
    writer.writeAttribute("EnrouteHours", QString::number(enrouteHours));
    writer.writeAttribute("EnrouteMinutes", QString::number(exportData.getEnrouteMinutes() - enrouteHours * 60));
    int enduranceHours = exportData.getEnduranceMinutes() / 60;
    writer.writeAttribute("FuelHours", QString::number(enduranceHours));
    writer.writeAttribute("FuelMinutes", QString::number(exportData.getEnduranceMinutes() - enduranceHours * 60));
    writer.writeAttribute("VoiceType", exportData.getVoiceType());

    writer.writeEndElement(); // FlightPlan
    writer.writeEndDocument();

    file.close();
    return true;
  }
  else
  {
    atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("While saving VFP file:"));
    return false;
  }
}

bool RouteExport::exportFlighplanAsIvap(const RouteExportData& exportData, const QString& filename,
                                        re::RouteExportType type)
{
  QFile file(filename);
  if(file.open(QFile::WriteOnly | QIODevice::Text))
  {
    // [FLIGHTPLAN]
    // CALLSIGN=VPI333
    // PIC=NAME
    // FMCROUTE=
    // LIVERY=
    // AIRLINE=VPI
    // SPEEDTYPE=N
    // POB=83
    // ENDURANCE=0215
    // OTHER=X-IvAp CREW OF 2 PILOT VPI07/COPILOT VPI007
    // ALT2ICAO=
    // ALTICAO=LFMP
    // EET=0115
    // DESTICAO=LFBO
    // ROUTE=TINOT UY268 DIVKO UM731 FJR
    // LEVEL=330
    // LEVELTYPE=F
    // SPEED=300
    // DEPTIME=2110
    // DEPICAO=LFKJ
    // TRANSPONDER=S
    // EQUIPMENT=SDFGW
    // WAKECAT=M
    // ACTYPE=B733
    // NUMBER=1
    // FLIGHTTYPE=N
    // RULES=I
    QTextStream stream(&file);
    stream << "[FLIGHTPLAN]" << endl;

    if(type == re::XIVAP)
    {
      stream << endl;
      writeIvapLine(stream, "CALLSIGN", exportData.getCallsign(), type);
      writeIvapLine(stream, "LIVERY", exportData.getLivery(), type);
      writeIvapLine(stream, "AIRLINE", exportData.getAirline(), type);
      writeIvapLine(stream, "PIC", exportData.getPilotInCommand(), type);
      writeIvapLine(stream, "ALT2ICAO", exportData.getAlternate2(), type);
      writeIvapLine(stream, "FMCROUTE", QString(), type);
    }
    else
    {
      writeIvapLine(stream, "ID", exportData.getCallsign(), type);
      writeIvapLine(stream, "ALTICAO2", exportData.getAlternate2(), type);
    }

    writeIvapLine(stream, "SPEEDTYPE", "N", type);
    writeIvapLine(stream, "POB", exportData.getPassengers(), type);
    writeIvapLine(stream, "ENDURANCE", minToHourMinStr(exportData.getEnduranceMinutes()), type);
    writeIvapLine(stream, "OTHER", exportData.getRemarks(), type);
    writeIvapLine(stream, "ALTICAO", exportData.getAlternate(), type);
    writeIvapLine(stream, "EET", minToHourMinStr(exportData.getEnrouteMinutes()), type);
    writeIvapLine(stream, "DESTICAO", exportData.getDestination(), type);
    writeIvapLine(stream, "ROUTE", exportData.getRoute(), type);
    writeIvapLine(stream, "LEVEL", exportData.getCruiseAltitude() / 100, type);
    writeIvapLine(stream, "LEVELTYPE", "F", type);
    writeIvapLine(stream, "SPEED", exportData.getSpeed(), type);
    writeIvapLine(stream, "DEPTIME", exportData.getDepartureTime().toString("HHmm"), type);
    writeIvapLine(stream, "DEPICAO", exportData.getDeparture(), type);
    writeIvapLine(stream, "TRANSPONDER", exportData.getTransponder(), type);
    writeIvapLine(stream, "EQUIPMENT", exportData.getEquipment(), type);
    writeIvapLine(stream, "WAKECAT", exportData.getWakeCategory(), type);
    writeIvapLine(stream, "ACTYPE", exportData.getAircraftType(), type);
    writeIvapLine(stream, "NUMBER", "1", type);
    writeIvapLine(stream, "FLIGHTTYPE", exportData.getFlightType(), type);
    writeIvapLine(stream, "RULES", exportData.getFlightRules(), type);

    file.close();
    return true;
  }
  else
  {
    atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("While saving FPL file:"));
    return false;
  }
}

bool RouteExport::exportFlighplan(const QString& filename,
                                  std::function<void(const atools::fs::pln::Flightplan& plan,
                                                     const QString& file)> exportFunc)
{
  qDebug() << Q_FUNC_INFO << filename;

  try
  {
    exportFunc(routeAdjustedToProcedureOptions(true /* replace custom procedure waypoints*/,
                                               true /* remove alternates */).getFlightplan(), filename);
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    return false;
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    return false;
  }
  return true;
}

bool RouteExport::exportFlighplanAsCorteIn(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;
  QString txt = RouteStringWriter().createStringForRoute(
    routeAdjustedToProcedureOptions(true /* replace custom procedure waypoints*/,
                                    true /* remove alternates */), 0.f,
    rs::DCT | rs::NO_FINAL_DCT | rs::START_AND_DEST | rs::SID_STAR | rs::SID_STAR_SPACE |
    rs::RUNWAY /*| rs::APPROACH unreliable */ | rs::FLIGHTLEVEL);

  const atools::fs::pln::Flightplan& flightplan = NavApp::getRouteConst().getFlightplan();

  QSet<QString> routeNames;
  QFile file(filename);
  if(file.open(QFile::ReadOnly | QIODevice::Text))
  {
    while(!file.atEnd())
    {
      QString line = file.readLine().toUpper().simplified();
      if(line.isEmpty())
        continue;

      // RTE LHRAMS01 EGLL 27L BPK7G BPK DCT CLN UL620 REDFA REDF1A EHAM I18R SPL CI30 FL250
      routeNames.insert(line.section(" ", 1, 1));
    }
    file.close();
  }
  else
  {
    atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("While reading corte.in file:"));
    return false;
  }

  QString name = flightplan.getDepartureIdent() + flightplan.getDestinationIdent();

  // Find a unique name between all loaded
  int i = 1;
  while(routeNames.contains(name) && i < 99)
  {
    QString str = name.left(6);
    name = QString("%1%2").arg(str).arg(i++, 8 - str.size(), 10, QChar('0'));
  }

  txt.prepend(QString("RTE %1 ").arg(name));

  // Check if we have to insert an endl first
  bool endsWithEol = atools::fileEndsWithEol(filename);

  // Append string to file
  if(file.open(QFile::Append | QIODevice::Text))
  {
    QTextStream stream(&file);

    if(!endsWithEol)
      stream << endl;
    stream << txt;
    file.close();
    return true;
  }
  else
  {
    atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("While saving to corte.in file:"));
    return false;
  }
}

bool RouteExport::exportFlighplanAsProSim(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;

  // <?xml version="1.0" encoding="UTF-8"?>
  // <companyroutes>
  // <route name="KDSMKOKC">KDSM DSM J25 TUL KOKC </route>
  // <route name="EDDHEDDS">EDDH IDEKO Y900 TIMEN UL126 WRB UN850 KRH T128 BADSO EDDS</route>
  // <route name="EDDSEDDH">EDDS KRH UZ210 NOSPA EDDL</route>
  // </companyroutes>

  // Read the XML file and keep all routes ===========================================
  QVector<std::pair<QString, QString> > routes;
  QSet<QString> routeNames;

  QFile file(filename);
  if(file.exists() && file.size() > 0)
  {
    if(file.open(QFile::ReadOnly | QIODevice::Text))
    {
      QXmlStreamReader reader(&file);

      while(!reader.atEnd())
      {
        if(reader.error() != QXmlStreamReader::NoError)
          throw atools::Exception("Error reading \"" + filename + "\": " + reader.errorString());

        QXmlStreamReader::TokenType token = reader.readNext();

        if(token == QXmlStreamReader::StartElement && reader.name() == "route")
        {
          QString name = reader.attributes().value("name").toString();
          QString route = reader.readElementText();
          routes.append(std::make_pair(name, route));
          routeNames.insert(name);
        }
      }
      file.close();
    }
    else
    {
      atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("While reading from companyroutes.xml file:"));
      return false;
    }
  }

  // Create maximum of two backup files
  QString backupFile = filename + "_lnm_backup";
  atools::io::FileRoller roller(1);
  roller.rollFile(backupFile);

  // Copy file to backup before opening
  bool result = QFile(filename).copy(backupFile);
  qDebug() << Q_FUNC_INFO << "Copied" << filename << "to" << backupFile << result;

  // Create route string
  QString route = RouteStringWriter().createStringForRoute(
    routeAdjustedToProcedureOptions(true /* replace custom procedure waypoints*/,
                                    true /* remove alternates */), 0.f, rs::START_AND_DEST);
  QString name = buildDefaultFilenameShort(QString(), QString());

  // Find a unique name between all loaded
  QString newname = name;
  int i = 1;
  while(routeNames.contains(newname) && i < 99)
    newname = QString("%1%2").arg(name).arg(i++, 2, 10, QChar('0'));

  // Add new route
  routes.append(std::make_pair(newname, route));

  // Save and overwrite new file ====================================================
  if(file.open(QFile::WriteOnly | QIODevice::Text))
  {
    QXmlStreamWriter writer(&file);
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(2);
    writer.writeStartDocument();

    writer.writeStartElement("companyroutes");

    for(const std::pair<QString, QString>& entry : routes)
    {
      // <route name="KDSMKOKC">KDSM DSM J25 TUL KOKC </route>
      writer.writeStartElement("route");
      writer.writeAttribute("name", entry.first);
      writer.writeCharacters(entry.second);
      writer.writeEndElement(); // route
    }

    writer.writeEndElement(); // companyroutes
    writer.writeEndDocument();
    file.close();
    return true;
  }
  else
  {
    atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("While saving to companyroutes.xml file:"));
    return false;
  }
}

bool RouteExport::exportFlightplanAsGpx(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;

  const AircraftTrack& aircraftTrack = NavApp::getAircraftTrack();
  atools::geo::LineString track;
  QVector<quint32> timestamps;

  for(const at::AircraftTrackPos& pos : aircraftTrack)
  {
    track.append(pos.pos);
    timestamps.append(pos.timestamp);
  }

  try
  {
    atools::fs::pln::FlightplanIO().saveGpx(
      routeAdjustedToProcedureOptions(true /* replace custom procedure waypoints*/,
                                      true /* remove alternates */).getFlightplan(),
      filename, track, timestamps,
      static_cast<int>(NavApp::getRouteConst().getCruisingAltitudeFeet()));
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    return false;
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    return false;
  }
  return true;
}

Route RouteExport::routeAdjustedToProcedureOptions(bool replaceCustomWp, bool removeAlternate)
{
  return routeAdjustedToProcedureOptions(NavApp::getRoute(), replaceCustomWp, removeAlternate);
}

Route RouteExport::routeAdjustedToProcedureOptions(const Route& route, bool replaceCustomWp, bool removeAlternate)
{
  Route rt = route.adjustedToProcedureOptions(NavApp::getMainUi()->actionRouteSaveApprWaypoints->isChecked(),
                                              NavApp::getMainUi()->actionRouteSaveSidStarWaypoints->isChecked(),
                                              replaceCustomWp, removeAlternate);

  // Update airway structures
  rt.updateAirwaysAndAltitude(false /* adjustRouteAltitude */, false /* adjustRouteType */);

  return rt;
}

QString RouteExport::minToHourMinStr(int minutes)
{
  int enrouteHours = minutes / 60;
  return QString("%1%2").arg(enrouteHours, 2, 10, QChar('0')).arg(minutes - enrouteHours * 60, 2, 10, QChar('0'));
}

void RouteExport::writeIvapLine(QTextStream& stream, const QString& key, const QString& value, re::RouteExportType type)
{
  stream << key << "=" << value << endl;
  if(type == re::XIVAP)
    stream << endl;
}

void RouteExport::writeIvapLine(QTextStream& stream, const QString& key, int value, re::RouteExportType type)
{
  stream << key << "=" << value << endl;
  if(type == re::XIVAP)
    stream << endl;
}
