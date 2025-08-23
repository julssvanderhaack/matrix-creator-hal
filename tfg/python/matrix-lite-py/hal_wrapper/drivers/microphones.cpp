#include "microphones.h"
#include "../../../../../cpp/driver/matrixio_bus.h"
#include "../../../../../cpp/driver/microphone_array.h"
#include "../../../../../cpp/driver/microphone_core.h"
#include "../matrix.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <queue>
#include <vector>

namespace py = pybind11;

template <typename T> class SafeQueue {
private:
  std::queue<T> queue_;
  std::mutex mutex_;
  std::condition_variable cond_;

public:
  void push(const T &item) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.push(item);
    }
    cond_.notify_one();
  }

  bool pop(T &item) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.empty())
      return false;
    item = queue_.front();
    queue_.pop();
    return true;
  }

  void wait_pop(T &item) {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [&] { return !queue_.empty(); });
    item = queue_.front();
    queue_.pop();
  }
};

struct AudioBlock {
  std::vector<std::vector<int16_t>> samples;
};

void producer_async_fun(matrix_hal::MicrophoneArray &mic_array,
                        SafeQueue<AudioBlock> &queue,
                        std::atomic<bool> &running,
                        std::atomic<int> &len_queue) {

  const uint32_t BLOCK_SIZE = mic_array.NumberOfSamples();
  const uint16_t CHANNELS = mic_array.Channels();

  while (running) {
    AudioBlock block;
    block.samples.resize(CHANNELS, std::vector<int16_t>(BLOCK_SIZE));
    mic_array.Read();

    for (uint32_t s = 0; s < BLOCK_SIZE; ++s) {
      for (uint16_t ch = 0; ch < CHANNELS; ++ch) {
        block.samples[ch][s] = mic_array.At(s, ch);
      }
    }
    queue.push(block);
    len_queue++;
  }
}

class Microphones {
private:
  std::thread bck_thread;
  std::atomic<bool> async_running;
  SafeQueue<AudioBlock> queue;
  std::atomic<int> len_queue{0};

  // NOTE: I don't like this, and we could use pointers to initialize them in
  // the constructor but... Initialization of class members is always in the
  // order that the class members appear in your class definition. So the
  // mic_array passed to mic core should already be initialized.
  matrix_hal::MicrophoneArray mic_array{false};
  matrix_hal::MicrophoneCore mic_core{mic_array};

private:
  AudioBlock producer_sync();
  void producer_async();

public:
  Microphones(int freq, int gain);
  ~Microphones();
  void start_async();
  void stop_async(bool drain_queue = false);
  std::vector<std::vector<int16_t>> read_sync();
  std::vector<std::vector<int16_t>> read_async();
  int len_async_queue();
  bool is_async_running();
};

Microphones::Microphones(const int freq, const int gain)
    : bck_thread{}, async_running{false}, queue{} {
  bool busReady = bus.Init();

  // Mic array
  mic_array.Setup(&bus);
  mic_array.SetSamplingRate(freq);
  if (gain > 0) {
    mic_array.SetGain(gain);
  }

  mic_core.Setup(&bus);
}

Microphones::~Microphones() {
  py::gil_scoped_release release; // Release the GIL in this function

  async_running = false;

  if (bck_thread.joinable()) {
    bck_thread.join();
  }
}

void Microphones::start_async() {
  py::gil_scoped_release release; // Release the GIL in this function

  if (async_running) {
    return;
  }

  async_running = true;

  // As we are in a class all methods have an implicit pointer to the current
  // instance as a first parameter. See
  // https://rafalcieslak.wordpress.com/2014/05/16/c11-stdthreads-managed-by-a-designated-class/
  // for an example this->bck_thread =
  // std::move(std::thread(&AudioConsumer::producer_async, this)); // With
  // pybind11, spawning a thread doesn't work with a class method.

  this->bck_thread = std::move(
      std::thread(&producer_async_fun, std::ref(mic_array), std::ref(queue),
                  std::ref(async_running),
                  std::ref(len_queue))); // But, it works with a free function
}

void Microphones::stop_async(bool drain_queue /* = false */) {
  py::gil_scoped_release release; // Release the GIL in this function

  if (!async_running) {
    return;
  }

  async_running = false;

  if (drain_queue) {
    AudioBlock b{};
    while (this->queue.pop(b)) {
      len_queue--;
    }
  }

  if (bck_thread.joinable()) {
    bck_thread.join();
  }
}

AudioBlock Microphones::producer_sync() {
  const uint32_t BLOCK_SIZE = mic_array.NumberOfSamples();
  const uint16_t CHANNELS = mic_array.Channels();

  AudioBlock block;
  block.samples.resize(CHANNELS, std::vector<int16_t>(BLOCK_SIZE));
  mic_array.Read();

  for (uint32_t s = 0; s < BLOCK_SIZE; ++s) {
    for (uint16_t ch = 0; ch < CHANNELS; ++ch) {
      block.samples[ch][s] = mic_array.At(s, ch);
    }
  }
  return block;
}

std::vector<std::vector<int16_t>> Microphones::read_sync() {
  AudioBlock ret;
  if (async_running) {
    // Return empty vector if we are in async reading mode.
    return ret.samples;
  }

  ret = producer_sync();
  return ret.samples;
}

std::vector<std::vector<int16_t>> Microphones::read_async() {

  AudioBlock b{};
  bool have_data = queue.pop(b);
  if (!async_running && !have_data) {
    return b.samples; // Empty vector if we are not in async mode or we have
                      // no more data to return.
  }

  len_queue--;
  return b.samples;
}

int Microphones::len_async_queue() { return len_queue.load(); }

bool Microphones::is_async_running() { return async_running.load(); }

void init_microphones(py::module &m) {
  py::class_<Microphones>(m, "microphone")
      .def(py::init<const int, const int>())
      .def("start_async", &Microphones::start_async)
      .def("stop_async", &Microphones::stop_async, py::arg("drain") = false)
      .def("len_async_queue", &Microphones::len_async_queue)
      .def("is_async_running", &Microphones::is_async_running)
      .def("read_async", &Microphones::read_async)
      .def("read_sync", &Microphones::read_sync);
}
