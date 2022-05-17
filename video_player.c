VIDEO PLAYER
/* base_task.c is a basic real-time task skeleton offered by liblitmus which is the user space library of Litmus-RT.  Updated the base_task.c to run as a video processing application using ffmpeg and SDL libraries.
 
References:  https://github.com/rambodrahmani/ffmpeg-video-player
http://dranger.com/ffmpeg/tutorial01.html
https://github.com/farhanr8/Litmus-RT_VideoApp
 
A LITMUS^RT real-time task can perform any system call, etc., but no real-time guarantees can be made if a system call blocks. To be on the safe side, it is advisable to use I/O for debugging purposes & from non-real-time sections
*/
 
// >>>>>>>>>>  HEADER FILE INCLUSION SECTION  <<<<<<<<<<<<
 
// Generic Libraries that work as Helpers
// To handle standard IO
#include <stdio.h>  
// To handle system calls and I/O functions
#include <unistd.h>  
// To handle memory allocation & process control
#include <stdlib.h>  
// To use built-in string functions with ease
#include <string.h>
// To verify assumptions made by the code & print diagnostic message if false
#include <assert.h>
// To use mathematical functions
#include <math.h>
// To use functions that manipulate date & time
#include <time.h>
 
// Libraries specific to FFMPEG 
/* A movie file has: Audio stream + Video stream.
Each stream has a set of frames encoded by different codec.
Codec defines how data is COded & DECoded.
Video data : Frames → Packets
*/
// To handle encoding & decoding 
#include <libavcodec/avcodec.h>
// To deal with various media container formats like AVI, Quicktime
#include <libavformat/avformat.h>
// To display the video at a different pixel size/aspect ratio than it was encoded
#include <libswscale/swscale.h>
// To buffer data if more input has been provided than the available output space
#include <libswresample/swresample.h>
/* Helps to use built-in structs
Typedef struct ImgUtils {
		const AVClass *class;
		int log_offset;
		void *log_ctx;
} ImgUtils;
*/
#include <libavutil/imgutils.h>
/* Provides a generic system to declare options on arbitrary structs. An option can have a help text, a type & a range of possible values. Options may then be enumerated, read & written to.
*/
#include <libavutil/opt.h>
 
// Libraries specific to SDL
// To support sound, file access, event handling & 2D pixel operations
#include <SDL/SDL.h>
// To handle SDL thread management routines
#include <SDL/SDL_thread.h>
 
// Libraries specific to LITMUS-RT
// To use the user place API library of Litmus-RT
#include "litmus.h"
 
// Comment these lines in code for now
https://stackoverflow.com/questions/7790262/a-simple-explanation-of-what-is-mingw
https://home.cs.colorado.edu/~main/cs1300/doc/mingwfaq.html
#ifdef __MINGW32__
 
// To tell the the processor that main() has no special meaning & hence, leave it as it is in order to prevent SDL from overriding main()
#undef main 
#endif
 
// >>>>>>>>>>  CONSTANT DEFINITIONS SECTION  <<<<<<<<<<<<
 
/* Any real-time scheduler needs at least Worst-Case Execution time & Period along with the Deadline associated with the job. We define such constants here in milliseconds. These can also be set from command line arguments.
*/
#define PERIOD            				10
#define WORST_CASE_TIME        		10
#define RELATIVE_DEADLINE		100
 
// We also define the limits for buffer & frame sizes
#define SDL_AUDIO_BUFFER_SIZE   1024
#define MAX_AUDIO_FRAME_SIZE  192000
 
 
// Defines a macro that helps to catch errors
#define CALL( exp ) do { \
 	int ret; \
 	ret = exp; \
 	if (ret != 0) \
  		fprintf(stderr, "%s failed: %m \n", #exp);\
 	else \
  		fprintf(stderr, "%s ok. \n", #exp); \
} while (0)
 
// >>>>>>>>>>  GLOBAL DECLARATIONS SECTION  <<<<<<<<<<<<
 
// Defines the user-defined struct to handle Packet queue & names it
typedef struct PacketQueue {
 	AVPacketList *first_pkt, *last_pkt;
 	int nb_packets, size;
 	SDL_mutex *mutex;
 	SDL_cond *cond;
} PacketQueue;
 
// Audio PacketQueue reference
PacketQueue audioQ;
 
// Initializes the variables associated with FFMPEG like audio stream & video stream indices, corresponding AVStream objects, queues, buffers, quit flag etc. 
int  i, videoStream, audioStream, isFrameDecodingFinished, quit = 0;
 
// Used for image scaling & is provided by FFMPEG
struct SwsContext  *swsCtx_Ptr = NULL;
 
// Helps to format IO context
AVFormatContext  *avFormatCtx_Ptr = NULL;
// Helps to handle the Codec context for audio & video streams
AVCodecContext  *audioCodecCtxRetrieved_Ptr = NULL, *audioCodecCtx_Ptr    = NULL;
AVCodecContext  *videoCodecCtxRetrieved_Ptr = NULL, *videoCodecCtx_Ptr = NULL;
 
// Helps to store & handle compressed data
AVPacket  packet;
 
// Helps to describe raw/decoded audio or video data
AVFrame  *frame_Ptr = NULL;
 
// Handles all information related to the AVCodec used
AVCodec  *videoCodec_Ptr = NULL, *audioCodec_Ptr = NULL;
 
// SDL Parameters
SDL_Overlay     *bmp;
SDL_Surface     *screen;
SDL_Rect        surfaceRect;
SDL_Event       event;
SDL_AudioSpec   desiredSpec, obtainedSpec;
 
// >>>>>>>>>>  FUNCTION DEFINITIONS SECTION  <<<<<<<<<<<<
 
// Declares periodically invoked job. Returns 0 if  the task should continue and  1 otherwise. Function definition follows later
int job(void);
 
// Method to initialize the given PacketQueue
void packet_queue_init(PacketQueue *q) {   // Method start
// Dynamically allocates & fills the memory for the audio queue with given value. memset(void* ptr, int value, size_t num) sets the first num bytes of memory block pointed by ptr to the given value
 	 memset(q, 0, sizeof(PacketQueue));
 
 	// Returns the initialized and unlocked mutex or NULL on failure
 	q->mutex = SDL_CreateMutex();
 	if (!q->mutex) {
       		printf("SDL_CreateMutex Error: %s \n", SDL_GetError());
       		return;
   	}
 
   	// Returns a new condition variable or NULL on failure
 	q->cond = SDL_CreateCond();
 	if (!q->cond) {
       	       	printf("SDL_CreateCond Error: %s.\n", SDL_GetError());
      	          return;
   	}
} // packet_queue_init() Method end
 
// Method to put the given AVPacket in the audio PacketQueue
int packet_queue_put(PacketQueue *q, AVPacket *pkt) { // Method start
AVPacketList *pkt1;
 
	// Initial checks
 	if (av_packet_ref(pkt) < 0)
  	 	return -1;
 
 	// Allocates memory to the new AVPacketList to be inserted  
pkt1 = av_malloc(sizeof(AVPacketList));
 	if (!pkt1)
   		return -1;
 // Adds reference to the given AVPacket
 pkt1->pkt = *pkt;
 pkt1->next = NULL;
 
// As all the newly-created mutexes begin in the unlocked state, this will not return while the mutex is locked by another thread & blocks it until the OS has chosen the caller as the next thread to lock it. We need this to ensure that only one process will access the resource at a time.
	SDL_LockMutex(q->mutex);
 
	// Inserts the new AVPacketList at the end of the queue
// Checks if the queue is empty. If so, inserts at the start
 	if (!q->last_pkt) 
   		q->first_pkt = pkt1;
	// If not, inserts at the end
  	else 
   		q->last_pkt->next = pkt1;
 
// Now, we point the last AVPacketList in the queue to the newly created AVPacketList and update respective pointers
 	q->last_pkt = pkt1;
q->nb_packets++;
q->size += pkt1->pkt.size;
 
	// Restarts a thread wait on a conditional variable
 	SDL_CondSignal(q->cond);
	
// Unlocks the mutex lock posed
 	SDL_UnlockMutex(q->mutex);
 
 	return 0;
}  // packet_queue_put() Method end
// Method to get the first AVPacket from the given PacketQueue
// Returns 0 if queue is empty, 1 if we extracted a packet from the queue & < 0 if we quit
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) { // Method start
 
 AVPacketList *pkt1;
 int ret;
 
// As all the newly-created mutexes begin in the unlocked state, this will not return while the mutex is locked by another thread & blocks it until the OS has chosen the caller as the next thread to lock it. We need this to ensure that only one process will access the resource at a time.
 SDL_LockMutex(q->mutex);
 
 for( ; ; ) {
	// Exits if quit flag is set
   	if(quit) {
     		ret = -1;
     		break;
   	}
 
   	// Points to the first AVPacketList in the queue
   	pkt1 = q->first_pkt;
 
   	// If the first packet is not NULL, the queue is not empty
   	if (pkt1) {
     		// Places the second packet in the queue at first position
     		q->first_pkt = pkt1->next;
 
     		// Checks if queue is empty after removal
     		if (!q->first_pkt)
       			q->last_pkt = NULL;
 
     		// Decrease the number & size of packets in the queue
     		q->nb_packets--;
     		q->size -= pkt1->pkt.size;
 
     		// Points pkt to the extracted packet
     		*pkt = pkt1->pkt;
 
		// Frees the dynamically allocated memory block
     		av_free(pkt1);
     		ret = 1;
     		break;
   	} // if (pkt1) close
 
// block = 0 : avoid waiting for an AVPacket to be inserted in queue
else if (!block) {
     		ret = 0;
     		break;
   	}
 
	// Else, we wait on condition variable & unlock the provided mutex
else 
     		SDL_CondWait(q->cond, q->mutex);
 } //  for( ; ; ) close
 
	// Unlocks the mutex lock posed
 	SDL_UnlockMutex(q->mutex);
 	return ret;
} // packet_queue_get() Method end
 
// Method to resample the audio data retrieved using FFMPEG before playing it & returns the size of the resampled data
static int audio_resampling(AVCodecContext * audio_decode_ctx,
AVFrame * decoded_audio_frame, enum AVSampleFormat out_sample_fmt, int out_channels, int out_sample_rate, uint8_t * out_buf) {  // Method start
 
	// Helper variables 
SwrContext * swr_ctx = NULL;
int ret = 0;
int64_t in_channel_layout = audio_decode_ctx->channel_layout;
int64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
int out_nb_channels = 0;
int out_linesize = 0;
int in_nb_samples = 0;
int out_nb_samples = 0;
int max_out_nb_samples = 0;
uint8_t ** resampled_data = NULL;
int resampled_data_size = 0;
 
   	// Quits if the global flag is set
   	if (quit)
       		return -1;
 
	// Allocates SwrContext
   	swr_ctx = swr_alloc();
 
// Checks if SwrContext is allocated
   	if (!swr_ctx) {
       		printf("Unable to allocate SwrContext !! \n");
       		return -1;
   	}
 
   	// Get input audio channels
in_channel_layout = (audio_decode_ctx->channels == av_get_channel_layout_nb_channels(audio_decode_ctx->channel_layout)) ?  audio_decode_ctx->channel_layout : av_get_default_channel_layout(audio_decode_ctx->channels);
 
   	// Checks if input audio channels are correctly retrieved
   	if (in_channel_layout <= 0) {
       		printf("Unable to retrieve input audio channels correctly !! \n");
       		return -1;
   	}
 
   	// Sets output audio channels based on the input audio channels
   	if (out_channels == 1)
       		out_channel_layout = AV_CH_LAYOUT_MONO;
   	else if (out_channels == 2)
       		out_channel_layout = AV_CH_LAYOUT_STEREO;
   	else
       		out_channel_layout = AV_CH_LAYOUT_SURROUND;
 
  	// Retrieves number of audio samples per channel
   	in_nb_samples = decoded_audio_frame->nb_samples;
   	if (in_nb_samples <= 0) {
       		printf("Unable to retrieve audio samples from channel !! \n");
       		return -1;
   	}
 
   	// Set SwrContext parameters for resampling
	// Channel, Sample rate and Sample format for In & Out channels 
   	av_opt_set_int(swr_ctx, "in_channel_layout", in_channel_layout, 0);
av_opt_set_int(swr_ctx, "in_sample_rate", audio_decode_ctx->sample_rate, 0);
av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", audio_decode_ctx->sample_fmt, 0);
av_opt_set_int(swr_ctx, "out_channel_layout", out_channel_layout, 0);
      	av_opt_set_int(swr_ctx, "out_sample_rate", out_sample_rate, 0);
av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", out_sample_fmt, 0);
 
   	// Now, we initialize the SwrContext after setting all values      
ret = swr_init(swr_ctx);
 
// Checks if initializing resampling context is successful
   	if (ret < 0) {
       		printf("Failed to initialize the resampling context !! \n");
       		return -1;
   	}
 
	// We now rescale the 64-bit integer with specified rounding
   	max_out_nb_samples = out_nb_samples = av_rescale_rnd(
           In_nb_samples, out_sample_rate, audio_decode_ctx->sample_rate,
           AV_ROUND_UP);
 
   	// Checks if rescaling was successful
   	if (max_out_nb_samples <= 0) {
       		printf("Rescaling the samples failed !! \n");
       		return -1;
   	}
 
   	// Gets number of output audio channels
out_nb_channels = av_get_channel_layout_nb_channels(out_channel_layout);
	
// Allocates a data pointers array, samples buffer for out_nb_samples & fills data pointers and line size accordingly
   	ret = av_samples_alloc_array_and_samples(&resampled_data,
          &out_linesize, out_nb_channels, out_nb_samples, out_sample_fmt, 0);
 
	// Checks if we are able to allocate destination samples
   	if (ret < 0) {
       		printf("Unable to allocate destination samples !! \n");
       		return -1;
   	}
 
   	// Retrieves output samples no. taking into account the progressive delay
out_nb_samples = av_rescale_rnd( swr_get_delay(swr_ctx, audio_decode_ctx->sample_rate) + in_nb_samples, out_sample_rate,
          audio_decode_ctx->sample_rate, AV_ROUND_UP);
 
   	// Checks if output samples number was correctly retrieved
   	if (out_nb_samples <= 0) {
       		printf("Failed to retrieve output samples number !! \n");
       		return -1;
   	}
 
	// If we retrieved more than what we are supposed to, reallocate memory
   	if (out_nb_samples > max_out_nb_samples) {
       		// Frees memory block and set pointer to NULL
       		av_free(resampled_data[0]);
 
      		// Allocate a samples buffer for out_nb_samples samples
ret = av_samples_alloc(resampled_data, &out_linesize, out_nb_channels, out_nb_samples, out_sample_fmt, 1);
 
       		// Checks if samples buffer is correctly allocated
       		if (ret < 0) {
           		printf("av_samples_alloc failed.\n");
          	 		return -1;
       		}
       		max_out_nb_samples = out_nb_samples;
   	} // if (out_nb_samples > max_out_nb_samples) close
 
	// Now, we perform actual data resampling
   	if (swr_ctx) {
          		ret = swr_convert(swr_ctx, resampled_data, out_nb_samples,
                 	(const uint8_t **) decoded_audio_frame->data,
                 	decoded_audio_frame->nb_samples);
 
      	// Checks if audio conversion was successful
      	if (ret < 0) {
           	printf("Unable to convert data & resample it.\n");
           	return -1;
       	}
 
       	// Gets the required buffer size for the given audio parameters
       	resampled_data_size = av_samples_get_buffer_size(&out_linesize,
          out_nb_channels, ret, out_sample_fmt, 1);
 
       	// Checks audio buffer size
       	if (resampled_data_size < 0) {
printf("Unable to assign required buffer size for the given audio parameters !! \n");
           	return -1;
       	}
   	} // if (swr_ctx) close
 
   	else {
       		printf("Null Swr Context !! \n");
       		return -1;
   	}
 
// Copies the resampled data to the output buffer
   	memcpy(out_buf, resampled_data[0], resampled_data_size);
 
	// Memory cleanup
      	if (resampled_data) {
       		// Free memory block and set pointer to NULL
       		av_freep(&resampled_data[0]);
   	}
 
   	av_freep(&resampled_data);
   	resampled_data = NULL;
   	if (swr_ctx)
       		swr_free(&swr_ctx);
 
   	return resampled_data_size;
} // audio_resampling() Method end
 
// If a packet is available in the queue, this method extracts & decodes it and once we have the frame, it simply resamples it & copies it to the audio buffer making sure data_size is smaller than the audio buffer 
int audio_decode_frame(AVCodecContext *audioCodecCtx, uint8_t *audio_buf, int buf_size) { // Method start
int len1 = 0;
 	int data_size = 0;
 
	// Allocates an AVPacket & sets its fields to default values
 	AVPacket * avPacket = av_packet_alloc();
 
 	// Helper variables to track packet size & data
static uint8_t *audio_pkt_data = NULL;
 	static int audio_pkt_size = 0;
 
 	// Allocates a new frame to decode AVFrame packets
 	static AVFrame * avFrame = NULL;
   	avFrame = av_frame_alloc();
 
	// Checks if we are able to allocate AVFrame
   	if (!avFrame) {
       		printf("Unable to allocate AVFrame !! \n");
       		return -1;
   	}
 
// As long as we don’t get any error in the frame OR until the audio buffer is not smaller than data_size, we proceed with the below
 	for ( ; ; ) {
   		if (quit)
     			return -1;
 
   		while(audio_pkt_size > 0) {
     			int got_frame = 0;
 
			// Obtains decoded data from decoder
int ret = avcodec_receive_frame(audioCodecCtx_Ptr, avFrame);
 
// On success, we got a frame
     			if (ret == 0) 
               			got_frame = 1;
        			
// If we receive EAGAIN error, it means there is no data currently available & hence, we should try again later
           		if (ret == AVERROR(EAGAIN)) 
               			ret = 0;
           
			// Give the decoder raw & compressed data in an AVPacket
           		if (ret == 0)                
ret = avcodec_send_packet(audioCodecCtx_Ptr, avPacket);
           
// If we receive EAGAIN error, it means there is no data currently available & hence, we should try again later
           		if (ret == AVERROR(EAGAIN)) 
               			ret = 0;
			
			// If error occurs, we return
           		else if (ret < 0) {
               			printf("Unable to decode audio !! \n");
               			return -1;
           		}
 
           		// If data is available & successfully decoded, update params
else                
len1 = avPacket->size;
 
// We will skip the frame if error occurs or if frame is empty
                		if (len1 < 0) {
             		audio_pkt_size = 0;
      	 		break;
     		}
	
		// Update params associated with the packet & buffer
     			audio_pkt_data += len1;
     			audio_pkt_size -= len1;
     			data_size = 0;
 
// When we have a frame, we resample the audio data retrieved using FFMPEG before playing it
     			if (got_frame) {
// Invokes audio_resampling → Makes a function call
                      		data_size = audio_resampling(
                               		audioCodecCtx_Ptr,
                               		avFrame,
                               		AV_SAMPLE_FMT_S16,
                               		audioCodecCtx_Ptr->channels,
                               		audioCodecCtx_Ptr->sample_rate,
                               		audio_buf
                           		);
	
				// We test the assumptions that are set using this macro
       				assert(data_size <= buf_size);
           		}
 
			// If there is no data yet, we try to get more frames
     			if (data_size <= 0) 
             			continue;
  
			// We return the data we have & check for more data later
     			return data_size;
   		} // while close
 
// Unreferences the buffer referenced by the packet & resets the remaining packet fields to their defaults 
   		if (avPacket->data)
     			av_packet_unref(avPacket);
 
   		// Gets more audio AVPacket
   		// Invokes packet_queue_get → Makes a function call
if (packet_queue_get(&audioQ, avPacket, 1) < 0) {
     			printf(" Unable to get more audio AVPacket !! \n");
     			return -1;
   		}
  
   		// Updates data associated with avPacket
audio_pkt_data = avPacket->data;
   		audio_pkt_size = avPacket->size;
 	}
}  // audio_decode_frame() Method end
 
<<>>
// Obtains data from audio_decode_frame(), stores it in a buffer & attempts to write as many bytes as the amount defined by len to SDL stream. Get more data if we don't have enough, or save it for later if we have more data.
void audio_callback(void *userdata, Uint8 *stream, int len) { // Method start
 	int len1, audio_size;
 
// Size of audio_buf = 1.5 x Size of the largest audio frame obtained from FFMPEG
    	static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
 	static unsigned int audio_buf_size = 0;
 	static unsigned int audio_buf_index = 0;
 
 	// Retrieves the audio codec context
 	audioCodecCtx_Ptr = (AVCodecContext *)userdata;
 
	// As long as SDL defined length > 0
 	while (len > 0) {
   		if (quit)
     			return;
 
		// We have already sent all our data & so, we get more
   		if (audio_buf_index >= audio_buf_size) {
			// Invokes audio_decode_frame → Makes a function call 
audio_size = audio_decode_frame(audioCodecCtx_Ptr, audio_buf, sizeof(audio_buf));
 
// Checks if there is any error in audio_decode_frame()’s result. If so, we output silence.
     		if (audio_size < 0) {
       			audio_buf_size = 1024;
 
			// Clear memory
       			memset(audio_buf, 0, audio_buf_size);
printf("audio_decode_frame() failed !! \n");
 
     		} 
else 
       			audio_buf_size = audio_size;
 			audio_buf_index = 0;
   		} // if (audio_buf_index >= audio_buf_size) close
 
		// Checks the length differences & sets to their minimum
   		len1 = audio_buf_size - audio_buf_index;
   		if (len1 > len)
     			len1 = len;
 
   		// Copies data from audio buffer to SDL stream
  		memcpy(stream, (uint8_t *)audio_buf + audio_buf_index, len1);
 
		// Update values
   		len -= len1;
   		stream += len1;
   		audio_buf_index += len1;
 	} // while close
} // audio_callback() Method end
 
// >>>>>>>>>>  MAIN METHOD (ENTRY POINT)  <<<<<<<<<<<<
 
/* Typically, main() does a couple of things: 1. parse command line parameters, 2. Setup work environment,  3. Setup real-time parameters, 4. Transition to real-time mode, 5. Invoke periodic or sporadic jobs, 6. Transition to background mode, 7. Clean up and exit.
*/
 
int main(int argc, char** argv) {  // Method start
 	int do_exit, ret;
 
/* An rt_task data structure holds task information like: task function to   be executed by real-time task, parameters, priority, stack size allocated for its variables etc.
 http://www.cs.ru.nl/J.Hooman/DES/XenomaiExercises/Exercise-1.html
*/
 	struct rt_task param;
 
 	// Sets up parameters associated with the real-time task
 	init_rt_task_param(&param);
param.exec_cost = ms2ns(WORST_CASE_TIME);
param.period = ms2ns(PERIOD);
 	param.relative_deadline = ms2ns(RELATIVE_DEADLINE);
param.priority = LITMUS_LOWEST_PRIORITY;
 
 // Sets the real-time task’s class : Soft / Hard
// Multimedia applications are usually Soft Real-time Systems
 param.cls = RT_CLASS_SOFT;
 
	// Do not enforce anything during budget overruns
 param.budget_policy = NO_ENFORCEMENT;
 
 	// Register all formats and codecs
   	// av_register_all();
 
// Checks if the format initializations are successful using SDL_Init() which returns 0 on success & negative error code otherwise 
  	if(SDL_Init(SDL_INIT_VIDEO | 
SDL_INIT_AUDIO | 
SDL_INIT_TIMER)) {
   fprintf(stderr, "Unable to initialize SDL - %s !! \n", SDL_GetError());
   exit(1);
 	}
 
// Opens the input file given. Returns 0 on success & negative AVERROR on failure. Expects (AVFormatContext **ps, const char *filename, AVInputFormat *fmt, AVDictionary **options) 
if(avformat_open_input(&avFormatCtx_Ptr, "/home/litmus/Videos/test.mp4", NULL, NULL) != 0) {
          		printf("Unable to open video file given !! \n");
   		return -1;
   	}
 
// Retrieves stream information by reading packets of media files. More useful for file formats with no headers such as MPEG.
 if(avformat_find_stream_info(avFormatCtx_Ptr, NULL) < 0){
          printf("Unable to find stream information !! \n");
   	return -1;
   }
 
// Prints detailed information about the IO format like duration, bitrate, streams, container, programs, codec etc. Expects (AVFormatContext **ps, int index, const char *url, int is_output)
av_dump_format(avFormatCtx_Ptr,0,"/home/litmus/Videos/test.mp4", 0);
 
// Finds the first video stream & audio stream
	videoStream = -1;
 	audioStream = -1;
 
// nb_streams gives the no. of elements in AVFormatContext.streams
// codecpar gives AVCodecParameters associated with that stream
// codec_type gives the type of encoded data
  	for (int i = 0; i < avFormatCtx_Ptr->nb_streams; i++) {
if(avFormatCtx_Ptr->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoStream < 0) 
     			videoStream = i;
   		     
if(avFormatCtx_Ptr->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStream < 0) 
     			audioStream = i;
		
// Once we get the first video stream & audio stream, we can break -> I think
 	}
 
	// Checks if we got the required data
  	if(videoStream == -1) {
   		printf("No video stream found !! \n");
   		return -1;  
}
 
 	if(audioStream == -1) {
   		printf("No audio stream found !! \n");
   		return -1; 
}
 
// Retrieves audio codec
// It finds a registered decoder with a matching Codec ID which is passed as a parameter. Returns the decoder if found, NULL otherwise.
audioCodec_Ptr = avcodec_find_decoder(avFormatCtx_Ptr->streams[audioStream]->codecpar->codec_id);
 
// Checks if we found the decoder
 	if(audioCodec_Ptr == NULL) {
   		fprintf(stderr, "Unsupported audio codec !! \n");
   		return -1;
 	}
 
	// Obtains audio codec context for the audio codec we retrieved earlier
// Allocates an AVCodecContext & sets its fields to default values. Returns 0 on success & negative value on error.
audioCodecCtxRetrieved_Ptr = avcodec_alloc_context3(audioCodec_Ptr);
 
// ​​Fills the Codec context based on the values from the supplied Codec parameters. Returns >= 0 success & negative AVERROR on failure.
ret = avcodec_parameters_to_context(audioCodecCtxRetrieved_Ptr, avFormatCtx_Ptr->streams[videoStream]->codecpar);
 
// Checks if we could obtain the audio context
 	if (ret != 0) { // < 0 would be more apt
     		printf("Unable to obtain audio codec context !! \n");
     		return -1;
 	}
	
	// Copies the obtained audio codec context
 	audioCodecCtx_Ptr = avcodec_alloc_context3(audioCodec_Ptr);
ret = avcodec_parameters_to_context(audioCodecCtx_Ptr, avFormatCtx_Ptr->streams[audioStream]->codecpar);
 
// Checks if we could copy the audio context
 	if (ret != 0) {  // < 0 would be more apt
      		printf("Unable to copy audio codec context !! \n");
       		return -1;
   	}
 
	// Set audio settings for desired specifications
 	desiredSpec.freq = audioCodecCtx_Ptr->sample_rate;
 	desiredSpec.format = AUDIO_S16SYS;
 	desiredSpec.channels = audioCodecCtx_Ptr->channels;
 	desiredSpec.silence = 0;
 	desiredSpec.samples = SDL_AUDIO_BUFFER_SIZE;
	// Invokes audio callback → Makes a function call
 	desiredSpec.callback = audio_callback; 
 	desiredSpec.userdata = audioCodecCtx_Ptr;
 
// Returns 0 if successful. Expects (SDL_AudioSpec *desired, SDL_AudioSpec *obtained)
 	if(SDL_OpenAudio(&desiredSpec, &obtainedSpec) < 0) {
   		fprintf(stderr, "SDL_OpenAudio: %s !! \n", SDL_GetError());
   		return -1;
 	}
 
 	// Initializes the audio AVCodecContext to use the given audio AVCodec
 	if(avcodec_open2(audioCodecCtx_Ptr, audioCodec_Ptr, NULL)){
   		printf("Unable to open audio codec !! \n");
   		return -1;
}
 
	// Initializes the audio packet queue → Makes a function call
packet_queue_init(&audioQ);
 
 	// Start playing audio on the given audio device
 	SDL_PauseAudio(0);
 
 	// Retrieves video codec
// It finds a registered decoder with a matching Codec ID which is passed as a parameter. Returns the decoder if found, NULL otherwise.
videoCodec_Ptr = avcodec_find_decoder(avFormatCtx_Ptr->streams[videoStream]->codecpar->codec_id);
 
// Checks if we found the decoder
 	if(videoCodec_Ptr == NULL) {
   		fprintf(stderr, "Unsupported video codec !! \n");
   		return -1;  	
}
 
 	// Obtains video codec context for the audio codec we retrieved earlier
// Allocates an AVCodecContext & sets its fields to default values. Returns 0 on success & negative value on error.
videoCodecCtxRetrieved_Ptr = avcodec_alloc_context3(videoCodec_Ptr);
 
// ​​Fills the Codec context based on the values from the supplied Codec parameters. Returns >= 0 success & negative AVERROR on failure.
ret = avcodec_parameters_to_context(videoCodecCtxRetrieved_Ptr, avFormatCtx_Ptr->streams[videoStream]->codecpar);
 
// Checks if we could obtain the video context
 	if (ret != 0) { // < 0 would be more apt
       		printf("Could not copy video codec context !! \n");
       		return -1;
   	}
 
	// Copies the obtained video codec context
 	videoCodecCtx_Ptr = avcodec_alloc_context3(videoCodec_Ptr);
ret = avcodec_parameters_to_context(videoCodecCtx_Ptr, avFormatCtx_Ptr->streams[videoStream]->codecpar);
 
// Checks if we could copy the video context
 	if (ret != 0) {  // < 0 would be more apt
      		printf("Unable to copy video codec context !! \n");
       		return -1;
   	}
 
	// Initialize video AVCodecContext to use the given video AVCodec
if(avcodec_open2(videoCodecCtx_Ptr, videoCodec_Ptr, NULL) < 0) {
printf("Unable to open video codec !! \n");
   		return -1;
}
 
// Allocate video frame
 	frame_Ptr = av_frame_alloc();
 
 	// Make a screen to display the video using the macro
 	#ifndef __DARWIN__
screen = SDL_SetVideoMode(videoCodecCtx_Ptr->width, videoCodecCtx_Ptr->height, 0, 0);
 	#else
screen = SDL_SetVideoMode(videoCodecCtx_Ptr->width, videoCodecCtx_Ptr->height, 24, 0);
 	#endif
 
	// Check if we could obtain a screen
 	if(!screen) {
   		fprintf(stderr, "SDL: could not set video mode - exiting\n");
   		exit(1);
 	}
 
  	// Allocate a place to put our YUV image on that screen
	// Takes (int width, int height, Uint32 format, SDL_Surface *display)
	// Returns the SDL_Overlay structure
 	bmp = SDL_CreateYUVOverlay(videoCodecCtx_Ptr->width,
          videoCodecCtx_Ptr->height, SDL_YV12_OVERLAY, screen);
 
// Initializes SWS context for software scaling, Allocates & returns a SwsContext.https://ffmpeg.org/doxygen/3.1/group__libsws.html#gaf360d1a9e0e60f906f74d7d44f9abfdd
 
swsCtx_Ptr = sws_getContext(videoCodecCtx_Ptr->width,
               videoCodecCtx_Ptr->height,
               videoCodecCtx_Ptr->pix_fmt,
               videoCodecCtx_Ptr->width,
               videoCodecCtx_Ptr->height,
               AV_PIX_FMT_YUV420P,
               SWS_BILINEAR,
               NULL,
               NULL,
               NULL ); 
 
 	// The task is in background mode upon startup.
 
// Initializes real-time properties for the entire program & returns 0 on success
CALL( init_litmus() );
 
// Sets up real-time task parameters for a given process & returns 0 on success. We create a sporadic task that does not specify a target partition. 
// set_rt_task_param(pid_t pid, struct rt_task* param)
CALL( set_rt_task_param(gettid(), &param) );
 
// Transitions into real-time mode 
fprintf(stderr, "%s\n", " >>>>> Setting to Real-Time task <<<<< \n");
CALL( task_mode(LITMUS_RT_TASK) );
 
// The task is now executed as a real-time task if the above steps are  successful.
fprintf(stderr,"%s\n", " >>>>> Running Real-Time task <<<<< \n");
 
// Invokes Real-time jobs 
do {
     // Wait until next job is released   
     sleep_next_period();
 
     // Invokes job we want to do → Makes a function call   
     do_exit = job();   
 
     // printf("%d",do_exit);
} while (!do_exit);
 
	// Transitions into background task mode
   	fprintf(stderr,"%s\n", " >>>>> Completed task successfully <<<<< \n");
   	fprintf(stderr,"%s\n", " >>>>> Changing to background task <<<<< \n");
   	CALL( task_mode(BACKGROUND_TASK) );
 
	// Clean up, print results & statistics and exit
// We first try to release the memory which is dynamically allocated
fprintf(stderr,"%s\n", " >>>>> Cleaning <<<<< \n");
 	av_frame_free(&frame_Ptr);
 
   	// Close the codecs
 	avcodec_close(videoCodecCtxRetrieved_Ptr);
 	avcodec_close(videoCodecCtx_Ptr);
 	avcodec_close(audioCodecCtxRetrieved_Ptr);
 	avcodec_close(audioCodecCtx_Ptr);
 
 	// Close the video file
 	avformat_close_input(&avFormatCtx_Ptr);
 	fprintf(stderr,"%s\n","Cleaning done...");
 	fprintf(stderr,"%s\n","Program ending...");
return 0;
} // main() Method end
 
// Method to define the video processing job
int job(void) { // Method start
AVFrame framePicture;
int ret;
 
// Reads next frame of the stream from the AVFormatContext & splits it into packets (Demultiplexing). Expects (AVFormatContext *s, AVPacket *pkt) & returns 0 if success & negative otherwise. It reads a single packet from the frame retrieved.
 	if(av_read_frame(avFormatCtx_Ptr, &packet) >= 0) {
 
// Checks if the video stream is found. Here, the single packet we obtained is the first packet & hence we check if what we have already is the same as this.
     	if(packet.stream_index == videoStream) {
 
     	// Decode video frame
     	// Give the decoder raw & compressed data in an AVPacket
          ret = avcodec_send_packet(videoCodecCtx_Ptr, &packet);
 
// Checks if decoding is successful
         	if (ret < 0) {
             	printf("Unable to send packet for decoding !! \n");
             	return -1;
         	}
	
// Retrieves decoded data as long as the frame is decoded
while (ret >= 0) {
 
         	// Obtains decoded data from decoder
ret = avcodec_receive_frame(videoCodecCtx_Ptr, frame_Ptr);
 
          // Checks if an entire frame was decoded
if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                 break;
           else if (ret < 0) {
                 printf(" Unable to decode video !! \n");
                 return -1;
            }
             else
                 	isFrameDecodingFinished = 1;
           
// Checks if we complete decoding the video frame if(isFrameDecodingFinished) {
	
		// Locks the overlay to directly access pixel data
         		SDL_LockYUVOverlay(bmp);
 
         		framePicture.data[0] = bmp->pixels[0];
         		framePicture.data[1] = bmp->pixels[2];
         		framePicture.data[2] = bmp->pixels[1];
         		framePicture.linesize[0] = bmp->pitches[0];
         		framePicture.linesize[1] = bmp->pitches[2];
         		framePicture.linesize[2] = bmp->pitches[1];
 
// As SDL uses YUV coloring sws_scale(swsCtx_Ptr, (uint8_t const * const *)frame_Ptr->data, frame_Ptr->linesize, 0, videoCodecCtx_Ptr->height, framePicture.data, framePicture.linesize);
 
// To display an overlay, it must be unlocked SDL_UnlockYUVOverlay(bmp);
 
		// Set surface values to display the overlay
         		surfaceRect.x = 0;
        		surfaceRect.y = 0;
         		surfaceRect.w = videoCodecCtx_Ptr->width;
         		surfaceRect.h = videoCodecCtx_Ptr->height;
 
// Blit the overlay to the surface specified when it was created. Blit: Quickly moves block of data into memory for 2D graphics
SDL_DisplayYUVOverlay(bmp, &surfaceRect);
 
// Unreferences the buffer referenced by the packet & resets the remaining packet fields to their defaults 
         		av_packet_unref(&packet);
       			} // if(isFrameDecodingFinished) close 
   		} // while close
} // if(packet.stream_index == videoStream) close
 
// Checks if the audio stream is found. Here, the single packet we obtained is the first packet & hence we check if what we have already is the same as this.
else if(packet.stream_index == audioStream) {
// Puts the audio packet into the queue → Makes a function call
     		packet_queue_put(&audioQ, &packet);
     	} 
 
// If not, it unreferences the buffer referenced by the packet & resets the remaining packet fields to their defaults 
else          
av_packet_unref(&packet);
     
		// Handles the quit event i.e. On Ctrl+C, SDL window quits
// Polls for currently pending events. Returns 1 if there are any else 0. If the event is not NULL, the next event is removed from the queue and stored in that area.
          SDL_PollEvent(&event);
 
	// Check all possibilities
     	switch(event.type) {
       		case SDL_QUIT:
       			quit = 1;
       			SDL_Quit();
       			exit(0);
       			// break;
      		default:
           		break;
     	}
  
   	return 0;
 }  // if(av_read_frame(pFormatCtx, &packet) >= 0) close 
 
else 
     return 1;
} // job() Method end