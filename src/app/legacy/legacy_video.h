#ifndef LEGACY_VIDEO_H
#define LEGACY_VIDEO_H

#include "legacy_rm.h"

void legacy_video_init(void);
void legacy_int10_service(struct rm_int13_frame* f);

#endif
