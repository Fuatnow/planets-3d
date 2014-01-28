#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "version.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow),
    settings(QSettings::IniFormat, QSettings::UserScope, QApplication::organizationName(), QApplication::applicationName()) {

    ui->setupUi(this);
    ui->PauseResume_Button->setFocus();

    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(close()));

    connect(ui->actionCenter_All,                       SIGNAL(triggered()),        &ui->centralwidget->universe,   SLOT(centerAll()));
    connect(ui->actionDelete,                           SIGNAL(triggered()),        &ui->centralwidget->universe,   SLOT(deleteSelected()));
    connect(ui->actionDelete_Escapees,                  SIGNAL(triggered()),        &ui->centralwidget->universe,   SLOT(deleteEscapees()));
    connect(ui->actionInteractive_Planet_Placement,     SIGNAL(triggered()),        ui->centralwidget,              SLOT(beginInteractiveCreation()));
    connect(ui->actionInteractive_Planet_Placement,     SIGNAL(triggered(bool)),    ui->toggleFiringModePushButton, SLOT(setChecked(bool)));
    connect(ui->actionInteractive_Orbital_Placement,    SIGNAL(triggered()),        ui->centralwidget,              SLOT(beginOrbitalCreation()));
    connect(ui->toggleFiringModePushButton,             SIGNAL(toggled(bool)),      ui->centralwidget,              SLOT(enableFiringMode(bool)));
    connect(ui->actionTake_Screenshot,                  SIGNAL(triggered()),        ui->centralwidget,              SLOT(takeScreenshot()));
    connect(ui->gridRangeSpinBox,                       SIGNAL(valueChanged(int)),  ui->centralwidget,              SLOT(setGridRange(int)));

    ui->statusbar->addPermanentWidget(planetCountLabel = new QLabel(ui->statusbar));
    ui->statusbar->addPermanentWidget(fpsLabel = new QLabel(ui->statusbar));
    ui->statusbar->addPermanentWidget(averagefpsLabel = new QLabel(ui->statusbar));
    fpsLabel->setFixedWidth(120);
    planetCountLabel->setFixedWidth(120);
    averagefpsLabel->setFixedWidth(160);

    connect(&ui->centralwidget->universe,   SIGNAL(updatePlanetCountMessage(QString)),      planetCountLabel, SLOT(setText(QString)));
    connect(ui->centralwidget,              SIGNAL(updateFPSStatusMessage(QString)),        fpsLabel,         SLOT(setText(QString)));
    connect(ui->centralwidget,              SIGNAL(updateAverageFPSStatusMessage(QString)), averagefpsLabel,  SLOT(setText(QString)));

    ui->menubar->insertMenu(ui->menuHelp->menuAction(), createPopupMenu())->setText(tr("Tools"));

    ui->firingSettings_DockWidget->hide();
    ui->randomSettings_DockWidget->hide();

    const QStringList &arguments = QApplication::arguments();

    foreach (const QString &argument, arguments) {
        if(QFileInfo(argument).filePath() != QApplication::applicationFilePath() && ui->centralwidget->universe.load(argument)){
            break;
        }
    }

    settings.beginGroup("MainWindow");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("state").toByteArray());
    settings.endGroup();
}

MainWindow::~MainWindow(){
    settings.beginGroup("MainWindow");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("state", saveState());
    settings.endGroup();

    delete ui;
    delete planetCountLabel;
    delete averagefpsLabel;
    delete fpsLabel;
}

void MainWindow::closeEvent(QCloseEvent *e){
    if(!ui->centralwidget->universe.isEmpty()){
        float tmpsimspeed = ui->centralwidget->universe.simspeed;
        ui->centralwidget->universe.simspeed = 0.0f;

        if(QMessageBox::warning(this, tr("Are You Sure?"), tr("Are you sure you wish to exit? (universe will not be saved...)"),
                                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::No){
            ui->centralwidget->universe.simspeed = tmpsimspeed;
            return e->ignore();
        }
    }
    e->accept();
}

void MainWindow::on_createPlanet_PushButton_clicked(){
    ui->centralwidget->universe.selected = ui->centralwidget->universe.addPlanet(
                Planet(QVector3D(ui->newPosX_SpinBox->value(),      ui->newPosY_SpinBox->value(),      ui->newPosZ_SpinBox->value()),
                       QVector3D(ui->newVelocityX_SpinBox->value(), ui->newVelocityY_SpinBox->value(), ui->newVelocityZ_SpinBox->value()) * PlanetsUniverse::velocityfac,
                       ui->newMass_SpinBox->value()));
}

void MainWindow::on_actionClear_Velocity_triggered(){
    if(ui->centralwidget->universe.isSelectedValid()){
        ui->centralwidget->universe.getSelected().velocity = QVector3D();
    }
}

void MainWindow::on_speed_Dial_valueChanged(int value){
    ui->centralwidget->universe.simspeed = float(value * speeddialmax) / ui->speed_Dial->maximum();
    ui->speedDisplay_lcdNumber->display(ui->centralwidget->universe.simspeed);
    if(ui->centralwidget->universe.simspeed <= 0.0f){
        ui->PauseResume_Button->setText(tr("Resume"));
        ui->PauseResume_Button->setIcon(QIcon(":/icons/silk/control_play_blue.png"));
    }else{
        ui->PauseResume_Button->setText(tr("Pause"));
        ui->PauseResume_Button->setIcon(QIcon(":/icons/silk/control_pause_blue.png"));
    }
}

void MainWindow::on_PauseResume_Button_clicked(){
    if(ui->speed_Dial->value() == 0){
        ui->speed_Dial->setValue(ui->speed_Dial->maximum() / speeddialmax);
    }else{
        ui->speed_Dial->setValue(0);
    }
}

void MainWindow::on_FastForward_Button_clicked(){
    if(ui->speed_Dial->value() == ui->speed_Dial->maximum() || ui->speed_Dial->value() == 0){
        ui->speed_Dial->setValue(ui->speed_Dial->maximum() / speeddialmax);
    }else{
        ui->speed_Dial->setValue(ui->speed_Dial->value() * 2);
    }
}

void MainWindow::on_actionNew_Simulation_triggered(){
    if(!ui->centralwidget->universe.isEmpty()){
        float tmpsimspeed = ui->centralwidget->universe.simspeed;
        ui->centralwidget->universe.simspeed = 0.0f;

        if(QMessageBox::warning(this, tr("Are You Sure?"), tr("Are you sure you wish to destroy the universe? (i.e. delete all planets.)"),
                                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes){
            ui->centralwidget->universe.deleteAll();
        }
        ui->centralwidget->universe.simspeed = tmpsimspeed;
    }
}

void MainWindow::on_actionOpen_Simulation_triggered(){
    float tmpsimspeed = ui->centralwidget->universe.simspeed;
    ui->centralwidget->universe.simspeed = 0.0f;

    QString filename = QFileDialog::getOpenFileName(this, tr("Open Simulation"), "", tr("Simulation files (*.xml)"));
    ui->centralwidget->universe.load(filename);

    ui->centralwidget->universe.simspeed = tmpsimspeed;
}

void MainWindow::on_actionSave_Simulation_triggered(){
    float tmpsimspeed = ui->centralwidget->universe.simspeed;
    ui->centralwidget->universe.simspeed = 0.0f;

    if(!ui->centralwidget->universe.isEmpty()){
        QString filename = QFileDialog::getSaveFileName(this, tr("Save Simulation"), "", tr("Simulation files (*.xml)"));
        ui->centralwidget->universe.save(filename);
    }else{
        QMessageBox::warning(this, tr("Error Saving Simulation."), tr("No planets to save!"));
    }

    ui->centralwidget->universe.simspeed = tmpsimspeed;
}

void MainWindow::on_actionFollow_Selection_triggered(){
    ui->centralwidget->following = ui->centralwidget->universe.selected;
    ui->centralwidget->followState = PlanetsWidget::Single;
}

void MainWindow::on_actionClear_Follow_triggered(){
    ui->centralwidget->following = 0;
    ui->centralwidget->followState = PlanetsWidget::FollowNone;
}

void MainWindow::on_actionPlain_Average_triggered(){
    ui->centralwidget->followState = PlanetsWidget::PlainAverage;
}

void MainWindow::on_actionWeighted_Average_triggered(){
    ui->centralwidget->followState = PlanetsWidget::WeightedAverage;
}

void MainWindow::on_actionAbout_triggered(){
    float tmpsimspeed = ui->centralwidget->universe.simspeed;
    ui->centralwidget->universe.simspeed = 0.0f;

    QMessageBox::about(this, tr("About Planets3D"),
                       tr("<html><head/><body>"
                          "<p>Planets3D is a simple 3D gravitational simulator</p>"
                          "<p>Website: <a href=\"https://github.com/chipgw/planets-3d\">github.com/chipgw/planets-3d</a></p>"
                          "<p>Build Info:</p><ul>"
                          "<li>Git sha1: %2</li>"
                          "<li>Build type: %3</li>"
                          "<li>Compiler: %4</li>"
                          "</ul></body></html>")
                       .arg(version::git_revision)
                       .arg(version::build_type)
                       .arg(version::compiler));

    ui->centralwidget->universe.simspeed = tmpsimspeed;
}

void MainWindow::on_actionAbout_Qt_triggered(){
    float tmpsimspeed = ui->centralwidget->universe.simspeed;
    ui->centralwidget->universe.simspeed = 0.0f;

    QMessageBox::aboutQt(this);

    ui->centralwidget->universe.simspeed = tmpsimspeed;
}

void MainWindow::on_stepsPerFrameSpinBox_valueChanged(int value){
    ui->centralwidget->universe.stepsPerFrame = value;
}

void MainWindow::on_trailLengthSpinBox_valueChanged(int value){
    Planet::pathLength = value;
}

void MainWindow::on_planetScaleDoubleSpinBox_valueChanged(double value){
    ui->centralwidget->drawScale = value;
}

void MainWindow::on_trailRecordDistanceDoubleSpinBox_valueChanged(double value){
    Planet::pathRecordDistance = value * value;
}

void MainWindow::on_firingVelocityDoubleSpinBox_valueChanged(double value){
    ui->centralwidget->firingSpeed = value * PlanetsUniverse::velocityfac;
}

void MainWindow::on_firingMassSpinBox_valueChanged(int value){
    ui->centralwidget->firingMass = value;
}

void MainWindow::on_actionGrid_toggled(bool value){
    ui->centralwidget->drawGrid = value;
}

void MainWindow::on_actionDraw_Paths_toggled(bool value){
    ui->centralwidget->drawPlanetTrails = value;
}

void MainWindow::on_actionPlanet_Colors_toggled(bool value){
    ui->centralwidget->drawPlanetColors = value;
}

void MainWindow::on_actionHide_Planets_toggled(bool value){
    ui->centralwidget->hidePlanets = value;

    if(value){
        ui->actionDraw_Paths->setChecked(true);
    }
}

void MainWindow::on_generateRandomPushButton_clicked(){
    ui->centralwidget->universe.generateRandom(ui->randomAmountSpinBox->value(), ui->randomRangeDoubleSpinBox->value(),
                                               ui->randomSpeedDoubleSpinBox->value() * PlanetsUniverse::velocityfac,
                                               ui->randomMassDoubleSpinBox->value());
}

const int MainWindow::speeddialmax = 32;
