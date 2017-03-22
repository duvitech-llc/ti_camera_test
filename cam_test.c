#include <pthread.h>
#include "util.h"

#define NBUF 6
#define CNT  500000
#define MAX_CAP 6

struct thr_data {
        struct display *disp;
        struct v4l2 *v4l2;
        uint32_t fourcc, width, height;
};

static void
usage(char *name)
{
	MSG("Usage: %s [OPTION]...", name);
	MSG("Test of buffer passing between v4l2 camera and display.");
	MSG("");
	MSG("viddec3test options:");
	MSG("\t-h, --help: Print this help and exit.");
	MSG("\t--multi <num>: Capture from <num> different devices.");
	MSG("\t\t\tEach device name and format would be parsed in the cmdline");
	MSG("");
	disp_usage();
	v4l2_usage();
}

void *
capture_loop(void *arg)
{
	struct thr_data *data = (struct thr_data *)arg;
	struct display *disp = data->disp;
	struct v4l2 *v4l2 = data->v4l2;
	uint32_t fourcc = data->fourcc;
	uint32_t width = data->width, height = data->height;

	struct buffer **buffers, *capt;
	int ret, i;

	buffers = disp_get_vid_buffers(disp, NBUF, fourcc, width, height);
	if (!buffers) {
		return NULL;
	}

	ret = v4l2_reqbufs(v4l2, buffers, NBUF);
	if (ret) {
		return NULL;
	}

	for (i = 0; i < NBUF; i++) {
		v4l2_qbuf(v4l2, buffers[i]);
	}

	ret = v4l2_streamon(v4l2);
	if (ret) {
		return NULL;
	}

	for (i = 1; i < CNT; i++) {

		capt = v4l2_dqbuf(v4l2);
		ret = disp_post_vid_buffer(disp, capt,
			0, 0, width, height);
		if (ret) {
			ERROR("Post buffer failed");
			return NULL;
		}
		v4l2_qbuf(v4l2, capt);
	}
	v4l2_streamoff(v4l2);

	MSG("Ok!");
	return disp;
}

int
main(int argc, char **argv)
{
	struct display *disp;
	struct v4l2 *v4l2;
	pthread_t threads[MAX_CAP];
	struct thr_data tdata[MAX_CAP];
	void *result = NULL;
	int ret = 0, i, multi = 1, idx = 0;
	char c;

	MSG("Opening Display..");
	disp = disp_open(argc, argv);
	if (!disp) {
		usage(argv[0]);
		return 1;
	}


	disp->multiplanar = false;

	for (i = 1; i < argc; i++) {
		if (!argv[i])
			continue;

		if (!strcmp("--multi", argv[i])) {
			argv[i++] = NULL;
			sscanf(argv[i], "%d", &multi);
			argv[i] = NULL;
			if(multi < 1 || multi > MAX_CAP) {
				usage(argv[i]);
				return 1;
			}
		}
	}

	for (i = 0; i < multi; i++) {
		MSG("Opening V4L2..");
		v4l2 = v4l2_open(argc, argv, &tdata[i].fourcc,
			&tdata[i].width, &tdata[i].height);
		if (!v4l2) {
			usage(argv[0]);
			return 1;
		}
		tdata[i].disp = disp;
		tdata[i].v4l2 = v4l2;
	}

	if (check_args(argc, argv)) {
		/* remaining args.. print usage msg */
		usage(argv[0]);
		return 0;
	}

	for (i = 0; i < multi; i++) {
		ret = pthread_create(&threads[i], NULL, capture_loop, &tdata[i]);
		if(ret) {
			MSG("Failed creating thread");
		}
	}

	for (i = 0; i < multi; i++) {
		pthread_join(threads[i], &result);
	}

	disp_close(disp);

	return ret;
}
