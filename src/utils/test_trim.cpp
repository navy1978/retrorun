#include <iostream>
#include <regex>



static std::string trim(std::string str)
{
    return regex_replace(str, std::regex("(^[ ]+)|([ ]+$)"), "");
}


int main() {

    char t[] = "      C++";
std::cout << t;
    std::cout << trim(t);
    return 0;
}

/**
 * TODO:
 * 
 * 




adding the save status in 3 differetn slot in the menu
- NON FUNZIONA NIENTE ALTRO DELLE ACTIONS
- IL NOEM DEL FILE CHE SI SALVA E SEMPRE LO STESSO






 *  // settging volumen: :
 amixer set 'Playback' 20%-    meno
 amixer set 'Playback' 20%+     piu

suonare un file per fare sentire il rumore:

aplay /usr/share/sounds/alsa/Noise.wav
oppure converti il file wav in c array :
https://www.youtube.com/watch?v=1cWkjzU5vg8
e poi lo suoni con la libreria che hai già ... in imput.cpp


 luminosita:

 cat /sys/class/backlight/backlight/brightness
 echo 30 > /sys/class/backlight/backlight/brightness


---info


~/retrorun # lscpu | egrep 'Model name|Socket|Thread|NUMA|CPU\(s\)'
CPU(s):              4
On-line CPU(s) list: 0-3
Model name:          Cortex-A35
Thread(s) per core:  1
Socket(s):       


---info 2

cat /proc/cpuinfo | grep Hardware


---- GPU

~ # cat /sys/devices/platform/ff400000.gpu/gpuinfo 
Mali-G31 1 cores r0p0 0x7093


governor:

/ # cat /sys/devices/platform/ff400000.gpu/devfreq/ff400000.gpu/cur_freq 
 *


find /sys/devices/platform/ -type d -name "*.gpu"

find /sys/devices/platform/ -maxdepth 2 -type d -name "*.gpu" -printf "%f\n"



https://github.com/AmberELEC/AmberELEC/blob/6a7a49240dc953fa7861b026bced95511a95abe5/packages/amberelec/profile.d/99-distribution.conf


*/
