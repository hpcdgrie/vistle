#ifndef VISTLE_GUI_PARAMETERCONNECTIONBUTTON_H
#define VISTLE_GUI_PARAMETERCONNECTIONBUTTON_H

#include <QPushButton>
#include <QString>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLineEdit>

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

class ParameterPopup: public QWidget {
    Q_OBJECT

public:
    ParameterPopup(const QStringList &parameters, QWidget *parent = nullptr);

signals:
    void parameterClicked(const QString &param);

private slots:
    void filterParameters(const QString &query);

private:
    void populateButtons(const QStringList &parameters);

    QStringList m_parameters;
    QScrollArea *m_scrollArea;
    QWidget *m_container;
    QVBoxLayout *m_containerLayout;
};

} // namespace gui

#endif // VISTLE_GUI_PARAMETERCONNECTIONBUTTON_H