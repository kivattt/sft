/*
 * We assume 44100hz samplerate and an input range of -1 to 1 in float32
*/

#include <iostream>
#include <fstream>
#include <string>
#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define TAU 6.28318530717958647692528676655900576839433879875021
#define PI  3.14159265358979323846264338327950288419716939937510

using std::string;

struct Freq {
	float amplitude;
	float phase;
};

// 1hz - 256hz
struct FT256 {
	Freq frequencies[256];
};

FT256 sft(float samples[512] /* 2x the fourier transform output */) {
	FT256 ret;

	float sineSignal[512];

	for (int freq = 1; freq <= 256; freq++) {
		for (int i = 0; i < 512; i++) {
			sineSignal[i] = sin(freq * (double(i)/512) * TAU);
		}

		float amplitudeMatch = 0; // TODO: set to lowest value (normalize to -1?)
		float phaseMatch = 0;

		for (int j = 0; j < 256 / float(freq) + 1; j++) {
			float amplitude = 0; // TODO: set to lowest value (normalize to -1?)
			for (int i = 0; i < 512; i++) {
				amplitude += sineSignal[(i + j) % 512] * samples[i];
			}

			if (amplitude > amplitudeMatch) {
				amplitudeMatch = amplitude;
				phaseMatch = float(j) / float(256) * PI * float(freq);
			}
		}

		ret.frequencies[freq-1].amplitude = amplitudeMatch;
		ret.frequencies[freq-1].phase = phaseMatch;
	}

	return ret;
}

struct FTArbitrarySize {
	FT256 *FTs;
	int numFTs;
};

// Cuts off the last samples that aren't a multiple of 512
FTArbitrarySize sft_arbitrary_size(float *samples, int numSamples) {
	FTArbitrarySize ret;

	int numFTs = numSamples/512;
	FT256 FTs[numFTs];

	for (int i = 0; i < numFTs; i++) {
		std::cout << "doing sft #" << i << '\n';
		FTs[i] = sft(&samples[i*512]);
	}

	ret.numFTs = numFTs;
	ret.FTs = FTs;
	return ret;
}

struct SamplesBlock {
	float samples[512];
};

SamplesBlock reconstruct_sft(FT256 &fourierTransform) {
	SamplesBlock ret;

	for (int i = 0; i < 512; i++) {
		ret.samples[i] = 0;
	}

	for (int freq = 1; freq < 256; freq++) {
		float amplitude = fourierTransform.frequencies[freq-1].amplitude;
		float phase = fourierTransform.frequencies[freq-1].phase;

		for (int i = 0; i < 512; i++) {
			// Divided by 512 to lower the amplitude to -1 to 1
			ret.samples[i] += sin(freq * (double(i) / 512) * TAU + phase) * amplitude / 512;
		}
	}
	return ret;
}

void view_ft256(FT256 &fourierTransform) {
	for (int i = 0; i < 256; i++) {
		std::cout << fourierTransform.frequencies[i].amplitude << '\n';
	}
}

void output_ft256(FT256 &fourierTransform, string outputFilename) {
	std::ofstream outputFile(outputFilename, std::ofstream::binary);

	SamplesBlock sb = reconstruct_sft(fourierTransform);
	outputFile.write(reinterpret_cast<char*>(&sb.samples), 512 * 4); // 4 bytes in a 32-bit float
	outputFile.close();
}

void output_ft_arbitrary_size(FTArbitrarySize fourierTransforms, string outputFilename) {
	std::ofstream outputFile(outputFilename, std::ofstream::binary);

	for (int i = 0; i < fourierTransforms.numFTs; i++) {
		SamplesBlock sb = reconstruct_sft(fourierTransforms.FTs[i]);

		outputFile.write(reinterpret_cast<char*>(&sb.samples), 512 * 4); // 4 bytes in a 32-bit float
	}
	outputFile.close();
}

int main() {
	int fd = open("data.raw", O_RDONLY);
	if (fd == -1) {
		return 1;
	}

	struct stat fi;
	if (fstat(fd, &fi) == -1) {
		return 2;
	}

	int size = fi.st_size;
	if (size == 0) {
		return 3;
	}

	float *data = (float*)mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);

	// do stuff
	//FT256 fourierTransform = sft(reinterpret_cast<float*>(data[0]));
	//FT256 fourierTransform = sft(data);

	FTArbitrarySize fourierTransform = sft_arbitrary_size(data, size/4); // 4 bytes in a 32-bit float
	output_ft_arbitrary_size(fourierTransform, "output.raw");

	//view_ft256(fourierTransform);
	//output_ft256(fourierTransform, "output.raw");

	close(fd);
	munmap(data, size);
}
