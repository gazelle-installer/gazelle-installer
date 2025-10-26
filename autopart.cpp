/***************************************************************************
 * Automatic partition layout builder for the installer.
 *
 *   Copyright (C) 2023 by AK-47
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 * This file is part of the gazelle-installer.
 ***************************************************************************/

#include <QApplication>
#include <QToolTip>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QMessageBox>

#include "msettings.h"
#include "mprocess.h"
#include "core.h"
#include "swapman.h"
#include "autopart.h"

using namespace Qt::Literals::StringLiterals;

AutoPart::AutoPart(MProcess &mproc, Core &mcore, PartMan *pman,
    Ui::MeInstall &ui, MIni &appConf, QObject *parent) noexcept
    : QObject(parent), proc(mproc), core(mcore), gui(ui), partman(pman)
{
    gui.boxAutoPart->installEventFilter(this);
    connect(gui.checkDualDrive, &QCheckBox::toggled, this, &AutoPart::checkDualDrive_toggled);
    connect(gui.comboDriveSystem, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &AutoPart::comboDriveSystem_currentIndexChanged);
    connect(gui.checkEncryptAuto, &QCheckBox::toggled, this, &AutoPart::checkEncryptAuto_toggled);
    connect(gui.checkHibernationReg, &QCheckBox::toggled, this,
        [this](bool checked){ setParams(true, gui.checkEncryptAuto->isChecked(), checked, true); });
    connect(gui.sliderPart, &QSlider::sliderPressed, this, &AutoPart::sliderPart_sliderPressed);
    connect(gui.sliderPart, &QSlider::actionTriggered, this, &AutoPart::sliderPart_actionTriggered);
    connect(gui.sliderPart, &QSlider::valueChanged, this, &AutoPart::sliderPart_valueChanged);

    connect(gui.spinRoot, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value){
        const int vmax = 100 - percent(minHome, available, true);
        if (value < gui.sliderPart->value() && value > vmax) value = vmax;
        gui.spinRoot->setValue(value); // Snap to avoid forbidden values.
        gui.sliderPart->setSliderPosition(value);
    });
    connect(gui.spinHome, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value){
        const int vmin = percent(minHome, available, true);
        if (value > (100 - gui.sliderPart->value()) && value < vmin) value = vmin;
        gui.spinHome->setValue(value); // Snap to avoid forbidden values.
        gui.sliderPart->setSliderPosition(100-value);
    });

    strRoot = tr("Root");
    strHome = tr("Home");
    strNone = "----"_L1;
    gui.spinHome->setSpecialValueText(strNone);
    appConf.setSection(u"Storage"_s);
    installFromRootDevice = appConf.getBoolean(u"InstallFromRootDevice"_s);
    appConf.setSection();
    refresh();
    // Ensure the displayed auto partition controls are appropriate.
    checkDualDrive_toggled(gui.checkDualDrive->isChecked());
}

void AutoPart::manageConfig(MSettings &config) noexcept
{
    config.setSection(u"Storage"_s, gui.pageInstallation);
    config.manageCheckBox(u"Encrypt"_s, gui.checkEncryptAuto);
    config.manageComboBox(u"System"_s, gui.comboDriveSystem, true);
    // Determine what the dual drive check box should be (if loading the config).
    if (!config.isSave()) {
        const QString &strhome = config.getString(u"Home"_s);
        gui.checkDualDrive->setChecked(!strhome.isEmpty() && strhome != config.getString(u"System"_s));
    }
    // Home drive (dual drive) or root/home partitioning (single drive).
    if (gui.checkDualDrive->isChecked()) {
        config.manageComboBox(u"Home"_s, gui.comboDriveHome, true);
    } else {
        if (config.isSave()) {
            config.manageComboBox(u"Home"_s, gui.comboDriveSystem, true);
            config.setInteger(u"RootPortion"_s, gui.sliderPart->value());
        } else if (config.contains(u"RootPortion"_s)) {
            const int portion = config.getInteger(u"RootPortion"_s);
            gui.sliderPart->setSliderPosition(portion);
            if (gui.sliderPart->value() != portion) {
                config.markBadWidget(gui.boxSliderPart);
            }
        }
    }
}

void AutoPart::scan() noexcept
{
    gui.comboDriveSystem->blockSignals(true);
    gui.comboDriveHome->blockSignals(true);
    gui.comboDriveSystem->clear();
    gui.comboDriveHome->clear();
    long long largestHome = 0;
    const bool dualDrive = gui.checkDualDrive->isChecked();
    for (PartMan::Iterator it(*partman); PartMan::Device *device = *it; it.next()) {
        if (device->type == PartMan::Device::DRIVE && (!device->flags.bootRoot || installFromRootDevice)) {
            QStringList excludes;
            const long long sizeHead = layoutHead(device, false, false, &excludes);
            if (dualDrive) {
                excludes.append(u"/home"_s); // Home on a separate drive.
            }
            const long long minSize = partman->volSpecTotal(u"/"_s, excludes).minimum;

            if ((device->size - sizeHead) >= minSize) {
                device->addToCombo(gui.comboDriveSystem);
            }
            if (device->size >= minHome) {
                device->addToCombo(gui.comboDriveHome);
                // Select the largest drive for home that is not already chosen for the system drive.
                if (dualDrive && device->size > largestHome && device->name != gui.comboDriveSystem->currentData()) {
                    largestHome = device->size;
                    gui.comboDriveHome->setCurrentIndex(gui.comboDriveHome->count() - 1);
                }
            }
        }
    }

    // Determine if the check box can be changed. If not, check/uncheck according to what is possible.
    canChangeDualDrive = true; // This is used by the event filter with boxAutoPart enabled property.
    // Toggling the check box should automatically re-run the scan.
    if (gui.comboDriveSystem->count() < 1) {
        gui.checkDualDrive->setChecked(true);
        canChangeDualDrive = false;
    } else if (gui.comboDriveHome->count() < 1) {
        gui.checkDualDrive->setChecked(false);
        canChangeDualDrive = false;
    } else if (gui.comboDriveSystem->count() == 1 && gui.comboDriveHome->count() == 1) {
        gui.checkDualDrive->setChecked(gui.comboDriveHome->itemData(0) != gui.comboDriveSystem->itemData(0));
        canChangeDualDrive = false;
    }
    gui.checkDualDrive->setEnabled(gui.boxAutoPart->isEnabled() && canChangeDualDrive);

    gui.comboDriveHome->blockSignals(false);
    gui.comboDriveSystem->blockSignals(false);
    comboDriveSystem_currentIndexChanged();
    refresh();
}
void AutoPart::refresh() noexcept
{
    // Allow the slider labels to fit all possible formatted sizes.
    const QString &strMB = sizeString(1023*GB) + '\n';
    const QFontMetrics &fmetrics = gui.labelSliderRoot->fontMetrics();
    int mwidth = fmetrics.boundingRect(QRect(), Qt::AlignCenter, strMB + strRoot).width();
    gui.labelSliderRoot->setMinimumWidth(mwidth);
    mwidth = fmetrics.boundingRect(QRect(), Qt::AlignCenter, strMB + strHome).width();
    gui.labelSliderHome->setMinimumWidth(mwidth);
    gui.labelSliderRoot->setText(strNone);
    gui.labelSliderHome->setText(strNone);
    // Refresh visual elements.
    sliderPart_valueChanged(gui.sliderPart->value());
}

bool AutoPart::validate(bool automatic, const QString &project) const noexcept
{
    PartMan::Device *drvsystem = selectedDrive(gui.comboDriveSystem);
    PartMan::Device *drvhome = selectedDrive(gui.comboDriveHome);
    if (!drvsystem || !drvhome) return false;
    if (!automatic && !core.detectEFI() && drvsystem->size >= 2*TB) {
        QMessageBox msgbox(gui.boxMain);
        msgbox.setIcon(QMessageBox::Warning);
        msgbox.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
        msgbox.setDefaultButton(QMessageBox::No);
        msgbox.setText(tr("The selected drive has a capacity of at least 2TB and must be"
            " formatted using GPT. On some systems, a GPT-formatted disk will not boot.")
            + '\n' + tr("Are you sure you want to continue?"));
        if (msgbox.exec() != QMessageBox::Yes) return false;
    }
    if (gui.checkDualDrive->isChecked()) {
        if (drvhome == drvsystem) {
            QMessageBox::critical(gui.boxMain, QString(),
                tr("The system and home drives cannot be the same for this type of installation."));
            return false;
        }
        QTreeWidgetItem *twroot = new QTreeWidgetItem(gui.treeConfirm);
        twroot->setText(0, tr("Format and use drives entirely for %1").arg(project));
        QTreeWidgetItem *twit = new QTreeWidgetItem(twroot);
        twit->setText(0, tr("System drive: %1").arg(drvsystem->friendlyName()));
        twit = new QTreeWidgetItem(twroot);
        twit->setText(0, tr("Home drive: %1").arg(drvhome->friendlyName()));
    } else {
        QTreeWidgetItem *twit = new QTreeWidgetItem(gui.treeConfirm);
        twit->setText(0, tr("Format and use the entire drive for %1: %2").arg(project, drvsystem->friendlyName()));
    }
    return true;
}

void AutoPart::setParams(bool swapfile, bool encrypt, bool hibernation, bool snapshot) noexcept
{
    QStringList volumes;
    PartMan::Device *drive = selectedDrive(gui.comboDriveSystem);
    if (!drive) return;
    available = drive->size - layoutHead(drive, encrypt, false, &volumes);
    volumes.append(u"/home"_s);
    const PartMan::VolumeSpec &vspecRoot = partman->volSpecTotal(u"/"_s, volumes);
    if (available <= vspecRoot.minimum) return;
    minRoot = vspecRoot.minimum;
    recRoot = vspecRoot.preferred;
    addSnapshot = 2 * vspecRoot.image;

    const PartMan::VolumeSpec &vspecHome = partman->volSpecTotal(u"/home"_s, volumes);
    minHome = vspecHome.minimum;
    recHome = vspecHome.preferred;
    if (swapfile) {
        const long long swapsize = SwapMan::recommended(hibernation);
        recRoot += swapsize;
        if (hibernation) minRoot += swapsize;
    }
    if (snapshot) recHome += addSnapshot; // squashfs + ISO

    const int rootMinPercent = percent(minRoot, available, true);
    gui.spinRoot->setMinimum(rootMinPercent);
    gui.spinHome->setMaximum(100 - rootMinPercent);

    gui.labelSliderRoot->setToolTip(tr("Recommended: %1\n"
        "Minimum: %2").arg(sizeString(recRoot), sizeString(minRoot)));
    gui.labelSliderHome->setToolTip(tr("Recommended: %1\n"
        "Minimum: %2").arg(sizeString(recHome), sizeString(minHome)));
    gui.spinRoot->setToolTip(gui.labelSliderRoot->toolTip());
    gui.spinHome->setToolTip(gui.labelSliderHome->toolTip());
    gui.sliderPart->triggerAction(QSlider::SliderNoAction); // Snap the slider within range.
}
void AutoPart::setPartSize(Part part, long long nbytes) noexcept
{
    if (part == Root) {
        sizeRoot = (nbytes >= minRoot) ? nbytes : minRoot;
        if (sizeRoot > (available - minHome)) sizeRoot = available;
    } else {
        sizeRoot = available - ((nbytes >= minHome) ? nbytes : minHome);
        if (sizeRoot < minRoot) sizeRoot = available;
    }
    gui.sliderPart->setValue(percent(sizeRoot, available, part==Root));
}
long long AutoPart::partSize(Part part) const noexcept
{
    return part==Root ? sizeRoot : (available - sizeRoot);
}

PartMan::Device *AutoPart::selectedDrive(const QComboBox *combo) const noexcept
{
    return partman->findByPath("/dev/"_L1 + combo->currentData().toString());
}

// Layout Builder
void AutoPart::builderGUI(PartMan::Device *drive) noexcept
{
    inBuilder = true;
    long long swapRec = SwapMan::recommended(false);
    long long swapHiber = SwapMan::recommended(true);
    // Borrow the partition slider assembly from the disk page.
    const int oldPos = gui.sliderPart->sliderPosition();
    QWidget *placeholder = new QWidget;
    QLayout *playout = gui.boxSliderPart->parentWidget()->layout();
    playout->replaceWidget(gui.boxSliderPart, placeholder);

    QDialog dialog(gui.treePartitions);
    QFormLayout layout(&dialog);
    dialog.setWindowTitle(tr("Layout Builder"));

    const QLocale &syslocale = QLocale::system();
    QLabel *labelBase = new QLabel(syslocale.formattedDataSize(
        minRoot, 1, QLocale::DataSizeTraditionalFormat), &dialog);
    QCheckBox *checkEncrypt = new QCheckBox(gui.checkEncryptAuto->text(), &dialog);
    QCheckBox *checkSwapFile = new QCheckBox('+' + syslocale.formattedDataSize(
        swapRec, 1, QLocale::DataSizeTraditionalFormat), &dialog);
    QCheckBox *checkHibernation = new QCheckBox('+' + syslocale.formattedDataSize(
        swapHiber-swapRec, 1, QLocale::DataSizeTraditionalFormat), &dialog);
    checkSnapshot = new QCheckBox('+' + syslocale.formattedDataSize(
        2*minRoot, 1, QLocale::DataSizeTraditionalFormat), &dialog);
    layout.addRow(gui.boxSliderPart);
    layout.addRow(checkEncrypt);
    layout.addRow(u"Base install size:"_s, labelBase);
    layout.addRow(u"Allow for a swap file of optimal size"_s, checkSwapFile);
    layout.addRow(u"Allow for hibernation support"_s, checkHibernation);
    layout.addRow(u"Allow for one standard snapshot"_s, checkSnapshot);

    // Is encryption possible?
    if ((drive->size - layoutHead(drive, true, false)) >= minRoot) {
        checkEncrypt->setEnabled(true);
    } else {
        checkEncrypt->setEnabled(false);
        checkEncrypt->setChecked(false);
    }

    auto updateUI = [&]() {
        available = drive->size - layoutHead(drive, checkEncrypt->isChecked(), false);
        // Is hibernation possible?
        bool canHibernate = checkSwapFile->isChecked() && (available >= (minRoot + swapHiber));
        checkHibernation->setEnabled(canHibernate);
        if (!canHibernate) checkHibernation->setChecked(false);

        auto check = qobject_cast<QCheckBox *>(sender());
        if (available <= 0) check->setChecked(false);
        setParams(checkSwapFile->isChecked(), checkEncrypt->isChecked(),
            checkHibernation->isChecked(), checkSnapshot->isChecked());
    };
    connect(checkSwapFile, &QCheckBox::toggled, &dialog, updateUI);
    connect(checkEncrypt, &QCheckBox::toggled, &dialog, updateUI);
    connect(checkHibernation, &QCheckBox::toggled, &dialog, updateUI);
    connect(checkSnapshot, &QCheckBox::toggled, &dialog, updateUI);
    checkSwapFile->setChecked(true); // Automatically triggers UI update.

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal, &dialog);
    layout.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        drive->clear();
        long long remain = drive->size;
        const bool crypto = checkEncrypt->isChecked();

        remain -= layoutHead(drive, crypto, true);
        drive->addPart(sizeRoot, u"/"_s, crypto);
        remain -= sizeRoot;
        if (remain > 0) {
            drive->addPart(remain, u"/home"_s, crypto);
        }
        drive->labelParts();
        drive->driveAutoSetActive();

        gui.boxSwapFile->setChecked(checkSwapFile->isChecked());
        gui.checkHibernation->setChecked(checkHibernation->isChecked());
    }
    // Return the partition slider assembly back to the disk page.
    playout->replaceWidget(placeholder, gui.boxSliderPart);
    comboDriveSystem_currentIndexChanged();
    gui.sliderPart->setSliderPosition(oldPos);
    delete placeholder;

    qApp->processEvents(); // Process residual signals.
    // Reset pointers to original controls.
    checkSnapshot = nullptr;
    inBuilder = false;
}

bool AutoPart::buildLayout() noexcept
{
    partman->scan();
    const bool crypto = gui.checkEncryptAuto->isChecked();
    PartMan::Device *drvroot = selectedDrive(gui.comboDriveSystem);
    PartMan::Device *drvhome = selectedDrive(gui.comboDriveHome);
    if (!drvroot || !drvhome) return false;

    drvroot->clear();

    long long rootSize = drvroot->size - layoutHead(drvroot, crypto, true);
    long long homeSize = drvhome->size - PARTMAN_SAFETY;
    if (gui.checkDualDrive->isChecked()) {
        drvhome->clear();
    } else {
        drvhome = drvroot;
        rootSize = partSize(Root);
        homeSize = partSize(Home);
    }

    drvroot->addPart(rootSize, u"/"_s, crypto);
    if (homeSize > 0) {
        drvhome->addPart(homeSize, u"/home"_s, crypto);
    }

    drvroot->labelParts();
    drvroot->driveAutoSetActive();
    if (drvhome != drvroot) {
        drvhome->labelParts();
        drvhome->driveAutoSetActive();
    }
    return true;
}

long long AutoPart::layoutHead(PartMan::Device *drive, bool crypto,
    bool addToTree, QStringList *volList) noexcept
{
    long long total = PARTMAN_SAFETY;

    if (core.detectEFI()) {
        const long long size = partman->volSpecs[u"ESP"_s].preferred;
        if (addToTree) drive->addPart(size, u"ESP"_s, crypto);
        if (volList) volList->append(u"ESP"_s);
        total += size;
    } else if (drive->format == "GPT"_L1) {
        if (addToTree) drive->addPart(1*MB, u"BIOS-GRUB"_s, crypto);
        if (volList) volList->append(u"BIOS-GRUB"_s);
        total += 1*MB;
    }

    if (crypto) {
        const long long size = partman->volSpecs[u"/boot"_s].preferred;
        if (addToTree) drive->addPart(size, u"/boot"_s, crypto);
        if (volList) volList->append(u"/boot"_s);
        total += size;
    }

    return total;
}

// Helpers

QString AutoPart::sizeString(long long size) noexcept
{
    QString strout(QLocale::system().formattedDataSize(size, 1, QLocale::DataSizeTraditionalFormat));
    if (strout.length() > 6) { // "10.0 GB" or greater -> "10 GB"
        return QLocale::system().formattedDataSize(size, 0, QLocale::DataSizeTraditionalFormat);
    }
    return strout;
}

// Event filter for dual drive checkbox
bool AutoPart::eventFilter(QObject *watched, QEvent *event) noexcept
{
    if (event->type() == QEvent::EnabledChange) {
        gui.checkDualDrive->setEnabled(gui.boxAutoPart->isEnabled() && canChangeDualDrive);
    }
    return QObject::eventFilter(watched, event);
}

// Slots

void AutoPart::checkDualDrive_toggled(bool checked) noexcept
{
    gui.labelDriveSystem->setText(checked ? tr("System drive:") : tr("Use disk:"));
    gui.labelDriveHome->setVisible(checked);
    gui.comboDriveHome->setVisible(checked);
    gui.boxSliderPart->setHidden(checked);
    scan();
}
void AutoPart::comboDriveSystem_currentIndexChanged() noexcept
{
    PartMan::Device *drive = selectedDrive(gui.comboDriveSystem);
    if (!drive) return;

    // Is encryption possible?
    if ((drive->size - layoutHead(drive, true, false)) >= minRoot) {
        gui.checkEncryptAuto->setEnabled(true);
    } else {
        gui.checkEncryptAuto->setEnabled(false);
        gui.checkEncryptAuto->setChecked(false);
    }

    // Refresh encrypt/hibernate capabilities and cascade to set parameters.
    checkEncryptAuto_toggled(gui.checkEncryptAuto->isChecked());
}
void AutoPart::checkEncryptAuto_toggled(bool checked) noexcept
{
    PartMan::Device *drive = selectedDrive(gui.comboDriveSystem);
    if (!drive) return;

    // Is hibernation possible?
    if ((drive->size - layoutHead(drive, checked, false)) >= (minRoot + SwapMan::recommended(true))) {
        gui.checkHibernationReg->setEnabled(true);
    } else {
        gui.checkHibernationReg->setEnabled(false);
        gui.checkHibernationReg->setChecked(false);
    }

    setParams(true, checked, gui.checkHibernationReg->isChecked(), true);
}

void AutoPart::sliderPart_sliderPressed() noexcept
{
    QString tipText(tr("%1% root\n%2% home"));
    const int val = gui.sliderPart->value();
    if (val==100) tipText = tr("Combined root and home");
    else if (val<1) tipText = tipText.arg(">0", "<100");
    else tipText = tipText.arg(val).arg(100-val);
    gui.sliderPart->setToolTip(tipText);
    if (gui.sliderPart->isSliderDown()) {
        QToolTip::showText(QCursor::pos(), tipText, gui.sliderPart);
    }
}

void AutoPart::sliderPart_actionTriggered(int action) noexcept
{
    int pos = gui.sliderPart->sliderPosition();
    const int oldPos = pos;
    if (action == QSlider::SliderPageStepAdd || action == QSlider::SliderPageStepSub
        || action == QSlider::SliderNoAction) {
        const int recPortionMin = percent(recRoot, available, true); // Recommended root size.
        const int recPortionMax = percent(available-recHome, available, true); // Recommended minimum home.
        if (pos < recPortionMin) pos = recPortionMin;
        else if (pos > recPortionMax) {
            if (action == QSlider::SliderPageStepAdd) pos = 100;
            else if (pos < 100) pos = recPortionMax;
        }
    } else {
        const int min = percent(minRoot, available, true);
        const int max = 100 - percent(minHome, available, true);
        if (pos < min) pos = min;
        else if (pos > max) pos = 100;
    }
    if (pos != oldPos) {
        qApp->beep();
        gui.sliderPart->setSliderPosition(pos);
        pos = gui.sliderPart->sliderPosition(); // Now snapped to valid range
    }
    // Always refresh if this is a programmatic purposeful action.
    if (action == QSlider::SliderNoAction && pos == gui.sliderPart->value()) {
        sliderPart_valueChanged(pos);
    }
}
void AutoPart::sliderPart_valueChanged(int value) noexcept
{
    sizeRoot = portion(available, value, MB);
    QString valstr = sizeString(sizeRoot);
    gui.labelSliderRoot->setText(valstr + '\n' + strRoot);

    gui.spinRoot->setValue(value);
    gui.spinHome->setValue(100-value);

    QPalette palRoot = QApplication::palette();
    QPalette palHome = QApplication::palette();
    if (sizeRoot < recRoot) palRoot.setColor(QPalette::WindowText, Qt::red);
    const long long newHome = available - sizeRoot;
    if (newHome == 0) {
        valstr = strNone;
        if (sizeRoot < (recRoot+recHome)) palRoot.setColor(QPalette::WindowText, Qt::red);
    } else {
        valstr = sizeString(newHome);
        valstr += '\n' + strHome;
        if (newHome < recHome) palHome.setColor(QPalette::WindowText, Qt::red);
    }
    gui.labelSliderHome->setText(valstr);
    gui.labelSliderRoot->setPalette(palRoot);
    gui.labelSliderHome->setPalette(palHome);
    sliderPart_sliderPressed(); // For the tool tip.

    // Unselect features that won't fit with the current configuration.
    const QStringList vols(sizeRoot < available ? "/home" : "/");
    const long long rmin = partman->volSpecTotal(u"/"_s, vols).minimum;
    if (checkSnapshot && checkSnapshot->isChecked()) {
        bool ok = false;
        if (!newHome) ok = (sizeRoot >= (rmin + addSnapshot));
        else ok = (newHome >= (minHome + addSnapshot));
        if (!ok) {
            checkSnapshot->setChecked(false);
            QApplication::beep();
        }
    }
}
