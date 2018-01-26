#include <stdio.h>
#include <iostream>
using namespace std;

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include <libswresample/swresample.h>
};
//�����
#pragma comment(lib,"avformat.lib")
//���߿⣬������ȡ������Ϣ��
#pragma comment(lib,"avutil.lib")
//�����Ŀ�
#pragma comment(lib,"avcodec.lib")

#pragma comment(lib,"swscale.lib") 
#pragma comment(lib,"swresample.lib")

int avError(int errNum) {
	char buf[1024];
	//��ȡ������Ϣ
	av_strerror(errNum, buf, sizeof(buf));
	cout << " failed! " << buf << endl;
	return -1;
}
int main() {
	AVFormatContext* pFormatCtx;
	AVOutputFormat* fmt;
	AVStream* audio_st;
	AVCodecContext* pCodecCtx;
	AVCodec* pCodec;

	uint8_t* frame_buf;
	AVFrame* frame;
	int size;

	FILE *in_file = fopen("tdjm.pcm", "rb");	//��ƵPCM�������� 
	int framenum = 1000;	//��Ƶ֡��
	const char* out_file = "tdjm.aac";					//����ļ�·��

	AVSampleFormat inSampleFmt = AV_SAMPLE_FMT_S16;
	AVSampleFormat outSampleFmt = AV_SAMPLE_FMT_FLTP;
	int sampleRate = 44100;
	int channels = 2;
	int sampleByte = 2;

	av_register_all();

	avformat_alloc_output_context2(&pFormatCtx, NULL, NULL, out_file);
	fmt = pFormatCtx->oformat;

	//ע�����·��
	if (avio_open(&pFormatCtx->pb, out_file, AVIO_FLAG_READ_WRITE) < 0)
	{
		printf("����ļ���ʧ�ܣ�\n");
		return -1;
	}
	//pCodec = avcodec_find_encoder_by_name("libfdk_aac");
	pCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (!pCodec) {
		cout << "pCodec find failed!" << endl;
		return -1;
	}
	audio_st = avformat_new_stream(pFormatCtx, pCodec);
	if (audio_st == NULL) {
		return -1;
	}
	pCodecCtx = audio_st->codec;
	pCodecCtx->codec_id = fmt->audio_codec;
	pCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
	pCodecCtx->sample_fmt = outSampleFmt;
	pCodecCtx->sample_rate = sampleRate;
	pCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
	pCodecCtx->channels = av_get_channel_layout_nb_channels(pCodecCtx->channel_layout);
	pCodecCtx->bit_rate = 64000;

	//�����ʽ��Ϣ
	av_dump_format(pFormatCtx, 0, out_file, 1);
	///2 ��Ƶ�ز��� �����ĳ�ʼ��
	SwrContext * asc = NULL;
	asc = swr_alloc_set_opts(asc,
		av_get_default_channel_layout(channels), outSampleFmt, sampleRate,//�����ʽ
		av_get_default_channel_layout(channels), inSampleFmt, sampleRate, 0, 0);//�����ʽ
	if (!asc)
	{
		cout << "swr_alloc_set_opts failed!";
		getchar();
		return -1;
	}
	int ret = swr_init(asc);

	if (!pCodec)
	{
		printf("û���ҵ����ʵı�������\n");
		return -1;
	}
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
	{
		printf("��������ʧ�ܣ�\n");
		return -1;
	}
	frame = av_frame_alloc();
	frame->nb_samples = pCodecCtx->frame_size;
	frame->format = pCodecCtx->sample_fmt;

	cout << "frame_size " << frame->nb_samples << endl;
	//����ÿһ֡���ֽ���
	size = av_samples_get_buffer_size(NULL, pCodecCtx->channels, pCodecCtx->frame_size, pCodecCtx->sample_fmt, 1);
	frame_buf = (uint8_t *)av_malloc(size);
	//һ�ζ�ȡһ֡��Ƶ���ֽ���
	int readSize = frame->nb_samples*channels*sampleByte;
	char *buf = new char[readSize];
	
	avcodec_fill_audio_frame(frame, pCodecCtx->channels, pCodecCtx->sample_fmt, (const uint8_t*)frame_buf, size, 1);

	audio_st->codecpar->codec_tag = 0;
	audio_st->time_base = audio_st->codec->time_base;
	cout << audio_st->time_base.den << " " << audio_st->time_base.num << " "<< size<<endl;
	//�ӱ��������Ʋ���
	avcodec_parameters_from_context(audio_st->codecpar, pCodecCtx);

	//д�ļ�ͷ
	avformat_write_header(pFormatCtx, NULL);
	AVPacket pkt;
	av_new_packet(&pkt, size);
	int apts = 0;

	for (int i = 0; i < framenum; i++) {
		//����PCM
		if (fread(buf, 1, readSize, in_file) < 0)
		{
			printf("�ļ���ȡ����\n");
			return -1;
		}
		else if (feof(in_file)) {
			break;
		}
		frame->pts = apts;
		apts += av_rescale_q(frame->nb_samples, { 1,sampleRate }, pCodecCtx->time_base);
		int got_frame = 0;
		//�ز���Դ����
		const uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
		indata[0] = (uint8_t *)buf;
		int len = swr_convert(asc, frame->data, frame->nb_samples, //�������������洢��ַ����������
			indata, frame->nb_samples
		);
		//����
		int ret = avcodec_send_frame(pCodecCtx, frame);
		if (ret < 0) {
			cout << "avcodec_send_frame error" << endl;
		}

		ret = avcodec_receive_packet(pCodecCtx, &pkt);
		//int ret = avcodec_encode_audio2(pCodecCtx, &pkt, frame, &got_frame);
		if (ret < 0)
		{
			cout << "avcodec_receive_packet��error " << ret << endl;;
			avError(ret);
			continue;
		}
		pkt.stream_index = audio_st->index;
		cout << i << " ";
		pkt.pts = av_rescale_q(pkt.pts, pCodecCtx->time_base, audio_st->time_base);
		pkt.dts = av_rescale_q(pkt.dts, pCodecCtx->time_base, audio_st->time_base);
		pkt.duration = av_rescale_q(pkt.duration, pCodecCtx->time_base, audio_st->time_base);
		//cout << pkt.pts << " " << pkt.dts << " " << pkt.duration << endl;
		ret = av_write_frame(pFormatCtx, &pkt);
		av_free_packet(&pkt);
	}
	//д�ļ�β
	av_write_trailer(pFormatCtx);
	//����
	if (audio_st)
	{
		avcodec_close(audio_st->codec);
		av_free(frame);
		av_free(frame_buf);
		//av_free(buf);
	}
	avio_close(pFormatCtx->pb);
	avformat_free_context(pFormatCtx);

	fclose(in_file);
	cout << "success !" << endl;
	return 0;
}