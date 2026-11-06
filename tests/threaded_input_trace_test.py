#!/usr/bin/env python3
"""Check real RG351MP aggregate traces; no ROM/core assets are required here.

Usage: threaded_input_trace_test.py control-True.log control-False.log \
    fixed-True.log menu-True.log fixed-False.log menu-False.log
Control must reproduce the pre-fix callback schedule (no fallback/early poll).
"""
import pathlib,re,sys

def read(path):
    text=pathlib.Path(path).read_text(errors='replace')
    rows=[]
    for line in text.splitlines():
        if 'INPUT_TRACE dt=' not in line:continue
        rows.append({key:(int(count),tid) for key,count,tid in
                     re.findall(r'(\w+)=(\d+)@([0-9a-f]+)',line)})
    assert rows, f'No trace rows: {path}'
    return text,rows

def active(rows):
    return [row for row in rows if row.get('run_enter',(0,''))[0]>50]

if __name__=='__main__':
    if len(sys.argv)!=7: raise SystemExit(__doc__)
    before_t,before_f,after_t,menu_t,after_f,menu_f=map(read,sys.argv[1:])
    assert any(r['update'][0]==0 and r['poll'][0]==0 and r['state'][0]>100
               and r['state'][1]!=r['loop'][1] for r in active(before_t[1]))
    assert any(r['poll'][0]>0 and r['update'][0]>0
               and r['state'][1]==r['loop'][1] for r in active(before_f[1]))
    assert any(r['poll'][0]==0 and r['update'][0]>=r['run_enter'][0]
               and r['state'][1]!=r['loop'][1] for r in active(after_t[1]))
    for text,rows in (after_t,after_f):
        assert any(r['evdev'][0]>0 and r['evdev_publish'][0]>0
                   and r['nonzero_state'][0]>0 for r in rows)
        assert 'Shutdown: core library closed' in text
    for text,rows in (menu_t,menu_f):
        assert text.count('Input: Info requested OK')>=2
        assert 'Shutdown: core library closed' in text
    print('Threaded/non-threaded real-input callback, menu and shutdown traces PASS')
