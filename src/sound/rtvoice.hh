// -*- mode: c++; -*-

#ifndef FREESPACE2_SOUND_RTVOICE_HH
#define FREESPACE2_SOUND_RTVOICE_HH

#include "defs.hh"

#include "sound/sound.hh"

// general
void rtvoice_set_qos (int qos);

// recording
int rtvoice_init_recording (int qos);
void rtvoice_close_recording ();
int rtvoice_start_recording (
    void (*user_callback) () = NULL, int callback_time = 175);
void rtvoice_stop_recording ();
void rtvoice_get_data (unsigned char** outbuf, int* size, double* gain);

// playback
int rtvoice_init_playback ();
void rtvoice_close_playback ();

int rtvoice_create_playback_buffer ();
void rtvoice_free_playback_buffer (int index);

void rtvoice_uncompress (
    unsigned char* data_in, int size_in, double gain, unsigned char* data_out,
    int size_out);

// return a sound handle, _NOT_ a buffer handle
sound_handle rtvoice_play (int handle, unsigned char* data, int size);

// pass in buffer handle returned from rtvoice_create_playback_buffer(), kills
// the _sound_ only
void rtvoice_stop_playback (int handle);
void rtvoice_stop_playback_all ();

#endif // FREESPACE2_SOUND_RTVOICE_HH
