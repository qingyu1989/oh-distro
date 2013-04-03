
#include "Footsteps.h"

using namespace TwoLegs;

Footsteps::Footsteps() {
	std::cout << "New Footsteps object created" << std::endl;
}

void Footsteps::newFootstep(footstep newstep) {
	footstep_hist.push_back(newstep);
	active_step = newstep;
}

void Footsteps::addFootstep(Eigen::Isometry3d RelativeFrameLocation, int foot) 
{
	//std::cout << "addFootstep function was called for Footsteps class" << std::endl;
	
	footstep new_footprint;
	
	/*
	new_footprint.footprintlocation.position = RelativeFrameLocation.translation;
	new_footprint.footprintlocation.orientation = RelativeFrameLocation.rotation;
	new_footprint.foot = foot;
	
	active_step.footprintlocation.position = RelativeFrameLocation.translation;
	active_step.footprintlocation.orientation = RelativeFrameLocation.rotation;
	*/
	
	new_footprint.foot = foot;
	new_footprint.footprintlocation = RelativeFrameLocation;
	
	footstep_hist.push_back(new_footprint);
	
	active_step = new_footprint;
}

Eigen::Isometry3d Footsteps::getLastStep() {
	//if (active_step.foot==LEFTFOOT)
	  //std::cout << " left  ";
	//else
	  //std::cout << " right ";
	
	return active_step.footprintlocation;
		
	// TODO - decide on a final rotational representation and convert this part of the code to use drc types
	
	//return trans;
}

int Footsteps::lastFoot() {
	return active_step.foot;
}

void Footsteps::reset() {
	footstep_hist.clear();
}
