#include <QDebug>
#include "msettings.h"

MSettings::MSettings(const QString &fileName, QObject *parent)
    : QSettings(fileName, QSettings::NativeFormat, parent)
{
}

void MSettings::dumpDebug()
{
    qDebug().noquote() << "Configuration:" << fileName();
    // top-level settings (version, etc)
    QStringList chkeys = childKeys();
    for (QString &strkey : chkeys) {
        qDebug().noquote().nospace() << " - " << strkey
                                     << ": " << value(strkey).toString();
    }
    // iterate through groups
    QStringList chgroup = childGroups();
    for (QString &strgroup : chgroup) {
        beginGroup(strgroup);
        qDebug().noquote().nospace() << " = " << strgroup << ":";
        QStringList chkeys = childKeys();
        for (QString &strkey : chkeys) {
            QString val(value(strkey).toString());
            if (!val.isEmpty() && strkey.endsWith("Pass", Qt::CaseInsensitive)) {
                val = "???";
            }
            qDebug().noquote().nospace() << "   - " << strkey << ": " << val;
        }
        endGroup();
    }
    qDebug() << "End of configuration.";
}

void MSettings::setSave(bool save)
{
    saving = save;
}

void MSettings::startGroup(const QString &prefix, QWidget *wgroup)
{
    beginGroup(prefix);
    group = wgroup;
}

void MSettings::setGroupWidget(QWidget *wgroup)
{
    group = wgroup;
}

void MSettings::markBadWidget(QWidget *widget)
{
    if (widget) {
        widget->setStyleSheet("QWidget { background: maroon; border: 2px inset red; }\n"
                              "QPushButton:!pressed { border-style: outset; }\n"
                              "QRadioButton { border-style: dotted; }");
    }
    if (group) group->setProperty("BAD", true);
    bad = true;
}

bool MSettings::isBadWidget(QWidget *widget)
{
    return widget->property("BAD").toBool();
}

void MSettings::manageComboBox(const QString &key, QComboBox *combo, const bool useData)
{
    const QVariant &comboval = useData ? combo->currentData() : QVariant(combo->currentText());
    if (saving) setValue(key, comboval);
    else {
        const QVariant &val = value(key, comboval);
        const int icombo = useData ? combo->findData(val, Qt::UserRole, Qt::MatchFixedString)
                         : combo->findText(val.toString(), Qt::MatchFixedString);
        if (icombo >= 0) combo->setCurrentIndex(icombo);
        else markBadWidget(combo);
    }
}

void MSettings::manageCheckBox(const QString &key, QCheckBox *checkbox)
{
    const QVariant state(checkbox->isChecked());
    if (saving) setValue(key, state);
    else checkbox->setChecked(value(key, state).toBool());
}

void MSettings::manageLineEdit(const QString &key, QLineEdit *lineedit)
{
    const QString &text = lineedit->text();
    if (saving) setValue(key, text);
    else lineedit->setText(value(key, text).toString());
}

void MSettings::manageSpinBox(const QString &key, QSpinBox *spinbox)
{
    const QVariant spinval(spinbox->value());
    if (saving) setValue(key, spinval);
    else {
        const int val = value(key, spinval).toInt();
        spinbox->setValue(val);
        if (val != spinbox->value()) markBadWidget(spinbox);
    }
}

int MSettings::manageEnum(const QString &key, const int nchoices,
        const char *choices[], const int curval)
{
    QVariant choice(curval >= 0 ? choices[curval] : "");
    if (saving) setValue(key, choice);
    else {
        const QString &val = value(key, choice).toString();
        for (int ixi = 0; ixi < nchoices; ++ixi) {
            if (!val.compare(QString(choices[ixi]), Qt::CaseInsensitive)) {
                return ixi;
            }
        }
        return -1;
    }
    return curval;
}

void MSettings::manageRadios(const QString &key, const int nchoices,
        const char *choices[], QRadioButton *radios[])
{
    // obtain the current choice
    int ixradio = -1;
    for (int ixi = 0; ixradio < 0 && ixi < nchoices; ++ixi) {
        if (radios[ixi]->isChecked()) ixradio = ixi;
    }
    // select the corresponding radio button
    ixradio = manageEnum(key, nchoices, choices, ixradio);
    if (ixradio >= 0) radios[ixradio]->setChecked(true);
    else {
        for (int ixi = 0; ixi < nchoices; ++ixi) {
            markBadWidget(radios[ixi]);
        }
    }
}
