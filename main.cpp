#include <iostream>
#include <opencv2/opencv.hpp>
extern "C"   //ffmpeg是采用c语言实现的 c++工程种 导入c语言
{
    //avcodec:编解码(最重要的库)
    #include <libavcodec/avcodec.h>
    //avformat:封装格式处理
    #include <libavformat/avformat.h>
    //swscale:视频像素数据格式转换
    #include <libswscale/swscale.h>
    //avdevice:各种设备的输入输出
    #include <libavdevice/avdevice.h>
    //avutil:工具库（大部分库都需要这个库的支持）
    #include <libavutil/avutil.h>

    #include <libavutil/imgutils.h>

}


int main() {
    // Initialize FFmpeg
    //av_register_all();//在新版本4.0以后，不需要调用该方法，可以直接使用所有模块。
    avformat_network_init();

    // Open RTSP stream
    AVFormatContext* formatContext = nullptr;
    if (avformat_open_input(&formatContext, "rtsp://192.168.0.100:6554/live/test0", nullptr, nullptr) != 0) {
        std::cerr << "Failed to open RTSP stream." << std::endl;
        return -1;
    }

    // Find stream info
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        std::cerr << "Failed to find stream info." << std::endl;
        return -1;
    }

    // Find video stream index
    int videoStreamIndex = -1;
    for (unsigned int i = 0; i < formatContext->nb_streams; ++i) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }

    if (videoStreamIndex == -1) {
        std::cerr << "Failed to find video stream." << std::endl;
        return -1;
    }

    // Get codec parameters for the video stream
    AVCodecParameters* codecParameters = formatContext->streams[videoStreamIndex]->codecpar;

    // Find video decoder
    const AVCodec* codec = avcodec_find_decoder(codecParameters->codec_id);
    if (codec == nullptr) {
        std::cerr << "Failed to find video decoder." << std::endl;
        return -1;
    }

    // Allocate codec context
    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    if (avcodec_parameters_to_context(codecContext, codecParameters) < 0) {
        std::cerr << "Failed to allocate codec context." << std::endl;
        return -1;
    }

    // Open codec
    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        std::cerr << "Failed to open codec." << std::endl;
        return -1;
    }
  
    // Prepare image conversion context
    SwsContext* swsContext = sws_getContext(codecContext->width, codecContext->height,
        codecContext->pix_fmt,
        codecContext->width, codecContext->height,
        //AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
        AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);


    // Create OpenCV window for displaying frames
    cv::namedWindow("RTSP Stream", cv::WINDOW_NORMAL);

    AVPacket packet;
    cv::Mat frame;

    frame = cv::Mat(codecContext->height, codecContext->width, CV_8UC3);

    AVFrame* pFrameRGB = av_frame_alloc();
    uint8_t* buffer = (uint8_t*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB24, codecContext->width, codecContext->height, 1));
    av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24, codecContext->width, codecContext->height, 1);

    struct SwsContext* sws_ctx = sws_getContext(codecContext->width, codecContext->height, codecContext->pix_fmt,
        codecContext->width, codecContext->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    while (av_read_frame(formatContext, &packet) >= 0) {
        if (packet.stream_index == videoStreamIndex) {
            // Decode video frame
            avcodec_send_packet(codecContext, &packet);
            AVFrame* avFrame = av_frame_alloc();
            int ret = avcodec_receive_frame(codecContext, avFrame);

            if (ret == 0) {
                // Convert frame to RGB
                sws_scale(sws_ctx, avFrame->data, avFrame->linesize, 0, codecContext->height, pFrameRGB->data, pFrameRGB->linesize);
                frame.data = pFrameRGB->data[0];
                // Display frame
                cv::imshow("RTSP Stream", frame);
                cv::waitKey(1);
            }

            av_frame_free(&avFrame);
        }

        av_packet_unref(&packet);
    }

    // Clean up
    avformat_close_input(&formatContext);
    avformat_network_deinit();
    avcodec_free_context(&codecContext);
    sws_freeContext(swsContext);

    return 0;
}


