
----------------------------------
-- dependencies
----------------------------------
require 'sys'
require 'xlua'

----------------------------------
-- load camera driver based on OS
----------------------------------
if useOpenCV then
   if not xlua.require 'camopencv' then
      xlua.error('failed to load camopencv wrapper: verify that camopencv is installed')
   end
    print('Using opencv')
elseif sys.OS == 'linux' then
   if not xlua.require 'v4l' then
      xlua.error('failed to load video4linux wrapper: verify that you have v4l2 libs')
   end
    print('Using v4l')
elseif sys.OS == 'macos' then
   if not xlua.require 'camopencv' then
      xlua.error('failed to load camopencv wrapper: verify that camopencv is installed')
   end
else
   xlua.error('no camera driver available for your OS, sorry :-(')
end

----------------------------------
-- package a little demo
----------------------------------
camera = {}
camera.testme = function()
                   require 'qtwidget'
                   print '--------------------------------------------------'
                   print 'grabbing frames from your camera for about 10secs'
                   local cam = image.Camera{}
                   local w = camera._w
                   local fps = 0
                   for i = 1,200 do -- ~10 seconds
                      sys.tic()
                      local frame = cam:forward()
                      w = image.display{image=frame, win=w, legend='camera capture ['..fps..'fps]'}
                      w.window:show()
                      local t = sys.toc()
                      if fps == 0 then fps = 1/t end
                      fps = math.ceil((1/t + fps)/2)
                   end
                   cam:stop()
                   print 'done: create your own frame grabber with image.Camera()'
                   print '--------------------------------------------------'
                   camera._w = w
                   w.window:hide()
                   return w
                end
