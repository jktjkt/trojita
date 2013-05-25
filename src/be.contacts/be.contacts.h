#ifndef BE_CONTACTED_H
#define BE_CONTACTED_H

class QFileSystemWatcher;
class QModelIndex;
class QStandardItem;
class QStandardItemModel;
class QSortFilterProxyModel;

namespace Ui {
    class Contacts;
    class OneContact;
}

#include <QDialog>
#include <QPixmap>

namespace BE {
    class Field;
    class Contacts : public QDialog {
        Q_OBJECT
    public:
        Contacts();
    protected:
        bool eventFilter(QObject *o, QEvent *e);
    private slots:
        void addContact();
        void scheduleAbookUpdate();
        void updateAbook();
        void updateFocusPolicy(QWidget *oldFocus, QWidget *newFocus);
        void removeCurrentContact();
        void remonitorAdressbook();
        void saveContacts();
        void setContact(const QModelIndex &index);
    private:
        void ensureAbookPath();
        void importPhoto(const QString &path);
        bool setPhoto(const QString &path);
        void readAbook(bool update = false);
    private:
        QStandardItem *m_currentContact;
        QStandardItemModel *m_contacts;
        QSortFilterProxyModel *m_sortFilterProxy;
        Ui::Contacts *m_ui;
        Ui::OneContact *m_ui2;
        QPixmap m_incognitoPic;
        QFileSystemWatcher *m_filesystemWatcher;
        QTimer *m_updateTimer;
    };
} // namepsace

#endif // BE_CONTACTED_H
