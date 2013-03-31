#include <stdio.h>
#include <inttypes.h>
#include <iostream>
#include <boost/shared_ptr.hpp>
#include <boost/assign/std/vector.hpp>
#include <lcm/lcm-cpp.hpp>
#include <lcmtypes/drc_lcmtypes.hpp>

// define the following in order to eliminate the deprecated headers warning
#define VTK_EXCLUDE_STRSTREAM_HEADERS
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/io/vtk_lib_io.h>
#include "pcl/PolygonMesh.h"

#include <pointcloud_tools/pointcloud_vis.hpp> // visualize pt clds
#include <affordance/AffordanceUtils.hpp>

using namespace pcl;
using namespace pcl::io;

using namespace std;
class Pass{
  public:
    Pass(boost::shared_ptr<lcm::LCM> &lcm_);
    
    ~Pass(){
    }    
    
  private:
    boost::shared_ptr<lcm::LCM> lcm_;
    void affordanceHandler(const lcm::ReceiveBuffer* rbuf, const std::string& channel, const  drc::affordance_collection_t* msg);
    pointcloud_vis* pc_vis_;
    
    AffordanceUtils affutils;
};

Pass::Pass(boost::shared_ptr<lcm::LCM> &lcm_): lcm_(lcm_){
  lcm_->subscribe("AFFORDANCE_COLLECTION",&Pass::affordanceHandler,this);  
  pc_vis_ = new pointcloud_vis( lcm_->getUnderlyingLCM() );
  cout << "Finished setting up\n";
}

void Pass::affordanceHandler(const lcm::ReceiveBuffer* rbuf, 
                             const std::string& channel, const  drc::affordance_collection_t* msg){
  cout << "got "<< msg->affs.size() <<" affs\n";
  for (size_t i=0; i < msg->affs.size() ; i++){
    int aff_id =i;
    int cfg_root = aff_id*10;

    drc::affordance_t a = msg->affs[aff_id];
    Eigen::Isometry3d pose = affutils.getPose( a.param_names, a.params );
    // obj: id name type reset
    // pts: id name type reset objcoll usergb rgb
    
    if (a.points.size() !=0 ){
      obj_cfg oconfig = obj_cfg(cfg_root,   string( "Affordance Pose " + std::to_string(i))   ,5,1);
      Isometry3dTime poseT = Isometry3dTime ( 0, pose  );
      pc_vis_->pose_to_lcm(oconfig,poseT);

      if (a.triangles.size() ==0){
        pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud = affutils.getCloudFromAffordance(a.points);
        ptcld_cfg pconfig = ptcld_cfg(cfg_root+1,  string( "Affordance Cloud " + std::to_string(i))     ,1,1, cfg_root,1, {0.2,0,0.2} );
        pc_vis_->ptcld_to_lcm(pconfig, *cloud, 0, 0);  
      }else{
        pcl::PolygonMesh::Ptr mesh = affutils.getMeshFromAffordance(a.points, a.triangles);
        ptcld_cfg pconfig = ptcld_cfg(cfg_root+2,    string( "Affordance Mesh " + std::to_string(i))     ,7,1, cfg_root,1, {0.2,0,0.2} );
        pc_vis_->mesh_to_lcm(pconfig, mesh, 0, 0);  
      }
      
      pcl::PointCloud<pcl::PointXYZRGB>::Ptr bb_cloud = affutils.getBoundingBoxCloud(a.bounding_pos, a.bounding_rpy, a.bounding_lwh);
      ptcld_cfg pconfig = ptcld_cfg(cfg_root+3,    string( "Affordance Bounding Box " + std::to_string(i))     ,4,1, cfg_root,1, {0.2,0,0.2} );
      pc_vis_->ptcld_to_lcm(pconfig, *bb_cloud, 0, 0);  
    }
  }
}


int main( int argc, char** argv ){
  boost::shared_ptr<lcm::LCM> lcm(new lcm::LCM);
  if(!lcm->good()){
    std::cerr <<"ERROR: lcm is not good()" <<std::endl;
  }
  
  Pass app(lcm);
  cout << "Demo Receive Ready" << endl << "============================" << endl;
  while(0 == lcm->handle());
  return 0;
}