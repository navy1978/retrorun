"""Actual presentation policy must yield panel-independent screenshot orientation."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'src/video/video-helper.cpp').read_text()
body=source[source.index('rr_rotation_t getBlitRotation()'):source.index('\nrr_rotation_t getRotation()')]
fixture='''#include "screenshot_capture.h"
#include <cassert>
enum {DISABLED,ENABLED,REVERSED};int tateState=DISABLED;
bool isGameVertical=false,native=false;
bool isTate(){return tateState!=DISABLED;}
bool hasDeviceRotatedScreen(){return native;}
'''
checks='''int main(){
 for(bool vertical:{false,true})for(bool panelNative:{false,true}){
  isGameVertical=vertical;native=panelNative;
#ifdef RR_PLATFORM_SDL
  const auto panel=RR_ROTATION_DEGREES_0;
  const auto tate=RR_ROTATION_DEGREES_90,reverse=RR_ROTATION_DEGREES_270;
#else
  const auto panel=native?RR_ROTATION_DEGREES_0:RR_ROTATION_DEGREES_270;
  const auto tate=RR_ROTATION_DEGREES_270,reverse=RR_ROTATION_DEGREES_90;
#endif
  tateState=DISABLED;assert(logicalCaptureRotation(getBlitRotation(),panel)==RR_ROTATION_DEGREES_0);
  tateState=ENABLED;assert(logicalCaptureRotation(getBlitRotation(),panel)==tate);
  tateState=REVERSED;assert(logicalCaptureRotation(getBlitRotation(),panel)==reverse);
 }
}'''
with tempfile.TemporaryDirectory(prefix='rr-screenshot-rotation-') as temp:
 for backend in ('go2','sdl'):
  cpp=Path(temp)/(backend+'.cpp');binary=Path(temp)/backend
  cpp.write_text(('#define RR_PLATFORM_SDL\n' if backend=='sdl' else '')+fixture+body+checks)
  subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-I'+str(root/'src/video'),'-I'+str(root/'src/platform'),str(cpp),'-o',str(binary)],check=True)
  subprocess.run([str(binary)],check=True)
print('Actual GO2/SDL screenshot policy: P/MP, portrait/landscape and Tate PASS')
