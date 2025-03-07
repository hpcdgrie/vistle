
#ifndef PROPERTY_WITHDRAG_BUTTON_H
#define PROPERTY_WITHDRAG_BUTTON_H

#include <QPushButton>


/*
This macro creates a class named ...WithButton that has a property and a button
Template classes not supported by Q_OBJECT
and Q_OBJECT and signal / slots are keywords that moc has to see
ugly useage: 
PROPERTY_WITHDRAG_BUTTON1(classname)
Q_OBJECT

signals:
...

PROPERTY_WITHDRAG_BUTTON2(classname)
 */


#define PROPERTY_WITHDRAG_BUTTON1(x) \
class x##WithButton : public QWidget\
{ 
    

#define PROPERTY_WITHDRAG_BUTTON2(x) \
    public: \
    x##WithButton(QWidget *parent = nullptr) \
    : property(new x(this)) \
    , button(new QPushButton("...", this)) \
    { \
        QHBoxLayout *layout = new QHBoxLayout(this);\
        layout->addWidget(property);\
        layout->addWidget(button);\
        layout->setContentsMargins(0, 0, 0, 0);\
        setLayout(layout);\
        button->setFixedWidth(20); \
        connect(button, &QPushButton::clicked, this, &x##WithButton::buttonClicked); \
        connect(button, &QPushButton::pressed, this, &x##WithButton::buttonPressed); \
        connect(button, &QPushButton::released, this, &x##WithButton::buttonReleased); \
    } \
    x *property = nullptr; \
    private: \
    QPushButton *button = nullptr; \
};


#define PROPERTY_WITHDRAG_BUTTON3(x, y) \
class x##WithButton : public QWidget\
{ \
    y\
    public: \
    x##WithButton(QWidget *parent = nullptr) \
    : property(new x(this)) \
    , button(new QPushButton("...", this)) \
    { \
        QHBoxLayout *layout = new QHBoxLayout(this);\
        layout->addWidget(property);\
        layout->addWidget(button);\
        layout->setContentsMargins(0, 0, 0, 0);\
        setLayout(layout);\
        button->setFixedWidth(20); \
        connect(button, &QPushButton::pressed, this, &x##WithButton::buttonPressed); \
        connect(button, &QPushButton::released, this, &x##WithButton::buttonReleased); \
    } \
    x *property = nullptr; \
    private: \
    QPushButton *button = nullptr; \
};


#endif // PROPERTY_WITHDRAG_BUTTON_H