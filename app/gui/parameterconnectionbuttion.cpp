#include "parameters.h"
#include "parameterconnectionbuttion.h"

#include <QDrag>
#include <QHBoxLayout>
#include <QMimeData>
#include <QScrollArea>

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
    QLineEdit *searchField = new QLineEdit(this);
    searchField->setPlaceholderText("Search...");
    layout->addWidget(searchField);
    connect(searchField, &QLineEdit::textChanged, this, &ParameterPopup::filterParameters);

    // Add scroll area
    m_scrollArea = new QScrollArea(this);
    m_container = new QWidget(m_scrollArea);
    m_containerLayout = new QVBoxLayout(m_container);

    m_container->setLayout(m_containerLayout);
    m_scrollArea->setWidget(m_container);
    m_scrollArea->setWidgetResizable(true);
    layout->addWidget(m_scrollArea);

    setLayout(layout);
    setMaximumHeight(300); // Set the maximum height for the popup

    // Populate initial buttons
    populateButtons(m_parameters);
}

void ParameterPopup::filterParameters(const QString &query)
{
    QStringList filteredParameters;
    for (const QString &param: m_parameters) {
        if (param.contains(query, Qt::CaseInsensitive)) {
            filteredParameters << param;
        }
    }
    populateButtons(filteredParameters);
}

void ParameterPopup::populateButtons(const QStringList &parameters)
{
    // Clear existing buttons
    QLayoutItem *child;
    while ((child = m_containerLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    // Add new buttons
    for (const QString &param: parameters) {
        QPushButton *button = new QPushButton(param, this);
        m_containerLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, param]() { emit parameterClicked(param); });
    }
}