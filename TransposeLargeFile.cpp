/*
The MIT License(MIT)

Copyright(c)[2015]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE
*/

#include <cstdio>
#include <memory>
#include <array>
#include <string>
#include <cstdlib>

const char FILE_DELIMITER = '\t';
const int FILE_BUF_READ_SIZE_BYTES = 1024 * 512;

#ifdef _WIN32
#define FPOS_LEN(t) t
#else
#define FPOS_LEN(t) t.__pos
#endif

namespace std {
	template <> struct default_delete<FILE> {
		void operator()(FILE *__ptr) const { fclose(__ptr); }
	};
}

using FileDelter = std::unique_ptr<FILE>;

namespace {
	FileDelter OpenReadFile(std::string read_file_name) {
		auto file = FileDelter(fopen(read_file_name.c_str(), "rb"));
		if (!file) {
			printf("Cannot open %s for read\n", read_file_name.c_str());
			exit(-1);
		}
		return file;
	}

	FileDelter OpenWriteFile(std::string write_file_name) {
		auto file = FileDelter(fopen(write_file_name.c_str(), "wb"));
		if (!file) {
			printf("Cannot open %s for write\n", write_file_name.c_str());
			exit(-1);
		}
		return file;
	}
}

// Allows accessing file as if it were a giant array (essentially a poor mimic
// for memory mapped IO).
class FileAsArray {
public:
	// Open input_file for reading. Program exists if it can't be opened.
	FileAsArray(std::string input_file) {
		read_file_ = OpenReadFile(input_file);
		fseek(read_file_.get(), 0L, SEEK_END);
		fpos_t tmp;
		fgetpos(read_file_.get(), &tmp); // Save the file length.
		file_length_ = FPOS_LEN(tmp);
		read_buf_ = std::make_unique<std::array<char, FILE_BUF_READ_SIZE_BYTES>>();
	}

	// Get the byte at position at in the file.  If that part of the file isn't
	// buffered read it into the buffer.
	inline char Get(int64_t at) {
		if (at >= read_buf_start_ && at < read_buf_end_) {
			return read_buf_->data()[at - read_buf_start_];
		}
		else {
			fpos_t tmp;
			FPOS_LEN(tmp) = at;
			fsetpos(read_file_.get(), &tmp);
			auto to_read = (std::size_t)std::min((int64_t)FILE_BUF_READ_SIZE_BYTES,
				(int64_t)(file_length_ - at));
			read_buf_end_ = at + to_read;
			fread(read_buf_->data(), 1, to_read, read_file_.get());
			read_buf_start_ = at;
			return read_buf_->data()[at - read_buf_start_];
		}
	}

	// Take [start, start + length] of the file and write it to write_file.
	// length must be < FILE_BUF_READ_SIZE_BYTES.
	void WriteTo(int64_t start, int64_t length, FILE *write_file) {
		if (length >= FILE_BUF_READ_SIZE_BYTES) {
			printf("Not designed to write more than %d bytes at a time\n",
				FILE_BUF_READ_SIZE_BYTES);
			exit(-1);
		}
		// Accessing the end, then the start will make our read buffer be in range.
		Get(start + length);
		Get(start);
		auto buf_idx = start - read_buf_start_;
		fwrite(&read_buf_->data()[buf_idx], 1, (std::size_t)length, write_file);
	}

	// Length of the file
	int64_t Length() { return file_length_; }

private:
	FileDelter read_file_;
	int64_t file_length_;
	int64_t read_buf_start_;
	int64_t read_buf_end_;
	std::unique_ptr<std::array<char, FILE_BUF_READ_SIZE_BYTES>> read_buf_;
};

// Read in a delimited file (trailing delimiters not allowed, \r not allowed)
// write it out as transposed row col cell value.
void Rewrite(std::string input_file_name, std::string output_file_name) {

	FileAsArray input_file(input_file_name);
	auto output_file = OpenWriteFile(output_file_name);

	int row = 0;
	int col = 0;

	int64_t file_length = input_file.Length();
	int64_t cur_pos = 0;
	int64_t last_start = 0;
	int correct_row_count = 0;
	int processed_rows = 0;

	int64_t one_percent = file_length / 100;
	int64_t cur_percent = 0;

	while (cur_pos < file_length) {
		auto cur_char = input_file.Get(cur_pos);
		bool is_delimiter = cur_char == FILE_DELIMITER;
		bool is_newline = cur_char == '\n';
		bool last_char_in_file = cur_pos == file_length - 1;
		if (is_delimiter || is_newline || last_char_in_file) {
			if (is_newline && input_file.Get(cur_pos - 1) == FILE_DELIMITER) {
				printf("Lines end in delimiter, not supported");
				exit(-1);
			}
			if (is_newline && input_file.Get(cur_pos - 1) == '\r') {
				printf("\\r endlines not supported.");
				exit(-1);
			}
			fprintf(output_file.get(), "%d %d ", row, col);
			if (last_char_in_file) {
				cur_pos++;
			}
			input_file.WriteTo(last_start, cur_pos - last_start, output_file.get());
			fputc('\n', output_file.get());
			last_start = cur_pos + 1; // don't care about the delimiter.
		}

		if (is_delimiter) {
			// Each column becomes a row.
			row++;
		}
		if (is_newline) {
			if (cur_pos > cur_percent) {
				printf("%3.0f%%\n", 100 * (cur_percent / (double)file_length));
				cur_percent += one_percent;
			}
			processed_rows++;
			col++;
			if (correct_row_count == 0) {
				correct_row_count = row;
				printf("Expect col count %d\n", row);
			}
			else if (correct_row_count != row) {
				printf("Row %d has wrong col count %d\n", processed_rows, row);
				// Nuke the file we were writing
				output_file = OpenWriteFile(output_file_name);
				exit(-1);
			}
			row = 0;
		}
		cur_pos++;
	}
}

namespace {
	struct LineBuf {
		int row;
		int col;
		char line[1024];
	};

	void NukeNl(char *line) {
		while (*line != '\n' && *line != '\0') {
			line++;
		}
		if (*line == '\n') {
			*line = '\0';
		}
	}

	void ReadToEndl(char *into, FILE *read) {
		int readc;
		while ((readc = fgetc(read)) != '\n' && readc != EOF) {
			*into = readc;
			into++;
		}
		*into = '\0';
	}
}

// Read in a sorted file row col value.  Write it out as a DELIMITED file
void MakeMatrix(std::string input_file_name, std::string output_file_name) {
	auto input_file = OpenReadFile(input_file_name);
	auto output_file = OpenWriteFile(output_file_name);

	LineBuf last_line;
	LineBuf cur_line;

	fscanf(input_file.get(), "%d %d ", &last_line.row, &last_line.col);
	ReadToEndl(&last_line.line[0], input_file.get());
	NukeNl(&last_line.line[0]);

	while (!feof(input_file.get())) {
		fscanf(input_file.get(), "%d %d ", &cur_line.row, &cur_line.col);
		ReadToEndl(&cur_line.line[0], input_file.get());
		NukeNl(&cur_line.line[0]);

		if (last_line.col != 0) {
			fputc(FILE_DELIMITER, output_file.get());
		}
		fputs(last_line.line, output_file.get());

		if (cur_line.row != last_line.row) {
			fputc('\n', output_file.get());
		}

		last_line = cur_line;
	}

	// Remove if you don't want \n at the end of your file.
	fputc('\n', output_file.get());
}

void PrintHelp(int argc, char *argv[]) {
	printf("Usage %s i input_file output_file\n", argv[0]);
	printf("Reads in input_file, writes each cell to output_file with a "
		"transposed row/column number");
	printf("\n");
	printf("to sort the resulting output use:  sort -n -k1,1 -k2,2 output_file "
		"-o outputfile.sorted\n");
	printf("\n");
	printf("Usage %s m input_file output_file\n", argv[0]);
	printf("Reads in a sorted file from the previous step and turns it back into "
		"a delimited file\n");
}

int main(int argc, char *argv[]) {
	if (argc == 4) {
		if (argv[1][0] == 'i') {
			printf("Rewriting %s to %s\n", argv[2], argv[3]);
			Rewrite(argv[2], argv[3]);
		}
		else if (argv[1][0] == 'm') {
			printf("MakeMatrix %s to %s\n", argv[2], argv[3]);
			MakeMatrix(argv[2], argv[3]);
		}
		return 0;
	}
	else {
		PrintHelp(argc, argv);
	}
}
