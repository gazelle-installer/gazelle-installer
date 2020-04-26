// Basic partition editor for the installer.
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

#ifndef MPARTED_H
#define MPARTED_H

#include <QObject>
#include <QTreeWidget>

#include "mprocess.h"
#include "blockdev.h"

class MParted : public QObject
{
    Q_OBJECT
    MProcess &proc;
    BlockDeviceList &listBlkDevs;
    QTreeWidget *treePartitions;
    QStringList listUsePresets;
    void comboUseEditFinish();
public:
    explicit MParted(MProcess &mproc, BlockDeviceList &bdlist, QObject *parent = nullptr);
    void setup(QTreeWidget *twParts);
    void populate();
signals:

public slots:
};

#endif // MPARTED_H
