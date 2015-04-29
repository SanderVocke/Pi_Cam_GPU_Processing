# Pi_Cam_GPU_Processing
GPU shader processing on Raspberry Pi camera image. Most code taken directly from: http://robotblogging.blogspot.nl/2013/10/gpu-accelerated-camera-processing-on.html

Changes:
- removed many shaders. Pipeline for now is YUV input textures -> RGB texture -> Output texture, with subsampled versions of RGB and Out.
- Added SDL window preview support that grabs a frame when s is pressed, and shows it.
