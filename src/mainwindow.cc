#include "hpp/gui/mainwindow.hh"
#include "ui_mainwindow.h"

#include <gepetto/viewer/corba/server.hh>

#include "hpp/gui/windows-manager.hh"
#include "hpp/gui/osgwidget.hh"
#include "hpp/gui/tree-item.hh"
#include "hpp/gui/dialog/dialogloadrobot.hh"
#include "hpp/gui/dialog/dialogloadenvironment.hh"
#include "hpp/gui/plugin-interface.hh"

#include <hpp/gui/meta.hh>

#if HPP_GUI_HAS_PYTHONQT
# include <hpp/gui/pythonwidget.hh>
#endif

namespace hpp {
  namespace gui {
    MainWindow* MainWindow::instance_ = NULL;

    MainWindow::MainWindow(Settings* settings, QWidget *parent) :
      QMainWindow(parent),
      settings_ (settings),
      ui_(new ::Ui::MainWindow),
      centralWidget_ (),
      osgViewerManagers_ (WindowsManager::create()),
      osgServer_ (NULL),
      backgroundQueue_(),
      worker_ ()
    {
      MainWindow::instance_ = this;
      ui_->setupUi(this);

      if (settings_->startGepettoCorbaServer) {
        osgServer_ = new CorbaServer (new ViewerServerProcess (
              new graphics::corbaServer::Server (
                osgViewerManagers_, 0, NULL, true)));
        osgServer_->start();
      }
      // This scene contains elements required for User Interaction.
      osg()->createScene("hpp-gui");

      // Setup the body tree view
      ui_->bodyTreeContent->init(ui_->bodyTree, ui_->toolBox);

      // Setup the main OSG widget
      connect (this, SIGNAL (createView(QString)), SLOT (onCreateView(QString)));

      connect (ui_->actionRefresh, SIGNAL (triggered()), SLOT (requestRefresh()));

      connect (&backgroundQueue_, SIGNAL (done(int)), this, SLOT (handleWorkerDone(int)));
      connect (&backgroundQueue_, SIGNAL (failed(int,const QString&)),
          this, SLOT (logJobFailed(int, const QString&)));
      connect (this, SIGNAL (sendToBackground(WorkItem*)),
          &backgroundQueue_, SLOT (perform(WorkItem*)));
      backgroundQueue_.moveToThread(&worker_);
      worker_.start();

      setupInterface();
#if HPP_GUI_HAS_PYTHONQT
      pythonWidget_ = new PythonWidget(this);
      insertDockWidget(pythonWidget_, Qt::RightDockWidgetArea, Qt::Horizontal);
#endif
    }

    MainWindow::~MainWindow()
    {
      osgViewerManagers_.reset();
      worker_.quit();
      if (osgServer_ != NULL) {
        osgServer_->wait();
        delete osgServer_;
      }
      worker_.wait();
      delete ui_;
    }

    MainWindow *MainWindow::instance()
    {
      return instance_;
    }

    void MainWindow::insertDockWidget(QDockWidget *dock, Qt::DockWidgetArea area, Qt::Orientation orientation)
    {
      addDockWidget(area, dock, orientation);
      dock->setVisible (false);
      dock->toggleViewAction ()->setIcon(QIcon::fromTheme("window-new"));
      dock->adjustSize();
      ui_->menuWindow->addAction(dock->toggleViewAction ());
    }

    void MainWindow::removeDockWidget(QDockWidget *dock)
    {
      ui_->menuWindow->removeAction(dock->toggleViewAction());
      QMainWindow::removeDockWidget(dock);
    }

    BackgroundQueue& MainWindow::worker()
    {
      return backgroundQueue_;
    }

    WindowsManagerPtr_t MainWindow::osg() const
    {
      return osgViewerManagers_;
    }

    OSGWidget *MainWindow::centralWidget() const
    {
      return centralWidget_;
    }

    BodyTreeWidget *MainWindow::bodyTree() const
    {
      return ui_->bodyTreeContent;
    }

    QList<OSGWidget *> MainWindow::osgWindows() const
    {
      return osgWindows_;
    }

    PluginManager *MainWindow::pluginManager()
    {
      return &(settings_->pluginManager_);
    }

    void MainWindow::log(const QString &text)
    {
      ui_->logText->insertHtml("<hr/><font color=black>"+text+"</font>");
    }

    void MainWindow::logError(const QString &text)
    {
      if (!ui_->dockWidget_log->isVisible()) {
        ui_->dockWidget_log->show();
      }
      QScrollBar* sb = ui_->logText->verticalScrollBar();
      bool SBwasAtBottom = sb->value() == sb->maximum();
      ui_->logText->insertHtml("<hr/><font color=red>"+text+"</font>");
      if (SBwasAtBottom)
        sb->setValue(sb->maximum());
    }

    void MainWindow::emitSendToBackground(WorkItem *item)
    {
      emit sendToBackground(item);
    }

    void MainWindow::logJobStarted(int id, const QString &text)
    {
      log (QString ("Starting job ") + QString::number (id) + ": " + text);
    }

    void MainWindow::logJobDone(int id, const QString &text)
    {
      log (QString ("Job ") + QString::number (id) + " done: " + text);
    }

    void MainWindow::logJobFailed(int id, const QString &text)
    {
      logError (QString ("Job ") + QString::number (id) + " failed: " + text);
    }

    OSGWidget *MainWindow::delayedCreateView(QString name)
    {
      delayedCreateView_.lock();
      emit createView(name);
      delayedCreateView_.lock();
      delayedCreateView_.unlock();
      return osgWindows_.last();
    }

    void MainWindow::requestRefresh()
    {
      emit refresh ();
    }

    OSGWidget *MainWindow::onCreateView() {
      return onCreateView ("hpp_gui_window_" + QString::number(osgWindows_.size()));
    }

    OSGWidget *MainWindow::onCreateView(QString objName)
    {
      OSGWidget* osgWidget = new OSGWidget (osgViewerManagers_, objName.toStdString(),
          this, 0);
      if (!osgWindows_.empty()) {
        QDockWidget* dockOSG = new QDockWidget (
            tr("OSG Viewer") + " " + QString::number (osgWindows_.size()), this);
        osgWidget->setObjectName(objName);
        dockOSG->setWidget(osgWidget);
        addDockWidget(Qt::RightDockWidgetArea, dockOSG);
      } else {
        // This OSGWidget should be the central view
        centralWidget_ = osgWidget;
        centralWidget_->setObjectName(objName);
        setCentralWidget(centralWidget_);
	#if PYTHONQT_NEED_INCLUDE==1
        pythonWidget_->addToContext("osg", centralWidget_);
	#endif
        connect(ui_->actionHome, SIGNAL (triggered()), centralWidget_, SLOT (onHome()));
        connect(ui_->actionSelection, SIGNAL (triggered()), centralWidget_, SLOT (selectionMode()));
        connect(ui_->actionCamera_control_mode, SIGNAL (triggered()), centralWidget_, SLOT (cameraManipulationMode()));
        ui_->osgToolBar->show();

        osg()->addSceneToWindow("hpp-gui", centralWidget_->windowID());
        connect(ui_->actionAdd_floor, SIGNAL (triggered()), centralWidget_, SLOT (addFloor()));
      }
      osgWindows_.append(osgWidget);
      delayedCreateView_.unlock();
      return osgWidget;
    }

    void MainWindow::openLoadRobotDialog()
    {
      statusBar()->showMessage("Loading a robot...");
      DialogLoadRobot* d = new DialogLoadRobot (this);
      if (d->exec () == QDialog::Accepted) {
        createCentralWidget();
        DialogLoadRobot::RobotDefinition rd = d->getSelectedRobotDescription();

        QDir dir (rd.packagePath_); dir.cd("urdf");
        QString urdfFile = dir.absoluteFilePath(rd.modelName_ + rd.urdfSuf_ + ".urdf");
        try {
          centralWidget_->loadURDF(rd.robotName_, urdfFile, rd.mesh_);
        } catch (std::runtime_error& exc) {
          logError (exc.what ());
        }
        robotNames_.append (rd.robotName_);

        QString what = QString ("Loading robot ") + rd.name_;
        WorkItem* item;
        foreach (ModelInterface* loader, pluginManager()->get <ModelInterface> ()) {
          item = new WorkItem_1 <ModelInterface, void,
               DialogLoadRobot::RobotDefinition>
                 (loader, &ModelInterface::loadRobotModel, rd);
          logJobStarted(item->id(), what);
          emit sendToBackground(item);
        }
      }
      d->close();
      statusBar()->clearMessage();
      d->deleteLater();
    }

    void MainWindow::openLoadEnvironmentDialog()
    {
      statusBar()->showMessage("Loading an environment...");
      DialogLoadEnvironment* e = new DialogLoadEnvironment (this);
      if (e->exec() == QDialog::Accepted) {
        createCentralWidget();
        DialogLoadEnvironment::EnvironmentDefinition ed = e->getSelectedDescription();

        QDir d (ed.packagePath_); d.cd("urdf");
        QString urdfFile = d.absoluteFilePath(ed.urdfFilename_ + ".urdf");
        try {
          osgViewerManagers_->addUrdfObjects(
              Traits<QString>::to_corba(ed.envName_).in(),
              Traits<QString>::to_corba(urdfFile   ).in(),
              Traits<QString>::to_corba(ed.mesh_   ).in(),
              true);
          osgViewerManagers_->addSceneToWindow(
              Traits<QString>::to_corba(ed.envName_).in(),
              centralWidget_->windowID());
        } catch (std::runtime_error& exc) {
          log (exc.what ());
        }
        bodyTree()->addBodyToTree(osgViewerManagers_->getScene(ed.envName_.toStdString()));

        QString what = QString ("Loading environment ") + ed.name_;
        WorkItem* item;
        foreach (ModelInterface* loader, pluginManager()->get <ModelInterface> ()) {
          item = new WorkItem_1 <ModelInterface, void,
               DialogLoadEnvironment::EnvironmentDefinition>
                 (loader, &ModelInterface::loadEnvironmentModel, ed);
          logJobStarted(item->id(), what);
          emit sendToBackground(item);
        }
      }
      statusBar()->clearMessage();
      e->close();
      e->deleteLater();
    }

    void MainWindow::handleWorkerDone(int /*id*/)
    {
    }

    void MainWindow::resetConnection()
    {
      foreach (CorbaInterface* e, pluginManager()->get <CorbaInterface> ()) {
        e->openConnection ();
      }
    }

    void MainWindow::about()
    {
      QString devString;
      devString = trUtf8("<p>Version %1. For more information visit <a href=\"%2\">%3</a></p>"
          "<p><small>Copyright (c) 2015 CNRS<br/>By Joseph Mirabel and others.</small></p>"
          "<p><small>"
          "hpp-gui is free software: you can redistribute it and/or modify it under the "
          "terms of the GNU Lesser General Public License as published by the Free "
          "Software Foundation, either version 3 of the License, or (at your option) "
          "any later version.<br/><br/>"
          "hpp-gui is distributed in the hope that it will be "
          "useful, but WITHOUT ANY WARRANTY; without even the implied warranty "
          "of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU "
          "General Lesser Public License for more details.  You should have "
          "received a copy of the GNU Lesser General Public License along with hpp-gui."
          "If not, see <a href=\"http://www.gnu.org/licenses\">http://www.gnu.org/licenses<a/>."
          "</small></p>"
          )
        .arg(QApplication::applicationVersion())
        .arg(QApplication::organizationDomain())
        .arg(QApplication::organizationDomain());

      QMessageBox::about(this, QApplication::applicationName(), devString);
    }

    void MainWindow::setupInterface()
    {
      // Menu "Window"
      QMenu* toolbar = ui_->menuWindow->addMenu("Tool bar");
      toolbar->setIcon(QIcon::fromTheme("configure-toolbars"));
      ui_->mainToolBar->setVisible(true);
      ui_->osgToolBar->setVisible(false);
      toolbar->addAction (ui_->mainToolBar->toggleViewAction ());
      toolbar->addAction (ui_->osgToolBar->toggleViewAction ());

      ui_->menuWindow->addSeparator();

      ui_->dockWidget_bodyTree->setVisible (false);
      ui_->dockWidget_bodyTree->toggleViewAction ()->setIcon(QIcon::fromTheme("window-new"));
      ui_->dockWidget_bodyTree->toggleViewAction ()->setShortcut(Qt::CTRL + Qt::ALT + Qt::Key_B);
      ui_->menuWindow->addAction(ui_->dockWidget_bodyTree->toggleViewAction ());
      ui_->dockWidget_log->setVisible (false);
      ui_->dockWidget_log->toggleViewAction ()->setIcon(QIcon::fromTheme("window-new"));
      ui_->dockWidget_log->toggleViewAction ()->setShortcut(Qt::CTRL + Qt::ALT + Qt::Key_L);
      ui_->menuWindow->addAction(ui_->dockWidget_log->toggleViewAction ());

      ui_->menuWindow->addSeparator();

      // Setup the status bar
      collisionIndicator_ = new LedIndicator (statusBar());
      collisionValidationActivated_ = new QCheckBox ();
      collisionValidationActivated_->setToolTip (tr("Automatically validate configurations."));
      collisionValidationActivated_->setCheckState (Qt::Checked);
      statusBar()->addPermanentWidget(collisionValidationActivated_);
      statusBar()->addPermanentWidget(collisionIndicator_);

      connect (collisionIndicator_, SIGNAL (mouseClickEvent()), SLOT(requestConfigurationValidation()));
      connect (ui_->actionAbout, SIGNAL (triggered ()), SLOT(about()));
      connect (ui_->actionReconnect, SIGNAL (triggered ()), SLOT(resetConnection()));
      connect (ui_->actionFetch_configuration, SIGNAL (triggered ()), SLOT(requestApplyCurrentConfiguration()));
    }

    void MainWindow::createCentralWidget()
    {
      if (!osgWindows_.empty()) return;
      onCreateView();
    }

    void MainWindow::requestApplyCurrentConfiguration()
    {
      emit applyCurrentConfiguration();
      if (collisionValidationActivated_->isChecked ())
        requestConfigurationValidation();
    }

    void MainWindow::requestConfigurationValidation()
    {
      emit configurationValidation();
    }

    void MainWindow::onOpenPluginManager()
    {
      PluginManagerDialog d (pluginManager(), this);
      d.exec ();
    }

    void MainWindow::configurationValidationStatusChanged (bool valid)
    {
      collisionIndicator_->switchLed (valid);
      int state = (valid)?0:1;
      foreach(const QString& s, robotNames_) {
        osg ()->setHighlight(s.toLocal8Bit().data(), state);
      }
    }

    void MainWindow::configurationValidationStatusChanged (QStringList bodiesInCollision)
    {
      QStringList lastBodiesInCollision = lastBodiesInCollision_;
      lastBodiesInCollision_.clear();
      collisionIndicator_->switchLed (bodiesInCollision.empty());
      foreach (QString b, lastBodiesInCollision) {
        if (bodiesInCollision.removeAll(b) == 0) {
          /// This body is not in collision
          osg ()->setHighlight(b.toLocal8Bit().data(), 0);
        } else {
          /// This body is still in collision
          lastBodiesInCollision_.append(b);
        }
      }
      QString tooltip ("Collision between ");
      tooltip += bodiesInCollision.join (", ");
      foreach(const QString& b, bodiesInCollision) {
        osg ()->setHighlight(b.toLocal8Bit().data(), 1);
        lastBodiesInCollision_.append(b);
      }
      collisionIndicator_->setToolTip (tooltip);
    }

    void MainWindow::requestSelectJointFromBodyName(const QString bodyName)
    {
      emit selectJointFromBodyName(bodyName);
    }
  } // namespace gui
} // namespace hpp
