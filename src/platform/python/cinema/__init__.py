from PIL.ImageChops import difference
from PIL.ImageOps import autocontrast
from PIL.Image import open as PIOpen

class VideoFrame(object):
    def __init__(self, pilImage):
        self.image = pilImage.convert('RGB')

    @staticmethod
    def diff(a, b):
        diff = difference(a.image, b.image)
        diffNormalized = autocontrast(diff)
        return (VideoFrame(diff), VideoFrame(diffNormalized))

    @staticmethod
    def load(path):
        with open(path, 'rb') as f:
            image = PIOpen(f)
            image.load()
            return VideoFrame(image)

    def save(self, path):
        return self.image.save(path)
