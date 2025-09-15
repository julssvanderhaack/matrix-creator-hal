void record_all_channels_wav(SafeQueue<AudioBlock> &queue,
                             matrix_hal::MicrophoneArray *mic_array,
                             std::atomic_bool &running, int duration,
                             std::string folder,
                             std::string initial_wav_filename) {
  const uint32_t SAMPLES_PER_BLOCK = mic_array->NumberOfSamples();
  // const uint16_t NUM_CHANNELS = mic_array->Channels();
  constexpr uint16_t NUM_CHANNELS_ = 8; // Hardcoded because we need constexpr
  const size_t BITS_PER_SAMPLE = 16;
  const size_t BITS_PER_BYTE = 2;
  const uint32_t frequency = mic_array->SamplingRate();

  uint32_t estimated_samples = frequency * duration;
  uint32_t data_size = estimated_samples * BITS_PER_SAMPLE / BITS_PER_BYTE;

  if (initial_wav_filename.empty()) {
    initial_wav_filename = "output";
  }

  // Trim spaces in the filenames strings
  rtrim_string(initial_wav_filename);
  ltrim_string(initial_wav_filename);

  auto time_str = std::string();
  auto now = std::chrono::system_clock::now();
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  char time_buffer[1024]; // This should be enough for a date no?
                          // "%F-%H-%M-%S". Write %F: iso data yy-mm-dd %H:hour,
                          // %M: minute, %S: second
  if (std::strftime(time_buffer, sizeof(time_buffer), "%F-%Hh-%Mm-%Ss",
                    std::localtime(&now_time))) {
    time_str = std::string{time_buffer};
  } else {
    time_str = std::string{"00-00-00-00-00"};
    std::cerr << "Error leyendo fecha" << std::endl;
  }

  std::string desired_extension = ".wav";
  std::string maybe_extension = std::string("");
  if (initial_wav_filename.length() < 4) {
    maybe_extension = desired_extension;
  } else {
    auto idx = initial_wav_filename.rfind('.');
    if (idx != std::string::npos) {
      // There is a . in the filename, check if the extension is .wav (rfind
      // finds the last .)
      std::string file_extension = initial_wav_filename.substr(idx + 1);
      if (file_extension != desired_extension) {
        // The file has other extension
        maybe_extension = desired_extension;
      } else {
        // The file has .wav as an extension
        maybe_extension = std::string("");
      }
    } else // There is no extension
    {
      maybe_extension = desired_extension;
    }
  }

  // Default to current folder
  if (folder.empty()) {
    folder = ".";
  } else { // Remove last / of folder, we add it ourselfs
    auto last_character_folder = folder.back();
    if (last_character_folder == '/') {
      folder.pop_back();
    }
  }

  std::array<std::string, NUM_CHANNELS_> filenames;
  std::array<std::ofstream, NUM_CHANNELS_> filehandles;

  for (size_t i = 0; i < NUM_CHANNELS_; i++) {
    std::string wavname = folder + "/" + "ch" + std::to_string(i + 1) + "-" +
                          time_str + "-" + initial_wav_filename +
                          maybe_extension;
    filenames[i] = wavname;
    filehandles[i] = std::move(
        std::ofstream(wavname, std::ios::binary)); // Is the move needed?
    if (!filehandles[i].is_open()) {
      std::cerr << "Error abriendo " << wavname << "para grabar" << std::endl;
      running = false;
      return;
    }
    write_wav_header(filehandles[i], frequency, BITS_PER_SAMPLE, 1, data_size);
  }

  while (running) {
    AudioBlock block;
    // TODO: We could use wait_pop and avoid this monstruosity.
    if (!queue.pop(block)) {
      using namespace std::literals::chrono_literals;
      double f = frequency / 1.0;
      // Time to read 512 samples, and avoid wakeups during the blocking of the
      // reading thread
      auto time = ((1.0 / f) * SAMPLES_PER_BLOCK);
      auto s_ms = (1ms * time * 1000.0); // *10e3 for converting s to ms.
      std::this_thread::sleep_for(s_ms);
      continue;
    }

    uint32_t block_size = block.samples[0].size();
    std::vector<std::vector<int16_t>> audios(
        NUM_CHANNELS_, std::vector<int16_t>(block_size, 0));

    for (size_t i = 0; i < NUM_CHANNELS_; i++) {
      std::vector<int16_t> ch_audio(block.samples[i]);
      // Write inside the while(running) loop, that way we write all the
      // data as soon as we can take it. Also Write automatically advances the
      // handle
      filehandles[i].write(reinterpret_cast<const char *>(ch_audio.data()),
                           ch_audio.size() * sizeof(int16_t));
    }
  }
}

void capture_audio(matrix_hal::MicrophoneArray *mic_array,
                   SafeQueue<AudioBlock> &queue, std::atomic_bool &running,
                   int duration) {
  const uint32_t BLOCK_SIZE = mic_array->NumberOfSamples();
  const uint16_t CHANNELS = mic_array->Channels();
  auto end_time =
      std::chrono::steady_clock::now() + std::chrono::seconds(duration);

  while (running &&
         (duration == 0 || std::chrono::steady_clock::now() < end_time)) {
    mic_array->Read();
    AudioBlock block;
    block.samples.resize(CHANNELS, std::vector<int16_t>(BLOCK_SIZE));
    for (uint32_t s = 0; s < BLOCK_SIZE; ++s)
      for (uint16_t ch = 0; ch < CHANNELS; ++ch)
        block.samples[ch][s] = mic_array->At(s, ch);
    queue.push(block);
  }
  running = false;
}
