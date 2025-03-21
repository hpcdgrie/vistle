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

#include "qtpropertybrowser.h"
namespace gui {

class ParameterConnectionBtn: public QPushButton {
    Q_OBJECT

public:
    ParameterConnectionBtn(int moduleId, const QString &paramName, QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;


private:
    int m_moduleId;
    QString m_paramName;
};

class ParameterConnectionLabel: public QLabel {
    Q_OBJECT

public:
    ParameterConnectionLabel(int moduleId, const QString &paramName, QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;


private:
    int m_moduleId;
    QString m_paramName;
};

class ParameterPopup: public QWidget {
    Q_OBJECT

public:
    ParameterPopup(const QStringList &parameters, QWidget *parent = nullptr);
    void setParameters(const QStringList &parameters);
signals:
    void parameterSelected(const QString &param);

private slots:
    void filterParameters(const QString &query);
    void onParameterSelected(QListWidgetItem *item);

private:
    void populateListWidget(const QStringList &parameters);

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