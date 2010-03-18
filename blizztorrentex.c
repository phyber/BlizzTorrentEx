/*
 * Torrent extractor for Blizzard downloaders, in C.
 * Based on the WowTorrentEx source that is around but doesn't seem to have
 * an author name inside it.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>

size_t find_torrent_data(FILE *file, size_t *len) {
	size_t fend;
	int ret;

	struct {
		int sig;
		int offset;
	} sigAndOffset;

	/* Find the end of the file.. */
	ret = fseek(file, -8, SEEK_END);
	if (ret != 0) {
		fprintf(stderr, "Error seeking in file: %s\n", strerror(errno));
		return 0;
	}
	fend = ftell(file);

	/* Hunt backwards through the file for the "PSER" signature.
	 * A 4 byte signed int offset follows that.
	 */
	while (fend > 0) {
		fseek(file, fend, SEEK_SET);
		ret = fread(&sigAndOffset, sizeof(sigAndOffset), 1, file);
		if (ret < 1) {
			break;
		}

		// If we find the sig...
		if (sigAndOffset.sig == 0x52455350) {
			printf("Torrent data found!\n");
			*len = -sigAndOffset.offset;
			return fend + sigAndOffset.offset;
		}
		fend--;
	}

	return 0;
}

int copy_file_part(FILE *file, FILE *fout, size_t start, size_t len) {
	char buf[4096];
	memset(buf, 0, sizeof(buf));
	int ret;
	size_t readbytes;

	printf("Copying torrent data to output file...\n");
	printf("Starting at: 0x%08zx with length %zd bytes\n", start, len);

	// Seek to the torrent start
	ret = fseek(file, start, SEEK_SET);
	if (ret != 0) {
		fprintf(stderr, "Error seeking to torrent start\n");
		return 1;
	}

	while (len) {
		if (len > sizeof(buf)) {
			ret = fread(buf, sizeof(buf), 1, file);
			readbytes = sizeof(buf);
			start += sizeof(buf);
			len -= sizeof(buf);
		}
		else {
			ret = fread(buf, len, 1, file);
			readbytes = len;
			start += len;
			len -= len;
		}
		if (ret != 0) {
			printf(".");
			ret = fwrite(buf, readbytes, 1, fout);
			if (ret == 0) {
				fprintf(stderr, "Error while writing torrent: %s\n", strerror(errno));
				return 1;
			}
		}
		else {
			break;
		}
		fseek(file, start, SEEK_SET);
	}
	printf("\n");

	ret = fclose(fout);
	// Warn about the closing error, but don't throw an error.
	if (ret != 0) {
		fprintf(stderr, "Error closing output file: %s\n", strerror(errno));
	}

	return 0;
}

int main (int argc, char **argv) {
	FILE *file;
	FILE *fout;
	size_t len = 0;
	size_t start;
	int ret;

	if (argc < 3) {
		fprintf(stderr, "Please supply a file to extract the .torrent from and the file to place the torrent data in.\n");
		fprintf(stderr, "Usage: %s <infile> <outfile>\n", argv[0]);
		return 1;
	}

	/*
	 * Open the input file.
	 */
	file = fopen(argv[1], "rb");
	if (file == NULL) {
		fprintf(stderr, "Error opening %s for reading: %s\n", argv[1], strerror(errno));
		return 1;
	}

	/*
	 * Hunt for the torrent data offset
	 */
	printf("Extracting from %s\n", argv[1]);
	start = find_torrent_data(file, &len);
	if (start == 0) {
		fprintf(stderr, "Couldn't find .torrent data\n");
		return 1;
	}

	/*
	 * Open the output file
	 */
	fout = fopen(argv[2], "wb");
	if (fout == NULL) {
		fprintf(stderr, "Error opening %s for writing: %s\n", argv[2], strerror(errno));
		return 1;
	}

	/*
	 * Copy torrent data to the output
	 */
	ret = copy_file_part(file, fout, start, len);
	if (ret != 0) {
		fprintf(stderr, "Error copying torrent data.\n");
		return 1;
	}

	/*
	 * Yay!
	 */
	ret = fclose(file);
	if (ret != 0) {
		fprintf(stderr, "Error closing input file: %s\n", strerror(errno));
	}

	printf("All done!\n");

	return 0;
}
