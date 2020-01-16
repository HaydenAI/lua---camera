require 'torch'
require 'sys'

useOpenCV = true
require 'camera'

width  = 640
height = 480
fps = 30
dir = "demo_test"

sys.execute(string.format('mkdir -p %s',dir))

local stream = 'udpsrc port=5001 ! application/x-rtp,media=video,payload=26,clock-rate=90000,encoding-name=JPEG,framerate=30/1 ! rtpjpegdepay ! jpegdec ! videoconvert ! appsink'

camera1 = image.Camera{idx=-1,width=width,height=height,fps=fps, stream =stream}

a1 = camera1:forward()
win = image.display{win=win,image={a1}}
f = 1

while true do
   sys.tic()
   a1 = camera1:forward()
   image.display{win=win,image={a1}}
   --image.savePNG(string.format("%s/frame_1_%05d.png",dir,f),a1)
   f = f + 1
   print("FPS: ".. 1/sys.toc()) 
end