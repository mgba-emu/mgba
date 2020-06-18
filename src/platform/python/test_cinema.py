import pytest
import cinema.test
import mgba.log
import os.path
import yaml

mgba.log.install_default(mgba.log.NullLogger())

def flatten(d):
    l = []
    for k, v in d.tests.items():
        if v.tests:
            l.extend(flatten(v))
        else:
            l.append(v)
    l.sort()
    return l

def pytest_generate_tests(metafunc):
    if 'vtest' in metafunc.fixturenames:
        tests = cinema.test.gather_tests(os.path.join(os.path.dirname(__file__), '..', '..', '..', 'cinema'))
        testList = flatten(tests)
        params = []
        for test in testList:
            marks = []
            xfail = test.settings.get('fail')
            if xfail and bool(xfail):
                marks = pytest.mark.xfail(reason=xfail if isinstance(xfail, str) else None)
            params.append(pytest.param(test, id=test.name, marks=marks))
        metafunc.parametrize('vtest', params, indirect=True)

@pytest.fixture
def vtest(request):
    return request.param

def test_video(vtest, pytestconfig):
    vtest.setup()
    if pytestconfig.getoption('--rebaseline'):
        vtest.generate_baseline()
    else:
        try:
            vtest.test()
        except IOError:
            raise
        if pytestconfig.getoption('--mark-succeeding') and 'fail' in vtest.settings:
            # TODO: This can fail if an entire directory is marked as failing
            settings = {}
            try:
                with open(os.path.join(vtest.path, 'manifest.yml'), 'r') as f:
                    settings = yaml.safe_load(f)
            except IOError:
                pass
            if 'fail' in settings:
                del settings['fail']
            else:
                settings['fail'] = False
            if settings:
                with open(os.path.join(vtest.path, 'manifest.yml'), 'w') as f:
                    yaml.dump(settings, f, default_flow_style=False)
            else:
                os.remove(os.path.join(vtest.path, 'manifest.yml'))
