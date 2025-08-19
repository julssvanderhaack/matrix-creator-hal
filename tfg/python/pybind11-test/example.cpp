// This is a test resembiling the desired audio interface to implement in c++
//
// Run this with the command: `g++  -lpthread -lfmt example.cpp -o example && ./example`
// Known to run under g++ 14.2.0 in ubuntu 25.04 plucky
#include "example.hpp"
#include <iostream>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

class AudioConsumer
{
private:
    std::thread bck_thread;
    std::atomic<bool> async_running;
    SafeQueue<Block> queue;
    int async_sleep_time_ms{512};
    int start_async_sleep_ms{10};

private:
    Block producer_sync();
    void producer_async();
    void setup_resources(int freq, int gain);

public:
    AudioConsumer(int freq, int gain);
    ~AudioConsumer();
    void start_async();
    void stop_async();
    Block read_sync();
    Block read_async();
};

AudioConsumer::AudioConsumer(int freq, int gain) : bck_thread{}, async_running{false}, queue{}
{
    // Initialize all the important member variables in the constructor.
    // Note that the thread must be initialized with the default constructor,
    // and later started in start_async, to be able to work correctly.
    // See the link in start_async.
    setup_resources(freq, gain);
}

void AudioConsumer::setup_resources(int freq, int gain)
{
    // Empty function in this case, relevant in the case of the matrix.
    // Important question how to propagate errors in initialization?
    // Normally in c++ you use an exception, but I don't know how well they translate with pybind.
    // An option is to use an error status/error code pair in the class, and set them on error.
    auto f = freq;
    auto g = gain;
    g = f;
    f = g;
}

AudioConsumer::~AudioConsumer()
{
    async_running = false;
    if (bck_thread.joinable())
    {
        bck_thread.join();
    }
}

void AudioConsumer::start_async()
{
    using namespace std::literals::chrono_literals;
    if (async_running)
    {
        return;
    }
    async_running = true;
    // As we are in a class all methods have an implicit pointer to the current instance as a first parameter.
    // See https://rafalcieslak.wordpress.com/2014/05/16/c11-stdthreads-managed-by-a-designated-class/ for an example
    this->bck_thread = std::move(std::thread(&AudioConsumer::producer_async, this));

    // Sleep the current thread to give time for the producer to start.
    std::this_thread::sleep_for(start_async_sleep_ms * 1ms);
}

void AudioConsumer::stop_async()
{
    if (!async_running)
    {
        return;
    }

    async_running = false;
    Block b{};
    while (this->queue.pop(b))
    {
        b = Block{};
    }

    if (bck_thread.joinable())
    {
        bck_thread.join();
    }
    bck_thread = std::thread{};
}

Block AudioConsumer::producer_sync()
{
    Block x{};
    x.samples = std::move(std::vector<int>{});
    x.samples.reserve(BLOCK_SIZE);
    for (size_t i = 0; i < BLOCK_SIZE; i++)
    {
        auto num = std::rand();
        x.samples.emplace_back(num);
    }
    return x;
}

Block AudioConsumer::read_sync()
{
    if (async_running)
    {
        return Block{};
    }
    return producer_sync();
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
        std::cout << "Producing" << std::endl;
        std::this_thread::sleep_for(async_sleep_time_ms * 1ms);
    }
}

Block AudioConsumer::read_async()
{
    if (!async_running)
    {
        return Block{};
    }

    Block b{};
    bool have_data = queue.pop(b);
    if (have_data)
    {
        return b;
    }
    else
    {
        return Block{};
    }
}

int main()
{
    // NOTE: We use scopes to run destructors and allow copy paste with the same code in diferent cases.
    using namespace std::literals::chrono_literals;

    auto aud = AudioConsumer{16000, 3};
    aud.start_async();
    {
        auto v_ex = aud.read_async();
        auto sples = v_ex.samples;
        if (sples.empty()) {std::cerr << "[]" << std::endl;}
        for (auto s : sples)
        {
            std::cerr << s << std::endl;
        }
    }

    std::cerr << "Read async again, expect empty vector" << std::endl;
    {
        auto v_ex = aud.read_async();
        auto sples = v_ex.samples;
        if (sples.empty()) {std::cerr << "[]" << std::endl;}
        for (auto s : sples)
        {
            std::cerr << s << std::endl;
        }
    }

    std::cerr << "Read sync, when async is active expect empty vector" << std::endl;
    {
        auto v_ex = aud.read_sync();
        auto sples = v_ex.samples;
        if (sples.empty()) {std::cerr << "[]" << std::endl;}
        for (auto s : sples)
        {
            std::cerr << s << std::endl;
        }
    }

    std::this_thread::sleep_for(512*1ms);

    std::cerr << "Read async again, after sleep" << std::endl;
    {
        auto v_ex = aud.read_async();
        auto sples = v_ex.samples;
        if (sples.empty()) {std::cerr << "[]" << std::endl;}
        for (auto s : sples)
        {
            std::cerr << s << std::endl;
        }
    }


    std::cerr << "Stop async, begin read sync" << std::endl;
    aud.stop_async();
    {
        auto v_ex = aud.read_sync();
        auto sples = v_ex.samples;
        if (sples.empty()) {std::cerr << "[]" << std::endl;}
        for (auto s : sples)
        {
            std::cerr << s << std::endl;
        }
    }

    std::cerr << "Start async again and read" << std::endl;
    aud.start_async();
    {
        auto v_ex = aud.read_async();
        auto sples = v_ex.samples;
        if (sples.empty()) {std::cerr << "[]" << std::endl;}
        for (auto s : sples)
        {
            std::cerr << s << std::endl;
        }
    }

    std::cerr << "Last don't stop async and expect no crash, as the the thread joins in the destructor" << std::endl;
}