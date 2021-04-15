import errno
import itertools
import os
import os.path
import pytest
from configparser import ConfigParser

def pytest_addoption(parser):
    parser.addoption("--rebaseline", action="store_true", help="output a new baseline instead of testing")
    parser.addoption("--mark-failing", action="store_true", help="mark all failing tests as failing")
    parser.addoption("--mark-succeeding", action="store_true", help="unmark all succeeding tests marked as failing")
    parser.addoption("--output-diff", help="output diffs for failed tests to directory")

EXPECTED = 'expected_%04u.png'
RESULT = 'result_%04u.png'
DIFF = 'diff_%04u.png'
DIFF_NORM = 'diff_norm_%04u.png'

def pytest_exception_interact(node, call, report):
    outroot = node.config.getoption("--output-diff")
    if report.failed and hasattr(node, 'funcargs'):
        vtest = node.funcargs.get('vtest')
        if outroot:
            if not vtest:
                return
            outdir = os.path.join(outroot, *vtest.full_path)
            try:
                os.makedirs(outdir)
            except OSError as e:
                if e.errno == errno.EEXIST and os.path.isdir(outdir):
                    pass
                else:
                    raise
            for i, expected, result, diff, diffNorm in zip(itertools.count(), vtest.baseline, vtest.frames, *zip(*vtest.diffs)):
                result.save(os.path.join(outdir, RESULT % i))
                if expected:
                    expected.save(os.path.join(outdir, EXPECTED % i))
                    diff.save(os.path.join(outdir, DIFF % i))
                    diffNorm.save(os.path.join(outdir, DIFF_NORM % i))

        if node.config.getoption("--mark-failing"):
            settings = ConfigParser()
            try:
                with open(os.path.join(vtest.path, 'config.ini'), 'r') as f:
                    settings.read_file(f)
            except IOError:
                pass
            settings.set('testinfo', 'fail', '1')
            with open(os.path.join(vtest.path, 'config.ini'), 'w') as f:
                settings.write(f)
