/***************************************************************************
 * MPassEdit class - QLineEdit modified for passwords.
 *
 *   Copyright (C) 2021 by AK-47
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
 **************************************************************************/

#include <cmath>
#include <QApplication>
#include <QContextMenuEvent>
#include <QMenu>
#include <QStringList>
#include <QString>
#include <QFile>
#include <QDebug>
#ifndef NO_ZXCVBN
    #include <zxcvbn.h>
#endif // NO_ZXCVBN
#include "mpassedit.h"

MPassEdit::MPassEdit(QWidget *parent)
    : QLineEdit(parent)
{
}

bool MPassEdit::isValid() const
{
    return lastValid;
}

void MPassEdit::setup(MPassEdit *slave, int min, int genMin, int wordMax)
{
    this->slave = slave;
    this->min = min;
    this->genMin = genMin;
    this->wordMax = wordMax;
    this->meter = meter;
    disconnect(this);
    disconnect(slave);
    connect(this, &QLineEdit::textChanged, this, &MPassEdit::masterTextChanged);
    connect(slave, &QLineEdit::textChanged, this, &MPassEdit::slaveTextChanged);
    if (min == 0) lastValid = true; // Control starts with no text
    generate(); // Pre-load the generator

    actionEye = addAction(QIcon(":/eye-show"), QLineEdit::TrailingPosition);
    actionEye->setCheckable(true);
    connect(actionEye, &QAction::toggled, this, &MPassEdit::eyeToggled);
    eyeToggled(false); // Initialize the eye.
    actionMeter = slave->addAction(QIcon(":/meter/wrong-0"), QLineEdit::TrailingPosition);
    connect(actionMeter, &QAction::triggered, slave, &QLineEdit::clear);

    masterTextChanged();
}

void MPassEdit::generate()
{
    static QStringList words;
    static int pos;
    if (words.isEmpty()) {
        QFile file("/usr/share/dict/words");
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            while (!file.atEnd()) words.append(file.readLine().trimmed());
            file.close();
        } else {
            words << "tuls" << "tihs" << "yssup" << "ssip" << "kcuf" << "gaf" << "ehcuod" << "kcid";
            words << "nmad" << "tnuc" << "parc" << "kcoc" << "hctib" << "dratsab" << "elohssa";
        }
        std::srand(unsigned(std::time(nullptr)));
        pos = words.count();
    }
    genText.clear();
    do {
        if (pos >= words.count()) {
            std::random_shuffle(words.begin(), words.end());
            pos = 0;
        }
        const QString &word = words.at(pos);
        if (word.length() < wordMax) {
            genText.append(words.at(pos));
            genText.append('-');
        }
        ++pos;
    } while (genText.length() <= genMin);
    genText.append(QString::number(std::rand() % 10));
}

void MPassEdit::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createStandardContextMenu();
    QAction *actGenPass = nullptr;
    if (slave != nullptr) {
        generate();
        menu->addSeparator();
        actGenPass = menu->addAction(tr("Use password") + ": " + genText);
    }
    const QAction *action = menu->exec(event->globalPos());
    if (actGenPass && action == actGenPass) {
        setText(genText);
        slave->setText(genText);
    }
    delete menu;
}

void MPassEdit::changeEvent(QEvent *event)
{
    const QEvent::Type etype = event->type();
    if(etype == QEvent::EnabledChange || etype == QEvent::Hide) {
        if(actionEye && !(isVisible() && isEnabled())) actionEye->setChecked(false);
    }
    QLineEdit::changeEvent(event);
}

void MPassEdit::masterTextChanged()
{
    slave->blockSignals(true);
    slave->clear();
    slave->blockSignals(false);
    #ifndef NO_ZXCVBN
    const double entropy = ZxcvbnMatch(text().toUtf8().constData(), nullptr, nullptr);
    if (entropy <= 0) score = 0; // Bad
    else if(entropy < 40) score = 1; // Poor
    else if(entropy < 65) score = 2; // Weak
    else if(entropy < 80) score = 3; // OK
    else if(entropy < 100) score = 4; // Good
    else score = 5; // Excellent
    #endif // NO_ZXCVBN
    slaveTextChanged(slave->text());
}

void MPassEdit::slaveTextChanged(const QString &slaveText)
{
    bool valid = (slaveText.length() >= min && slaveText == text());
    actionMeter->setIcon(QIcon((valid?":/meter/right-":":/meter/wrong-") + QString::number(score)));
    if (valid != lastValid) {
        lastValid = valid;
        emit validationChanged(valid);
    }
}

void MPassEdit::eyeToggled(bool checked)
{
    actionEye->setIcon(QIcon(checked ? ":/eye-hide" : ":/eye-show"));
    actionEye->setToolTip(checked ? tr("Hide the password") : tr("Show the password"));
    setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
}
