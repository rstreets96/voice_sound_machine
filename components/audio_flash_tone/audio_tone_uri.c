/*This is tone file*/

const char* tone_uri[] = {
   "flash://tone/0_dingdong.mp3",
   "flash://tone/1_haode.mp3",
   "flash://tone/2_rain.mp3",
   "flash://tone/3_timer.mp3",
};

int get_tone_uri_num()
{
    return sizeof(tone_uri) / sizeof(char *) - 1;
}
