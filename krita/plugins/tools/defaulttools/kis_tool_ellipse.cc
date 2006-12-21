/*
 *  kis_tool_ellipse.cc - part of Krayon
 *
 *  Copyright (c) 2000 John Califf <jcaliff@compuzone.net>
 *  Copyright (c) 2002 Patrick Julien <freak@codepimps.org>
 *  Copyright (c) 2004 Boudewijn Rempt <boud@valdyas.org>
 *  Copyright (c) 2004 Clarence Dang <dang@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <QPainter>

#include <kaction.h>
#include <kactioncollection.h>
#include <kdebug.h>
#include <klocale.h>

#include "kis_painter.h"
#include "kis_tool_ellipse.h"
#include "KoPointerEvent.h"
#include "kis_paintop_registry.h"
#include "kis_undo_adapter.h"
#include "QPainter"
#include "kis_cursor.h"
#include "KoCanvasBase.h"


KisToolEllipse::KisToolEllipse(KoCanvasBase * canvas)
    : KisToolShape(canvas, KisCursor::load("tool_ellipse_cursor.png", 6, 6)),
      m_dragging (false)
{
    setObjectName("tool_ellipse");
    
    m_painter = 0;
    m_currentImage = 0;
    m_dragStart = QPointF(0, 0);
    m_dragEnd = QPointF(0, 0);
}

KisToolEllipse::~KisToolEllipse()
{
}

void KisToolEllipse::paint(QPainter& gc)
{
    if (m_dragging)
        paintEllipse(gc, QRect());
}

void KisToolEllipse::paint(QPainter& gc, const QRect& rc)
{
    if (m_dragging)
        paintEllipse(gc, rc);
}

void KisToolEllipse::paint(QPainter& gc, KoViewConverter &converter)
{
    if (m_dragging)
        paintEllipse(gc, QRect());
}


void KisToolEllipse::mousePressEvent(KoPointerEvent *event)
{
     if (!m_canvas || !m_currentImage) return;

    if (!m_currentBrush) return;

    if (event->button() == Qt::LeftButton) {
	QPointF pos = convertToPixelCoord(event);
        m_dragging = true;
        m_dragStart = m_dragCenter = m_dragEnd = pos;
        //draw(m_dragStart, m_dragEnd);
    }
}

void KisToolEllipse::mouseMoveEvent(KoPointerEvent *event)
{
    if (m_dragging) {
        QRectF bound;

        bound.setTopLeft(m_dragStart);
        bound.setBottomRight(m_dragEnd);
        m_canvas->updateCanvas(convertToPt(bound.normalized()));

	QPointF pos = convertToPixelCoord(event);
        // erase old lines on canvas
        //draw(m_dragStart, m_dragEnd);
        // move (alt) or resize ellipse
        if (event->modifiers() & Qt::AltModifier) {
            QPointF trans = pos - m_dragEnd;
            m_dragStart += trans;
            m_dragEnd += trans;
        } else {
            QPointF diag = pos - (event->modifiers() & Qt::ControlModifier
				  ? m_dragCenter : m_dragStart);
            // circle?
            if (event->modifiers() & Qt::ShiftModifier) {
                double size = qMax(fabs(diag.x()), fabs(diag.y()));
                double w = diag.x() < 0 ? -size : size;
                double h = diag.y() < 0 ? -size : size;
                diag = QPointF(w, h);
            }

            // resize around center point?
            if (event->modifiers() & Qt::ControlModifier) {
                m_dragStart = m_dragCenter - diag;
                m_dragEnd = m_dragCenter + diag;
            } else {
                m_dragEnd = m_dragStart + diag;
            }
        }
        // draw new lines on canvas
        //draw(m_dragStart, m_dragEnd);
	bound.setTopLeft(m_dragStart);
        bound.setBottomRight(m_dragEnd);
        /* FIXME Which rectangle to repaint */
        m_canvas->updateCanvas(convertToPt(bound.normalized()));

        m_dragCenter = QPointF((m_dragStart.x() + m_dragEnd.x()) / 2,
                                (m_dragStart.y() + m_dragEnd.y()) / 2);
    }
}

void KisToolEllipse::mouseReleaseEvent(KoPointerEvent *event)
{
    QPointF pos = convertToPixelCoord(event);

    if (!m_canvas || !m_currentImage)
        return;

    if (m_dragging && event->button() == Qt::LeftButton) {
        // erase old lines on canvas
        //draw(m_dragStart, m_dragEnd);
        m_dragging = false;

        if (m_dragStart == m_dragEnd)
            return;

        if (!m_currentImage)
            return;

        if (!m_currentImage->activeDevice())
            return;

        KisPaintDeviceSP device = m_currentImage->activeDevice ();
	delete m_painter;
	m_painter = new KisPainter( device );
	Q_CHECK_PTR(m_painter);

        if (m_currentImage->undo()) m_painter->beginTransaction (i18n ("Ellipse"));

        m_painter->setPaintColor(m_currentFgColor);
        m_painter->setBackgroundColor(m_currentBgColor);
        m_painter->setFillStyle(fillStyle());
        m_painter->setBrush(m_currentBrush);
        m_painter->setPattern(m_currentPattern);
        m_painter->setOpacity(m_opacity);
        m_painter->setCompositeOp(m_compositeOp);
	KisPaintOp * op = KisPaintOpRegistry::instance()->paintOp(m_currentPaintOp, m_currentPaintOpSettings, m_painter);
        m_painter->setPaintOp(op); // Painter takes ownership

        m_painter->paintEllipse(m_dragStart, m_dragEnd-m_dragStart, PRESSURE_DEFAULT/*event->pressure()*/, event->xTilt(), event->yTilt());
	QRect bound = m_painter->dirtyRect();
	device->setDirty( bound );
        notifyModified();

	if (m_canvas) {
	    m_canvas->updateCanvas(convertToPt(bound.normalized()));
	    //m_canvas->updateCanvas(convertToPt(bound.normalized()));
	}

	if (m_currentImage->undo()) {
            m_currentImage->undoAdapter()->addCommand(m_painter->endTransaction());
        }
	delete m_painter;
	m_painter = 0;

        //KisUndoAdapter *adapter = m_currentImage->undoAdapter();
        //if (adapter) {
        //    adapter->addCommand(painter.endTransaction());
        //}
    }
}


void KisToolEllipse::paintEllipse()
{
    if (m_canvas) {
        QPainter gc(m_canvas->canvasWidget());
        QRect rc;

        paintEllipse(gc, rc);
    }
}

void KisToolEllipse::paintEllipse(QPainter& gc, const QRect&)
{
    if (m_canvas) {
        QPen old = gc.pen();
        QPen pen(Qt::SolidLine);
        QPoint start;
        QPoint end;

        gc.setPen(pen);
        
	start = QPoint(static_cast<int>(m_dragStart.x()), static_cast<int>(m_dragStart.y()));
	end = QPoint(static_cast<int>(m_dragEnd.x()), static_cast<int>(m_dragEnd.y()));
	gc.drawEllipse(QRect(start, end));
        gc.setPen(old);
    }
}


// void KisToolEllipse::draw(const QPointF& start, const QPointF& end )
// {
//     if (!m_subject || !m_currentImage)
//         return;

//     KisCanvasController *controller = m_subject->canvasController ();
//     KisCanvas *canvas = controller->kiscanvas();
//     QPainter p (canvas->canvasWidget());

//     //p.setRasterOp (Qt::NotROP);
//     p.drawEllipse (QRect (controller->windowToView (start).floorQPoint(), controller->windowToView (end).floorQPoint()));
//     p.end ();
// }


#include "kis_tool_ellipse.moc"
