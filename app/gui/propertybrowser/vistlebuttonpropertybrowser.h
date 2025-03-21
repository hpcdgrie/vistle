#ifndef VISTLE_GUI_BUTTON_PROPERTYBROWSER_H
#define VISTLE_GUI_BUTTON_PROPERTYBROWSER_H

#include "qtpropertybrowser.h"

//unfortunately, the QtButtonPropertyBrowser implementation is hidden in QtButtonPropertyBrowserPrivate, so we have to copy the code
class VistleButtonPropertyBrowserPrivate;

class QT_QTPROPERTYBROWSER_EXPORT VistleButtonPropertyBrowser : public QtAbstractPropertyBrowser
{
    Q_OBJECT
public:

    VistleButtonPropertyBrowser(QWidget *parent = 0);
    ~VistleButtonPropertyBrowser();

    void setExpanded(QtBrowserItem *item, bool expanded);
    bool isExpanded(QtBrowserItem *item) const;

Q_SIGNALS:

    void collapsed(QtBrowserItem *item);
    void expanded(QtBrowserItem *item);

protected:
    virtual void itemInserted(QtBrowserItem *item, QtBrowserItem *afterItem);
    virtual void itemRemoved(QtBrowserItem *item);
    virtual void itemChanged(QtBrowserItem *item);
    // additional nodule id of the module for which the properties are shown
    int m_moduleId = 0;
private:
    VistleButtonPropertyBrowserPrivate *d_ptr;
    Q_DECLARE_PRIVATE(VistleButtonPropertyBrowser)
    Q_DISABLE_COPY(VistleButtonPropertyBrowser)
    Q_PRIVATE_SLOT(d_func(), void slotUpdate())
    Q_PRIVATE_SLOT(d_func(), void slotEditorDestroyed())
    Q_PRIVATE_SLOT(d_func(), void slotToggled(bool))

};

#endif // VISTLE_GUI_BUTTON_PROPERTYBROWSER_H