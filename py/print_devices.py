import pyaudio

def list_devices():
	p = pyaudio.PyAudio()
	cnt = p.get_device_count()

	print(f"Available devices:{cnt}")
	print("-----------------------")

	for i in range(cnt):
		info = p.get_device_info_by_index(i)
		name = info["name"]
		idx = info["index"]
		inp_chs = info["maxInputChannels"]
		opt_chs = info["maxOutputChannels"]
		sample_rate = info["defaultSampleRate"]

		print(f"Dev{idx} Name:{name} In:{inp_chs} Out:{opt_chs} SR:{sample_rate}")
		#print(f"   {info}")

	p.terminate()

list_devices()

