/***************************************************************************
 * PassEdit class - QLineEdit operating as a pair for editing passwords.
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
#include "passedit.h"

PassEdit::PassEdit(QWidget *parent)
    : QLineEdit(parent)
{
}

bool PassEdit::isValid() const
{
    return lastValid;
}

void PassEdit::setup(PassEdit *slave, int min, int genMin, int wordMax)
{
    this->slave = slave;
    this->min = min;
    this->genMin = genMin;
    this->wordMax = wordMax;
    disconnect(this);
    disconnect(slave);
    connect(this, &QLineEdit::textChanged, this, &PassEdit::masterTextChanged);
    connect(slave, &QLineEdit::textChanged, this, &PassEdit::slaveTextChanged);
    if (min == 0) lastValid = true; // Control starts with no text
    generate(); // Pre-load the generator

    actionEye = addAction(QIcon(":/eye-show"), QLineEdit::TrailingPosition);
    actionEye->setCheckable(true);
    connect(actionEye, &QAction::toggled, this, &PassEdit::eyeToggled);
    eyeToggled(false); // Initialize the eye.
    #ifndef NO_ZXCVBN
    actionMeter = slave->addAction(QIcon(":/meter/0"), QLineEdit::TrailingPosition);
    #endif

    masterTextChanged();
}

void PassEdit::generate()
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

void PassEdit::contextMenuEvent(QContextMenuEvent *event)
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

void PassEdit::changeEvent(QEvent *event)
{
    const QEvent::Type etype = event->type();
    if(etype == QEvent::EnabledChange || etype == QEvent::Hide) {
        if(actionEye && !(isVisible() && isEnabled())) actionEye->setChecked(false);
    }
    QLineEdit::changeEvent(event);
}

void PassEdit::masterTextChanged()
{
    slave->clear();
    setPalette(QPalette());
    slave->setPalette(QPalette());
    const bool valid = (text().isEmpty() && min == 0);

    #ifndef NO_ZXCVBN
    const double entropy = ZxcvbnMatch(text().toUtf8().constData(), nullptr, nullptr);
    int score;
    if (entropy <= 0) score = 0; // Negligible
    else if(entropy < 40) score = 1; // Very weak
    else if(entropy < 70) score = 2; // Weak
    else if(entropy < 100) score = 3; // Moderate
    else if(entropy < 130) score = 4; // Strong
    else score = 5; // Very strong
    actionMeter->setIcon(QIcon(":/meter/" + QString::number(score)));
    static const char *ratings[] = {
        QT_TR_NOOP("Negligible"), QT_TR_NOOP("Very weak"), QT_TR_NOOP("Weak"),
        QT_TR_NOOP("Moderate"), QT_TR_NOOP("Strong"), QT_TR_NOOP("Very strong")
    };
    actionMeter->setToolTip(tr("Password strength: %1").arg(tr(ratings[score])));
    #endif

    // The validation could change if the box is empty and no minimum is set.
    if (valid != lastValid) {
        lastValid = valid;
        emit validationChanged(valid);
    }
}

void PassEdit::slaveTextChanged(const QString &slaveText)
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

void PassEdit::eyeToggled(bool checked)
{
    actionEye->setIcon(QIcon(checked ? ":/eye-hide" : ":/eye-show"));
    actionEye->setToolTip(checked ? tr("Hide the password") : tr("Show the password"));
    setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
}
