// libavcodec_hvec_16bit.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

// https://askubuntu.com/questions/922563/set-bit-depth-in-ffmpeg-encoding-for-hevc
// https://gist.github.com/psteinb/bf12348d799108f390f5

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
}

void print_error(int error)
{
	char szBuffer[1024];
	av_strerror(error, szBuffer, sizeof(szBuffer)); 
	printf("Error: %s\n", szBuffer); 
}

int main()
{
	//AV_PIX_FMT_YUV420P12
	int ret = 0; 
	AVPixelFormat destFormat = AV_PIX_FMT_YUV420P12;
	//AVPixelFormat destFormat = AV_PIX_FMT_YUV420P;
	const char* nameHint = "hevc";
	const int FPS = 25; 
	const int DurationInSeconds = 20; 


	const AVOutputFormat* oFormat = av_guess_format(nameHint, "c:/users/brush/desktop/output.mp4", nullptr);

	AVFormatContext* ocontext; 
	ret = avformat_alloc_output_context2(&ocontext, oFormat, nullptr, "c:/users/brush/desktop/output.mp4");
	if (ret < 0)
	{
		print_error(ret); 
		return 0; 
	}

	const AVCodec* codec = avcodec_find_encoder(oFormat->video_codec);
	if (codec == nullptr)
	{
		printf("H265 isn't installed.\n");
		return 0;
	}

	AVCodecContext* context = avcodec_alloc_context3(codec);
	if (context == nullptr)
	{
		printf("Failed to allocate the context.\n");
		return 0;
	}

	AVStream* stream = avformat_new_stream(ocontext, codec);

	stream->codecpar->codec_id = oFormat->video_codec;
	stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
	stream->codecpar->width = 512;
	stream->codecpar->height = 512;
	stream->codecpar->format = destFormat;

	avcodec_parameters_to_context(context, stream->codecpar);

	context->width = 512;
	context->height = 512;
	context->gop_size = 10;
	context->max_b_frames = 1;
	context->time_base.num = 1;
	context->time_base.den = FPS;
	context->pix_fmt = destFormat;


	ret = av_opt_set(context->priv_data, "preset", "medium", 0);
	if (ret < 0)
	{
		print_error(ret);
		return 0;
	}

	avcodec_parameters_from_context(stream->codecpar, context);

	ret = avcodec_open2(context, codec, nullptr);
	if (ret < 0)
	{
		print_error(ret);
		return 0;
	}

	if (!(oFormat->flags & AVFMT_NOFILE)) {
		if ((ret = avio_open(&ocontext->pb, "c:/users/brush/desktop/output.mp4", AVIO_FLAG_WRITE)) < 0) {
			return 0 ;
		}
	}

	if ((ret = avformat_write_header(ocontext, NULL)) < 0) {
		return 0;
	}

	auto frame = av_frame_alloc(); 

	frame->width = 512; 
	frame->height = 512; 
	frame->format = destFormat;

	ret = av_frame_get_buffer(frame, 32); 
	if (ret < 0)
	{
		print_error(ret); 
		return 0; 
	}

	//auto swsContext = sws_getContext(512, 512, AVPixelFormat::AV_PIX_FMT_GRAY16LE, 512, 512, AV_PIX_FMT_YUV420P12, SWS_BICUBIC, nullptr, nullptr, nullptr); 
	//auto swsContext = sws_getContext(512, 512, AVPixelFormat::AV_PIX_FMT_RGB24, 512, 512, AV_PIX_FMT_YUV420P12, SWS_BICUBIC, nullptr, nullptr, nullptr);
	auto swsContext = sws_getContext(512, 512, AVPixelFormat::AV_PIX_FMT_RGB24, 512, 512, destFormat, SWS_BICUBIC, nullptr, nullptr, nullptr);

	uint8_t* data = new uint8_t[512 * 512 * 3];

	int seed = 0; 
	AVPacket* packet; 
	for (int i = 0; i < DurationInSeconds * FPS; i++)
	{
		packet = av_packet_alloc();
		packet->data = nullptr; 
		packet->size = 0; 
		//packet->flags |= AV_PKT_FLAG_KEY; 
		for (int y = 0; y < 512; y++)
		{
			for (int x = 0; x < 512; x++, seed++)
			{
				int index = (512 * y + x) * 3; 
				data[index] = (uint8_t)((i * seed) * 50);
				data[index + 1] = (uint8_t)(index + seed);
				data[index+2] = (uint8_t)(50 * (i * seed));
				//data[0][] = (uint8_t)(y*x + x);
				//frame->data[1][512 * y + x] = (uint8_t)(y * x + x);
				//frame->data[2][512 * y + x] = (uint8_t)(y * x + x);
			}
		}

		int inLinesize[1] = { 3 * 512 };
		const uint8_t* ptr = (uint8_t*)data; 
		ret = sws_scale(swsContext, (const uint8_t* const*)&ptr, inLinesize, 0, 512, frame->data, frame->linesize);
		if (ret < 0)
		{
			print_error(ret); 
			return 0; 
		}
		frame->pts = (1.0 / 25) * 90000 * (i); // i think there's a bug here
		//https://stackoverflow.com/questions/46497574/ffmpeg-resource-temporarily-unavailable
		for (;;)
		{
			ret = ::avcodec_send_frame(context, frame);
			if (ret < 0)
			{
				print_error(ret);
				return 0;
			}

			ret = ::avcodec_receive_packet(context, packet);
			if (ret == AVERROR(EAGAIN))
			{
				printf("-"); 
				continue; 
			}
			else if(ret < 0)
			{
				print_error(ret);
				return 0;
			}
			printf("+"); 
			break; 

		}

		ret = av_interleaved_write_frame(ocontext, packet);
		if (ret < 0)
		{
			print_error(ret);
			return 0;
		}
		av_packet_free(&packet);
	}

	packet = av_packet_alloc();
	packet->data = nullptr;
	packet->size = 0;
	packet->flags |= AV_PKT_FLAG_KEY;
	for (;;)
	{
		ret = ::avcodec_send_frame(context, frame);
		if (ret < 0)
		{
			print_error(ret);
			return 0;
		}

		ret = ::avcodec_receive_packet(context, packet);
		if (ret == AVERROR(EAGAIN))
		{
			printf("-");
			continue;
		}
		else if (ret < 0)
		{
			print_error(ret);
			return 0;
		}
		printf("+");
		break;
	}
	ret = av_interleaved_write_frame(ocontext, packet);
	if (ret < 0)
	{
		print_error(ret);
		return 0;
	}
	av_packet_free(&packet);

	ret = av_write_trailer(ocontext);
	if (!(oFormat->flags & AVFMT_NOFILE)) {
		int err = avio_close(ocontext->pb);
		if (err < 0) {
		}
	}
}
