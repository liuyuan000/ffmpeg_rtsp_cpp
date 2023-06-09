# 本工程使用说明

本工程是在Windows电脑上的VS进行测试的，需要依赖OpenCV和FFmpeg库，请自行准备。

## 环境准备

- Windows电脑
- Visual Studio
- OpenCV库
- FFmpeg库

## 使用方法

1. 将本工程下载到本地。
2. 将OpenCV和FFmpeg库添加到本工程中。
3. 打开`main.cpp`文件，将拉流地址更改为自己的地址。
4. 编译并运行本工程。

## 注意事项

- 视频流的格式为yuv420，本工程将其转换成RGB后通过OpenCV进行显示。
- 如果编译或运行过程中出现问题，请检查您的环境是否正确配置。
- 本工程仅供学习交流使用，不得用于商业用途。
