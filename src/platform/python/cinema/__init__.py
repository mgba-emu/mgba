from PIL.ImageChops import difference
from PIL.ImageOps import autocontrast
from PIL.Image import open as PIOpen


class VideoFrame(object):
    def __init__(self, pil_image):
        self.image = pil_image.convert('RGB')

    @staticmethod
    def diff(a, b):
        diff = difference(a.image, b.image)
        diff_normalized = autocontrast(diff)
        return (VideoFrame(diff), VideoFrame(diff_normalized))

    @staticmethod
    def load(path):
        with open(path, 'rb') as f:
            image = PIOpen(f)
            image.load()
            return VideoFrame(image)

    def save(self, path):
        return self.image.save(path)
