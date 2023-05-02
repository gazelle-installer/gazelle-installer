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
#include <QStringLiteral>
#include <QToolTip>
#include <QSettings>
#include <QFormLayout>
#include <QDialogButtonBox>

#include "partman.h"
#include "swapman.h"
#include "autopart.h"

AutoPart::AutoPart(MProcess &mproc, PartMan *pman, Ui::MeInstall &ui, const class QSettings &appConf, long long homeNeeded)
    : QObject(ui.boxSliderPart), proc(mproc), gui(ui), partman(pman)
{
    checkHibernation = gui.checkHibernationReg;

    connect(gui.comboDisk, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AutoPart::diskChanged);
    connect(gui.boxEncryptAuto, &QGroupBox::toggled, this, &AutoPart::toggleEncrypt);
    connect(gui.checkHibernationReg, &QCheckBox::clicked, this,
        [=](bool checked){ setParams(true, gui.boxEncryptAuto->isChecked(), checked, true); });
    connect(gui.sliderPart, &QSlider::sliderPressed, this, &AutoPart::sliderPressed);
    connect(gui.sliderPart, &QSlider::actionTriggered, this, &AutoPart::sliderActionTriggered);
    connect(gui.sliderPart, &QSlider::valueChanged, this, &AutoPart::sliderValueChanged);

    strRoot = tr("Root");
    strHome = tr("Home");
    strNone = "----";
    minRoot = pman->rootSpaceNeeded;
    minHome = homeNeeded;
    prefRoot = minRoot + appConf.value("ROOT_BUFFER", 5000).toLongLong() * MB;
    prefHome = minHome + appConf.value("HOME_BUFFER", 2000).toLongLong() * MB;
    installFromRootDevice = appConf.value("INSTALL_FROM_ROOT_DEVICE").toBool();
    refresh();
}

void AutoPart::manageConfig(MSettings &config)
{
    config.startGroup("Storage", gui.pageDisk);
    config.manageComboBox("Drive", gui.comboDisk, true);
    config.manageGroupCheckBox("DriveEncrypt", gui.boxEncryptAuto);
    if (config.isSave()) {
        config.setValue("RootPortion", gui.sliderPart->value());
    } else if (config.contains("RootPortion")) {
         const int portion = config.value("RootPortion").toInt();
         gui.sliderPart->setSliderPosition(portion);
         if (gui.sliderPart->value() != portion) {
             config.markBadWidget(gui.boxSliderPart);
         }
    }
    config.endGroup();
}

void AutoPart::scan()
{
    gui.comboDisk->clear();
    for (DeviceItemIterator it(*partman); DeviceItem *item = *it; it.next()) {
        if (item->type == DeviceItem::Drive && item->size >= partman->rootSpaceNeeded
                && (!item->flags.bootRoot || installFromRootDevice)) {
            item->addToCombo(gui.comboDisk);
        }
    }
    refresh();
}
void AutoPart::refresh()
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

void AutoPart::setParams(bool swapfile, bool encrypt, bool hibernation, bool snapshot)
{
    available = buildLayout(-1, encrypt, false);
    if (available <= minRoot) return;

    recRoot = prefRoot;
    if (swapfile) recRoot += SwapMan::recommended(hibernation);
    if (snapshot) recRoot += (2 * minRoot); // Root + (squashfs + ISO)
    recHome = prefHome;
    recPortionMin = percent(recRoot, available, true); // Recommended root size.
    recPortionMax = 100 - percent(recHome, available); // Recommended minimum home.

    gui.labelSliderRoot->setToolTip(tr("Recommended: %1\n"
        "Minimum: %2").arg(sizeString(recRoot), sizeString(minRoot)));
    gui.labelSliderHome->setToolTip(tr("Recommended: %1\n"
        "Minimum: %2").arg(sizeString(recHome), sizeString(minHome)));
    gui.sliderPart->triggerAction(QSlider::SliderNoAction); // Snap the slider within range.
}
void AutoPart::setPartSize(Part part, long long nbytes)
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
long long AutoPart::partSize(Part part)
{
    return part==Root ? sizeRoot : (available - sizeRoot);
}

// Layout Builder
void AutoPart::builderGUI(DeviceItem *drive)
{
    long long swapRec = SwapMan::recommended(false);
    long long swapHiber = SwapMan::recommended(true);
    // Borrow the partition slider assembly from the disk page.
    drvitem = drive;
    const int oldPos = gui.sliderPart->sliderPosition();
    QWidget *placeholder = new QWidget;
    QLayout *playout = gui.boxSliderPart->parentWidget()->layout();
    playout->replaceWidget(gui.boxSliderPart, placeholder);

    QDialog dialog(gui.treePartitions);
    QFormLayout layout(&dialog);
    dialog.setWindowTitle("Build Layout");

    const QLocale &syslocale = QLocale::system();
    QLabel *labelBase = new QLabel(syslocale.formattedDataSize(
        minRoot, 1, QLocale::DataSizeTraditionalFormat), &dialog);
    QCheckBox *checkEncrypt = new QCheckBox(gui.boxEncryptAuto->title(), &dialog);
    QCheckBox *checkSwapFile = new QCheckBox('+' + syslocale.formattedDataSize(
        swapRec, 1, QLocale::DataSizeTraditionalFormat), &dialog);
    checkHibernation = new QCheckBox('+' + syslocale.formattedDataSize(
        swapHiber-swapRec, 1, QLocale::DataSizeTraditionalFormat), &dialog);
    checkSnapshot = new QCheckBox('+' + syslocale.formattedDataSize(
        2*minRoot, 1, QLocale::DataSizeTraditionalFormat), &dialog);
    layout.addRow(gui.boxSliderPart);
    layout.addRow(checkEncrypt);
    layout.addRow("Base install size:", labelBase);
    layout.addRow("Allow for a swap file of optimal size", checkSwapFile);
    layout.addRow("Allow for hibernation support", checkHibernation);
    layout.addRow("Allow for one standard snapshot", checkSnapshot);

    // Is encryption possible?
    const bool canEncrypt = (buildLayout(-1, true, false) >= minRoot);
    checkEncrypt->setEnabled(canEncrypt);
    if (!canEncrypt) checkEncrypt->setChecked(false);

    auto updateUI = [=](bool) {
        long long available = buildLayout(-1, checkEncrypt->isChecked(), false);
        // Is hibernation possible?
        bool canHibernate = checkSwapFile->isChecked() && (available >= (minRoot + swapHiber));
        checkHibernation->setEnabled(canHibernate);
        if (!canHibernate) checkHibernation->setChecked(false);

        auto check = qobject_cast<QCheckBox *>(sender());
        if (available <= 0) check->setChecked(false);
        setParams(checkSwapFile->isChecked(), checkEncrypt->isChecked(),
            checkHibernation->isChecked(), checkSnapshot->isChecked());
    };
    connect(checkSwapFile, &QCheckBox::clicked, &dialog, updateUI);
    connect(checkEncrypt, &QCheckBox::clicked, &dialog, updateUI);
    connect(checkHibernation, &QCheckBox::clicked, &dialog, updateUI);
    connect(checkSnapshot, &QCheckBox::clicked, &dialog, updateUI);
    checkSwapFile->setChecked(true);
    updateUI(true);

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal, &dialog);
    layout.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        buildLayout(sizeRoot, checkEncrypt->isChecked());
        gui.boxSwap->setChecked(checkSwapFile->isChecked());
        gui.checkHibernation->setChecked(checkHibernation->isChecked());
    }
    // Return the partition slider assembly back to the disk page.
    playout->replaceWidget(placeholder, gui.boxSliderPart);
    diskChanged();
    gui.sliderPart->setSliderPosition(oldPos);
    delete placeholder;

    qApp->processEvents(); // Process residual signals.
    // Reset pointers to original controls.
    checkHibernation = gui.checkHibernationReg;
    checkSnapshot = nullptr;
}

long long AutoPart::buildLayout(long long rootFormatSize, bool crypto, bool updateTree)
{
    if (updateTree) drvitem->clear();
    if (rootFormatSize < 0) rootFormatSize = LLONG_MAX;
    long long remaining = drvitem->size - PARTMAN_SAFETY;

    // Boot partitions.
    if (proc.detectEFI()) {
        if (updateTree) drvitem->addPart(256*MB, "ESP", crypto);
        remaining -= 256*MB;
    } else if (drvitem->willUseGPT()) {
        if (updateTree) drvitem->addPart(1*MB, "BIOS-GRUB", crypto);
        remaining -= 1*MB;
    }
    long long rootMin = partman->rootSpaceNeeded;
    const long long bootMin = partman->bootSpaceNeeded;
    if (!crypto) rootMin += bootMin;
    else {
        int bootFormatSize = 1*GB;
        if (bootFormatSize < bootMin) bootFormatSize = bootMin;
        if (updateTree) drvitem->addPart(bootFormatSize, "boot", crypto);
        remaining -= bootFormatSize;
    }

    // Root
    if (rootFormatSize > remaining) rootFormatSize = remaining;
    if (rootFormatSize < rootMin) return -((drvitem->size - remaining) + rootMin);
    remaining -= rootFormatSize;
    // Home
    if (updateTree) {
        drvitem->addPart(rootFormatSize, "root", crypto);
        if (remaining > 0) drvitem->addPart(remaining, "home", crypto);
        drvitem->labelParts();
        drvitem->driveAutoSetActive();
    }
    return rootFormatSize;
}

// Helpers

QString AutoPart::sizeString(long long size)
{
    QString strout(QLocale::system().formattedDataSize(size, 1, QLocale::DataSizeTraditionalFormat));
    if (strout.length() > 6) { // "10.0 GB" or greater -> "10 GB"
        return QLocale::system().formattedDataSize(size, 0, QLocale::DataSizeTraditionalFormat);
    }
    return strout;
}

// Slots

void AutoPart::diskChanged()
{
    drvitem = partman->findByPath("/dev/" + gui.comboDisk->currentData().toString());
    if (!drvitem) return;

    // Is encryption possible?
    const bool canEncrypt = (buildLayout(-1, true, false) >= minRoot);
    gui.boxEncryptAuto->setEnabled(canEncrypt);
    if (!canEncrypt) gui.boxEncryptAuto->setChecked(false);

    // Refresh encrypt/hibernate capabilities and cascade to set parameters.
    toggleEncrypt(gui.boxEncryptAuto->isChecked());
}
void AutoPart::toggleEncrypt(bool checked)
{
    // Is hibernation possible?
    const bool canHibernate = (buildLayout(-1, checked, false) >= (minRoot + SwapMan::recommended(true)));
    gui.checkHibernationReg->setEnabled(canHibernate);
    if (!canHibernate) gui.checkHibernationReg->setChecked(false);

    setParams(true, checked, gui.checkHibernationReg->isChecked(), true);
    gui.pushNext->setEnabled(!checked || gui.textCryptoPass->isValid());
}

void AutoPart::sliderPressed()
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

void AutoPart::sliderActionTriggered(int action)
{
    int pos = gui.sliderPart->sliderPosition();
    const int oldPos = pos;
    if (action == QSlider::SliderPageStepAdd || action == QSlider::SliderPageStepSub
        || action == QSlider::SliderNoAction) {
        if (pos < recPortionMin) pos = recPortionMin;
        else if (pos > recPortionMax) {
            pos = (action == QSlider::SliderPageStepSub) ? recPortionMax : 100;
        }
    } else {
        const int min = percent(minRoot, available, true);
        const int max = 100 - percent(minHome, available);
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
void AutoPart::sliderValueChanged(int value)
{
    sizeRoot = portion(available, value, MB);
    QString valstr = sizeString(sizeRoot);
    gui.labelSliderRoot->setText(valstr + "\n" + strRoot);

    QPalette palRoot = QApplication::palette();
    QPalette palHome = QApplication::palette();
    if (value < recPortionMin) palRoot.setColor(QPalette::WindowText, Qt::red);
    const long long newHome = available - sizeRoot;
    if (newHome == 0) valstr = strNone;
    else {
        valstr = sizeString(newHome);
        valstr += "\n" + strHome;
        if (value > recPortionMax) palHome.setColor(QPalette::WindowText, Qt::red);
    }
    gui.labelSliderHome->setText(valstr);
    gui.labelSliderRoot->setPalette(palRoot);
    gui.labelSliderHome->setPalette(palHome);
    sliderPressed(); // For the tool tip.

    // Unselect features that won't fit with the current configuration.
    if (checkHibernation->isChecked()
        && sizeRoot < (minRoot + SwapMan::recommended(true))) {
        checkHibernation->setChecked(false);
        QApplication::beep();
    }
    if (checkSnapshot && checkSnapshot->isChecked() && sizeRoot < (3 * minRoot)) {
        checkSnapshot->setChecked(false);
        QApplication::beep();
    }
}
