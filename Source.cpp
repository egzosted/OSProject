/*
	Autor: Micha³ Piekarski 175456
	Przedmiot: Oprogramowanie Systemowe
	Zadanie: Kopiowanie pliku .txt z systemu hosta na pen drive z systemem plików NTFS

	Za³o¿enie: Plik znajdzie siê w katalogu "Dest", VBS znajduje siê pod adresem 0x100200.
			   Aby zmieniæ te wartoœci nale¿y zmieniæ numer indesku w
			   tablicy MFT folderu docelowego (domyœlnie Dest) poprzez zmianê sta³ej, a
			   do zmiany sektora VBS nale¿y zmieniæ jego adres.

	Uruchomienie: Program bierze argumenty: 
		1. Œcie¿ka do pliku tekstowego w systemie hosta
		2. Obraz dysku z systemem plików.
*/

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
using namespace std;

const int SECTOR_SIZE = 512;
const int CLUSTER_SIZE = 4096;
const int DIRECTORY_MFT_IND = 44; // indeks katalogu docelowego w tablicy MFT
const int SECTORS_PER_INDEX = 2;

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
	// adres tablicy MFT to 8 bitowe pole o offsecie 48, które znajduje siê w VBS
	int MFT_offset = 48;
	// odczyt od najstarszego bajtu zgodnie z little endian
	for (int i = MFT_offset + 7; i >= MFT_offset; i--)
	{
		c_MFT_address[ind] = vbs.bytes[i];
		// przesuwamy o 8 bitów, aby wczytaæ now¹ wartoœæ na najm³odszy bajt
		i_MFT_address <<= 8;
		int j = (int)c_MFT_address[ind];
		// alternatywa ³¹czy poprzednie bajty z aktualnym
		i_MFT_address |= j;
		ind++;
	}
	int final_sector = i_MFT_address * 8 + vbs.sector_no;
	return final_sector;
}

/*
Funkcja zwraca d³ugoœæ atrybutu filename, w którym rozpoczyna siê tablica $MFT
arg1: tablica charów, która przchowa atrybut 0x30 (filename), arg2 nazwa pliku do kopiowania
return: d³ugoœæ atrybutu
*/

int create_file_name(char* bytes, string filename)
{
	bytes[0] = 0x30; // wartoœæ atrybutu
	bytes[14] = 0x03; // attribute Id
	bytes[20] = 0x18; // offset do atrybutu
	bytes[22] = 0x01; // indexed flag
	bytes[24] = 0x2C; // numer indeksu katalogu nadrzêdnego w MFT
	bytes[30] = 0x04; // sequence number katalogu nadrzêdnego
	bytes[72] = 0x20;
	bytes[73] = 0x20;
	int length = 80;
	bytes[80] = filename.size();
	length += 2;
	for (int i = 0; i < filename.size(); i++)
	{
		bytes[82 + 2 * i] = filename[i];
		length += 2;
	}
	if (length % 8 != 0)
	{
		while (length % 8 != 0)
			length++;
	}
	bytes[4] = length; // d³ugoœæ wraz z nag³ówkiem
	bytes[16] = length - 24; // d³ugoœæ atrybutu
	return length;
	
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
	
	int MFT_sector = get_MFT_sector_no(vbs);
	//for (auto start = istream_iterator<sector>(disk_image), end = istream_iterator<sector>(); start != end; start++)
	//{
	//	sector read = *start;
	//}

	// utworzenie nag³ówka dla rekordu plikowego

	// utworzenie atrybutu 0x10 dla rekordu plikowego

	// utworzenie atrybutu 0x30 dla rekordu plikowego
	char filename_bytes[512] = { 0 };
	int filename_length = create_file_name(filename_bytes, txt_path);

	// utworzenie atrybutu 0x40 dla rekordu plikowego

	// utworzenie atrybutu 0x80 dla rekordu plikowego

	// do³¹czenie wêz³a dla pliku w katalogie nadrzêdnym
	// odczytanie sektora katalogu nadrzêdnego dla kopiowanego pliku
	//int dest_sector = MFT_sector + SECTORS_PER_INDEX * DIRECTORY_MFT_IND;
	//advance(s, dest_sector - vbs.sector_no);
	//sector dest = *s;
	disk_image.close();
	return 0;
}