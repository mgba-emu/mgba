local state = {}
state.width = canvas:screenWidth()
state.height = canvas:screenHeight()
state.overlay = canvas:newLayer(state.width, state.height)
state.painter = image.newPainter(state.overlay.image)
state.painter:loadFont(script.dir .. "/SourceSans3-Regular.otf")
state.painter:setFontSize(10.5)
state.painter:setFill(true)
state.painter:setBlend(true)

state.text = "Sphinx of black quartz judge my vow"
state.metrics = state.painter:textRunMetrics(state.text)
state.painter:setFillColor(0x80FF0000)
state.painter:drawRectangle(0, 0, state.metrics:width(), state.metrics:ascender())
state.painter:setFillColor(0x800000FF)
state.painter:drawRectangle(0, state.metrics:ascender(), state.metrics:width(), state.metrics:descender())
state.painter:setFillColor(0xFFFFFFFF)
state.painter:drawText(state.text, 0, 0)

state.overlay:update()
