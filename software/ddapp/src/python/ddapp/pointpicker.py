import PythonQt
from PythonQt import QtCore, QtGui
import ddapp.visualization as vis
import ddapp.objectmodel as om
from ddapp.debugVis import DebugData
import ddapp.vtkAll as vtk
import numpy as np


class PointPicker(object):

    def __init__(self, view, obj=None, callback=None, numberOfPoints=2, drawLines=True):

        self.view = view
        self.obj = obj
        self.numberOfPoints = numberOfPoints
        self.drawLines = drawLines
        self.annotationObj = None
        self.annotationFunc = callback
        self.clear()

    def start(self):
        self.installEventFilter()
        self.clear()

    def stop(self):
        self.removeEventFilter()

    def installEventFilter(self):

        self.eventFilter = PythonQt.dd.ddPythonEventFilter()
        self.view.vtkWidget().installEventFilter(self.eventFilter)

        self.eventFilter.addFilteredEventType(QtCore.QEvent.MouseMove)
        self.eventFilter.addFilteredEventType(QtCore.QEvent.MouseButtonPress)
        self.eventFilter.addFilteredEventType(QtCore.QEvent.KeyPress)
        self.eventFilter.addFilteredEventType(QtCore.QEvent.KeyRelease)
        self.eventFilter.connect('handleEvent(QObject*, QEvent*)', self.onEvent)

    def removeEventFilter(self):
        self.view.vtkWidget().removeEventFilter(self.eventFilter)

    def onEvent(self, obj, event):

        if event.modifiers() != QtCore.Qt.ShiftModifier:
            if self.annotationObj:
                self.annotationObj.setProperty('Visible', False)
            return

        if self.annotationObj:
            self.annotationObj.setProperty('Visible', True)

        if event.type() == QtCore.QEvent.MouseMove:
            self.onMouseMove(vis.mapMousePosition(obj, event), event.modifiers())
        elif event.type() == QtCore.QEvent.MouseButtonPress:
            self.onMousePress(vis.mapMousePosition(obj, event), event.modifiers())

    def clear(self):
        self.annotationObj = None
        self.points = [None for i in xrange(self.numberOfPoints)]
        self.hoverPos = None
        self.lastMovePos = [0, 0]

    def onMouseMove(self, displayPoint, modifiers=None):
        self.lastMovePos = displayPoint
        self.tick()

    def onMousePress(self, displayPoint, modifiers=None):

        for i in xrange(self.numberOfPoints):
            if self.points[i] is None:
                self.points[i] = self.hoverPos
                break

        if self.points[-1] is not None:
            self.finish()

    def finish(self):

        points = [p.copy() for p in self.points]
        if self.annotationFunc is not None:
            self.annotationFunc(*points)

        self.clear()


    def draw(self):

        d = DebugData()

        points = [p if p is not None else self.hoverPos for p in self.points]

        # draw points
        for p in points:
            if p is not None:
                d.addSphere(p, radius=0.008)

        if self.drawLines:
            # draw lines
            for a, b in zip(points, points[1:]):
                if b is not None:
                    d.addLine(a, b)

            # connect end points
            if points[-1] is not None:
                d.addLine(points[0], points[-1])

        self.annotationObj = vis.updatePolyData(d.getPolyData(), 'annotation', parent='segmentation')
        self.annotationObj.setProperty('Color', QtGui.QColor(255, 0, 0))
        self.annotationObj.actor.SetPickable(False)


    def tick(self):

        if self.obj is None:
            self.hoverPos, prop, _ = vis.pickPoint(self.lastMovePos, self.view, pickType='points', tolerance=0.01)
            if prop is None:
                self.hoverPos = None
        else:
            self.hoverPos = vis.pickPoint(self.lastMovePos, self.view, obj=self.obj, pickType='points', tolerance=0.01)

        self.draw()



class ImagePointPicker(object):

    def __init__(self, imageView, obj=None, callback=None, drawLines=True):

        self.imageView = imageView
        self.view = imageView.view
        self.obj = obj
        self.drawLines = drawLines
        self.annotationObj = None
        self.annotationFunc = callback
        self.doubleClickCallback = None
        self.clear()

    def start(self):
        self.installEventFilter()
        self.clear()

    def stop(self):
        self.removeEventFilter()

    def installEventFilter(self):

        self.eventFilter = PythonQt.dd.ddPythonEventFilter()
        self.view.vtkWidget().installEventFilter(self.eventFilter)

        self.eventFilter.addFilteredEventType(QtCore.QEvent.MouseMove)
        self.eventFilter.addFilteredEventType(QtCore.QEvent.MouseButtonPress)
        self.eventFilter.addFilteredEventType(QtCore.QEvent.MouseButtonDblClick)
        self.eventFilter.addFilteredEventType(QtCore.QEvent.KeyPress)
        self.eventFilter.addFilteredEventType(QtCore.QEvent.KeyRelease)
        self.eventFilter.connect('handleEvent(QObject*, QEvent*)', self.onEvent)

    def removeEventFilter(self):
        self.view.vtkWidget().removeEventFilter(self.eventFilter)

    def onEvent(self, obj, event):

        if event.type() == QtCore.QEvent.MouseButtonDblClick and event.button() == QtCore.Qt.LeftButton:
            if self.doubleClickCallback:
                self.doubleClickCallback(vis.mapMousePosition(obj, event), event.modifiers(), self.imageView)

        if event.type() in (QtCore.QEvent.MouseMove, QtCore.QEvent.MouseButtonPress, QtCore.QEvent.Wheel):
            self.updateCursor(vis.mapMousePosition(obj, event))

        if event.modifiers() != QtCore.Qt.ShiftModifier:
            if self.annotationObj:
                self.hoverPos = None
                self.draw()
                self.annotationObj.setProperty('Color', QtGui.QColor(255, 255, 0)) # color overlay lines yellow
                self.clear()
            return

        if self.annotationObj:
            self.annotationObj.setProperty('Visible', True)

        if event.type() == QtCore.QEvent.MouseMove:
            self.onMouseMove(vis.mapMousePosition(obj, event), event.modifiers())
            self.imageView.rayDebug(vis.mapMousePosition(obj, event))

        elif event.type() == QtCore.QEvent.MouseButtonPress:
            self.onMousePress(vis.mapMousePosition(obj, event), event.modifiers())


    def clear(self):
        self.annotationObj = None
        self.points = []
        self.hoverPos = None
        self.lastMovePos = [0, 0]

    def onMouseMove(self, displayPoint, modifiers=None):
        self.lastMovePos = displayPoint
        self.tick()

    def onMousePress(self, displayPoint, modifiers=None):
        self.points.append(self.hoverPos)


    def finish(self):
        points = [p.copy() for p in self.points]
        if self.annotationFunc is not None:
            self.annotationFunc(*points)
        self.clear()

    def draw(self):

        d = DebugData()

        points = list(self.points)
        points.append(self.hoverPos)

        # draw points
        for p in points:
            if p is not None:
                d.addSphere(p, radius=2) # radius seems to be in pixels 0.08

                # add a cross-hair
                d.addLine( [p[0] , p[1]-50 , p[2]] , [p[0] , p[1]+50 , p[2]] )
                d.addLine( [p[0]-50 , p[1] , p[2]] , [p[0]+50 , p[1] , p[2]] )

        if self.drawLines:
            # draw lines
            for a, b in zip(points, points[1:]):

                if a is not None:
                    if b is not None:
                        d.addLine(a, b)

            # connect end points
            #if points[-1] is not None:
            #    d.addLine(points[0], points[-1])

        if self.annotationObj:
            self.annotationObj.setPolyData(d.getPolyData())
        else:
            self.annotationObj = vis.updatePolyData(d.getPolyData(), 'annotation', parent='segmentation', color=[1,0,0], view=self.view) # draw red lines over image
            self.annotationObj.actor.SetPickable(False)
            self.annotationObj.actor.GetProperty().SetLineWidth(2)

    def updateCursor(self, mousePos):

        worldPoint = [0.0, 0.0, 0.0, 0.0]
        vtk.vtkInteractorObserver.ComputeDisplayToWorld(self.view.renderer(), mousePos[0], mousePos[1], 0, worldPoint)

        d = DebugData()
        center = np.array([worldPoint[0], worldPoint[1], -1])
        d.addLine(center + [0, -3000, 0], center + [0, 3000, 0])
        d.addLine(center + [-3000, 0, 0], center + [3000, 0, 0])
        self.cursorObj = vis.updatePolyData(d.getPolyData(), 'cursor', alpha=0.5, view=self.view)
        self.cursorObj.actor.SetUseBounds(False)
        self.cursorObj.actor.SetPickable(False)
        self.view.render()

    def tick(self):

        worldPoint = [0.0, 0.0, 0.0, 0.0]
        vtk.vtkInteractorObserver.ComputeDisplayToWorld(self.view.renderer(), self.lastMovePos[0], self.lastMovePos[1], 0, worldPoint)
        imageDimensions = self.imageView.getImage().GetDimensions()

        if 0.0 <= worldPoint[0] <= imageDimensions[0] and 0.0 <= worldPoint[1] <= imageDimensions[1]:
            self.hoverPos = [worldPoint[0], worldPoint[1], -1.0]
        else:
            self.hoverPos = None

        self.draw()


class PlacerWidget(object):

    def __init__(self, view, handle, points):

        assert handle.actor
        assert handle.actor.GetUserTransform()

        self.view = view
        self.handle = handle
        self.points = points
        self.moving = False

    def start(self):
        self.installEventFilter()

    def stop(self):
        self.removeEventFilter()

    def installEventFilter(self):

        self.eventFilter = PythonQt.dd.ddPythonEventFilter()
        self.view.vtkWidget().installEventFilter(self.eventFilter)

        self.eventFilter.addFilteredEventType(QtCore.QEvent.MouseMove)
        self.eventFilter.addFilteredEventType(QtCore.QEvent.MouseButtonPress)
        self.eventFilter.addFilteredEventType(QtCore.QEvent.MouseButtonRelease)
        self.eventFilter.connect('handleEvent(QObject*, QEvent*)', self.onEvent)

    def removeEventFilter(self):
        self.view.vtkWidget().removeEventFilter(self.eventFilter)

    def onEvent(self, obj, event):

        if event.type() == QtCore.QEvent.MouseMove:
            self.onMouseMove(vis.mapMousePosition(obj, event), event.modifiers())
        elif event.type() == QtCore.QEvent.MouseButtonPress and event.button() == QtCore.Qt.LeftButton:
            self.onMousePress(vis.mapMousePosition(obj, event), event.modifiers())
        elif event.type() == QtCore.QEvent.MouseButtonRelease and event.button() == QtCore.Qt.LeftButton:
            self.onMouseRelease(vis.mapMousePosition(obj, event), event.modifiers())

    def onMouseMove(self, displayPoint, modifiers=None):

        self.updateHighlight(displayPoint)

        if not self.moving:
            return


        self.eventFilter.setEventHandlerResult(True)

        pickPoint = self.getPointPick(displayPoint)
        # print displayPoint, pickPoint

        if pickPoint is not None:
            t = self.handle.actor.GetUserTransform()
            assert t
            currentPos = np.array(t.GetPosition())
            t.Translate(np.array(pickPoint - currentPos))
            t.Modified()
            self.handle._renderAllViews()

    def onMousePress(self, displayPoint, modifiers=None):

        picked = self.getHandlePick(displayPoint)
        if picked is not None:
            self.moving = True
            self.eventFilter.setEventHandlerResult(True)

    def onMouseRelease(self, displayPoint, modifiers=None):

        if self.moving:
            self.eventFilter.setEventHandlerResult(True)
            self.moving = False

    def updateHighlight(self, displayPoint):
        if self.getHandlePick(displayPoint) is not None:
            self.handle.actor.GetProperty().SetAmbient(0.5)
            self.handle._renderAllViews()
        else:
            self.handle.actor.GetProperty().SetAmbient(0.0)
            self.handle._renderAllViews()

    def getHandlePick(self, displayPoint):
        return vis.pickPoint(displayPoint, self.view, obj=self.handle, pickType='cells', tolerance=0.01)

    def getPointPick(self, displayPoint):
        return vis.pickPoint(displayPoint, self.view, obj=self.points, pickType='points', tolerance=0.01)
