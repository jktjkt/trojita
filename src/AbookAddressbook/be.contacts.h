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

namespace Gui {
class AbookAddressbook;
}

#include <QDialog>
#include <QPixmap>

namespace BE {
    class Field;
    class Contacts : public QDialog {
        Q_OBJECT
    public:
        explicit Contacts(Gui::AbookAddressbook *abook);
        virtual ~Contacts();

        void manageContact(const QString &mail, const QString &prettyName);

    protected:
        bool eventFilter(QObject *o, QEvent *e);
        virtual void closeEvent(QCloseEvent *);
    private slots:
        void addContact();
        void updateFocusPolicy(QWidget *oldFocus, QWidget *newFocus);
        void removeCurrentContact();
        void saveContacts();
        void setContact(const QModelIndex &index);
    private:
        void importPhoto(const QString &path);
        bool setPhoto(const QString &path);
    private:
        QStandardItem *m_currentContact;
        QSortFilterProxyModel *m_sortFilterProxy;
        Ui::Contacts *m_ui;
        Ui::OneContact *m_ui2;
        QPixmap m_incognitoPic;
        Gui::AbookAddressbook *m_abook;
        bool m_dirty;
    };
} // namepsace

#endif // BE_CONTACTED_H
