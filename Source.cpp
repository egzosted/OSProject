#include <iostream>
#include <fstream>
#include <string>
using namespace std;

const int SECTOR_SIZE = 512;
const int CLUSTER_SIZE = 4096;

struct sector {
	int address;
	char bytes[SECTOR_SIZE];
};

// Przeci¹¿enie operatora >>, aby z wejœcia pobieraæ ca³y sektor
istream& operator>>(std::istream& s, sector& sect) {
	s.read(sect.bytes, sizeof(sect.bytes));
	return s;
}

int main(int* argc, char** argv)
{
	const int TEXTFILE = 1;
	const int IMAGEFILE = 2;
	string txt_path(argv[TEXTFILE]);
	string img_path(argv[IMAGEFILE]);
	ifstream disk_image(img_path, ios::binary);
	sector vbs;
	vbs.address = 0x100200;
	int i = 0, limit = 1;
	for (auto start = istream_iterator<sector>(disk_image), end = istream_iterator<sector>(); start != end; start++)
	{
		sector read = *start;
		cout << read.bytes[0];
		i++;
		if (i == limit)
			break;
	}
	disk_image.close();
	return 0;
}