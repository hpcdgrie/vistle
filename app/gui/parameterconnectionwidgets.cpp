#include "parameters.h"
#include "parameterconnectionwidgets.h"

#include <QBuffer>
#include <QDrag>
#include <QHBoxLayout>
#include <QMimeData>
#include <QPainter>
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

void setParamText(QLabel *label, const QString &paramName, bool connected)
{
    QPixmap iconPixmap(":/icons/parameterconnected.svg");

    int iconHeight = label->fontMetrics().height();
    QPixmap scaledPixmap = iconPixmap.scaledToHeight(0.8 * iconHeight, Qt::SmoothTransformation);

    if (connected) {
        QPalette p;
        QColor color = p.color(QPalette::Active, QPalette::Highlight);
        std::cerr << "color: " << color.red() << " " << color.green() << " " << color.blue() << std::endl;
        // Create a new pixmap with the same size as the scaled pixmap
        QPixmap coloredPixmap(scaledPixmap.size());
        coloredPixmap.fill(Qt::transparent); // Fill with transparent color

        // Use QPainter to draw the original pixmap with a color overlay
        QPainter painter(&coloredPixmap);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawPixmap(0, 0, scaledPixmap);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(coloredPixmap.rect(), color); // Set the desired color (e.g., red)
        painter.end();
        scaledPixmap = coloredPixmap;
    }

    // Convert the pixmap to base64
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    scaledPixmap.save(&buffer, "PNG");
    QString base64Data = QString::fromLatin1(byteArray.toBase64().data());
    // Set the HTML content of the label
    QString htmlContent = QString("<img src='data:image/png;base64,%1'> %2").arg(base64Data).arg(paramName);
    label->setText(htmlContent);
}

ParameterConnectionLabel::ParameterConnectionLabel(int moduleId, const QString &paramName, QWidget *parent)
: QLabel(parent), m_moduleId(moduleId), m_paramName(paramName)
{
    setParamText(this, displayName(paramName), false);
}

void ParameterConnectionLabel::connectParam(int moduleId, const QString &paramName)
{
    if (moduleId == m_moduleId && paramName == m_paramName)
        return;
    m_connectedParameters.push_back({moduleId, paramName});
    // setStyleSheet("color: blue; border: 1px solid black; padding: 2px;");
    setParamText(this, displayName(m_paramName), true);
}

void ParameterConnectionLabel::disconnectParam(int moduleId, const QString &paramName)
{
    auto it = std::find_if(
        m_connectedParameters.begin(), m_connectedParameters.end(),
        [moduleId, paramName](const Connection &c) { return c.moduleId == moduleId && c.paramName == paramName; });
    if (it != m_connectedParameters.end())
        m_connectedParameters.erase(it);
    if (m_connectedParameters.empty())
        setParamText(this, displayName(m_paramName), false);
}

void ParameterConnectionLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
    }
    QLabel::mousePressEvent(event);
}

void ParameterConnectionLabel::mouseReleaseEvent(QMouseEvent *event)
{
    m_pressed = false;
    if (event->button() != Qt::LeftButton) {
        return;
    }
    initParameterPopup();
    if (m_parameterPopup->isVisible()) {
        m_parameterPopup->close();
        return;
    }
    QPoint globalPos = mapToGlobal(QPoint(0, height()));
    m_parameterPopup->move(globalPos);
    m_parameterPopup->show();
}

bool ParameterConnectionLabel::event(QEvent *event)
{
    if (event->type() == QEvent::Enter) {
        setStyleSheet("background-color: lightgray;");
        for (const auto &param: m_connectedParameters) {
            highlightModule(param.moduleId);
        }
    } else if (event->type() == QEvent::Leave) {
        setStyleSheet("");
        highlightModule(-1);
    } else if (m_pressed && event->type() == QEvent::MouseMove) {
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
        m_pressed = false;
    }
    return QLabel::event(event);
}

void ParameterConnectionLabel::initParameterPopup()
{
    QStringList parameters;
    for (const auto &c: m_connectedParameters) {
        parameters << QString("%1: %2").arg(c.moduleId).arg(c.paramName);
    }
    m_parameterPopup = std::make_unique<ParameterPopup>(parameters);
    m_parameterPopup->enableXBtn(true);
    connect(m_parameterPopup.get(), &ParameterPopup::parameterSelected, this,
            [this](const QString &param) { m_parameterPopup->close(); });
    connect(m_parameterPopup.get(), &ParameterPopup::parameterHovered, this,
            [this](int moduleId, const QString &param) { emit highlightModule(moduleId); });
    connect(m_parameterPopup.get(), &ParameterPopup::parameterDisconnected, this,
            [this](int moduleId, const QString &param) {
                emit disconnectParameters(m_moduleId, parameterName(m_paramName), moduleId, param);
            });
    m_parameterPopup->setAttribute(Qt::WA_Hover);
}


ParameterListItemWithX::ParameterListItemWithX(const QString &text, QWidget *parent): QWidget(parent), m_text(text)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    m_label = new QLabel(this);
    m_removeButton = new QPushButton("X", this);

    layout->addWidget(m_label);
    layout->addWidget(m_removeButton);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);

    connect(m_removeButton, &QPushButton::clicked, this, &ParameterListItemWithX::onRemoveButtonClicked);
}

void ParameterListItemWithX::onRemoveButtonClicked()
{
    emit removeRequested(m_text);
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
            emit parameterHovered(-1, "");
            emit parameterHovered(moduleId, paramName);
        } else if (!item) {
            lastHoveredItem = nullptr;
            emit parameterHovered(-1, "");
        }
    }
    return QWidget::event(event);
}

void ParameterPopup::setParameters(const QStringList &parameters)
{
    m_parameters = putSystemParamsAtTheEnd(parameters);
    (this->*populateFnc)(m_parameters);
}

void ParameterPopup::enableXBtn(bool enable)
{
    enable ? populateFnc = &ParameterPopup::populateListWidgetWithXBtn
           : populateFnc = &ParameterPopup::populateListWidget;
    (this->*populateFnc)(m_parameters);
}


void ParameterPopup::filterParameters(const QString &query)
{
    QStringList filteredParameters;
    for (const QString &param: m_parameters) {
        if (param.contains(query, Qt::CaseInsensitive)) {
            filteredParameters << param;
        }
    }
    (this->*populateFnc)(filteredParameters);
}

void ParameterPopup::populateListWidget(const QStringList &parameters)
{
    m_listWidget->clear();
    for (const QString &param: parameters) {
        QListWidgetItem *item = new QListWidgetItem(displayName(param), m_listWidget);
        m_listWidget->addItem(item);
    }
}

QString wrapText(const QListWidget *listWidget, const QString &text)
{
    QFontMetrics metrics(listWidget->font());
    QStringList words = text.split(' ');
    QString wrappedText;
    QString line;

    for (const QString &word: words) {
        if (metrics.horizontalAdvance(line + word) > listWidget->width()) {
            wrappedText += line + '\n';
            line = word + ' ';
        } else {
            line += word + ' ';
        }
    }
    wrappedText += line.trimmed();
    return wrappedText;
}

void ParameterPopup::populateListWidgetWithXBtn(const QStringList &parameters)
{
    m_listWidget->clear();
    if (m_parameters.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem(
            wrapText(m_listWidget, "Drag onto a module in the workflow view to connect this parameter with one of the "
                                   "module's parameters. Connected parameters share their value."),
            m_listWidget);
    }
    for (const QString &param: parameters) {
        QListWidgetItem *item = new QListWidgetItem(displayName(param), m_listWidget);
        ParameterListItemWithX *widgetItem = new ParameterListItemWithX(displayName(param), m_listWidget);
        connect(widgetItem, &ParameterListItemWithX::removeRequested, this, [this](const QString &text) {
            // Remove the item from the list
            for (int i = 0; i < m_listWidget->count(); ++i) {
                QListWidgetItem *item = m_listWidget->item(i);
                if (item->text() == text) {
                    delete m_listWidget->takeItem(i);
                    break;
                }
            }
            auto moduleId = text.split(":").first().toInt();
            auto paramName = text.split(":").last().trimmed();
            std::cerr << "disconnecting " << moduleId << ", " << paramName.toStdString() << std::endl;
            emit parameterDisconnected(moduleId, parameterName(paramName));
        });
        item->setSizeHint(widgetItem->sizeHint());
        m_listWidget->setItemWidget(item, widgetItem);
    }
    // Set the height of the list widget
    const int maxVisibleItems = 5;
    const int itemHeight = m_listWidget->sizeHintForRow(0);
    const int itemCount = m_listWidget->count();
    const int visibleItemCount = qMin(itemCount, maxVisibleItems);
    const int totalHeight = visibleItemCount * itemHeight + 2 * m_listWidget->frameWidth();
    m_listWidget->setFixedHeight(totalHeight);
}

void ParameterPopup::onParameterSelected(QListWidgetItem *item)
{
    emit parameterSelected(parameterName(item->text()));
}