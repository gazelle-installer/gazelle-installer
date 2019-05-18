//
//   Copyright (C) 2003-2009 by Warren Woodford
//   Heavily edited, with permision, by anticapitalista for antiX 2011-2014.
//   Heavily revised by dolphin oracle, adrian, and anticaptialista 2018.
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

#include <QSettings>
#include <QDebug>

#include "mmain.h"
#include "minstall.h"



MInstall *minstall;
bool firstShow;

MMain::MMain(QStringList args = QStringList())
{
    setupUi(this);
    minstall = new MInstall(mainFrame, args);

    //setup system variables
    QSettings settings("/usr/share/gazelle-installer-data/installer.conf", QSettings::NativeFormat);
    PROJECTNAME = settings.value("PROJECT_NAME").toString();
    PROJECTSHORTNAME = settings.value("PROJECT_SHORTNAME").toString();
    PROJECTVERSION = settings.value("VERSION").toString();
    setWindowTitle(PROJECTNAME + " " + tr("Installer"));
    firstShow = true;

    setWindowFlags(Qt::Window); // for the close, min and max buttons
    // ensure the help widgets are displayed correctly when started
    // this heap-allocated object will be deleted by Qt when posted
    QResizeEvent *evresize = new QResizeEvent(size(), size());
    qApp->postEvent(this, evresize);
    QEvent evpalette(QEvent::ApplicationPaletteChange);
    changeEvent(&evpalette);
}

MMain::~MMain() {
}

/////////////////////////////////////////////////////////////////////////

void MMain::setHelpText(const QString &text)
{
    mainHelp->setText(text);
}

void MMain::closeEvent(QCloseEvent *event)
{
    if (minstall->abort(true)) {
        event->accept();
        minstall->cleanup();
        QWidget::closeEvent(event);
    } else {
        event->ignore();
    }
}

/////////////////////////////////////////////////////////////////////////
// public slots

void MMain::showEvent(QShowEvent *)
{
    if (firstShow) {
        firstShow = false;
        minstall->firstRefresh(this);
    }
}

void MMain::resizeEvent(QResizeEvent *)
{
    minstall->resize(mainFrame->size());
    mainHelp->resize(tab->size());
    helpbackdrop->resize(mainHelp->size());
}

void MMain::changeEvent(QEvent *event)
{
    const QEvent::Type etype = event->type();
    if (etype == QEvent::ApplicationPaletteChange
        || etype == QEvent::PaletteChange || etype == QEvent::StyleChange)
    {
        QPalette pal = mainHelp->style()->standardPalette();
        QColor col = pal.color(QPalette::Base);
        col.setAlpha(150);
        pal.setColor(QPalette::Base, col);
        mainHelp->setPalette(pal);
    }
}
