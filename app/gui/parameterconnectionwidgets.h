#ifndef VISTLE_GUI_PARAMETERCONNECTIONBUTTON_H
#define VISTLE_GUI_PARAMETERCONNECTIONBUTTON_H

#include <QPushButton>
#include <QString>
#include <QEvent>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <vector>
#include "qtpropertybrowser.h"
namespace gui {

QString displayName(QString parameterName);

QString parameterName(QString displayName);

class ParameterPopup;
class ParameterConnectionLabel: public QLabel {
    Q_OBJECT

public:
    ParameterConnectionLabel(int moduleId, const QString &paramName, QWidget *parent = nullptr);
    void connectParam(int moduleId, const QString &paramName);
    void disconnectParam(int moduleId, const QString &paramName);

signals:
    void highlightModule(int moduleId); //sends -1 if no module is to be highlighted
    void disconnectParameters(int fromId, const QString &fromName, int toId, const QString &toName);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    bool event(QEvent *event) override;

private:
    int m_moduleId;
    QString m_paramName;
    struct Connection {
        int moduleId;
        QString paramName;
    };
    std::vector<Connection> m_connectedParameters;
    std::unique_ptr<ParameterPopup> m_parameterPopup;
    bool m_pressed = false;
    void initParameterPopup();
};

class ParameterListItemWithX: public QWidget {
    Q_OBJECT

public:
    ParameterListItemWithX(const QString &text, QWidget *parent = nullptr);

signals:
    void removeRequested(const QString &text);

private slots:
    void onRemoveButtonClicked();

private:
    QLabel *m_label;
    QPushButton *m_removeButton;
    QString m_text;
};

class ParameterPopup: public QWidget {
    Q_OBJECT

public:
    ParameterPopup(const QStringList &parameters, QWidget *parent = nullptr);
    void setParameters(const QStringList &parameters);
    void enableXBtn(bool enable);
signals:
    void parameterSelected(const QString &param);
    void parameterHovered(int moduleId, const QString &param);
    void parameterDisconnected(int moduleId, const QString &param);

private slots:
    void filterParameters(const QString &query);
    void onParameterSelected(QListWidgetItem *item);

private:
    void populateListWidget(const QStringList &parameters);
    void populateListWidgetWithXBtn(const QStringList &parameters);
    bool event(QEvent *event) override;

    QStringList m_parameters;
    QListWidget *m_listWidget;
    QLineEdit *m_searchField;
    void (ParameterPopup::*populateFnc)(const QStringList &) = &ParameterPopup::populateListWidget;
};

class VistleAbstractPropertyManager: public QtAbstractPropertyManager {
    Q_OBJECT
    // QtProperty *createProperty() override;
};

} // namespace gui

#endif // VISTLE_GUI_PARAMETERCONNECTIONBUTTON_H