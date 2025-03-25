#include "parameters.h"
#include "parameterconnectionwidgets.h"

#include <QDrag>
#include <QHBoxLayout>
#include <QMimeData>
#include <QScrollArea>

#include <iostream>

using namespace gui;

QString gui::displayName(QString parameterName)
{
    return parameterName.replace("_", " ").trimmed();
}

QString gui::parameterName(QString displayName)
{
    return displayName.replace(" ", "_").trimmed();
}


ParameterConnectionLabel::ParameterConnectionLabel(int moduleId, const QString &paramName, QWidget *parent)
: QLabel(parent), m_moduleId(moduleId), m_paramName(paramName)
{}

void ParameterConnectionLabel::connectParam(int moduleId, const QString &paramName)
{
    if (moduleId == m_moduleId && paramName == m_paramName)
        return;
    m_connectedParameters.push_back({moduleId, paramName});
    setStyleSheet("color: blue; border: 1px solid black; padding: 2px;");
}

void ParameterConnectionLabel::disconnectParam(int moduleId, const QString &paramName)
{
    auto it = std::find_if(
        m_connectedParameters.begin(), m_connectedParameters.end(),
        [moduleId, paramName](const Connection &c) { return c.moduleId == moduleId && c.paramName == paramName; });
    if (it != m_connectedParameters.end())
        m_connectedParameters.erase(it);
    if (m_connectedParameters.empty())
        setStyleSheet("");
}

void ParameterConnectionLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QDrag *drag = new QDrag(this);
        QMimeData *mimeData = new QMimeData;

        // Set the MIME data
        mimeData->setText(QString("Module ID: %1\nParameter Name: %2\n").arg(m_moduleId).arg(m_paramName));
        QByteArray encodedData;
        QDataStream stream(&encodedData, QIODevice::WriteOnly);
        stream << m_moduleId << parameterName(m_paramName);
        mimeData->setData(Parameters::mimeFormat(), encodedData);
        drag->setMimeData(mimeData);

        // Start the drag event
        drag->exec(Qt::CopyAction | Qt::MoveAction);
        std::cerr << "dragging " << m_moduleId << ", " << m_paramName.toStdString() << std::endl;
    }
    QLabel::mousePressEvent(event);
}

void ParameterConnectionLabel::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_connectedParameters.empty() || event->button() != Qt::LeftButton) {
        QLabel::mouseReleaseEvent(event);
        return;
    }
    initParameterPopup();
    QPoint globalPos = mapToGlobal(QPoint(0, height()));
    m_parameterPopup->move(globalPos);
    m_parameterPopup->show();
    QLabel::mouseReleaseEvent(event);
}

void ParameterConnectionLabel::mouseReleaseEvent(QMouseEvent *event)
{
    mouseDoubleClickEvent(event);
}

void ParameterConnectionLabel::initParameterPopup()
{
    if (m_parameterPopup)
        return;
    QStringList parameters;
    for (const auto &c: m_connectedParameters) {
        parameters << QString("%1: %2").arg(c.moduleId).arg(c.paramName);
    }
    m_parameterPopup = new ParameterPopup(parameters);
    connect(m_parameterPopup, &ParameterPopup::parameterSelected, this, [this](const QString &param) {
        // Handle parameter button click
        // vistle::Port from(m_parameterConnectionRequest.moduleId, m_parameterConnectionRequest.paramName.toStdString(),
        //                   vistle::Port::Type::PARAMETER);
        // vistle::Port to(m_id, param.toStdString(), vistle::Port::Type::PARAMETER);
        // vistle::VistleConnection::the().connect(&from, &to);
        m_parameterPopup->close();
    });
    connect(m_parameterPopup, &ParameterPopup::parameterHovered, this,
            [this](int moduleId, const QString &param) { emit highlightModule(moduleId); });
    m_parameterPopup->setAttribute(Qt::WA_Hover);
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

bool ParameterPopup::event(QEvent *event)
{
    static QListWidgetItem *lastHoveredItem = nullptr;
    if (event->type() == QEvent::HoverMove) {
        QHoverEvent *hoverEvent = static_cast<QHoverEvent *>(event);
        QPoint listWidgetPos = m_listWidget->mapFrom(this, hoverEvent->pos());
        QListWidgetItem *item = m_listWidget->itemAt(listWidgetPos);
        if (item && item != lastHoveredItem) {
            lastHoveredItem = item;
            auto moduleId = item->text().split(":").first().toInt();
            auto paramName = item->text().split(":").last().trimmed();
            emit parameterHovered(moduleId, paramName);
        }
        if (!item) {
            lastHoveredItem = nullptr;
            emit parameterHovered(-1, "");
        }
    }
    return QWidget::event(event);
}

void ParameterPopup::setParameters(const QStringList &parameters)
{
    m_parameters = putSystemParamsAtTheEnd(parameters);
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
        QListWidgetItem *item = new QListWidgetItem(displayName(param), m_listWidget);
        m_listWidget->addItem(item);
    }
}

void ParameterPopup::onParameterSelected(QListWidgetItem *item)
{
    emit parameterSelected(parameterName(item->text()));
}