#include "cava/cavacore.h"
#include <portaudio.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// include sockets for linux
#ifdef __linux__
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

// include sockets for windows
#ifdef _WIN32
// TODO: add includes
#endif

const char *LED_IP = "192.168.0.122";
const unsigned int LED_PORT = 1337;

const unsigned int LEDs = 150;
const unsigned int BARS = LEDs / 2;

struct pa_data
{
    struct cava_plan *plan;
    int sockfd;
#ifdef __linux__
    struct sockaddr_in addr_in;
    socklen_t addr_len;
#endif
};

typedef struct
{
    double r; // a fraction between 0 and 1
    double g; // a fraction between 0 and 1
    double b; // a fraction between 0 and 1
} rgb;

typedef struct
{
    double h; // angle in degrees
    double s; // a fraction between 0 and 1
    double v; // a fraction between 0 and 1
} hsv;

static hsv rgb2hsv(rgb in);
static rgb hsv2rgb(hsv in);

hsv rgb2hsv(rgb in)
{
    hsv out;
    double min, max, delta;

    min = in.r < in.g ? in.r : in.g;
    min = min < in.b ? min : in.b;

    max = in.r > in.g ? in.r : in.g;
    max = max > in.b ? max : in.b;

    out.v = max; // v
    delta = max - min;
    if (delta < 0.00001)
    {
        out.s = 0;
        out.h = 0; // undefined, maybe nan?
        return out;
    }
    if (max > 0.0)
    {                          // NOTE: if Max is == 0, this divide would cause a crash
        out.s = (delta / max); // s
    }
    else
    {
        // if max is 0, then r = g = b = 0
        // s = 0, h is undefined
        out.s = 0.0;
        out.h = 0.0; // its now undefined
        return out;
    }
    if (in.r >= max)                   // > is bogus, just keeps compilor happy
        out.h = (in.g - in.b) / delta; // between yellow & magenta
    else if (in.g >= max)
        out.h = 2.0 + (in.b - in.r) / delta; // between cyan & yellow
    else
        out.h = 4.0 + (in.r - in.g) / delta; // between magenta & cyan

    out.h *= 60.0; // degrees

    if (out.h < 0.0)
        out.h += 360.0;

    return out;
}

rgb hsv2rgb(hsv in)
{
    double hh, p, q, t, ff;
    long i;
    rgb out;

    if (in.s <= 0.0)
    { // < is bogus, just shuts up warnings
        out.r = in.v;
        out.g = in.v;
        out.b = in.v;
        return out;
    }
    hh = in.h;
    if (hh >= 360.0)
        hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = in.v * (1.0 - in.s);
    q = in.v * (1.0 - (in.s * ff));
    t = in.v * (1.0 - (in.s * (1.0 - ff)));

    switch (i)
    {
    case 0:
        out.r = in.v;
        out.g = t;
        out.b = p;
        break;
    case 1:
        out.r = q;
        out.g = in.v;
        out.b = p;
        break;
    case 2:
        out.r = p;
        out.g = in.v;
        out.b = t;
        break;

    case 3:
        out.r = p;
        out.g = q;
        out.b = in.v;
        break;
    case 4:
        out.r = t;
        out.g = p;
        out.b = in.v;
        break;
    case 5:
    default:
        out.r = in.v;
        out.g = p;
        out.b = q;
        break;
    }
    return out;
}

void rainbow_visualiser(double *output, uint8_t *led_buf, struct pa_data *pa_data)
{
    // buffer left channel
    for (int i = 0; i < LEDs / 2; ++i, led_buf += 3)
    {
        hsv color;
        color.h = ((float)i / (LEDs / 2)) * 360;
        color.s = 1;
        color.v = output[i] / pa_data->plan->average_max;

        rgb rcolor = hsv2rgb(color);
        led_buf[0] = rcolor.r * 255;
        led_buf[1] = rcolor.g * 255;
        led_buf[2] = rcolor.b * 255;
    }

    // buffer right channel
    for (int i = LEDs - 1; i >= LEDs / 2; --i, led_buf += 3)
    {
        hsv color;
        color.h = ((float)(i - LEDs / 2) / (LEDs / 2)) * 360;
        color.s = 1;
        color.v = output[i] / pa_data->plan->average_max;

        rgb rcolor = hsv2rgb(color);
        led_buf[0] = rcolor.r * 255;
        led_buf[1] = rcolor.g * 255;
        led_buf[2] = rcolor.b * 255;
        // printf("%d %d %d\n", color.h, color.s, color.v);
    }
}

void max_freq_visualiser(double *output, uint8_t *led_buf, struct pa_data *pa_data)
{
    // figure out the max bar and output only that color + its intensity
    static float last_max_i = -1;
    int max_i = 0;
    for (int i = 0; i < LEDs / 2; ++i)
        if (output[i] > output[max_i])
            max_i = i;
    if (last_max_i = -1)
        last_max_i = max_i;
    else
        last_max_i += (last_max_i - max_i) * 1;

    hsv color;
    color.h = ((float)last_max_i / (LEDs / 2)) * 360;
    color.s = 1;
    color.v = output[(int)last_max_i] / pa_data->plan->average_max;

    rgb rcolor = hsv2rgb(color);
    for (int i = 0; i < LEDs * 3; i += 3)
        led_buf[i] = rcolor.r * 255,
        led_buf[i + 1] = rcolor.g * 255,
        led_buf[i + 2] = rcolor.b * 255;
}

void bass_visualiser(double *output, uint8_t *led_buf, struct pa_data *pa_data)
{
    // figure out the max bar and output only that color + its intensity
    double max_val = 0;
    for (int i = 0; i < LEDs / 2 / 4; ++i)
        max_val = output[i] > max_val ? output[i] : max_val;

    hsv color;
    color.h = 360 / 12;
    color.s = 1;
    color.v = max_val / pa_data->plan->average_max;

    rgb rcolor = hsv2rgb(color);
    for (int i = 0; i < LEDs * 3; i += 3)
        led_buf[i] = rcolor.r * 255,
        led_buf[i + 1] = rcolor.g * 255,
        led_buf[i + 2] = rcolor.b * 255;
}

void avg_loudness_visualiser(double *output, uint8_t *led_buf, struct pa_data *pa_data)
{
    double avg = 0;
    for (int i = 0; i < LEDs; ++i)
        avg += output[i];
    avg /= LEDs;

    hsv color;
    color.h = 0;
    color.s = 1;
    color.v = avg / pa_data->plan->average_max;

    rgb rcolor = hsv2rgb(color);
    for (int i = 0; i < LEDs * 3; i += 3)
        led_buf[i] = rcolor.r * 255,
        led_buf[i + 1] = rcolor.g * 255,
        led_buf[i + 2] = rcolor.b * 255;
}

static int pa_callback(const void *input_buf, void *output_buf, unsigned long frames_per_buf, const PaStreamCallbackTimeInfo *duration, PaStreamCallbackFlags status, void *userdata)
{
    struct pa_data *pa_data = (struct pa_data *)userdata;
    const float *input = (const float *)input_buf;
    double inputconv[frames_per_buf * 2];
    for (ulong i = 0; i < frames_per_buf * 2; ++i)
        inputconv[i] = input[i];
    (void)output_buf;

    double output[LEDs];
    cava_execute(inputconv, frames_per_buf * 2, output, pa_data->plan);
    printf("%lf %lf %lf %lf %lf\n", output[0], output[1], output[2], output[3], output[4]);

    uint8_t buf[LEDs * 3];
    rainbow_visualiser(output, buf, pa_data);
    // max_freq_visualiser(output, buf, pa_data);
    // bass_visualiser(output, buf, pa_data);
    // avg_loudness_visualiser(output, buf, pa_data);

    // send data to strip
#ifdef __linux__
    size_t bytes_sent = sendto(pa_data->sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&pa_data->addr_in, pa_data->addr_len);
#endif
#ifdef _WIN32
// TODO: windows impl
    size_t bytes_send = -1;
#endif
    if (bytes_sent == -1)
        printf("error\n");
    return 0;
}

int main()
{
    struct cava_plan *plan = cava_init(BARS, 44100, 2, 0, 0.2, 50, 10000);

#ifdef __linux__
    struct sockaddr_in addr_in = {
        .sin_family = AF_INET,
        .sin_port = htons(LED_PORT)};
    inet_aton(LED_IP, &addr_in.sin_addr);
#endif
#ifdef _WIN32
// TODO: windows impl
#endif

    struct pa_data data =
        {
            .plan = plan,
            .sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP),
#ifdef __linux
            .addr_in = addr_in,
            .addr_len = sizeof(addr_in)
#endif
#ifdef _WIN32
// TODO: windows impl
#endif
        };

    Pa_Initialize();

    int numDevices = Pa_GetDeviceCount();
    int selectedDevice = 0;
    for (int i = 0; i < numDevices; ++i)
    {
        PaDeviceInfo *devInfo = Pa_GetDeviceInfo(i);

#ifdef __linux__
        if (strcmp(devInfo->name, "pulse") == 0)
        {
            selectedDevice = i;
            break;
        }
#endif
#ifdef _WIN32
// TODO: windows impl
#endif
    }

    PaStream *stream;
    PaStreamParameters inputParams;
    inputParams.channelCount = 2;
    inputParams.device = selectedDevice;
    inputParams.hostApiSpecificStreamInfo = NULL;
    inputParams.sampleFormat = paFloat32;
    inputParams.suggestedLatency = Pa_GetDeviceInfo(selectedDevice)->defaultLowInputLatency;

    Pa_OpenStream(&stream, &inputParams, NULL, 44100, 44100 / 30, paNoFlag, pa_callback, &data);
    PaError err = Pa_StartStream(stream);

    while (1)
    {
    }
    Pa_CloseStream(stream);
    Pa_Terminate();
    return 0;
}