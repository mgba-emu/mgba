from setuptools import setup
import re
import os
import os.path
import sys
import subprocess


def get_version_component(piece):
    return subprocess.check_output(['cmake', '-DPRINT_STRING={}'.format(piece), '-P', '../../../version.cmake']).decode('utf-8').strip()


version = '{}.{}.{}'.format(*(get_version_component(p) for p in ('LIB_VERSION_MAJOR', 'LIB_VERSION_MINOR', 'LIB_VERSION_PATCH')))
if not get_version_component('GIT_TAG'):
    version += '.{}+g{}'.format(*(get_version_component(p) for p in ('GIT_REV', 'GIT_COMMIT_SHORT')))

setup(
    name="mgba",
    version=version,
    author="Jeffrey Pfau",
    author_email="jeffrey@endrift.com",
    url="http://github.com/mgba-emu/mgba/",
    packages=["mgba"],
    setup_requires=['cffi>=1.6', 'pytest-runner'],
    install_requires=['cffi>=1.6', 'cached-property'],
    extras_require={'pil': ['Pillow>=2.3'], 'cinema': ['pytest']},
    tests_require=['pytest'],
    cffi_modules=["_builder.py:ffi"],
    license="MPL 2.0",
    classifiers=[
        "Programming Language :: C",
        "Programming Language :: Python :: 2",
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)",
        "Topic :: Games/Entertainment",
        "Topic :: System :: Emulators"
    ]
)
