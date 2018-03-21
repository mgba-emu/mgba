from mgba.image import Image
from collections import namedtuple
from . import VideoFrame

Output = namedtuple('Output', ['video'])

class Tracer(object):
    def __init__(self, core):
        self.core = core
        self.fb = Image(*core.desiredVideoDimensions())
        self.core.setVideoBuffer(self.fb)
        self._videoFifo = []

    def yieldFrames(self, skip=0, limit=None):
        self.core.reset()
        skip = (skip or 0) + 1
        while skip > 0:
            frame = self.core.frameCounter
            self.core.runFrame()
            skip -= 1
        while frame <= self.core.frameCounter and limit != 0:
            self._videoFifo.append(VideoFrame(self.fb.toPIL()))
            yield frame
            frame = self.core.frameCounter
            self.core.runFrame()
            if limit is not None:
                assert limit >= 0
                limit -= 1

    def video(self, generator=None, **kwargs):
        if not generator:
            generator = self.yieldFrames(**kwargs)
        try:
            while True:
                if self._videoFifo:
                    result = self._videoFifo[0]
                    self._videoFifo = self._videoFifo[1:]
                    yield result
                else:
                    next(generator)
        except StopIteration:
            return

    def output(self, **kwargs):
        generator = self.yieldFrames(**kwargs)

        return mCoreOutput(video=self.video(generator=generator, **kwargs))
