#include "clickablelabel.h"

#include <QVariant>
#include <QStyle>

ClickableLabel::ClickableLabel( QWidget * parent )
: QLabel( parent )
{
    setContentsMargins(0, 0, 0, 0);
    setIndent(0);
    
    setProperty("hover", QVariant(false));
    setProperty("checked", QVariant(false));
    setProperty("pressed", QVariant(false));
    
    mouseIn = false;
}
 
void ClickableLabel::mousePressEvent( QMouseEvent * event ) 
{
    setProperty("pressed", QVariant(true));
    style()->unpolish(this);
    style()->polish(this);
}

void ClickableLabel::mouseReleaseEvent ( QMouseEvent * event )
{
    setProperty("pressed", QVariant(false));
    style()->unpolish(this);
    style()->polish(this);
    if (mouseIn)
    {
        Q_EMIT clicked();
    }
}

void ClickableLabel::enterEvent(QEvent * event)
{
    mouseIn = true;
    setProperty("hover", QVariant(true));
    style()->unpolish(this);
    style()->polish(this);
}

void ClickableLabel::leaveEvent(QEvent * event)
{
    mouseIn = false;
    setProperty("hover", QVariant(false));
    style()->unpolish(this);
    style()->polish(this);
}

void ClickableLabel::setChecked(bool checked)
{
    setProperty("checked", QVariant(checked));
    style()->unpolish(this);
    style()->polish(this);
}

bool ClickableLabel::isChecked()
{
    return property("checked").toBool();
}
