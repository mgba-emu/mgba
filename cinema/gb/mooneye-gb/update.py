#!/usr/bin/env python
import os
import os.path
import shutil
import yaml
from cinema.util import dict_merge

suffixes = {
    'C': 'CGB',
    'S': 'SGB',
    'A': 'AGB',
    'mgb': 'MGB',
    'sgb': 'SGB',
    'sgb2': 'SGB2',
    'cgb': 'CGB',
    'agb': 'AGB',
    'ags': 'AGB',
}

def ingestDirectory(path, dest):
    for root, _, files in os.walk(path, topdown=False):
        root = root[len(os.path.commonprefix([root, path])):]
        if root.startswith('utils'):
            continue
        for file in files:
            fname, ext = os.path.splitext(file)
            if ext not in ('.gb', '.sym'):
                continue

            try:
                os.makedirs(os.path.join(dest, root, fname))
            except OSError:
                pass

            if ext in ('.gb', '.sym'):
                shutil.copy(os.path.join(path, root, file), os.path.join(dest, root, fname, 'test' + ext))

            for suffix, model in suffixes.items():
                if fname.endswith('-' + suffix):
                    manifest = {}
                    try:
                        with open(os.path.join(dest, root, fname, 'manifest.yml'), 'r') as f:
                            manifest = yaml.safe_load(f) or {}
                    except IOError:
                        pass
                    dict_merge(manifest, {
                        'config': {
                            'gb.model': model
                        }
                    })
                    with open(os.path.join(dest, root, fname, 'manifest.yml'), 'w') as f:
                        yaml.dump(manifest, f)

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description='Update mooneye-gb test suite')
    parser.add_argument('source', type=str, help='directory containing built tests')
    parser.add_argument('dest', type=str, nargs='?', default=os.path.dirname(__file__), help='directory to contain ingested tests')
    args = parser.parse_args()

    ingestDirectory(args.source, args.dest)
