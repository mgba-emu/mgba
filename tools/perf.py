#!/usr/bin/env python
from __future__ import print_function
import argparse
import csv
import os
import shlex
import signal
import socket
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
        if not self.renderer:
            args.append('-N')
        elif self.renderer == 'threaded-software':
            args.append('-T')
        args.append(self.rom)
        env = {}
        if 'LD_LIBRARY_PATH' in os.environ:
            env['LD_LIBRARY_PATH'] = os.path.abspath(os.environ['LD_LIBRARY_PATH'])
            env['DYLD_LIBRARY_PATH'] = env['LD_LIBRARY_PATH'] # Fake it on OS X
        proc = subprocess.Popen(args, stdout=subprocess.PIPE, cwd=cwd, universal_newlines=True, env=env)
        try:
            self.wait(proc)
            proc.wait()
        except:
            proc.kill()
            raise
        if proc.returncode:
            print('Game crashed!', file=sys.stderr)
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

class PerfServer(object):
    ITERATIONS_PER_INSTANCE = 50
    RETRIES = 4

    def __init__(self, address, root='/', command=None):
        s = address.rsplit(':', 1)
        if len(s) == 1:
            self.address = (s[0], 7216)
        else:
            self.address = (s[0], s[1])
        self.command = None
        if command:
            self.command = shlex.split(command)
        self.iterations = self.ITERATIONS_PER_INSTANCE
        self.socket = None
        self.results = []
        self.reader = None
        self.root = root

    def _start(self, test):
        if self.command:
            server_command = list(self.command)
        else:
            server_command = [os.path.join(os.getcwd(), PerfTest.EXECUTABLE)]
        server_command.extend(['-PD'])
        if hasattr(test, "frames"):
            server_command.extend(['-F', str(test.frames)])
        if not test.renderer:
            server_command.append('-N')
        elif test.renderer == 'threaded-software':
            server_command.append('-T')
        for backoff in range(self.RETRIES):
            try:
                subprocess.check_call(server_command)
                break
            except subprocess.CalledProcessError as e:
                print("Failed to start server:", e, file=sys.stderr)
                if backoff == self.RETRIES - 1:
                    raise
                time.sleep(2 ** backoff)
        time.sleep(3)
        for backoff in range(self.RETRIES):
            try:
                self.socket = socket.create_connection(self.address, timeout=1000)
                break
            except OSError as e:
                print("Failed to connect:", e, file=sys.stderr)
                if backoff == self.RETRIES - 1:
                    raise
                time.sleep(2 ** backoff)
        kwargs = {}
        if sys.version_info[0] >= 3:
            kwargs["encoding"] = "utf-8"
        self.reader = csv.DictReader(self.socket.makefile(**kwargs))

    def run(self, test):
        if not self.socket:
            self._start(test)
        self.socket.send(os.path.join(self.root, test.rom).encode("utf-8"))
        self.results.append(next(self.reader))
        self.iterations -= 1
        if self.iterations == 0:
            self.finish()
            self.iterations = self.ITERATIONS_PER_INSTANCE

    def finish(self):
        self.socket.send(b"\n");
        self.reader = None
        self.socket.close()
        time.sleep(5)
        self.socket = None

class Suite(object):
    def __init__(self, cwd, wall=None, game=None, renderer='software'):
        self.cwd = cwd
        self.tests = []
        self.wall = wall
        self.game = game
        self.renderer = renderer
        self.server = None

    def set_server(self, server):
        self.server = server

    def collect_tests(self):
        roms = []
        for f in os.listdir(self.cwd):
            if f.endswith('.gba') or f.endswith('.zip') or f.endswith('.gbc') or f.endswith('.gb'):
                roms.append(f)
        roms.sort()
        for rom in roms:
            self.add_tests(rom)

    def add_tests(self, rom):
        if self.wall:
            self.tests.append(WallClockTest(rom, self.wall, renderer=self.renderer))
        if self.game:
            self.tests.append(GameClockTest(rom, self.game, renderer=self.renderer))

    def run(self):
        results = []
        sock = None
        for test in self.tests:
            print('Running test {}'.format(test.name), file=sys.stderr)
            last_result = None
            if self.server:
                self.server.run(test)
                last_result = self.server.results[-1]
            else:
                try:
                    test.run(self.cwd)
                except KeyboardInterrupt:
                    print('Interrupted, returning early...', file=sys.stderr)
                    return results
                if test.results:
                    results.append(test.results)
                    last_result = results[-1]
            if last_result:
                print('{:.2f} fps'.format(int(last_result['frames']) * 1000000 / float(last_result['duration'])), file=sys.stderr)
        if self.server:
            self.server.finish()
            results.extend(self.server.results)
        return results

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-w', '--wall-time', type=float, default=0, metavar='TIME', help='wall-clock time')
    parser.add_argument('-g', '--game-frames', type=int, default=0, metavar='FRAMES', help='game-clock frames')
    parser.add_argument('-N', '--disable-renderer', action='store_const', const=True, help='disable video rendering')
    parser.add_argument('-T', '--threaded-renderer', action='store_const', const=True, help='threaded video rendering')
    parser.add_argument('-s', '--server', metavar='ADDRESS', help='run on server')
    parser.add_argument('-S', '--server-command', metavar='COMMAND', help='command to launch server')
    parser.add_argument('-o', '--out', metavar='FILE', help='output file path')
    parser.add_argument('-r', '--root', metavar='PATH', type=str, default='/perfroms', help='root path for server mode')
    parser.add_argument('directory', help='directory containing ROM files')
    args = parser.parse_args()

    renderer = 'software'
    if args.disable_renderer:
        renderer = None
    elif args.threaded_renderer:
        renderer = 'threaded-software'
    s = Suite(args.directory, wall=args.wall_time, game=args.game_frames, renderer=renderer)
    if args.server:
        if args.server_command:
            server = PerfServer(args.server, args.root, args.server_command)
        else:
            server = PerfServer(args.server, args.root)
        s.set_server(server)
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
