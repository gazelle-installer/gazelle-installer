// Basic partition manager for the installer.
//
//   Copyright (C) 2020 by AK-47
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

#ifndef PARTMAN_H
#define PARTMAN_H

#include <QObject>
#include <QTreeWidget>
#include <QString>
#include <QMap>

#include "mprocess.h"
#include "blockdev.h"
#include "ui_meinstall.h"

class PartMan : public QObject
{
    Q_OBJECT
    enum TreeColumns {
        Device,
        Size,
        Label,
        UseFor,
        Encrypt,
        Type,
        Options
    };
    MProcess &proc;
    BlockDeviceList &listBlkDevs;
    Ui::MeInstall &gui;
    QWidget *master;
    QStringList listUsePresets;
    QMap<QString, QTreeWidgetItem *> mounts;
    QList<QTreeWidgetItem *> swaps;
    void comboUseTextChange(const QString &text);
public:
    bool sync;
    PartMan(MProcess &mproc, BlockDeviceList &bdlist, Ui::MeInstall &ui, QWidget *parent);
    void populate();
    QWidget *composeValidate(const QString &minSizeText, bool automatic, const QString &project);
signals:

public slots:
};

#endif // PARTMAN_H
