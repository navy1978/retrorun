"""Compile the actual getRotation body with hardware-independent input stubs."""
from pathlib import Path
import os, subprocess, tempfile

root = Path(__file__).resolve().parents[1]
source = (root/'src/video/video-helper.cpp').read_text()
start = source.index('rr_rotation_t getRotation()')
end = source.index('\nint getFixedWidth', start)
body = source[start:end]
fixture = r'''
#include <cassert>
#include <string>
enum rr_rotation_t {RR_ROTATION_DEGREES_0, RR_ROTATION_DEGREES_90,
                    RR_ROTATION_DEGREES_180, RR_ROTATION_DEGREES_270};
enum {DISABLED, ENABLED, REVERSED};
int tateState=DISABLED; bool isGameVertical=false, native=true;
std::string coreName;
bool isTate(){return tateState!=DISABLED;}
bool hasDeviceRotatedScreen(){return native;}
'''
checks = r'''
int main(){
 coreName="MAME 2003-Plus"; isGameVertical=true;
 assert(getRotation()==RR_ROTATION_DEGREES_0); // 1941: core already rotates.
 isGameVertical=false;
 assert(getRotation()==RR_ROTATION_DEGREES_0); // SF2 must remain unchanged.
 isGameVertical=true;native=false;
 assert(getRotation()==RR_ROTATION_DEGREES_270); // Existing panel transform.
 native=true;tateState=ENABLED;
 assert(getRotation()==RR_ROTATION_DEGREES_180);
 tateState=REVERSED;assert(getRotation()==RR_ROTATION_DEGREES_0);
 tateState=DISABLED;coreName="FinalBurn Neo";
 assert(getRotation()==RR_ROTATION_DEGREES_270); // Unrelated cores unchanged.
}
'''
with tempfile.TemporaryDirectory(prefix='rr-portrait-rotation-') as temporary:
    cpp=Path(temporary)/'rotation.cpp';binary=Path(temporary)/'rotation'
    cpp.write_text(fixture+body+checks)
    subprocess.run([os.environ.get('CXX','c++'),'-std=c++17',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print('Actual GO2 rotation: MAME portrait/landscape, panel transforms, Tate and unrelated cores passed')
