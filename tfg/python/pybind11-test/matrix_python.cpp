#include "matrix_python.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <iostream>

namespace py = pybind11;

// Important question how to propagate errors in initialization?  Or during a
// read (we use return emtpy vectors, on error, but there are several causes)?
// Normally in c++ you use an exception, but I don't know how well they
// translate with pybind.  An option is to use an error status/error code pair
// in the class, and set them on error, but we have to perform cleaup and this
// may be messy and error-prone.

void producer_async_fun(SafeQueue<Block> &queue,
                        std::atomic<bool> &running,
                        std::atomic<int> &len_queue,
                        int async_sleep_time_ms)
{
    using namespace std::literals::chrono_literals;
    while (running)
    {
        Block x{};
        x.samples = std::vector<int>{};
        x.samples.reserve(BLOCK_SIZE);

        for (size_t i = 0; i < BLOCK_SIZE; i++)
        {
            auto num = std::rand();
            x.samples.emplace_back(num);
        }

        queue.push(x);
        len_queue++;

        std::this_thread::sleep_for(async_sleep_time_ms * 1ms);
    }
}

class AudioConsumer
{
private:
    std::thread bck_thread;
    std::atomic<bool> async_running;
    SafeQueue<Block> queue;
    std::atomic<int> len_queue{0};
    int async_sleep_time_ms{512};

private:
    Block producer_sync();
    void producer_async();
    void setup_resources(int freq, int gain);

public:
    AudioConsumer(int freq, int gain);
    ~AudioConsumer();
    void start_async();
    void stop_async(bool drain_queue = false);
    std::vector<int> read_sync();
    std::vector<int> read_async();
    int len_async_queue();
};

AudioConsumer::AudioConsumer(const int freq, const int gain) : bck_thread{}, async_running{false}, queue{}
{
    // Initialize all the important member variables in the constructor.
    // Note that the thread must be initialized with the default constructor,
    // and later started in start_async, to be able to work correctly.
    // See the link in start_async.
    setup_resources(freq, gain);
}

void AudioConsumer::setup_resources(int freq, int gain)
{
    // Nonsensical function in this case, but, relevant in the case of the matrix.
    auto f = freq;
    auto g = gain;
    g = f;
    f = g;
}

AudioConsumer::~AudioConsumer()
{
    py::gil_scoped_release release; // Release the GIL in this function

    async_running = false;

    if (bck_thread.joinable())
    {
        bck_thread.join();
    }
}

void AudioConsumer::start_async()
{
    using namespace std::literals::chrono_literals;

    py::gil_scoped_release release; // Release the GIL in this function

    if (async_running)
    {
        return;
    }

    async_running = true;

    // As we are in a class all methods have an implicit pointer to the current instance as a first parameter.
    // See https://rafalcieslak.wordpress.com/2014/05/16/c11-stdthreads-managed-by-a-designated-class/ for an example
    // this->bck_thread = std::move(std::thread(&AudioConsumer::producer_async, this)); // With pybind11, spawning a thread doesn't work with a class method.

    this->bck_thread = std::move(std::thread(
        &producer_async_fun,
        std::ref(queue),
        std::ref(async_running),
        std::ref(len_queue),
        async_sleep_time_ms)); // But, it works with a free function
}

void AudioConsumer::stop_async(bool drain_queue /* = false */)
{
    py::gil_scoped_release release; // Release the GIL in this function

    if (!async_running)
    {
        return;
    }

    async_running = false;

    if (drain_queue)
    {
        Block b{};
        while (this->queue.pop(b))
        {
            len_queue--;
        }
    }

    if (bck_thread.joinable())
    {
        bck_thread.join();
    }
}

Block AudioConsumer::producer_sync()
{
    Block x{};
    x.samples.reserve(BLOCK_SIZE);
    for (size_t i = 0; i < BLOCK_SIZE; i++)
    {
        auto num = std::rand();
        x.samples.emplace_back(num);
    }
    return x;
}

std::vector<int> AudioConsumer::read_sync()
{
    Block ret;
    if (async_running)
    {
        return ret.samples; // Return empty vector if we are in async reading mode.
    }

    ret = producer_sync();
    return ret.samples;
}

void AudioConsumer::producer_async()
{
    using namespace std::literals::chrono_literals;

    while (async_running)
    {
        Block x{};
        x.samples = std::move(std::vector<int>{});
        x.samples.reserve(BLOCK_SIZE);
        for (size_t i = 0; i < BLOCK_SIZE; i++)
        {
            auto num = std::rand();
            x.samples.emplace_back(num);
        }
        queue.push(x);
        std::this_thread::sleep_for(async_sleep_time_ms * 1ms);
    }
}

std::vector<int> AudioConsumer::read_async()
{

    Block b{};
    bool have_data = queue.pop(b);
    if (!async_running && !have_data)
    {
        return b.samples; // Empty vector if we are not in async mode or we have no more data to return.
    }

    len_queue--;
    return b.samples;
}

int AudioConsumer::len_async_queue()
{
    return len_queue.load();
}

int add(int i, int j) { return i + j; }

PYBIND11_MODULE(matrix_pybind, m)
{
    m.doc() = "pybind11 example plugin"; // optional module docstring

    m.def("add", &add, "A function that adds two numbers");

    py::classh<AudioConsumer>(m, "AudioConsumer")
        .def(py::init<const int, const int>())
        .def("start_async", &AudioConsumer::start_async)
        .def("stop_async", &AudioConsumer::stop_async, py::arg("drain") = false)
        .def("len_async_queue", &AudioConsumer::len_async_queue)
        .def("read_async", &AudioConsumer::read_async)
        .def("read_sync", &AudioConsumer::read_sync);
}