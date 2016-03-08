/**
 * A class to compute a suitable final configuration for humanoids grasping motions.
 */

#ifndef FINALPOSEPLANNER_HPP_
#define FINALPOSEPLANNER_HPP_

#include <math.h>
#include <Eigen/Dense>
#include <boost/shared_ptr.hpp>
#include <lcm/lcm-cpp.hpp>

#include "drake/systems/plants/RigidBodyTree.h"
#include "drake/systems/plants/RigidBodyIK.h"
#include "drake/systems/plants/constraint/RigidBodyConstraint.h"
#include "drake/drakeShapes_export.h"
#include "capabilityMap/CapabilityMap.hpp"
#include "drake/systems/plants/IKoptions.h"

struct FPPOutput
{
	int n_valid_samples;
	int n_valid_samples_used;
	double cost;
	double computation_time;
	double IK_time;
};

class FinalPosePlanner
{
public:
	/**
	 * Default constructor.
	 */
	FinalPosePlanner();

	/**
	 * Finds a suitable final configuration for reaching a given end-effector pose.
	 * \param robot   A RigidBodyTree. The manipulating robot.
	 * \param end_effector    A string containing the name of the end-effector to be used.
	 * \param start_configuration     A robot.num_positions x 1 double vector. The starting robot configuration.
	 * \param endeffector_final_pose    A 6x1(rpy-xyz) or 7x1(quat-xyz) double vector. The final end effector pose.
	 * \param additional_constraints A vector of RigidBodyConstraints. Additional constraints for the final pose.
	 * \param nominal_configuration     A robot.num_positions x 1 double vector. The nominal robot configuration to be used for IK.
	 * \param capability_map     A CapabilityMap object. The map representing robot reaching capabilities.
	 * \param ik_options     IKOptions object to be used for IK.
	 * \param endeffector_point     A 3 x 1 double vector. The position of the reaching point relative to the end-effector expressed in end-effector coordinate frame.
	 * \return An integer\n
	 * 1   Final pose has been found.\n
	 * 12  Error: Incorrect input.
	 */
	int findFinalPose(RigidBodyTree &robot, std::string end_effector, std::string endeffector_side, Eigen::VectorXd start_configuration,
			Eigen::VectorXd endeffector_final_pose, const std::vector<RigidBodyConstraint *> &additional_constraints, Eigen::VectorXd nominal_configuration,
			CapabilityMap &capability_map, std::vector<Eigen::Vector3d> point_cloud, IKoptions ik_options, boost::shared_ptr<lcm::LCM> lcm, FPPOutput &output,
			double min_distance = 0.005, Eigen::Vector3d endeffector_point = Eigen::Vector3d(0,0,0)); //todo: active collision options?

private:
	/**
	 * Checks that a configuration has the right number of elements and that all values are within joint limits.
	 * \param robot A rigid body tree
	 * \param configuration The configuration to check
	 * \param variable_name The name of the variable to check (only used for error messages)
	 * \return 12 if the configuration is invalid 0 otherwise.
	 */
	int checkConfiguration(const RigidBodyTree &robot, const Eigen::VectorXd &configuration, std::string variable_name);

//	void generateEndeffectorConstraints(RigidBodyTree &robot, std::vector<RigidBodyConstraint *> &constraint_vector, int endeffector_id,
//			Eigen::Matrix<double, 7, 1> endeffector_final_pose, Eigen::Vector3d endeffector_point,
//			Eigen::Vector3d position_tolerance = Eigen::Vector3d(0,0,0), double angular_tolerance = 1./180.*M_PI);
};



#endif /* FINALPOSEPLANNER_HPP_ */
