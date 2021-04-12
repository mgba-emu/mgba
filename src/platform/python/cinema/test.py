import os
import os.path
import mgba.core
import mgba.image
import cinema.movie
import glob
import re
from copy import deepcopy
from cinema import VideoFrame
from cinema.util import dict_merge
from configparser import ConfigParser


class CinemaTest(object):
    TEST = 'test.(mvl|gb|gba|nds)'

    def __init__(self, path, root, settings={}):
        self.full_path = path or []
        self.path = os.path.abspath(os.path.join(root, *self.full_path))
        self.root = root
        self.name = '.'.join(path)
        self.settings = settings
        try:
            with open(os.path.join(self.path, 'config.ini'), 'r') as f:
                cfg = ConfigParser()
                cfg.read_file(f)
                settings = {}
                if 'testinfo' in cfg:
                    settings = dict(cfg['testinfo'])
                if 'ports.cinema' in cfg:
                    settings['config'] = dict(cfg['ports.cinema'])
                dict_merge(self.settings, settings)
        except IOError:
            pass
        self.tests = {}

    def __repr__(self):
        return '<%s %s>' % (self.__class__.__name__, self.name)

    def setup(self):
        results = [f for f in glob.glob(os.path.join(self.path, 'test.*')) if re.search(self.TEST, f)]
        self.core = mgba.core.load_path(results[0])
        if 'config' in self.settings:
            self.config = mgba.core.Config(defaults=self.settings['config'])
            self.core.load_config(self.config)
            self.core.reset()

    def add_test(self, name, cls=None, settings={}):
        cls = cls or self.__class__
        new_settings = deepcopy(self.settings)
        dict_merge(new_settings, settings)
        self.tests[name] = cls(self.full_path + [name], self.root, new_settings)
        return self.tests[name]

    def output_settings(self):
        output_settings = {}
        if 'frames' in self.settings:
            output_settings['limit'] = int(self.settings['frames'])
        if 'skip' in self.settings:
            output_settings['skip'] = int(self.settings['skip'])
        return output_settings

    def __lt__(self, other):
        return self.path < other.path


class VideoTest(CinemaTest):
    BASELINE = 'baseline_%04u.png'

    def setup(self):
        super(VideoTest, self).setup()
        self.tracer = cinema.movie.Tracer(self.core)

    def generate_frames(self):
        for i, frame in enumerate(self.tracer.video(**self.output_settings())):
            try:
                baseline = VideoFrame.load(os.path.join(self.path, self.BASELINE % i))
                yield baseline, frame, VideoFrame.diff(baseline, frame)
            except IOError:
                yield None, frame, (None, None)

    def test(self):
        self.baseline, self.frames, self.diffs = zip(*self.generate_frames())
        assert not any(any(diffs[0].image.convert("L").point(bool).getdata()) for diffs in self.diffs)

    def generate_baseline(self):
        for i, frame in enumerate(self.tracer.video(**self.output_settings())):
            frame.save(os.path.join(self.path, self.BASELINE % i))


def gather_tests(root=os.getcwd()):
    tests = CinemaTest([], root)
    for path, _, files in os.walk(root):
        test = [f for f in files if re.match(CinemaTest.TEST, f)]
        if not test:
            continue
        prefix = os.path.commonprefix([path, root])
        suffix = path[len(prefix)+1:]
        test_path = suffix.split(os.sep)
        test_root = tests
        for component in test_path[:-1]:
            new_test = test_root.tests.get(component)
            if not new_test:
                new_test = test_root.add_test(component)
            test_root = new_test
        test_root.add_test(test_path[-1], VideoTest)
    return tests
