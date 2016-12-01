#include "clickableqrimage.h"
#include "guiutil.h"

#include <QVariant>
#include <QStyle>
#include <QMimeData>
#include <QDrag>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>
#include <QPainter>

#ifdef USE_QRCODE
#include <qrencode.h>
#endif

ClickableQRImage::ClickableQRImage(QWidget *parent):
    QLabel(parent), contextMenu(0)
{
    contextMenu = new QMenu();
    QAction *saveImageAction = new QAction(tr("&Save Image..."), this);
    connect(saveImageAction, SIGNAL(triggered()), this, SLOT(saveImage()));
    contextMenu->addAction(saveImageAction);
    QAction *copyImageAction = new QAction(tr("&Copy Image"), this);
    connect(copyImageAction, SIGNAL(triggered()), this, SLOT(copyImage()));
    contextMenu->addAction(copyImageAction);
    
    setWordWrap(true);
}

QImage ClickableQRImage::exportImage()
{
    if(!pixmap())
        return QImage();
    return pixmap()->toImage();
}

void ClickableQRImage::setCode(const QString& qrString)
{
    m_qrString = qrString;
    
    QRcode *code = QRcode_encodeString(m_qrString.toUtf8().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (!code)
    {
        setText(tr("Error encoding URI into QR Code."));
        return;
    }
    QImage qrImage = QImage(code->width , code->width , QImage::Format_RGB32);
    qrImage.fill(0xffffff);
    unsigned char *p = code->data;
    for (int y = 0; y < code->width; y++)
    {
        for (int x = 0; x < code->width; x++)
        {
            qrImage.setPixel(x , y, ((*p & 1) ? 0x0 : 0xffffff));
            p++;
        }
    }
    QRcode_free(code);

    QImage qrAddrImage = QImage(width(), height(), QImage::Format_RGB32);
    qrAddrImage.fill(0xffffff);
    QPainter painter(&qrAddrImage);
    painter.drawImage(20, 20, qrImage.scaled(width()-40, height()-40));
    /*QFont font = GUIUtil::fixedPitchFont();
    font.setPixelSize(12);
    painter.setFont(font);*/
    painter.end();

    setPixmap(QPixmap::fromImage(qrAddrImage));
}

void ClickableQRImage::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton && pixmap())
    {
        event->accept();
        QMimeData *mimeData = new QMimeData;
        mimeData->setImageData(exportImage());

        QDrag *drag = new QDrag(this);
        drag->setMimeData(mimeData);
        drag->exec();
    } else {
        QLabel::mousePressEvent(event);
        Q_EMIT clicked();
    }
}

void ClickableQRImage::saveImage()
{
    if(!pixmap())
        return;
    QString fn = GUIUtil::getSaveFileName(this, tr("Save QR Code"), QString(), tr("PNG Image (*.png)"), NULL);
    if (!fn.isEmpty())
    {
        exportImage().save(fn);
    }
}

void ClickableQRImage::copyImage()
{
    if(!pixmap())
        return;
    QApplication::clipboard()->setImage(exportImage());
}

void ClickableQRImage::contextMenuEvent(QContextMenuEvent *event)
{
    if(!pixmap())
        return;
    contextMenu->exec(event->globalPos());
}
