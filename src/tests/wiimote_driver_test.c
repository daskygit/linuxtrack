#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "cal.h"
#include "wiimote_driver.h"

int main(int argc, char **argv) {
  struct frame_type frame;
  struct camera_control_block ccb;

  ccb.device.category = wiimote;
/*  ccb.mode = diagnostic; */
/*   ccb.mode = operational_1dot; */
  ccb.mode = operational_3dot; 

  cal_init(&ccb);

/* loop forever reading and printing results */
/*   while (true) { */
/*     cal_get_frame(&ccb, &frame); */
/*     frame_print(frame); */
/*     frame_free(&ccb, &frame); */
/*   } */
  
  int i;
  for(i=0; i<500; i++) {
    cal_get_frame(&ccb, &frame);
    frame_print(frame);
    frame_free(&ccb, &frame);
    ltr_int_usleep(10000);
  }

  /* call disconnects, turns all LEDs off */
  cal_shutdown(&ccb);
  return 0;
}
