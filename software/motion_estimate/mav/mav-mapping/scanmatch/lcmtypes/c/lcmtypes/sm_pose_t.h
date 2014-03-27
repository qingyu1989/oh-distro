/** THIS IS AN AUTOMATICALLY GENERATED FILE.  DO NOT MODIFY
 * BY HAND!!
 *
 * Generated by lcm-gen
 **/

#include <stdint.h>
#include <stdlib.h>
#include <lcm/lcm_coretypes.h>
#include <lcm/lcm.h>

#ifndef _sm_pose_t_h
#define _sm_pose_t_h

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _sm_pose_t sm_pose_t;
struct _sm_pose_t
{
    int64_t    utime;
    double     pos[3];
    double     vel[3];
    double     orientation[4];
    double     rotation_rate[3];
    double     accel[3];
};

sm_pose_t   *sm_pose_t_copy(const sm_pose_t *p);
void sm_pose_t_destroy(sm_pose_t *p);

typedef struct _sm_pose_t_subscription_t sm_pose_t_subscription_t;
typedef void(*sm_pose_t_handler_t)(const lcm_recv_buf_t *rbuf,
             const char *channel, const sm_pose_t *msg, void *user);

int sm_pose_t_publish(lcm_t *lcm, const char *channel, const sm_pose_t *p);
sm_pose_t_subscription_t* sm_pose_t_subscribe(lcm_t *lcm, const char *channel, sm_pose_t_handler_t f, void *userdata);
int sm_pose_t_unsubscribe(lcm_t *lcm, sm_pose_t_subscription_t* hid);
int sm_pose_t_subscription_set_queue_capacity(sm_pose_t_subscription_t* subs,
                              int num_messages);


int  sm_pose_t_encode(void *buf, int offset, int maxlen, const sm_pose_t *p);
int  sm_pose_t_decode(const void *buf, int offset, int maxlen, sm_pose_t *p);
int  sm_pose_t_decode_cleanup(sm_pose_t *p);
int  sm_pose_t_encoded_size(const sm_pose_t *p);

// LCM support functions. Users should not call these
int64_t __sm_pose_t_get_hash(void);
int64_t __sm_pose_t_hash_recursive(const __lcm_hash_ptr *p);
int     __sm_pose_t_encode_array(void *buf, int offset, int maxlen, const sm_pose_t *p, int elements);
int     __sm_pose_t_decode_array(const void *buf, int offset, int maxlen, sm_pose_t *p, int elements);
int     __sm_pose_t_decode_array_cleanup(sm_pose_t *p, int elements);
int     __sm_pose_t_encoded_array_size(const sm_pose_t *p, int elements);
int     __sm_pose_t_clone_array(const sm_pose_t *p, sm_pose_t *q, int elements);

#ifdef __cplusplus
}
#endif

#endif
