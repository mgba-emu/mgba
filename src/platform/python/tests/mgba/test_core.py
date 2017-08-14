import pytest

def test_core_import():
    try:
        import mgba.core
    except:
        raise AssertionError
