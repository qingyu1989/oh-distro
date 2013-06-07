#include "distTransform.h"
#include <iostream>
#include <assert.h>

#include <pcl/point_types.h>
#include <pcl/common/centroid.h>
#include <pcl/io/pcd_io.h>
#include <Eigen/SVD>
#include <Eigen/Eigen>
#include <bot_core/rotations.h>

using namespace std;
using namespace pcl;
using namespace Eigen;

#define ASSERT_PI(b) {if(!(b)) {cout << "Error on line: " << __LINE__ << endl; exit(-1);}}
#define SQ(x) ((x)*(x))

static Matrix3f ypr2rot(Vector3f ypr){
  double rpy[]={ypr[2],ypr[1],ypr[0]};
  double q[4];
  double mat[9];
  bot_roll_pitch_yaw_to_quat(rpy,q);
  int rc=bot_quat_to_matrix(q,mat);
  Matrix3d mat2(mat);
  for(int j=0;j<3;j++){
    for(int i=0;i<3;i++){
      mat2(j,i) = mat[j*3+i];
    }
  }
  return mat2.cast<float>();
}

void align_pts_3d(const vector<Vector3f>& pts_ref, const vector<Vector3f>& pts_cur,
                  Matrix3f& R, Vector3f& T){
  ASSERT_PI(pts_ref.size()==pts_cur.size());

  // compute mean
  Vector3f avg_ref(0,0,0), avg_cur(0,0,0);
  int N=pts_ref.size();
  for(int i=0;i<N;i++){
    avg_ref+=pts_ref[i];
    avg_cur+=pts_cur[i];
  }
  avg_ref/=N; avg_cur/=N;

  // sub mean
  vector<Vector3f> p_ref(N), p_cur(N);
  for(int i=0;i<N;i++){
    p_ref[i] = pts_ref[i]-avg_ref;
    p_cur[i] = pts_cur[i]-avg_cur;
    
  }

  // trans mat
  Matrix4f T_ref = Matrix4f::Identity();
  Matrix4f T_cur = Matrix4f::Identity();
  T_ref.block<3,1>(0,3) = -avg_ref; 
  T_cur.block<3,1>(0,3) = -avg_cur;

  // svd
  Matrix3f svdmat = Matrix3f::Zero();
  for(int i=0;i<N;i++){
    for(int j=0;j<N;j++){
      svdmat +=  p_ref[i]*p_cur[i].transpose();
    }
  }
  JacobiSVD<Matrix3f> svd = svdmat.jacobiSvd();
  Matrix3f u = svd.matrixU();
  Matrix3f v = svd.matrixV();

  Matrix3f R3 = u*v.transpose();
  Matrix4f R4 = Matrix4f::Identity();
  R4.block<3,3>(0,0) = R3;
  Matrix4f P = T_ref.inverse()*R4*T_cur;
  
  // results
  R = P.block<3,3>(0,0);
  T = P.block<3,1>(0,3);

}

void create_voxels(const vector<Vector3f>& pts, float res, float padding,
                   vector<float>& vol, Vector3i& vol_size, Matrix4f& world_to_vol)
{
  Vector3f pt_min, pt_max;
  pt_min = pt_max = pts[0];
  for(int i=0;i<pts.size();i++){
    pt_min = pt_min.cwiseMin(pts[i]);
    pt_max = pt_max.cwiseMax(pts[i]);
  }

  // create world to vol
  world_to_vol = Matrix4f::Identity();
  world_to_vol.block<3,1>(0,3) = Vector3f(-padding,-padding,-padding);
  Matrix4f scale = Matrix4f::Identity();
  scale(0,0) = res;
  scale(1,1) = res;
  scale(2,2) = res;
  world_to_vol = scale*world_to_vol;
  Matrix4f trans = Matrix4f::Identity();
  trans.block<3,1>(0,3) = pt_min;
  world_to_vol = trans*world_to_vol;
  world_to_vol = world_to_vol.inverse();
  
  // alloc vol
  Vector3f sizef = (pt_max-pt_min)/res+2*Vector3f(padding,padding,padding);
  Vector3i size(ceil(sizef[0]),  ceil(sizef[1]), ceil(sizef[2]));
  vol_size = size;
  vol.clear();
  vol.resize(size[0]*size[1]*size[2]);
  //cout << size.transpose( ) << endl;
  //cout << pt_max.transpose( ) << " " << pt_min.transpose() << endl;


  // populate vol
  for(int i=0;i<pts.size();i++){
    Vector4f pts4(pts[i][0],pts[i][1],pts[i][2],1.0);
    Vector4f vol4 = world_to_vol*pts4;
    Vector3i voli(round(vol4[0]),round(vol4[1]),round(vol4[2]));
    //cout << i << " " << voli.transpose() << endl;
    //cout << i << " " << size.transpose() << endl;

    ASSERT_PI(voli[0]>=0 && voli[1]>=0 && voli[2]>=0);
    ASSERT_PI(voli[0]<size[0] && voli[1]<size[1] && voli[2]<size[2]);
    int index = voli[0]*size[1]*size[2] + voli[1]*size[2] + voli[2];
    ASSERT_PI(index<vol.size());
    vol[index] = 1;
  }
}

void point_pairs_from_dist_inds(vector<int>& dist_inds, Vector3i vol_size, const vector<Vector3f>& pts, 
                                Matrix3f R, Vector3f T,
                                vector<Vector3f>& p0, vector<Vector3f>& p1)
{
  for(int i=0;i<pts.size();i++){
    Vector3f pt_cur = R*pts[i]+T; 
    Vector3i pt_int(round(pt_cur[0]), round(pt_cur[1]), round(pt_cur[2]));
    bool good=true;
    for(int j=0;j<3;j++) if(pt_int[j]<0||pt_int[j]>=vol_size[j]) good=false;

    if(good){
      int ind=pt_int[0]*vol_size[1]*vol_size[2] + pt_int[1]*vol_size[2] + pt_int[2]; //TODO: double check
      int model_ind = dist_inds[ind];
      Vector3f modelPos;
      modelPos[2] = model_ind%vol_size[2];
      model_ind/=vol_size[2];
      modelPos[1] = model_ind%vol_size[1];
      model_ind/=vol_size[1];
      modelPos[0] = model_ind;
      ASSERT_PI(model_ind<vol_size[0]);
      p0.push_back(modelPos);
      p1.push_back(pt_cur);
    }
  }
}

void align_coarse_to_fine(
          const vector<Vector3f>& pts_model, 
          const vector<Vector3f>& pts_data, 
          const vector<float>& res_range
          //TODO pose_init
          )
{
  /////////////////////////////
  // coarse align
  float res = res_range[0];
  float padding = 10; //TODO

  // convert model to voxels
  vector<float> dist_xform;
  vector<int> dist_inds;
  Vector3i vol_size;
  Matrix4f world_to_vol;
  create_voxels(pts_model, res, padding, dist_xform, vol_size, world_to_vol);

  // perform distance transform on model
  dist_inds.resize(dist_xform.size());
  distTransform(dist_xform.data(), dist_inds.data(), vol_size[2], vol_size[1], vol_size[0]); 
  float maxDist = SQ(vol_size[0]) + SQ(vol_size[1]) + SQ(vol_size[2]);
  maxDist = sqrt(maxDist);
  for(int i=0;i<dist_xform.size();i++) dist_xform[i]/=maxDist;
  Vector3f avg_model(0,0,0);
  for(int i=0;i<pts_model.size();i++) avg_model+=pts_model[i];
  avg_model/=pts_model.size();
  //TODO pts_data_dec = decimate_points(pts_data,res);
  vector<Vector3f> pts_data_dec = pts_data;
  
  // iterate through angles
  float angle_step=30;
  //TODO: allow limit to search
  for(float roll=-180; roll<180; roll+=angle_step){
    for(float pitch=-90; pitch<90; pitch+=angle_step){
      for(float yaw=-180; yaw<180; yaw+=angle_step){
        Matrix3f R = ypr2rot(Vector3f(yaw,pitch,roll));
        //Matrix3f Rt = R.transpose();
        vector<Vector3f> pts(pts_data_dec.size());
        for(int i=0;i<pts.size();i++) pts[i] = R*pts_data_dec[i]; //TODO: this transpose ok, should R be transpose??
        Vector3f pts_mean(0,0,0);
        for(int i=0;i<pts.size();i++) pts_mean+=pts[i];
        pts_mean/=pts.size();
        Vector3f T=avg_model-pts_mean;
        for(int i=0;i<pts.size();i++) {
          Vector3f p = pts[i]+T;
          Vector4f p4(p[0],p[1],p[2],1);
          p4=world_to_vol*p4; 
          pts[i]=Vector3f(p4[0],p4[1],p4[2]); 
        }

        // compute point pairs and align
        vector<Vector3f> p0,p1;
        point_pairs_from_dist_inds(dist_inds, vol_size, pts, Matrix3f::Identity(), Vector3f(0,0,0), p0, p1);
        Matrix3f R_opt;
        Vector3f T_opt;
        align_pts_3d(p0,p1,R_opt,T_opt);
        //TODO left off at dist_xform_error
      }  
    }
  }
}


/* Notes
   distTransform L M N: L is the inner loop
   create_voxels: size[2] is the inner loop
   TODO: use one convention.
*/



int main(int argc, char*argv[]){
  if(argc!=3) {
    cerr << "usage: testfitting model.pcd cloud.pcd\n";
    exit(-1);
  }
  
  // load model
  PCDReader reader;
  PointCloud<pcl::PointXYZRGB>::Ptr modelcloud(new pcl::PointCloud<pcl::PointXYZRGB>());
  int rc = reader.read(argv[1], *modelcloud);
  ASSERT_PI(rc==0);

  // load cloud
  PointCloud<pcl::PointXYZRGB>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGB>());
  rc = reader.read(argv[2], *cloud);
  ASSERT_PI(rc==0);
  
  vector<Vector3f> modelcloudV(modelcloud->size());
  for(int i=0;i<modelcloud->size();i++) {
    modelcloudV[i][0] = modelcloud->at(i).x;
    modelcloudV[i][1] = modelcloud->at(i).y;
    modelcloudV[i][2] = modelcloud->at(i).z;
  }

  vector<Vector3f> cloudV(cloud->size());
  for(int i=0;i<cloud->size();i++) {
    cloudV[i][0] = cloud->at(i).x;
    cloudV[i][1] = cloud->at(i).y;
    cloudV[i][2] = cloud->at(i).z;
  }

  vector<float> modelvol;
  Vector3i modelvolsize;
  Matrix4f model_world_to_vol;
  create_voxels(modelcloudV, 0.01, 0, modelvol, modelvolsize, model_world_to_vol);

  vector<float> cloudvol;
  Vector3i cloudvolsize;
  Matrix4f cloud_world_to_vol;
  create_voxels(cloudV, 0.01, 0, cloudvol, cloudvolsize, cloud_world_to_vol);
  cout << cloudvolsize[2] << " " << cloudvolsize[1] << " " << cloudvolsize[0] << endl;
  vector<float> clouddist = cloudvol;
  for(int i=0;i<1000;i++){
    clouddist = cloudvol;
    vector<int> cloudidx(cloudvol.size());
    distTransform(clouddist.data(), cloudidx.data(), cloudvolsize[2], cloudvolsize[1], cloudvolsize[0]); 
  }
  cout << "done\n";

  FILE*fp = fopen("dump.bin", "wb");
  fwrite(clouddist.data(),sizeof(float),cloudvolsize[2]*cloudvolsize[1]*cloudvolsize[0],fp);
  fclose(fp);

  PointCloud<pcl::PointXYZRGB>::Ptr c2(new pcl::PointCloud<pcl::PointXYZRGB>());

  for(int i=0;i<cloudvolsize[0];i++){
    for(int j=0;j<cloudvolsize[1];j++){
      for(int k=0;k<cloudvolsize[2];k++){
        int idx = i*cloudvolsize[1]*cloudvolsize[2] + j*cloudvolsize[2] + k;
        ASSERT_PI(idx<cloudvol.size());
        if(cloudvol[idx]){
          PointXYZRGB pt;
          pt.x = i;
          pt.y = j;
          pt.z = k;
          c2->points.push_back(pt);
        }
      }
    }
  }
  c2->width=1;c2->height=c2->size();
  PCDWriter writer;
  writer.write("test.pcd",*c2);
                              

}

