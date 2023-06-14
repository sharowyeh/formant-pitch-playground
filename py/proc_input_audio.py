import pyaudio
import soundfile as sf
import numpy as np

def process_audio_frame(frame, output_file):
    # Perform your real-time processing on the audio frame
    # This function will be called for each audio frame

    # Example: Print the size of the audio frame
    print(f"Received audio frame of size {len(frame)} bytes")
    output_file.write(frame)

def stream_audio_input(device_index, sample_rate=44100, channels=2, chunk=1024):
    audio_format = pyaudio.paFloat32

    p = pyaudio.PyAudio()
    # Use device info for input parameters from pyaudio
    dev_info = p.get_device_info_by_index(device_index)
    name = dev_info["name"]
    device_index = dev_info["index"]
    sample_rate = dev_info["defaultSampleRate"]
    channels = dev_info["maxInputChannels"]
    print(f"Retrieve input from {name} sample rate {sample_rate} chs {channels}")
    # NOTE: pyaudio and soundfile require sample_rate in integer type
    sample_rate = int(sample_rate)

    stream = p.open(format=audio_format,
                    channels=channels,
                    rate=sample_rate,
                    input=True,
                    frames_per_buffer=chunk,
                    input_device_index=device_index)

    print("Streaming audio input...")

    try:
        with sf.SoundFile("output.wav", mode="wb", samplerate=sample_rate, channels=channels) as output_file:
            while True:
                data = stream.read(chunk)
                # decode dtype binary to float32 which pyaudio format assigned paFloat32
                # NOTE: based on my unreliable python knowledge, python primitive float is float64, need to force using numpy.float32 as dtype argument
                frame = np.fromstring(data, dtype=np.float32)
                #print(f"debug: {frame}")
                process_audio_frame(frame, output_file)
            
    except KeyboardInterrupt:
        print("Stream stopped by the user.")

    stream.stop_stream()
    stream.close()
    p.terminate()

# Usage example:
input_device_index = 0  # Specify the input device index you want to stream from
stream_audio_input(input_device_index)

