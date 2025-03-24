#ifndef VISTLE_GUI_PARAMETERCONNECTIONBUTTON_H
#define VISTLE_GUI_PARAMETERCONNECTIONBUTTON_H

#include <QPushButton>
#include <QString>
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
protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void mouseDoubleClickEvent(QMouseEvent *event);

private:
    int m_moduleId;
    QString m_paramName;
    struct Connection {
        int moduleId;
        QString paramName;
    };
    std::vector<Connection> m_connectedParameters;
    ParameterPopup *m_parameterPopup = nullptr;

    void initParameterPopup();
};

class ParameterPopup: public QWidget {
    Q_OBJECT

public:
    ParameterPopup(const QStringList &parameters, QWidget *parent = nullptr);
    void setParameters(const QStringList &parameters);
signals:
    void parameterSelected(const QString &param);
    void parameterHovered(int moduleId, const QString &param);

private slots:
    void filterParameters(const QString &query);
    void onParameterSelected(QListWidgetItem *item);

private:
    void populateListWidget(const QStringList &parameters);
    bool event(QEvent *event) override;

    QStringList m_parameters;
    QListWidget *m_listWidget;
    QLineEdit *m_searchField;
};

class VistleAbstractPropertyManager: public QtAbstractPropertyManager {
    Q_OBJECT
    // QtProperty *createProperty() override;
};

} // namespace gui

#endif // VISTLE_GUI_PARAMETERCONNECTIONBUTTON_H