----------------------------------
-- dependencies
----------------------------------
require 'torch'
require 'xlua'
require 'image'
require 'libcamopencv'

----------------------------------
-- a camera class
----------------------------------
local Camera = torch.class('image.Camera')

function Camera:__init(...)
   -- parse args
   local args, idx, width, height, stream = xlua.unpack(
      {...},
      'image.Camera', nil,
      {arg='idx', type='number', help='camera index', default=0},
      {arg='width', type='number', help='frame width', default=1920},
      {arg='height', type='number', help='frame height', default=1080},
      {arg='stream', type='string', help='Stream URL', default=''}
   )

   -- init vars
   self.tensorsized = torch.FloatTensor(3, height, width)
   self.buffer = torch.FloatTensor()
   self.tensortyped = torch.Tensor(3, height, width)
   self.idx = idx
   self.width = width
   self.height = height
   self.idx = idx
   self.stream = stream
end

   -- init capture
function Camera:start()
   self.fidx = libcamopencv.initCam(self.idx, self.width, self.height, self.stream)
end

function Camera:forward(crop_ypos_ratio)
   if (crop_ypos_ratio == nil) then
      crop_ypos_ratio = 0.5
   end
   libcamopencv.grabFrame(self.fidx,  self.buffer, crop_ypos_ratio, self.width, self.height)
   if self.tensorsized:size(2) ~= self.buffer:size(2) or self.tensorsized:size(3) ~= self.buffer:size(3) then
      image.scale(self.tensorsized, self.buffer)
   else
      self.tensorsized = self.buffer
   end
   if self.tensortyped:type() ~= self.tensorsized:type() then
      self.tensortyped:copy(self.tensorsized)
   else
      self.tensortyped = self.tensorsized
   end
   return self.tensortyped
end


function Camera:extractLines(buffer)
   libcamopencv.extractLines(buffer, self.width, self.height, 3)
end

function Camera:convert(min, max, buffer)
   libcamopencv.convert(min, max, buffer, self.width, self.height, 3)
end

function Camera:imageMult(scale, buffer)
   libcamopencv.imageMult(scale, buffer, self.width, self.height, 3)
end

function Camera:stop()
  libcamopencv.releaseCam(self.fidx)
  print('stopping camera')
end
