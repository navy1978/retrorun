"""Actual SDL runtime startup ordering; original slow-load/no-input-poll core only."""
from pathlib import Path
import os,subprocess,sys,tempfile
runtime,core=[str(Path(p).resolve()) for p in sys.argv[1:3]]
prefix=[os.environ['RR_TEST_LOADER'],'--library-path',os.environ['RR_TEST_LIBRARY_PATH']] if os.environ.get('RR_TEST_LOADER') else []
with tempfile.TemporaryDirectory(prefix='rr-startup-loading-') as folder:
 root=Path(folder)
 for managed in [False,True]:
  case=root/str(managed);case.mkdir();(case/'saves').mkdir();rom=case/'own.bin';rom.write_bytes(b'Original diagnostic only')
  cfg=case/'runtime.cfg';cfg.write_text('retrorun_video_renderer = software\nretrorun_auto_save = false\nretrorun_auto_load = false\nretrorun_log_level = DEBUG\nretrorun_core_log_level = INFO\nretrorun_log_to_file = true\n'+('retrorun_profile_mode = managed\nretrorun_profile_base = '+str(cfg)+'\n' if managed else ''))
  alsa=case/'alsa.conf';alsa.write_text('pcm.!default { type null }\n')
  env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='alsa',ALSA_CONFIG_PATH=str(alsa))
  result=subprocess.run([*prefix,runtime,'-c',str(cfg),'-s',str(case/'saves'),'-d',str(case),core,str(rom)],env=env,capture_output=True,text=True,timeout=10)
  assert result.returncode==0,(managed,result.returncode,result.stderr[-1200:])
  log=(case/'retrorun.log').read_text();marker='Original fixture: content load entered';early='Startup loading: shown before content load'
  assert marker in log
  if managed:assert early in log and log.index(early)<log.index(marker)
  else:assert early not in log
 print('Managed startup precedes slow content loading; legacy unchanged; no-poll quit clean PASS')
