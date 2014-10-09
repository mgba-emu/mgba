#!/usr/bin/env python
from __future__ import print_function
import argparse
import csv
import os
import signal
import subprocess
import sys
import time

class PerfTest(object):
    EXECUTABLE = 'mgba-perf'

    def __init__(self, rom, renderer='software'):
        self.rom = rom
        self.renderer = renderer
        self.results = None
        self.name = 'Perf Test: {}'.format(rom)

    def get_args(self):
        return []

    def wait(self, proc):
        pass

    def run(self, cwd):
        args = [os.path.join(os.getcwd(), self.EXECUTABLE), '-P']
        args.extend(self.get_args())
        if self.renderer != 'software':
            args.append('-N')
        args.append(self.rom)
        env = {}
        if 'LD_LIBRARY_PATH' in os.environ:
            env['LD_LIBRARY_PATH'] = os.environ['LD_LIBRARY_PATH']
            env['DYLD_LIBRARY_PATH'] = os.environ['LD_LIBRARY_PATH'] # Fake it on OS X
        proc = subprocess.Popen(args, stdout=subprocess.PIPE, cwd=cwd, universal_newlines=True, env=env)
        try:
            self.wait(proc)
            proc.wait()
        except:
            proc.kill()
            raise
        if proc.returncode < 0:
            return
        reader = csv.DictReader(proc.stdout)
        self.results = next(reader)

class WallClockTest(PerfTest):
    def __init__(self, rom, duration, renderer='software'):
        super(WallClockTest, self).__init__(rom, renderer)
        self.duration = duration
        self.name = 'Wall-Clock Test ({} seconds, {} renderer): {}'.format(duration, renderer, rom)

    def wait(self, proc):
        time.sleep(self.duration)
        proc.send_signal(signal.SIGINT)

class GameClockTest(PerfTest):
    def __init__(self, rom, frames, renderer='software'):
        super(GameClockTest, self).__init__(rom, renderer)
        self.frames = frames
        self.name = 'Game-Clock Test ({} frames, {} renderer): {}'.format(frames, renderer, rom)

    def get_args(self):
        return ['-F', str(self.frames)]

class Suite(object):
    def __init__(self, cwd, wall=None, game=None):
        self.cwd = cwd
        self.tests = []
        self.wall = wall
        self.game = game

    def collect_tests(self):
        roms = []
        for f in os.listdir(self.cwd):
            if f.endswith('.gba'):
                roms.append(f)
        roms.sort()
        for rom in roms:
            self.add_tests(rom)

    def add_tests(self, rom):
        if self.wall:
            self.tests.append(WallClockTest(rom, self.wall))
            self.tests.append(WallClockTest(rom, self.wall, renderer=None))
        if self.game:
            self.tests.append(GameClockTest(rom, self.game))
            self.tests.append(GameClockTest(rom, self.game, renderer=None))

    def run(self):
        results = []
        for test in self.tests:
            print('Running test {}'.format(test.name), file=sys.stderr)
            try:
                test.run(self.cwd)
            except KeyboardInterrupt:
                print('Interrupted, returning early...', file=sys.stderr)
                return results
            if test.results:
                results.append(test.results)
        return results

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-w', '--wall-time', type=float, default=120, metavar='TIME', help='wall-clock time')
    parser.add_argument('-g', '--game-frames', type=int, default=120*60, metavar='FRAMES', help='game-clock frames')
    parser.add_argument('-o', '--out', metavar='FILE', help='output file path')
    parser.add_argument('directory', help='directory containing ROM files')
    args = parser.parse_args()

    s = Suite(args.directory, wall=args.wall_time, game=args.game_frames)
    s.collect_tests()
    results = s.run()
    fout = sys.stdout
    if args.out:
        fout = open(args.out, 'w')
    writer = csv.DictWriter(fout, results[0].keys())
    writer.writeheader()
    writer.writerows(results)
    if fout is not sys.stdout:
        fout.close()
