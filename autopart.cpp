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
    connect(gui.comboDisk, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AutoPart::diskChanged);
    connect(gui.checkEncryptAuto, &QCheckBox::toggled, this, &AutoPart::toggleEncrypt);
    connect(gui.checkHibernationReg, &QCheckBox::toggled, this,
        [this](bool checked){ setParams(true, gui.checkEncryptAuto->isChecked(), checked, true); });
    connect(gui.sliderPart, &QSlider::sliderPressed, this, &AutoPart::sliderPressed);
    connect(gui.sliderPart, &QSlider::actionTriggered, this, &AutoPart::sliderActionTriggered);
    connect(gui.sliderPart, &QSlider::valueChanged, this, &AutoPart::sliderValueChanged);

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
}

void AutoPart::manageConfig(MSettings &config) noexcept
{
    config.setSection(u"Storage"_s, gui.pageDisk);
    config.manageComboBox(u"Drive"_s, gui.comboDisk, true);
    config.manageCheckBox(u"DriveEncrypt"_s, gui.checkEncryptAuto);
    if (config.isSave()) {
        config.setInteger(u"RootPortion"_s, gui.sliderPart->value());
    } else if (config.contains(u"RootPortion"_s)) {
         const int portion = config.getInteger(u"RootPortion"_s);
         gui.sliderPart->setSliderPosition(portion);
         if (gui.sliderPart->value() != portion) {
             config.markBadWidget(gui.boxSliderPart);
         }
    }
}

void AutoPart::scan() noexcept
{
    long long minSpace = partman->volSpecTotal(u"/"_s, QStringList()).minimum;
    gui.comboDisk->blockSignals(true);
    gui.comboDisk->clear();
    for (PartMan::Iterator it(*partman); PartMan::Device *device = *it; it.next()) {
        if (device->type == PartMan::Device::DRIVE && (!device->flags.bootRoot || installFromRootDevice)) {
            if (buildLayout(device, LLONG_MAX, false, false) >= minSpace) {
                device->addToCombo(gui.comboDisk);
            }
        }
    }
    gui.comboDisk->blockSignals(false);
    diskChanged();
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
    sliderValueChanged(gui.sliderPart->value());
}

bool AutoPart::validate(bool automatic, const QString &project) const noexcept
{
    PartMan::Device *drive = selectedDrive();
    if (!drive) return false;
    if (!automatic && !core.detectEFI() && drive->size >= 2*TB) {
        QMessageBox msgbox(gui.boxMain);
        msgbox.setIcon(QMessageBox::Warning);
        msgbox.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
        msgbox.setDefaultButton(QMessageBox::No);
        msgbox.setText(tr("The selected drive has a capacity of at least 2TB and must be"
            " formatted using GPT. On some systems, a GPT-formatted disk will not boot.")
            + '\n' + tr("Are you sure you want to continue?"));
        if (msgbox.exec() != QMessageBox::Yes) return false;
    }
    gui.treeConfirm->clear();
    QTreeWidgetItem *twit = new QTreeWidgetItem(gui.treeConfirm);
    twit->setText(0, tr("Format and use the entire disk (%1) for %2").arg(drive->name, project));
    return true;
}

void AutoPart::setParams(bool swapfile, bool encrypt, bool hibernation, bool snapshot) noexcept
{
    QStringList volumes;
    PartMan::Device *drive = selectedDrive();
    if (!drive) return;
    available = buildLayout(drive, -1, encrypt, false, &volumes);
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

PartMan::Device *AutoPart::selectedDrive() const noexcept
{
    return partman->findByPath("/dev/"_L1 + gui.comboDisk->currentData().toString());
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
    const bool canEncrypt = (buildLayout(drive, -1, true, false) >= minRoot);
    checkEncrypt->setEnabled(canEncrypt);
    if (!canEncrypt) checkEncrypt->setChecked(false);

    auto updateUI = [&]() {
        available = buildLayout(drive, -1, checkEncrypt->isChecked(), false);
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
        buildLayout(drive, sizeRoot, checkEncrypt->isChecked());
        gui.boxSwapFile->setChecked(checkSwapFile->isChecked());
        gui.checkHibernation->setChecked(checkHibernation->isChecked());
    }
    // Return the partition slider assembly back to the disk page.
    playout->replaceWidget(placeholder, gui.boxSliderPart);
    diskChanged();
    gui.sliderPart->setSliderPosition(oldPos);
    delete placeholder;

    qApp->processEvents(); // Process residual signals.
    // Reset pointers to original controls.
    checkSnapshot = nullptr;
    inBuilder = false;
}

long long AutoPart::buildLayout(PartMan::Device *drive, long long rootFormatSize,
    bool crypto, bool updateTree, QStringList *volList) noexcept
{
    if (updateTree) drive->clear();
    if (rootFormatSize < 0) rootFormatSize = LLONG_MAX;
    long long remaining = drive->size - PARTMAN_SAFETY;

    // Boot partitions.
    if (core.detectEFI()) {
        if (updateTree) drive->addPart(256*MB, u"ESP"_s, crypto);
        if (volList) volList->append(u"ESP"_s);
        remaining -= 256*MB;
    } else if (drive->format == "GPT"_L1) {
        if (updateTree) drive->addPart(1*MB, u"BIOS-GRUB"_s, crypto);
        if (volList) volList->append(u"BIOS-GRUB"_s);
        remaining -= 1*MB;
    }
    if (crypto) {
        const long long bootFormatSize = partman->volSpecs[u"/boot"_s].preferred;
        if (updateTree) drive->addPart(bootFormatSize, u"/boot"_s, crypto);
        if (volList) volList->append(u"/boot"_s);
        remaining -= bootFormatSize;
    }

    // Root and Home
    if (rootFormatSize > remaining) rootFormatSize = remaining;
    remaining -= rootFormatSize;
    if (volList) {
        volList->append(u"/"_s);
        if (remaining > 0) volList->append(u"/home"_s);
    }
    if (updateTree) {
        drive->addPart(rootFormatSize, u"/"_s, crypto);
        if (remaining > 0) drive->addPart(remaining, u"/home"_s, crypto);
        drive->labelParts();
        drive->driveAutoSetActive();
    }
    return rootFormatSize;
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

// Slots

void AutoPart::diskChanged() noexcept
{
    PartMan::Device *drive = selectedDrive();
    if (!drive) return;

    // Is encryption possible?
    const bool canEncrypt = (buildLayout(drive, -1, true, false) >= minRoot);
    gui.checkEncryptAuto->setEnabled(canEncrypt);
    if (!canEncrypt) gui.checkEncryptAuto->setChecked(false);

    // Refresh encrypt/hibernate capabilities and cascade to set parameters.
    toggleEncrypt(gui.checkEncryptAuto->isChecked());
}
void AutoPart::toggleEncrypt(bool checked) noexcept
{
    PartMan::Device *drive = selectedDrive();
    if (!drive) return;
    // Is hibernation possible?
    const bool canHibernate = (buildLayout(drive, -1, checked, false) >= (minRoot + SwapMan::recommended(true)));
    gui.checkHibernationReg->setEnabled(canHibernate);
    if (!canHibernate) gui.checkHibernationReg->setChecked(false);

    setParams(true, checked, gui.checkHibernationReg->isChecked(), true);
}

void AutoPart::sliderPressed() noexcept
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

void AutoPart::sliderActionTriggered(int action) noexcept
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
    if (action == QSlider::SliderNoAction && pos == gui.sliderPart->value()) sliderValueChanged(pos);
}
void AutoPart::sliderValueChanged(int value) noexcept
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
    sliderPressed(); // For the tool tip.

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
