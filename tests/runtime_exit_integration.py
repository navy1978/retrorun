"""Trusted-core SDL integration using original ROM bytes, no game/BIOS assets.

Usage: runtime_exit_integration.py RUNTIME STELLA_CORE [ABI_FIXTURE_LIBRARY [NO_INPUT_POLL_CORE]]
RR_TEST_LOADER / RR_TEST_LIBRARY_PATH optionally select the target SDK loader.
Requires SDL dummy video and ALSA null support, or equivalent host drivers.
"""
from pathlib import Path
import os,json,subprocess,sys,tempfile,zipfile
runtime,core=map(lambda value:str(Path(value).resolve()),sys.argv[1:3]);abi=sys.argv[3] if len(sys.argv)>3 else None
prefix=[os.environ['RR_TEST_LOADER'],'--library-path',os.environ['RR_TEST_LIBRARY_PATH']] if os.environ.get('RR_TEST_LOADER') else []
with tempfile.TemporaryDirectory(prefix='rr-exit-integration-') as temporary:
 root=Path(temporary);(root/'saves').mkdir();cfg=root/'config.cfg';cfg.write_text('retrorun_video_renderer = software\nretrorun_auto_save = false\nretrorun_auto_load = false\nretrorun_log_level = ERROR\n')
 (root/'alsa.conf').write_text('pcm.!default { type null }\n')
 env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='alsa',ALSA_CONFIG_PATH=str(root/'alsa.conf'))
 data=bytearray([0xea]*4096);data[:5]=bytes([0x78,0xd8,0x4c,2,240]);data[-6:]=bytes([0,240]*3)
 rom=root/'original.a26';rom.write_bytes(data);empty=root/'empty.a26';empty.touch();bad=root/'bad.zip';bad.write_bytes(b'not a ZIP');rejected=root/'original.unsupported';rejected.write_bytes(data)
 def run(code,selected=core,content=rom,extra=(),environment=env,arguments=None):
  args=arguments if arguments is not None else ['-c',str(cfg),'-s',str(root/'saves'),'-d',str(root),*extra,selected,str(content)]
  result=subprocess.run([*prefix,runtime,*args],env=environment,capture_output=True,text=True,timeout=12)
  assert result.returncode==code,(code,result.returncode,result.stdout[-1200:],result.stderr[-1200:]);return result
 run(64,arguments=[]);run(65,selected=str(root/'missing.so'))
 if abi:run(66,selected=abi)
 run(67,content=root/'missing.a26');run(67,content=empty);run(68,content=bad);run(69,content=rejected)
 run(71,environment=dict(env,SDL_AUDIODRIVER='rr-invalid-driver'))
 run(72,extra=['--resume-slot','2'])
 if len(sys.argv)>4:run(0,selected=str(Path(sys.argv[4]).resolve()))
 success=run(0,extra=['--benchmark','1','--benchmark-warmup','0']);record=next(line.split(':',1)[1] for line in success.stdout.splitlines() if line.startswith('BENCHMARK_JSON:'));frames=json.loads(record)['counters']['core_frames'];assert frames>0
 print('Actual runtime exit codes 64/65/'+('66/' if abi else '')+'67/68/69/71/72 verified; original Atari ROM completed '+str(frames)+' frames and clean exit')
