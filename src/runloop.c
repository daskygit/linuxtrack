#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include "cal.h"
#include "utils.h"
#include "runloop.h"
#include "pref_global.h"

static pthread_cond_t state_cv = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t state_mx = PTHREAD_MUTEX_INITIALIZER;
static bool change_flag = false;
static struct frame_type frame;
static bool frame_acquired = false;

int ltr_int_rl_run(struct camera_control_block *ccb, frame_callback_fun cbk)
{
  assert(ccb != NULL);
  assert(cbk != NULL);
  int retval;
  enum ltr_request_t my_request;
  bool stop_flag = false;
  struct timespec tp;
  unsigned int counter = 0;
  ltr_int_cal_set_state(INITIALIZING);
  if(ltr_int_tracker_init(ccb) != 0){
    ltr_int_log_message("Problem initializing tracker!\n");
    ltr_int_cal_set_state(err_NOT_INITIALIZED);
    return -1;
  }
  struct reflector_model_type rm;
  ltr_int_get_model_setup(&rm);
  frame.bloblist.blobs = ltr_int_my_malloc(sizeof(struct blob_type) * MAX_BLOBS);
  frame.bloblist.num_blobs = MAX_BLOBS;
  if((rm.type == SINGLE) || (rm.type == FACE)){
    frame.bloblist.expected_blobs = 1;
  }else{
    frame.bloblist.expected_blobs = 3;
  }

  frame.bitmap = NULL;

  ltr_int_cal_set_state(RUNNING);
  while(1){
    switch(ltr_int_cal_get_state()){
      case RUNNING:
        pthread_mutex_lock(&state_mx);
        my_request = ltr_int_get_state_request();
        pthread_mutex_unlock(&state_mx);
        switch(my_request){
          case PAUSE:
            ltr_int_cal_set_state(PAUSED);
            ltr_int_tracker_pause();
            break;
          case SHUTDOWN:
            stop_flag = true;
            break;
          default:
            frame_acquired = false;
            retval = ltr_int_tracker_get_frame(ccb, &frame, &frame_acquired);
            if(retval == -1){
              ltr_int_log_message("Error getting frame! (rv = %d)\n", retval);
              ltr_int_cal_set_state(err_PROCESSING_FRAME);
              stop_flag = true;
            }else{
              if(frame_acquired){
                clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
                frame.usec = tp.tv_sec * 1000000 + tp.tv_nsec / 1000;
                frame.counter = ++counter;
                frame.usec = ltr_int_get_ts();
                if((retval = cbk(ccb, &frame)) < 0){
                  ltr_int_log_message("Error processing frame! (rv = %d)\n", retval);
                  ltr_int_cal_set_state(err_PROCESSING_FRAME);
                  stop_flag = true;
                }
              }
            }
            break;
        }
        break;
      case PAUSED:
        pthread_mutex_lock(&state_mx);
        while(!change_flag){
          pthread_cond_wait(&state_cv, &state_mx);
        }
        my_request = ltr_int_get_state_request();
        change_flag = false;
        pthread_mutex_unlock(&state_mx);
        switch(my_request){
          case RUN:
            ltr_int_tracker_resume();
            ltr_int_cal_set_state(RUNNING);
            break;
          case SHUTDOWN:
            ltr_int_tracker_resume();
            stop_flag = true;
            break;
          default:
            break;
        }
        break;
      default:
        assert(0);
        break;
    }
    if(stop_flag == true){
      break;
    }
  }

  ltr_int_tracker_close();
  ltr_int_frame_free(ccb, &frame);
  ltr_int_cal_set_state(STOPPED);
  return 0;
}

static int signal_request()
{
  pthread_mutex_lock(&state_mx);
  change_flag = true;
  pthread_cond_broadcast(&state_cv);
  pthread_mutex_unlock(&state_mx);
  return 0;
}

int ltr_int_rl_shutdown()
{
  return signal_request();
}

int ltr_int_rl_suspend()
{
  return signal_request();
}

int ltr_int_rl_wakeup()
{
  return signal_request();
}

