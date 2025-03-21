#include "parameters.h"
#include "parameterconnectionwidgets.h"

#include <QDrag>
#include <QHBoxLayout>
#include <QMimeData>
#include <QScrollArea>

#include <iostream>

using namespace gui;

ParameterConnectionBtn::ParameterConnectionBtn(int moduleId, const QString &paramName, QWidget *parent)
: QPushButton("...", parent), m_moduleId(moduleId), m_paramName(paramName)
{}

void ParameterConnectionBtn::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QDrag *drag = new QDrag(this);
        QMimeData *mimeData = new QMimeData;

        // Set the MIME data
        mimeData->setText(QString("Module ID: %1\nParameter Name: %2\n").arg(m_moduleId).arg(m_paramName));
        QByteArray encodedData;
        QDataStream stream(&encodedData, QIODevice::WriteOnly);
        stream << m_moduleId << m_paramName;
        mimeData->setData(Parameters::mimeFormat(), encodedData);
        drag->setMimeData(mimeData);

        // Start the drag event
        drag->exec(Qt::CopyAction | Qt::MoveAction);
    }
    QPushButton::mousePressEvent(event);
}

ParameterConnectionLabel::ParameterConnectionLabel(int moduleId, const QString &paramName, QWidget *parent)
: QLabel(parent), m_moduleId(moduleId), m_paramName(paramName)
{}

void ParameterConnectionLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QDrag *drag = new QDrag(this);
        QMimeData *mimeData = new QMimeData;

        // Set the MIME data
        mimeData->setText(QString("Module ID: %1\nParameter Name: %2\n").arg(m_moduleId).arg(m_paramName));
        QByteArray encodedData;
        QDataStream stream(&encodedData, QIODevice::WriteOnly);
        stream << m_moduleId << m_paramName;
        mimeData->setData(Parameters::mimeFormat(), encodedData);
        drag->setMimeData(mimeData);

        // Start the drag event
        drag->exec(Qt::CopyAction | Qt::MoveAction);
        std::cerr << "dragging " << m_moduleId << ", " << m_paramName.toStdString() << std::endl;
    }
    QLabel::mousePressEvent(event);
}

QStringList putSystemParamsAtTheEnd(const QStringList &params)
{
    auto parameters = params;
    std::sort(parameters.begin(), parameters.end(), [](const QString &a, const QString &b) {
        if (a.startsWith('_') && !b.startsWith('_')) {
            return false;
        }
        if (!a.startsWith('_') && b.startsWith('_')) {
            return true;
        }
        return a < b;
    });
    return parameters;
}

ParameterPopup::ParameterPopup(const QStringList &parameters, QWidget *parent)
: QWidget(parent, Qt::Popup), m_parameters(parameters)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    // Add search field
    m_searchField = new QLineEdit(this);
    m_searchField->setPlaceholderText("Search...");
    layout->addWidget(m_searchField);
    connect(m_searchField, &QLineEdit::textChanged, this, &ParameterPopup::filterParameters);

    // Add list widget
    m_listWidget = new QListWidget(this);
    layout->addWidget(m_listWidget);
    connect(m_listWidget, &QListWidget::itemClicked, this, &ParameterPopup::onParameterSelected);

    setLayout(layout);
    setMaximumHeight(300); // Set the maximum height for the popup

    // Populate initial list widget
    populateListWidget(m_parameters);
}

void ParameterPopup::setParameters(const QStringList &parameters)
{
    m_parameters = parameters;
    populateListWidget(m_parameters);
}

void ParameterPopup::filterParameters(const QString &query)
{
    QStringList filteredParameters;
    for (const QString &param: m_parameters) {
        if (param.contains(query, Qt::CaseInsensitive)) {
            filteredParameters << param;
        }
    }
    populateListWidget(filteredParameters);
}

void ParameterPopup::populateListWidget(const QStringList &parameters)
{
    m_listWidget->clear();
    for (const QString &param: parameters) {
        QListWidgetItem *item = new QListWidgetItem(param, m_listWidget);
        m_listWidget->addItem(item);
    }
}

void ParameterPopup::onParameterSelected(QListWidgetItem *item)
{
    emit parameterSelected(item->text());
}