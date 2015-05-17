import os
import copy
import math
import functools
import numpy as np

from ddapp import transformUtils

from ddapp.asynctaskqueue import AsyncTaskQueue
from ddapp import objectmodel as om
from ddapp import visualization as vis
from ddapp import robotstate
from ddapp import segmentation
from ddapp import planplayback
from ddapp.pointpicker import PointPicker
from ddapp import vtkAll as vtk
from ddapp.simpletimer import SimpleTimer
from ddapp import affordanceupdater

from ddapp.debugVis import DebugData
from ddapp import affordanceitems
from ddapp import ikplanner


import ioUtils

class TableDemo(object):

    def __init__(self, robotStateModel, playbackRobotModel, ikPlanner, manipPlanner, footstepPlanner, atlasDriver, lhandDriver, rhandDriver, multisenseDriver, view, sensorJointController, planPlaybackFunction):
        self.planPlaybackFunction = planPlaybackFunction
        self.robotStateModel = robotStateModel
        self.playbackRobotModel = playbackRobotModel
        self.ikPlanner = ikPlanner
        self.manipPlanner = manipPlanner
        self.footstepPlanner = footstepPlanner
        self.atlasDriver = atlasDriver
        self.lhandDriver = lhandDriver
        self.rhandDriver = rhandDriver
        self.multisenseDriver = multisenseDriver
        self.sensorJointController = sensorJointController
        self.view = view

        # live operation flags:
        self.useFootstepPlanner = True
        self.visOnly = False
        self.planFromCurrentRobotState = True
        useDevelopment = False
        if (useDevelopment):
            self.visOnly = True
            self.planFromCurrentRobotState = False

        self.optionalUserPromptEnabled = True
        self.requiredUserPromptEnabled = True

        self.plans = []
        self.frameSyncs = {}

        self.useLeftArm = False
        self.useRightArm = True

        self.tableData = None
        self.binFrame = None

        # top level switch between BDI or IHMC (locked base) and MIT (moving base and back)
        self.lockBack = True
        self.lockBase = True

        self.constraintSet = []

        self.reachDist = 0.125

        extraModels = [self.robotStateModel]
        self.affordanceUpdater  = affordanceupdater.AffordanceGraspUpdater(self.robotStateModel, self.ikPlanner, extraModels)


    def addPlan(self, plan):
        self.plans.append(plan)


    ### Table and Bin Focused Functions
    def userFitTable(self):
        self.tableData = None
        self.picker = PointPicker(self.view, numberOfPoints=2, drawLines=True, callback=self.onSegmentTable)
        self.picker.start()

    def userFitBin(self):
        self.binFrame = None
        self.picker = PointPicker(self.view, numberOfPoints=2, drawLines=True, callback=self.onSegmentBin)
        self.picker.start()

    def waitForTableFit(self):
        while not self.tableData:
            yield

    def waitForBinFit(self):
        while not self.binFrame:
            yield

    def getInputPointCloud(self):
        polyData = segmentation.getCurrentRevolutionData()
        if polyData is None:
            obj = om.findObjectByName('scene')
            if obj:
                polyData = obj.polyData

        return polyData

    def onSegmentTable(self, p1, p2):
        print p1
        print p2
        self.picker.stop()
        om.removeFromObjectModel(self.picker.annotationObj)
        self.picker = None

        om.removeFromObjectModel(om.findObjectByName('table demo'))

        self.tableData = segmentation.segmentTableEdge(self.getInputPointCloud(), p1, p2)
        self.tableObj = vis.showPolyData(self.tableData.mesh, 'table', parent='table demo', color=[0,1,0])
        self.tableFrame = vis.showFrame(self.tableData.frame, 'table frame', parent=self.tableObj, scale=0.2)
        self.tableObj.actor.SetUserTransform(self.tableFrame.transform)

    def onSegmentBin(self, p1, p2):
        print p1
        print p2
        self.picker.stop()
        om.removeFromObjectModel(self.picker.annotationObj)
        self.picker = None

        om.removeFromObjectModel(om.findObjectByName('bin frame'))

        binEdge = p2 - p1
        zaxis = [0.0, 0.0, 1.0]
        xaxis = np.cross(binEdge, zaxis)
        xaxis /= np.linalg.norm(xaxis)
        yaxis = np.cross(zaxis, xaxis)

        t = transformUtils.getTransformFromAxes(xaxis, yaxis, zaxis)
        t.PostMultiply()
        t.Translate(p1)

        self.binFrame = vis.showFrame(t, 'bin frame', parent=None, scale=0.2)

    def sortClustersOnTable(self, clusters):
        '''
        returns list copy of clusters, sorted left to right using the
        table coordinate system.  (Table y axis points right to left)
        '''
        tableYAxis = self.tableData.axes[1]
        tableOrigin = np.array(self.tableData.frame.GetPosition())

        origins = [np.array(c.frame.GetPosition()) for c in clusters]
        dists = [np.dot(origin-tableOrigin, -tableYAxis) for origin in origins]

        return [clusters[i] for i in np.argsort(dists)]


    def cleanupSegmentedObjects(self):
        om.removeFromObjectModel(om.findObjectByName('segmentation'))
        self.clusterObjects = None
        self.segmentationData = None

    def segmentTableObjects(self):

        tableCentroid = segmentation.computeCentroid(self.tableData.box)
        self.tableData.frame.TransformPoint(tableCentroid, tableCentroid)

        data = segmentation.segmentTableScene(self.getInputPointCloud(), tableCentroid)
        data.clusters = self.sortClustersOnTable(data.clusters)

        self.clusterObjects = vis.showClusterObjects(data.clusters, parent='segmentation')
        self.segmentationData = data


    def graspTableObject(self, side):

        #linkName = self.ikPlanner.getHandLink(side)
        #t = self.ikPlanner.getLinkFrameAtPose(linkName, self.getPlanningStartPose())
        #linkFrame = vis.updateFrame(t, '%s frame' % linkName, scale=0.2, visible=False, parent='planning')



        obj, objFrame = self.getNextTableObject(side)
        #frameSync = vis.FrameSync()
        #frameSync.addFrame(linkFrame)
        #frameSync.addFrame(objFrame)
        #self.frameSyncs[linkName] = frameSync

        #self.playbackRobotModel.connectModelChanged(self.onRobotModelChanged)

        self.affordanceUpdater.graspAffordance( obj.getProperty('Name') , side)


    def dropTableObject(self, side):

        obj, _ = self.getNextTableObject(side)
        obj.setProperty('Visible', False)
        for child in obj.children():
            child.setProperty('Visible', False)

        self.affordanceUpdater.ungraspAffordance( obj.getProperty('Name'))

    def getNextTableObject(self, side):

        assert len(self.clusterObjects)
        obj = self.clusterObjects[0] if side == 'left' else self.clusterObjects[-1]
        frameObj = obj.findChild(obj.getProperty('Name') + ' frame')
        return obj, frameObj

    # TODO: deprecate this function: (to end of section):
    def computeTableStanceFrame(self):
        assert self.tableData

        zGround = 0.0
        tableHeight = self.tableData.frame.GetPosition()[2] - zGround

        t = vtk.vtkTransform()
        t.PostMultiply()
        t.Translate(-0.35, self.tableData.dims[1]*0.5, -tableHeight)
        t.Concatenate(self.tableData.frame)

        self.tableStanceFrame = vis.showFrame(t, 'table stance frame', parent=self.tableObj, scale=0.2)


    def computeBinStanceFrame(self):
        assert self.binFrame

        zGround = 0.0
        binHeight = self.binFrame.transform.GetPosition()[2] - zGround

        t = vtk.vtkTransform()
        t.PostMultiply()
        t.Translate(-0.45, 0.1, -binHeight)
        t.Concatenate(self.binFrame.transform)

        self.binStanceFrame = vis.showFrame(t, 'bin stance frame', parent=None, scale=0.2)

        t = vtk.vtkTransform()
        t.PostMultiply()
        t.RotateZ(30)
        t.Translate(-0.8, 0.4, -binHeight)
        t.Concatenate(self.binFrame.transform)

        self.startStanceFrame = vis.showFrame(t, 'start stance frame', parent=None, scale=0.2)

    def moveRobotToTableStanceFrame(self):
        self.teleportRobotToStanceFrame(self.tableStanceFrame.transform)

    def moveRobotToBinStanceFrame(self):
        self.teleportRobotToStanceFrame(self.binStanceFrame.transform)

    def moveRobotToStartStanceFrame(self):
        self.teleportRobotToStanceFrame(self.startStanceFrame.transform)

    def planFootstepsToTable(self):
        self.planFootsteps(self.tableStanceFrame.transform)

    def planFootstepsToBin(self):
        self.planFootsteps(self.binStanceFrame.transform)

    def planFootstepsToStart(self):
        self.planFootsteps(self.startStanceFrame.transform)

    ### End Object Focused Functions ###############################################################
    ### Planning Functions ########################################################################

    def planFootsteps(self, goalFrame):
        startPose = self.getPlanningStartPose()
        request = self.footstepPlanner.constructFootstepPlanRequest(startPose, goalFrame)
        self.footstepPlan = self.footstepPlanner.sendFootstepPlanRequest(request, waitForResponse=True)

    def planWalking(self):
        startPose = self.getPlanningStartPose()
        plan = self.footstepPlanner.sendWalkingPlanRequest(self.footstepPlan, startPose, waitForResponse=True)
        self.addPlan(plan)

    def planWalkToStance(self, stanceTransform):
        if self.useFootstepPlanner:
            self.planFootsteps(stanceTransform)
            self.planWalking()
        else:
            self.teleportRobotToStanceFrame(stanceTransform)


    def getRaisedArmPose(self, startPose, side):
        return self.ikPlanner.getMergedPostureFromDatabase(startPose, 'General', 'arm up pregrasp', side)

    def getPreDropHighPose(self, startPose, side):
        return self.ikPlanner.getMergedPostureFromDatabase(startPose, 'table clearing', 'pre drop 1', side)

    def getPreDropLowPose(self, startPose, side):
        return self.ikPlanner.getMergedPostureFromDatabase(startPose, 'table clearing', 'pre drop 2', side)

    def getLoweredArmPose(self, startPose, side):
        return self.ikPlanner.getMergedPostureFromDatabase(startPose, 'General', 'handdown', side)

    def planPreGrasp(self, side):
        startPose = self.getPlanningStartPose()
        endPose = self.ikPlanner.getMergedPostureFromDatabase(startPose, 'General', 'arm up pregrasp', side=side)
        newPlan = self.ikPlanner.computePostureGoal(startPose, endPose)
        self.addPlan(newPlan)

    def planDropPostureRaise(self, side):
        startPose = self.getPlanningStartPose()
        poseA = self.getRaisedArmPose(startPose, side)
        poseB = self.getPreDropHighPose(startPose, side)
        poseC = self.getPreDropLowPose(startPose, side)

        plan = self.ikPlanner.computeMultiPostureGoal([startPose, poseA, poseB, poseC])
        self.addPlan(plan)

    def planDropPostureLower(self, side):
        startPose = self.getPlanningStartPose()
        poseA = self.getPreDropHighPose(startPose, side)
        poseB = self.getRaisedArmPose(startPose, side)
        poseC = self.getLoweredArmPose(startPose, side)

        plan = self.ikPlanner.computeMultiPostureGoal([startPose, poseA, poseB, poseC])
        self.addPlan(plan)

    def planDropPostureSwap(self, lowerSide, raiseSide):

        startPose = self.getPlanningStartPose()

        poseA = self.getRaisedArmPose(startPose, raiseSide)
        poseA = self.getPreDropHighPose(poseA, lowerSide)

        poseB = self.getPreDropHighPose(poseA, raiseSide)
        poseB = self.getRaisedArmPose(poseB, lowerSide)

        poseC = self.getPreDropLowPose(poseB, raiseSide)
        poseC = self.getLoweredArmPose(poseC, lowerSide)

        plan = self.ikPlanner.computeMultiPostureGoal([startPose, poseA, poseB, poseC])
        self.addPlan(plan)


    def planLowerArmAndStand(self, side):
        startPose = self.getPlanningStartPose()
        endPose = self.getLoweredArmPose(startPose, side)
        endPose, info = self.ikPlanner.computeStandPose(endPose)

        plan = self.ikPlanner.computePostureGoal(startPose, endPose)
        self.addPlan(plan)


    def planReachToTableObject(self, side):

        obj, frame = self.getNextTableObject(side)
        startPose = self.getPlanningStartPose()

        self.constraintSet = self.ikPlanner.planGraspOrbitReachPlan(startPose, side, frame, constraints=None, dist=self.reachDist, lockBase=self.lockBase, lockBack=self.lockBack, lockArm=False)

        loweringSide = 'left' if side == 'right' else 'right'
        armPose = self.getLoweredArmPose(startPose, loweringSide)
        armPoseName = 'lowered_arm_pose'
        self.ikPlanner.ikServer.sendPoseToServer(armPose, armPoseName)

        loweringSideJoints = []
        if (loweringSide == 'left'):
          loweringSideJoints += self.ikPlanner.leftArmJoints
        else:
          loweringSideJoints += self.ikPlanner.rightArmJoints

        reachingSideJoints = []
        if (side == 'left'):
          reachingSideJoints += self.ikPlanner.leftArmJoints
        else:
          reachingSideJoints += self.ikPlanner.rightArmJoints


        armPostureConstraint = self.ikPlanner.createPostureConstraint(armPoseName, loweringSideJoints)
        armPostureConstraint.tspan = np.array([1.0, 1.0])
        self.constraintSet.constraints.append(armPostureConstraint)
        self.constraintSet.runIk()

        #armPose = self.getRaisedArmPose(startPose, side)
        #armPoseName = 'raised_arm_pose'
        #self.ikPlanner.ikServer.sendPoseToServer(armPose, armPoseName)
        #armPostureConstraint = self.ikPlanner.createPostureConstraint(armPoseName, reachingSideJoints)
        #armPostureConstraint.tspan = np.array([0.5, 0.5])
        #self.constraintSet.constraints.append(armPostureConstraint)

        print 'planning reach to'
        plan = self.constraintSet.runIkTraj()
        self.addPlan(plan)


    def planTouchTableObject(self, side):

        obj, frame = self.getNextTableObject(side)

        startPose = self.getPlanningStartPose()
        self.constraintSet = self.ikPlanner.planGraspOrbitReachPlan(startPose, side, frame, dist=0.05, lockBase=self.lockBase, lockBack=self.lockBack)

        self.constraintSet.constraints[-1].tspan = [-np.inf, np.inf]
        self.constraintSet.constraints[-2].tspan = [-np.inf, np.inf]
        self.constraintSet.runIk()

        print 'planning touch'
        plan = self.constraintSet.runIkTraj()
        self.addPlan(plan)


    def planLiftTableObject(self, side):

        startPose = self.getPlanningStartPose()
        self.constraintSet = self.ikPlanner.planEndEffectorDelta(startPose, side, [0.0, 0.0, 0.15])

        self.constraintSet.constraints[-1].tspan[1] = 1.0

        endPose, info = self.constraintSet.runIk()
        endPose = self.getRaisedArmPose(endPose, side)

        reachingSideJoints = []
        if (side == 'left'):
          reachingSideJoints += self.ikPlanner.leftArmJoints
        else:
          reachingSideJoints += self.ikPlanner.rightArmJoints


        endPoseName = 'raised_arm_end_pose'
        self.ikPlanner.ikServer.sendPoseToServer(endPose, endPoseName)
        postureConstraint = self.ikPlanner.createPostureConstraint(endPoseName, reachingSideJoints)
        postureConstraint.tspan = np.array([2.0, 2.0])
        self.constraintSet.constraints.append(postureConstraint)

        #postureConstraint = self.ikPlanner.createPostureConstraint('q_nom', robotstate.matchJoints('.*_leg_kny'))
        #postureConstraint.tspan = np.array([2.0, 2.0])
        #self.constraintSet.constraints.append(postureConstraint)

        #postureConstraint = self.ikPlanner.createPostureConstraint('q_nom', robotstate.matchJoints('back'))
        #postureConstraint.tspan = np.array([2.0, 2.0])
        #self.constraintSet.constraints.append(postureConstraint)

        print 'planning lift'
        plan = self.constraintSet.runIkTraj()
        self.addPlan(plan)


    ####### Module for an arm to sweep out a gaze-constrained trajectory to map an area:
    ######### Target Focused Functions ##################################################################
    def spawnTargetAffordance(self):
        for obj in om.getObjects():
             if obj.getProperty('Name') == 'target':
                 om.removeFromObjectModel(obj)

        # These are input settings that should be set in the class:
        self.targetSweepType = 'gaze' # gaze or orientation
        length = 0.3
        self.coneThresholdDegrees = 5.0 # 0 is ok for reaching but often too tight for a trajectory
        targetFrame = transformUtils.frameFromPositionAndRPY([0.6,0.2,0.6],[180,0,90])

        folder = om.getOrCreateContainer('affordances')
        z = DebugData()
        z.addLine(np.array([0,0,0]), np.array([-length,0,0]), radius=0.02) # main bar
        z.addLine(np.array([-length,0,0]), np.array([-length,0,length]), radius=0.02) # main bar
        z.addLine(np.array([-length,0,length]), np.array([0,0,length]), radius=0.02) # main bar
        z.addLine(np.array([0,0,length]), np.array([0,0,0]), radius=0.02) # main bar
        targetMesh = z.getPolyData()

        self.targetAffordance = vis.showPolyData(targetMesh, 'target', color=[0.0, 1.0, 0.0], cls=affordanceitems.FrameAffordanceItem, parent=folder, alpha=0.3)
        self.targetAffordance.actor.SetUserTransform(targetFrame)
        self.targetFrame = vis.showFrame(targetFrame, 'target frame', parent=self.targetAffordance, visible=False, scale=0.2)
        self.targetFrame = self.targetFrame.transform

        params = dict(length=length, otdf_type='target', friendly_name='target')
        self.targetAffordance.setAffordanceParams(params)
        self.targetAffordance.updateParamsFromActorTransform()

    def drawTargetPath(self):
        path = DebugData()
        for i in range(1,len(self.targetPath)):
          p0 = self.targetPath[i-1].GetPosition()
          p1 = self.targetPath[i].GetPosition()
          path.addLine ( np.array( p0 ) , np.array(  p1 ), radius= 0.005)

        pathMesh = path.getPolyData()
        self.targetPathMesh = vis.showPolyData(pathMesh, 'target frame desired path', color=[0.0, 0.3, 1.0], parent=self.targetAffordance, alpha=0.6)
        self.targetPathMesh.actor.SetUserTransform(self.targetFrame)

    def resetTargetPath(self):
        for obj in om.getObjects():
            if obj.getProperty('Name') == 'target frame desired':
                om.removeFromObjectModel(obj)
        for obj in om.getObjects():
            if obj.getProperty('Name') == 'target frame desired path':
                om.removeFromObjectModel(obj)

    def computeNextTargetFrame(self):
        assert self.targetAffordance
        t = transformUtils.frameFromPositionAndRPY(self.nextPosition, [0, 0, 0])
        self.faceTransformLocal = transformUtils.copyFrame(t) # copy required
        t.Concatenate(self.targetFrame)
        self.faceFrameDesired = vis.showFrame(t, 'target frame desired', parent=self.targetAffordance, visible=False, scale=0.2)


    ######### Higher Level Planning Functions ##################################################################
    def planTargetSweep(self):
        self.graspingHand = 'left'

        self.nextPosition =[0,0,0]
        self.targetPath = []
        self.resetTargetPath()

        self.graspToHandLinkFrame = self.ikPlanner.newGraspToHandFrame(self.graspingHand)
        self.computeNextTargetFrame()
        self.initConstraintSet()
        self.targetPath.append(self.faceTransformLocal)

        pointsPerSide = 3
        deltaDistance = self.targetAffordance.params.get('length') / pointsPerSide # 5cm was good
        for i in xrange(pointsPerSide*0,pointsPerSide*1):
            self.nextPosition[0] += -deltaDistance
            self.computeNextTargetFrame()
            self.addConstraintForTargetFrame(self.faceFrameDesired, i+1)
            self.targetPath.append(self.faceTransformLocal)

        for i in xrange(pointsPerSide*1,pointsPerSide*2):
            self.nextPosition[2] += deltaDistance
            self.computeNextTargetFrame()
            self.addConstraintForTargetFrame(self.faceFrameDesired, i+1)
            self.targetPath.append(self.faceTransformLocal)

        for i in xrange(pointsPerSide*2,pointsPerSide*3):
            self.nextPosition[0] += deltaDistance
            self.computeNextTargetFrame()
            self.addConstraintForTargetFrame(self.faceFrameDesired, i+1)
            self.targetPath.append(self.faceTransformLocal)

        for i in xrange(pointsPerSide*3,pointsPerSide*4):
            self.nextPosition[2] += -deltaDistance
            self.computeNextTargetFrame()
            self.addConstraintForTargetFrame(self.faceFrameDesired, i+1)
            self.targetPath.append(self.faceTransformLocal)

        self.drawTargetPath()
        #self.ikPlanner.ikServer.maxDegreesPerSecond = self.speedLow
        self.planTrajectory()
        #self.ikPlanner.ikServer.maxDegreesPerSecond = self.speedHigh

    def planTargetReach(self):
        self.graspingHand='left'
        self.graspToHandLinkFrame = self.ikPlanner.newGraspToHandFrame(self.graspingHand)

        # A single one shot gaze-constrained reach: place xyz at goal and align y-axis of hand with x-axis of goal
        worldToTargetFrame = vis.updateFrame(self.targetFrame, 'gaze goal', visible=False, scale=0.2, parent=om.getOrCreateContainer('affordances'))

        self.initConstraintSet()
        self.addConstraintForTargetFrame(worldToTargetFrame, 1)
        self.planTrajectory()

    ######### Lower Level Planning Functions ##################################################################
    def planTrajectory(self):
        self.ikPlanner.ikServer.usePointwise = False
        plan = self.constraintSet.runIkTraj()
        self.addPlan(plan)

    def initConstraintSet(self):
        # create constraint set
        startPose = self.getPlanningStartPose()
        startPoseName = 'gaze_plan_start'
        endPoseName = 'gaze_plan_end'
        self.ikPlanner.addPose(startPose, startPoseName)
        self.ikPlanner.addPose(startPose, endPoseName)
        self.constraintSet = ikplanner.ConstraintSet(self.ikPlanner, [], startPoseName, endPoseName)
        self.constraintSet.endPose = startPose

        # add body constraints
        bodyConstraints = self.ikPlanner.createMovingBodyConstraints(startPoseName, lockBase=self.lockBase, lockBack=self.lockBack, lockLeftArm=self.graspingHand=='right', lockRightArm=self.graspingHand=='left')
        self.constraintSet.constraints.extend(bodyConstraints)

    def addConstraintForTargetFrame(self,goalFrame, t):
        if (self.targetSweepType is 'orientation'):
            self.appendPositionOrientationConstraintForTargetFrame(goalFrame, t)
        elif (self.targetSweepType is 'gaze'):
            # align the palmGazeAxis axis (on the hand) with the vector 'targetAxis' from worldToTargetFrame?
            palmGazeAxis = self.ikPlanner.getPalmToHandLink(self.graspingHand).TransformVector([0,1,0])
            self.appendPositionGazeConstraintForTargetFrame(goalFrame, t, targetAxis=[0.0, 0.0, 1.0], bodyAxis=palmGazeAxis)

    def appendPositionGazeConstraintForTargetFrame(self, goalFrame, t, targetAxis=[-1.0, 0.0, 0.0], bodyAxis=[-1.0, 0.0, 0.0]):
        gazeConstraint = self.ikPlanner.createGazeGraspConstraint(self.graspingHand, goalFrame, self.graspToHandLinkFrame, self.coneThresholdDegrees , targetAxis, bodyAxis)
        gazeConstraint.tspan = [t, t]
        self.constraintSet.constraints.insert(0, gazeConstraint)

        positionConstraint, _ = self.ikPlanner.createPositionOrientationGraspConstraints(self.graspingHand, goalFrame, self.graspToHandLinkFrame)
        positionConstraint.tspan = [t, t]
        self.constraintSet.constraints.append(positionConstraint)

    def appendPositionOrientationConstraintForTargetFrame(self, goalFrame, t):
        positionConstraint, orientationConstraint = self.ikPlanner.createPositionOrientationGraspConstraints(self.graspingHand, goalFrame, self.graspToHandLinkFrame)
        positionConstraint.tspan = [t, t]
        orientationConstraint.tspan = [t, t]
        self.constraintSet.constraints.append(positionConstraint)
        self.constraintSet.constraints.append(orientationConstraint)


    ### End Planning Functions ####################################################################
    ########## Glue Functions #####################################################################
    def teleportRobotToStanceFrame(self, frame):
        self.sensorJointController.setPose('q_nom')
        stancePosition = frame.GetPosition()
        stanceOrientation = frame.GetOrientation()

        q = self.sensorJointController.q.copy()
        q[:2] = [stancePosition[0], stancePosition[1]]
        q[5] = math.radians(stanceOrientation[2])
        self.sensorJointController.setPose('EST_ROBOT_STATE', q)


    def getHandDriver(self, side):
        assert side in ('left', 'right')
        return self.lhandDriver if side == 'left' else self.rhandDriver

    def openHand(self, side):
        #self.getHandDriver(side).sendOpen()
        self.getHandDriver(side).sendCustom(0.0, 100.0, 100.0, 0)

    def closeHand(self, side):
        self.getHandDriver(side).sendCustom(100.0, 100.0, 100.0, 0)

    def sendNeckPitchLookDown(self):
        self.multisenseDriver.setNeckPitch(40)

    def sendNeckPitchLookForward(self):
        self.multisenseDriver.setNeckPitch(15)

    def waitForAtlasBehaviorAsync(self, behaviorName):
        assert behaviorName in self.atlasDriver.getBehaviorMap().values()
        while self.atlasDriver.getCurrentBehaviorName() != behaviorName:
            yield

    def printAsync(self, s):
        yield
        print s

    def optionalUserPrompt(self, message):
        if not self.optionalUserPromptEnabled:
            return

        yield
        result = raw_input(message)
        if result != 'y':
            raise Exception('user abort.')

    def requiredUserPrompt(self, message):
        if not self.requiredUserPromptEnabled:
            return

        yield
        result = raw_input(message)
        if result != 'y':
            raise Exception('user abort.')

    def delay(self, delayTimeInSeconds):
        yield
        t = SimpleTimer()
        while t.elapsed() < delayTimeInSeconds:
            yield

    def waitForCleanLidarSweepAsync(self):
        currentRevolution = self.multisenseDriver.displayedRevolution
        desiredRevolution = currentRevolution + 2
        while self.multisenseDriver.displayedRevolution < desiredRevolution:
            yield

    def getEstimatedRobotStatePose(self):
        return self.sensorJointController.getPose('EST_ROBOT_STATE')

    def getPlanningStartPose(self):
        if self.planFromCurrentRobotState:
            return self.getEstimatedRobotStatePose()
        else:
            if self.plans:
                return robotstate.convertStateMessageToDrakePose(self.plans[-1].plan[-1])
            else:
                return self.getEstimatedRobotStatePose()

    def cleanupFootstepPlans(self):
        om.removeFromObjectModel(om.findObjectByName('walking goal'))
        om.removeFromObjectModel(om.findObjectByName('footstep plan'))
        self.footstepPlan = None

    def playSequenceNominal(self):
        assert None not in self.plans
        self.planPlaybackFunction(self.plans)

    def commitManipPlan(self):
            self.manipPlanner.commitManipPlan(self.plans[-1])

    def commitFootstepPlan(self):
        self.footstepPlanner.commitFootstepPlan(self.footstepPlan)

    def waitForPlanExecution(self, plan):
        planElapsedTime = planplayback.PlanPlayback.getPlanElapsedTime(plan)
        return self.delay(planElapsedTime + 1.0)


    def animateLastPlan(self):
        plan = self.plans[-1]
        if not self.visOnly:
            self.commitManipPlan()

        return self.waitForPlanExecution(plan)

    def onRobotModelChanged(self, model):

        for linkName in self.frameSyncs.keys():
            t = self.playbackRobotModel.getLinkFrame(linkName)
            vis.updateFrame(t, '%s frame' % linkName, scale=0.2, visible=False, parent='planning')


    ######### Nominal Plans and Execution  #################################################################
    def prepTestDemoSequence(self):
        '''
        Running this function should launch a full planning sequence
        to pick to objects, walk and drop.
        Requires footstep footstepPlanner
        '''

        filename = os.path.expanduser('~/drc-testing-data/tabletop/table-and-bin-scene.vtp')
        scene = ioUtils.readPolyData(filename)
        vis.showPolyData(scene,"scene")

        #stanceFrame = transformUtils.frameFromPositionAndRPY([0, 0, 0], [0, 0, 123.0])
        #self.teleportRobotToStanceFrame(stanceFrame)

        self.userFitTable()
        self.onSegmentTable( np.array([-1.72105646,  2.73210716,  0.79449952]), np.array([-1.67336452,  2.63351011,  0.78698605]) )
        self.userFitBin()
        self.onSegmentBin( np.array([-0.02, 2.43, 0.61 ]), np.array([-0.40,  2.79,  0.61964661]) )
        self.computeTableStanceFrame()
        self.computeBinStanceFrame()

        # Actually plan the sequence:
        #self.demoSequence()


    def prepIhmcDemoSequenceFromFile(self):

        filename = os.path.expanduser('~/drc-testing-data/ihmc_table/ihmc_table.vtp')
        polyData = ioUtils.readPolyData( filename )
        vis.showPolyData( polyData,'scene')
        self.prepIhmcDemoSequence()



    def prepIhmcDemoSequence(self):
        self.userFitBin()
        self.onSegmentBin( np.array([ 0.62, -1.33, 0.80]), np.array([ 0.89, -0.87, 0.57]) )
        self.userFitTable()
        self.onSegmentTable( np.array([ 1.11, 0.11, 0.85]), np.array([ 0.97, 0.044, 0.84]) )

        self.segmentTableObjects()
        self.computeBinStanceFrame()
        self.computeTableStanceFrame()


    def planSequence(self):
        self.useFootstepPlanner = True
        side = 'left' # left, right, both

        self.cleanupFootstepPlans()
        self.planFromCurrentRobotState = False
        self.segmentTableObjects()
        self.plans = []

        # Go home
        self.planWalkToStance(self.startStanceFrame.transform)

        # Pick Objects from table:
        self.planWalkToStance(self.tableStanceFrame.transform)
        if (side == 'left'):
            self.planSequenceTablePick('left')
        elif (side == 'right'):
            self.planSequenceTablePick('right')
        elif (side == 'both'):
            self.planSequenceTablePick('left')
            self.planSequenceTablePick('right')

        # Go home
        self.planWalkToStance(self.startStanceFrame.transform)

        # Go to Bin
        self.planWalkToStance(self.binStanceFrame.transform)

        # Drop into the Bin:
        if (side == 'left'):
            self.planDropPostureRaise('left')
            self.dropTableObject('left')
            self.planDropPostureLower('left')
        elif (side == 'right'):
            self.planDropPostureRaise('right')
            self.dropTableObject('right')
            self.planDropPostureLower('right')
        elif (side == 'both'):
            self.planDropPostureRaise('left')
            self.dropTableObject('left')
            self.planDropPostureLower('left')
            self.planDropPostureRaise('right')
            self.dropTableObject('right')
            self.planDropPostureLower('right')

        # Go home
        self.planWalkToStance(self.startStanceFrame.transform)


    def planSequenceTablePick(self, side):
        self.planPreGrasp(side)
        self.planReachToTableObject(side)
        self.planTouchTableObject(side)
        self.graspTableObject(side)
        self.planLiftTableObject(side)


    def autonomousExecute(self):

        self.planFromCurrentRobotState = True
        self.visOnly = False
        #self.ikPlanner.ikServer.usePointwise = True
        #self.ikPlanner.ikServer.maxDegreesPerSecond = 20

        taskQueue = AsyncTaskQueue()
        #self.addTasksToQueueInit(taskQueue)

        # Go home
        self.addTasksToQueueWalking(taskQueue, self.startStanceFrame.transform, 'Walk to Start')

        # Pick Objects from table:
        self.addTasksToQueueWalking(taskQueue, self.tableStanceFrame.transform, 'Walk to Table')
        taskQueue.addTask(self.printAsync('Pick with Left Arm'))
        self.addTasksToQueueTablePick(taskQueue, 'left')
        #taskQueue.addTask(self.printAsync('Pick with Right Arm'))
        #self.addTasksToQueueTablePick(taskQueue, 'right')

        # Go home
        self.addTasksToQueueWalking(taskQueue, self.startStanceFrame.transform, 'Walk to Start')

        # Go to Bin
        self.addTasksToQueueWalking(taskQueue, self.binStanceFrame.transform, 'Walk to Bin')

        # Drop into the Bin:
        taskQueue.addTask(self.printAsync('Drop from Left Arm'))
        self.addTasksToQueueDropIntoBin(taskQueue, 'left')
        #taskQueue.addTask(self.printAsync('Drop from Right Arm'))
        #self.addTasksToQueueDropIntoBin(taskQueue, 'right')

        # Go home
        self.addTasksToQueueWalking(taskQueue, self.startStanceFrame.transform, 'Walk to Start')
        taskQueue.addTask(self.printAsync('done!'))

        return taskQueue


    def addTasksToQueueInit(self, taskQueue):

        taskQueue.addTask(self.printAsync('user fit table'))
        taskQueue.addTask(self.userFitTable)
        taskQueue.addTask(self.waitForTableFit)

        taskQueue.addTask(self.printAsync('user fit bin'))
        taskQueue.addTask(self.userFitBin)
        taskQueue.addTask(self.waitForBinFit)

        taskQueue.addTask(self.computeTableStanceFrame)
        taskQueue.addTask(self.computeBinStanceFrame)


    def addTasksToQueueTablePick(self, taskQueue, side):
        taskQueue.addTask(self.requiredUserPrompt('continue? y/n: '))
        taskQueue.addTask(functools.partial(self.planPreGrasp, side))
        taskQueue.addTask(self.animateLastPlan)

        taskQueue.addTask(self.requiredUserPrompt('continue? y/n: '))
        taskQueue.addTask(functools.partial(self.planReachToTableObject, side))
        taskQueue.addTask(self.animateLastPlan)

        taskQueue.addTask(self.requiredUserPrompt('continue? y/n: '))
        taskQueue.addTask(functools.partial(self.planTouchTableObject, side))
        taskQueue.addTask(self.animateLastPlan)

        taskQueue.addTask(self.requiredUserPrompt('continue? y/n: '))
        taskQueue.addTask(functools.partial(self.closeHand, side))
        taskQueue.addTask(functools.partial(self.graspTableObject, side))

        taskQueue.addTask(self.requiredUserPrompt('continue? y/n: '))
        taskQueue.addTask(functools.partial(self.planLiftTableObject, side))
        taskQueue.addTask(self.animateLastPlan)


    def addTasksToQueueDropIntoBin(self, taskQueue, side):
        taskQueue.addTask(self.requiredUserPrompt('continue? y/n: '))
        taskQueue.addTask(functools.partial(self.planDropPostureRaise, side))
        taskQueue.addTask(self.animateLastPlan)

        taskQueue.addTask(functools.partial(self.openHand, side))
        taskQueue.addTask(functools.partial(self.dropTableObject, side))

        taskQueue.addTask(self.requiredUserPrompt('continue? y/n: '))
        taskQueue.addTask(functools.partial(self.planDropPostureLower, side))
        taskQueue.addTask(self.animateLastPlan)


    def addTasksToQueueWalking(self, taskQueue, stanceTransform, message):
        taskQueue.addTask(self.printAsync(message))
        taskQueue.addTask( functools.partial(self.planWalkToStance, stanceTransform ))
        taskQueue.addTask(self.optionalUserPrompt('Send footstep plan. continue? y/n: '))
        taskQueue.addTask(self.commitFootstepPlan)
        #taskQueue.addTask(self.animateLastPlan) # ought to wait until arrival, currently doesnt wait the right amount of time
        taskQueue.addTask(self.requiredUserPrompt('Have you arrived? y/n: '))
