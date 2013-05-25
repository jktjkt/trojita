#include <QApplication>
#include <QDir>
#include <QSettings>
#include "AbookAddressbook/AbookAddressbook.h"
#include "AbookAddressbook/be.contacts.h"

int main(int argc, char **argv) {
    if (argc > 1 && argv[1][0] != '-') {
        QCoreApplication a(argc, argv);
        if (qstrcmp(argv[1], "import")) {
            qWarning("unknown command \"%s\"", argv[1]);
            return 1;
        }
        if (argc == 2) {
            qWarning("you must specify an adress string to import, eg.\n"
            "%s import \"Joe User <joe@users.com>\"", argv[0]);
            return 2;
        }
        QMap<QString, QString> imports;
        for (int i = 2; i < argc; ++i) {
            QString arg = QString::fromLocal8Bit(argv[i]);
            QString mail, name;
            QStringList contact = arg.split(" ", QString::SkipEmptyParts);
            foreach (const QString &token, contact) {
                if (token.contains('@'))
                    mail = token;
                else
                    name = name.isEmpty() ? token : name + " " + token;
            }
            if (mail.isEmpty()) {
                qWarning("error, \"%s\" does not seem to contain a mail address", arg.toLocal8Bit().data());
                continue;
            }
            mail.remove('<');
            mail.remove('>');
            if (name.isEmpty())
                name = mail;
            imports.insert(name, mail);
        }
        if (imports.isEmpty()) {
            qWarning("nothing to import");
            return 2;
        }

        int lastContact = -1, updates = 0, adds = 0;
        QSettings abook(QDir::homePath() + "/.abook/addressbook", QSettings::IniFormat);
        abook.setIniCodec("UTF-8");
        QStringList contacts = abook.childGroups();
        foreach (const QString &contact, contacts) {
            abook.beginGroup(contact);
            int id = contact.toInt();
            if (id > lastContact)
                lastContact = id;
            const QString name = abook.value("name", QString()).toString();
            QMap<QString,QString>::iterator it = imports.begin(), end = imports.end();
            while (it != end) {
                if (it.key() == name) {
                    QStringList mails = abook.value("email", QString()).toStringList();
                    if (!mails.contains(it.value())) {
                        ++updates;
                        mails << it.value();
                        abook.setValue("email", mails);
                    }
                    it = imports.erase(it);
                    continue;
                }
                else
                    ++it;
            }
            abook.endGroup();
        }
        QMap<QString,QString>::const_iterator it = imports.constBegin(), end = imports.constEnd();
        while (it != end) {
            ++adds;
            abook.beginGroup(QString::number(++lastContact));
            abook.setValue("name", it.key());
            abook.setValue("email", it.value());
            abook.endGroup();
            ++it;
        }
        qWarning("updated %d and added %d contacts", updates, adds);
        return 0;
    }
    QApplication a(argc, argv);
    BE::Contacts w(new Gui::AbookAddressbook());
    w.show();
    return a.exec();
}
