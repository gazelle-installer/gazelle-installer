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
#endif
#include "mpassedit.h"

MPassEdit::MPassEdit(QWidget *parent)
    : QLineEdit(parent)
{
}

bool MPassEdit::isValid() const
{
    return lastValid;
}

void MPassEdit::setup(MPassEdit *slave, QProgressBar *meter,
                      int min, int genMin, int wordMax)
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
    if (meter) {
        meter->setRange(0, 100);
        meter->setValue(0);
        meter->setFormat("!!!");
        meter->setTextVisible(false);
    }
    actionEye = addAction(QIcon(":/eye-show"), QLineEdit::TrailingPosition);
    actionEye->setCheckable(true);
    connect(actionEye, &QAction::toggled, this, &MPassEdit::eyeToggled);
    eyeToggled(false); // Initialize the eye.

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
    slave->clear();
    setPalette(QApplication::palette());
    slave->setPalette(QApplication::palette());
    const bool valid = (text().isEmpty() && min == 0);
    if (meter) {
        #ifndef NO_ZXCVBN
        int score = 4;
        double entropy = ZxcvbnMatch(text().toUtf8().constData(), nullptr, nullptr);
        // entropy *= 0.301029996; // Log
        qDebug() << log10(entropy);
        #else // !NO_ZXCVBN

        int score = 0;
        const QString &t = text();
        if (!t.isEmpty()) {
            int changes = 0;
            int cats = 0;
            QChar oldchar = '\0';
            for (const QChar &c : t) {
                if (oldchar != c) {
                    ++changes;
                    oldchar = c;
                }
                if (c.isUpper()) cats |= 1;
                else if (c.isLower()) cats |= 2;
                else if (c.isSpace()) cats |= 4;
                else if (c.isPunct()) cats |= 8;
                else cats |= 16;
            }
            int textLen = t.length();
            changes = (changes * 14) / textLen;
            for (int i = 0; i < 5; ++i) {
                score += (cats&1) * changes;
                cats >>= 1;
            }
            score += textLen <= 30 ? textLen : 30;
        }
        #endif // NO_ZXCVBN
        meter->setValue(score);
        QPalette pal = meter->palette();
        score = (255 * score) / 100;
        pal.setColor(QPalette::Highlight, QColor(255 - score, score, 0, 70));
        meter->setPalette(pal);
    }
    if (valid != lastValid) {
        lastValid = valid;
        emit validationChanged(valid);
    }
}

void MPassEdit::slaveTextChanged(const QString &slaveText)
{
    QPalette pal = palette();
    bool valid = true;
    if (slaveText == text()) {
        QColor col(255, 255, 0, 40);
        if (slaveText.length() >= min) col.setRgb(0, 255, 0, 40);
        else valid = false;
        pal.setColor(QPalette::Base, col);
    } else {
        pal.setColor(QPalette::Base, QColor(255, 0, 0, 70));
        valid = false;
    }
    setPalette(pal);
    slave->setPalette(pal);
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
