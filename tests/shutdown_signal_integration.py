"""Send real termination signals to a real SDL runtime with original SRAM fixture."""
from pathlib import Path
import os, signal, subprocess, sys, tempfile, time
runtime,core=[str(Path(p).resolve()) for p in sys.argv[1:3]]
prefix=[os.environ['RR_TEST_LOADER'],'--library-path',os.environ['RR_TEST_LIBRARY_PATH']] if os.environ.get('RR_TEST_LOADER') else []
with tempfile.TemporaryDirectory(prefix='rr-shutdown-') as temporary:
 root=Path(temporary)
 for sig in (signal.SIGTERM,signal.SIGINT):
  case=root/str(sig);case.mkdir();(case/'saves').mkdir()
  rom=case/'own.bin';rom.write_bytes(b'Original shutdown fixture')
  cfg=case/'config.cfg';cfg.write_text('retrorun_video_renderer = software\nretrorun_auto_save = false\nretrorun_auto_load = false\nretrorun_log_level = DEBUG\nretrorun_core_log_level = INFO\nretrorun_log_to_file = true\n')
  alsa=case/'alsa.conf';alsa.write_text('pcm.!default { type null }\n')
  env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='alsa',ALSA_CONFIG_PATH=str(alsa))
  with (case/'console.log').open('w') as output:
   process=subprocess.Popen([*prefix,runtime,'-c',str(cfg),'-s',str(case/'saves'),'-d',str(case),core,str(rom)],env=env,stdout=output,stderr=output)
   try:
    deadline=time.monotonic()+15
    log=case/'retrorun.log'
    while time.monotonic()<deadline:
     if log.exists() and 'Shutdown fixture ready' in log.read_text():break
     assert process.poll() is None,(process.returncode,(case/'console.log').read_text()[-1500:])
     time.sleep(.05)
    else:raise AssertionError('fixture did not reach gameplay')
    process.send_signal(sig)
    assert process.wait(timeout=15)==0,(sig,(case/'console.log').read_text()[-1500:])
    saves=list((case/'saves').rglob('*.srm'))
    assert len(saves)==1,(sig,list((case/'saves').rglob('*')))
    assert saves[0].read_bytes()==b'RR-SHUTDOWN-SRAM'
    assert 'Shutdown: core unloaded and deinitialized' in log.read_text()
   finally:
    if process.poll() is None:process.kill();process.wait()
 print('SIGTERM/SIGINT: normal runtime cleanup and exact SRAM persistence PASS')
