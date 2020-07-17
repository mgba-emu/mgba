from mgba.image import Image
from collections import namedtuple
from . import VideoFrame

Output = namedtuple('Output', ['video'])


class Tracer(object):
    def __init__(self, core):
        self.core = core
        self._video_fifo = []

    def yield_frames(self, skip=0, limit=None):
        self.framebuffer = Image(*self.core.desired_video_dimensions())
        self.core.set_video_buffer(self.framebuffer)
        self.core.reset()
        skip = (skip or 0) + 1
        while skip > 0:
            frame = self.core.frame_counter
            self.framebuffer = Image(*self.core.desired_video_dimensions())
            self.core.set_video_buffer(self.framebuffer)
            self.core.run_frame()
            skip -= 1
        while frame <= self.core.frame_counter and limit != 0:
            self._video_fifo.append(VideoFrame(self.framebuffer.to_pil()))
            yield frame
            frame = self.core.frame_counter
            self.core.run_frame()
            if limit is not None:
                assert limit >= 0
                limit -= 1

    def video(self, generator=None, **kwargs):
        if not generator:
            generator = self.yield_frames(**kwargs)
        try:
            while True:
                if self._video_fifo:
                    yield self._video_fifo.pop(0)
                else:
                    next(generator)
        except StopIteration:
            return
