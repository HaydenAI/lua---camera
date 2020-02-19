require 'torch'
require 'sys'

useOpenCV = true
require 'camera'

width  = 976
height = 208
fps = 30
dir = "demo_test"

sys.execute(string.format('mkdir -p %s',dir))

local stream = 'shmsrc socket-path=/tmp/tmpsock ! video/x-raw, format=(string)BGR, width=(int)1920, height=(int)1080, framerate=(fraction)20/1 ! appsink'

camera1 = image.Camera{idx=-1,width=width,height=height,fps=fps, stream =stream}

camera1:start()
a1 = camera1:forward()
win = image.display{win=win,image={a1}}
f = 1
sys.tic()
while true do
   a1 = camera1:forward()
   camera1:convert(-13, 0.01, a1)
   camera1:imageMult(13, a1)

   camera1:extractLines(a1)

   image.display{win=win,image={a1}}
   --image.savePNG(string.format("%s/frame_1_%05d.png",dir,f),a1)
   f = f + 1
   print("FPS: ".. f/sys.toc())
end