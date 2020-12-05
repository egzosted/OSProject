#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
using namespace std;

const int SECTOR_SIZE = 512;
const int CLUSTER_SIZE = 4096;

struct sector {
	int address;
	int sector_no;
	char bytes[SECTOR_SIZE];
};

// Przeciazenie operatora >>, aby z wejscia pobierac caly sektor
istream& operator>>(std::istream& s, sector& sect) {
	s.read(sect.bytes, sizeof(sect.bytes));
	return s;
}

/*
Funkcja zwraca numer sektora, w którym rozpoczyna siê tablica $MFT
arg1: sektor VBS, do odczytania numeru klastra $MFT
return: numer sektora
*/

int get_MFT_sector_no(sector vbs)
{
	char c_MFT_address[9];
	int ind = 0;
	int i_MFT_address = 0;
	for (int i = 55; i >= 48; i--)
	{
		c_MFT_address[ind] = vbs.bytes[i];
		i_MFT_address <<= 8;
		int j = (int)c_MFT_address[ind];
		i_MFT_address |= j;
		ind++;
	}
	int final_sector = i_MFT_address * 8 + vbs.sector_no;
	return final_sector;
}

int main(int* argc, char** argv)
{
	// odczytanie z linii poleceñ œcie¿ki do pliku tekstowego i obrazu dysku
	const int TEXTFILE = 1;
	const int IMAGEFILE = 2;
	string txt_path(argv[TEXTFILE]);
	string img_path(argv[IMAGEFILE]);

	// otwarcie obrazu dysku w trybie binarnym
	ifstream disk_image(img_path, ios::binary);

	// pobranie sektora vbs dla pen drive Toshiba 16GB (dla innej struktury wystarczy zmieniæ vbs.address)
	sector vbs;
	vbs.address = 0x100200;
	vbs.sector_no = vbs.address / SECTOR_SIZE;
	auto s = istream_iterator<sector>(disk_image);
	advance(s, vbs.sector_no); // przesuniêcie iteratora obrazu dysku z pocz¹tku na sektor vbs
	vbs = *s;
	vbs.address = 0x100200;
	vbs.sector_no = vbs.address / SECTOR_SIZE;
	
	cout << get_MFT_sector_no(vbs);
	//for (auto start = istream_iterator<sector>(disk_image), end = istream_iterator<sector>(); start != end; start++)
	//{
	//	sector read = *start;
	//}
	disk_image.close();
	return 0;
}