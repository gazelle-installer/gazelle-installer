// MLineEdit class - QLineEdit modified for passwords.
//
//   Copyright (C) 2021 by AK-47
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
// This file is part of the gazelle-installer.

#ifndef MLINEEDIT_H
#define MLINEEDIT_H

#include <QLineEdit>
#include <QPushButton>

class MLineEdit : public QLineEdit
{
    Q_OBJECT
private:
    MLineEdit *slave = nullptr;
    QPushButton *nextButton = nullptr;
    QString genText;
    int min, genMin, wordMax;
    bool lastValid = false;
    void generate();
    void menuGenText();
    void masterTextChanged();
    void slaveTextChanged(const QString &slaveText);
protected:
    void contextMenuEvent(QContextMenuEvent *event);
public:
    MLineEdit(QWidget *parent = nullptr);
    void setup(MLineEdit *slave, int min=0, int genMin=16, int wordMax=5);
    bool isValid();
signals:
    void validationChanged(bool valid);
};

#endif // MLINEEDIT_H
