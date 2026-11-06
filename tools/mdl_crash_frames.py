#!/usr/bin/env python3
"""Compare all three crash MDLs during real crashes in disposable games."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import os
from pathlib import Path
import re
import subprocess

from gen_mdl_crash import CRASH_NAMES

FLIGHTS=('ANTILLES','PARIS','EXTRARIO')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('bin/mdl-crash-frames'))
    parser.add_argument('--manifest',type=Path,default=Path('bin/mdl-crash/modules.json'))
    parser.add_argument('--jobs',type=int,default=3)
    parser.add_argument('--passes',type=int,default=1200)
    args=parser.parse_args()
    if args.passes<1200 or not 1<=args.jobs<=3:
        parser.error('requires at least 1200 passes and 1..3 jobs')
    display=os.environ.get('MDL_GAME_DISPLAY',':86')
    if not re.fullmatch(r':\d+',display): parser.error('display base must be :number')
    records=json.loads(args.manifest.read_text())
    if tuple(Path(r['installed']).stem for r in records)!=CRASH_NAMES:
        raise ValueError('expected the three reference crash models')
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    (output/'summary.txt').write_text('')
    subprocess.run(['python3','-B','tools/fly_scripts.py','bin/mdl-diff/scripts'],check=True)

    def run_flight(index,flight,record):
        env=dict(os.environ,HG2D_FRAMES_MDLS='1',HG2D_FRAMES_NOSOUND='1',
                 HG2D_FRAMES_CRASH_TEST='1',HG2D_FRAMES_OUT=str(output/flight),
                 HG2D_FRAMES_EXPECT_MDL=Path(record['installed']).name,
                 HG2D_FRAMES_MDL_MANIFEST=str(args.manifest.resolve()),
                 HG2D_FRAMES_DISPLAY=':'+str(int(display[1:])+index))
        env.pop('HG2D_FRAMES_CRASH_ALTITUDE',None)
        env.pop('HG2D_FRAMES_CRASH_COCKPIT_ONLY',None)
        if flight=='EXTRARIO':
            env.update(HG2D_FRAMES_CRASH_ALTITUDE='100',HG2D_FRAMES_CRASH_COCKPIT_ONLY='1')
        result=subprocess.run(['tools/hg2d_frames.sh',f'Situatio\\{flight}.STN',
                               str(args.passes),'bin/mdl-diff/scripts/crash_models.txt'],
                              env=env,text=True,capture_output=True)
        transcript=result.stdout+result.stderr
        (output/(flight+'.result.txt')).write_text(transcript)
        if result.returncode: raise RuntimeError(f'{flight}: comparison failed\n{transcript}')
        return f"{flight}: {Path(record['installed']).name}, {args.passes} passes, intended crash model loaded on all sides, identical frames"

    summaries=[]
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures=[pool.submit(run_flight,i,flight,record)
                 for i,(flight,record) in enumerate(zip(FLIGHTS,records))]
        for future in as_completed(futures):
            summary=future.result();summaries.append(summary)
            (output/'summary.txt').write_text('\n'.join(sorted(summaries))+'\n')
            print(summary,flush=True)


if __name__=='__main__': main()
