#include <string.h>
#include <stdlib.h>
#include <stdexcept>

#include <collision/collision_object_sphere.h>
#include <collision/collision_object_point_cloud.h>


using namespace std;
using namespace Eigen;
using namespace KDL;
using namespace collision;

/**
 * Collision_Object_Point_Cloud
 * class constructor
 */
Collision_Object_Point_Cloud::
Collision_Object_Point_Cloud( string id,
                              unsigned int maxPoints,
                              double pointRadius ) : Collision_Object( id ),
                                                          _max_points( maxPoints ),
                                                          _point_radius( pointRadius ),
                                                          _points(),
                                                          _collision_objects() {
  _load_collision_objects( pointRadius );
  for( unsigned int i = 0; i < _collision_objects.size(); i++ ){
    if( _collision_objects[ i ] != NULL ){
      for( unsigned int j = 0; j < _collision_objects[ i ]->bt_collision_objects().size(); j++ ){
        _bt_collision_objects.push_back( _collision_objects[ i ]->bt_collision_objects()[ j ] );
      }
    }
  }
}

/**
 * ~Collision_Object_Point_Cloud
 * class destructor
 */
Collision_Object_Point_Cloud::
~Collision_Object_Point_Cloud(){
  for( unsigned int i = 0; i < _collision_objects.size(); i++ ){
    if( _collision_objects[ i ] != NULL ){
      delete _collision_objects[ i ];
      _collision_objects[ i ] = NULL;
    }
  }
  _collision_objects.clear();
}

/**
 * Collision_Object_Point_Cloud
 * copy constructor
 */
Collision_Object_Point_Cloud::
Collision_Object_Point_Cloud( const Collision_Object_Point_Cloud& other ): Collision_Object( other ),
                                                                            _max_points( other._max_points ),
                                                                            _point_radius( other._point_radius ),
                                                                            _points( other._points ),
                                                                            _collision_objects() {
  _load_collision_objects( _point_radius );
}   

/** 
 * set
 * sets the point cloud to the points
 */
void
Collision_Object_Point_Cloud::
set( vector< Vector3f >& points ){
  _points = points;
  for( unsigned int i = 0; i < _points.size(); i++ ){
    if( i < _collision_objects.size() ){
      _collision_objects[ i ]->set_transform( _points[ i ], Vector4f( 0.0, 0.0, 0.0, 1.0 ) );
      _collision_objects[ i ]->set_active( true );
    } else {
      cout << "adding too many points to the point cloud collision object" << endl;
    }
  }
  for( unsigned int i = _points.size(); i < _max_points; i++ ){
    if( i < _collision_objects.size() ){
      _collision_objects[ i ]->set_active( false );
    }
  }
  return;
}

/** 
 * set_transform
 * sets the world-frame position and orientation of the collision object
 */
void
Collision_Object_Point_Cloud::
set_transform( const Vector3f position,
                const Vector4f orientation )
{
  throw std::runtime_error("Not Implemented: collision_object_point_cloud.cc --> set_transform");
}

void
Collision_Object_Point_Cloud::
set_transform( const Frame& transform ){
  throw std::runtime_error("Not Implemented: collision_object_point_cloud.cc --> set_transform");
}

/**
 * matches_uid
 */
Collision_Object*
Collision_Object_Point_Cloud::
matches_uid( unsigned int uid ){
  for( unsigned int i = 0; i < _collision_objects.size(); i++ ){
    vector< btCollisionObject* > bt_collision_object_vector = _collision_objects[ i ]->bt_collision_objects();
    for( unsigned int j = 0; j < bt_collision_object_vector.size(); j++ ){
      if( bt_collision_object_vector[ j ]->getBroadphaseHandle()->getUid() == uid ){
        return _collision_objects[ i ];
      }
    }
  }
  return NULL;
}

/**
 * _load_collision_objects
 * iterates through all of the links and loads collision objects based on the link type
 */
void
Collision_Object_Point_Cloud::
_load_collision_objects( double pointRadius ){
  while( _collision_objects.size() != _max_points ){
    char buffer[ 80 ];
    sprintf( buffer, "%06d", ( int )( _collision_objects.size() ) );
    _collision_objects.push_back( new Collision_Object_Sphere( string( buffer ), pointRadius ) ); // used by lidar filtering
  }
  return;
}