/*
	Autor: Micha� Piekarski 175456
	Przedmiot: Oprogramowanie Systemowe
	Zadanie: Kopiowanie pliku .txt z systemu hosta na pen drive z systemem plik�w NTFS

	Za�o�enie: Plik znajdzie si� w katalogu "Dest", VBS znajduje si� pod adresem 0x100200.
			   Aby zmieni� te warto�ci nale�y zmieni� numer indesku w
			   tablicy MFT folderu docelowego (domy�lnie Dest) poprzez zmian� sta�ej, a
			   do zmiany sektora VBS nale�y zmieni� jego adres.

	Uruchomienie: Program bierze argumenty: 
		1. �cie�ka do pliku tekstowego w systemie hosta
		2. Obraz dysku z systemem plik�w.
*/

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
using namespace std;

const int SECTOR_SIZE = 512;
const int CLUSTER_SIZE = 4096;
const int DIRECTORY_MFT_IND = 38; // indeks katalogu docelowego w tablicy MFT
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
Funkcja zwraca numer sektora, w kt�rym rozpoczyna si� tablica $MFT
arg1: sektor VBS, do odczytania numeru klastra $MFT
return: numer sektora
*/

int get_MFT_sector_no(sector vbs)
{
	char c_MFT_address[9];
	int ind = 0;
	int i_MFT_address = 0;
	// adres tablicy MFT to 8 bitowe pole o offsecie 48, kt�re znajduje si� w VBS
	int MFT_offset = 48;
	// odczyt od najstarszego bajtu zgodnie z little endian
	for (int i = MFT_offset + 7; i >= MFT_offset; i--)
	{
		c_MFT_address[ind] = vbs.bytes[i];
		// przesuwamy o 8 bit�w, aby wczyta� now� warto�� na najm�odszy bajt
		i_MFT_address <<= 8;
		int j = (int)c_MFT_address[ind];
		// alternatywa ��czy poprzednie bajty z aktualnym
		i_MFT_address |= j;
		ind++;
	}
	int final_sector = i_MFT_address * 8 + vbs.sector_no;
	return final_sector;
}

/*
Funkcja zwraca d�ugo�� atrybutu filename, w kt�rym rozpoczyna si� tablica $MFT
arg1: tablica char�w, kt�ra przchowa atrybut 0x30 (filename), arg2 nazwa pliku do kopiowania
return: d�ugo�� atrybutu
*/

int create_file_name(char* bytes, string filename)
{
	bytes[0] = 0x30; // warto�� atrybutu
	bytes[14] = 0x03; // attribute Id
	bytes[20] = 0x18; // offset do atrybutu
	bytes[22] = 0x01; // indexed flag
	bytes[24] = 0x25; // numer indeksu katalogu nadrz�dnego w MFT
	bytes[30] = 0x01; // sequence number katalogu nadrz�dnego
	bytes[80] = 0x20;
	bytes[81] = 0x20;
	int length = 88;
	bytes[88] = filename.size();
	length += 2;
	for (int i = 0; i < filename.size(); i++)
	{
		bytes[90 + 2 * i] = filename[i];
		length += 2;
	}
	if (length % 8 != 0)
	{
		while (length % 8 != 0)
			length++;
	}
	bytes[4] = length; // d�ugo�� wraz z nag��wkiem
	bytes[16] = length - 24; // d�ugo�� atrybutu
	return length;
}

/*
Funkcja zwraca d�ugo�� atrybutu filename, w kt�rym rozpoczyna si� tablica $MFT
arg1: tablica char�w, kt�ra przchowa atrybut 0x30 (filename), arg2 nazwa pliku do kopiowania
return: d�ugo�� atrybutu
*/

int create_file_header(char* bytes)
{
	int length = 0;
	// uzupe�nienie magic number
	bytes[0] = 0x46;
	bytes[1] = 0x49;
	bytes[2] = 0x4C;
	bytes[3] = 0x45;
	// offset do pola update sequence (pole 2 bitowe)
	bytes[4] = 0x30;
	// update sequence size in word (2 bajty)
	bytes[5] = 0x03;
	// ??? $LogFile sequence number

	// sequence number
	bytes[16] = 0x01;
	// hard link count
	bytes[18] = 0x01;
	// offset to first attribute
	bytes[20] = 0x38;
	bytes[22] = 0x01;
	// real size of file record
	bytes[24] = 0x00;
	bytes[26] = 0x04;
	// allocated size of file record
	bytes[28] = 0x00;
	bytes[30] = 0x04;
	// next attribute id
	bytes[40] = 0x05;
	// number of MFT record
	bytes[46] = 0x2E;
	// update sequence number
	bytes[48] = 0x01;
	length = 56;
	return length;
}

int create_standard_attribute(char* bytes)
{
	int length = 96;
	bytes[0] = 0x10;
	bytes[4] = length; // d�ugo�� wraz z nag��wkiem
	bytes[20] = 0x18;
	bytes[16] = length - 24; // d�ugo�� atrybutu
	return length;
}

int create_data_attribute(char* bytes)
{
	int length = 24;
	bytes[0] = 0x10;
	bytes[4] = length; // d�ugo�� wraz z nag��wkiem
	bytes[10] = 0x18; // offset to name
	bytes[14] = 0x01; // attribute id
	bytes[20] = 0x18;
	bytes[16] = 0x00; // d�ugo�� atrybutu
	return length;
}

int main(int* argc, char** argv)
{
	// odczytanie z linii polece� �cie�ki do pliku tekstowego i obrazu dysku
	const int TEXTFILE = 1;
	const int IMAGEFILE = 2;
	string txt_path(argv[TEXTFILE]);
	string img_path(argv[IMAGEFILE]);

	// otwarcie obrazu dysku w trybie binarnym
	ifstream disk_image(img_path, ios::binary);

	// pobranie sektora vbs dla partycji NTFS w folderze projektu (ntfs.vhd) (dla innej struktury wystarczy zmieni� vbs.address)
	sector vbs;
	vbs.address = 0x1000000;
	vbs.sector_no = vbs.address / SECTOR_SIZE;
	auto s = istream_iterator<sector>(disk_image);
	advance(s, vbs.sector_no); // przesuni�cie iteratora obrazu dysku z pocz�tku na sektor vbs
	vbs = *s;
	vbs.address = 0x1000000;
	vbs.sector_no = vbs.address / SECTOR_SIZE;
	
	int MFT_sector = get_MFT_sector_no(vbs);

	advance(s, MFT_sector - vbs.sector_no); // przesuni�cie iteratora obrazu dysku z pocz�tku na sektor MFT
	sector mft;

	advance(s, DIRECTORY_MFT_IND * SECTORS_PER_INDEX); // przesuni�cie iteratora na folder Destination
	auto s_file = s;
	advance(s_file, SECTORS_PER_INDEX);
	// szukanie wolnego miejsca na nowy rekord plikowy
	sector record;
	int record_sector = 719606;
	while (true)
	{
		record = *s_file;
		if (record.bytes[0] == 0)
			break;
		advance(s_file, SECTORS_PER_INDEX);
		record_sector += SECTORS_PER_INDEX;
	}


	// utworzenie nag��wka dla rekordu plikowego
	char header_bytes[512] = { 0 };
	int header_length = create_file_header(header_bytes);

	// utworzenie atrybutu 0x10 dla rekordu plikowego
	char standard_bytes[512] = { 0 };
	int standard_length = create_standard_attribute(standard_bytes);

	// utworzenie atrybutu 0x30 dla rekordu plikowego
	char filename_bytes[512] = { 0 };
	int filename_length = create_file_name(filename_bytes, txt_path);

	// utworzenie atrybutu 0x80 dla rekordu plikowego
	char data_bytes[512] = { 0 };
	int data_length = create_data_attribute(data_bytes);

	header_bytes[24] = 0x24;
	header_bytes[25] = 0x01;

	char file_bytes[512] = { 0 };
	// sklejenie atrybut�w w jeden sektor
	int j = 0;
	for (int i = 0; i < header_length; i++)
	{
		file_bytes[j] = header_bytes[i];
		j++;
	}
	for (int i = 0; i < standard_length; i++)
	{
		file_bytes[j] = standard_bytes[i];
		j++;
	}
	for (int i = 0; i < filename_length; i++)
	{
		file_bytes[j] = filename_bytes[i];
		j++;
	}
	for (int i = 0; i < data_length; i++)
	{
		file_bytes[j] = data_bytes[i];
		j++;
	}
	file_bytes[j] = 0xFF;
	file_bytes[j + 1] = 0xFF;
	file_bytes[j + 2] = 0xFF;
	file_bytes[j + 3] = 0xFF;
	// do��czenie w�z�a dla pliku w katalogie nadrz�dnym
	sector dest = *s;
	disk_image.close();
	int offsetx90 = 0;
	for (offsetx90; offsetx90 < 512; offsetx90 += 4)
	{
		if ((uint8_t)dest.bytes[offsetx90] == 0x90 && dest.bytes[offsetx90 + 1] == 0x00 && dest.bytes[offsetx90 + 2] == 0x00 && dest.bytes[offsetx90 + 3] == 0x00)
			break;
	}
	dest.bytes[offsetx90 + 4] += 0x70;
	dest.bytes[offsetx90 + 16] += 0x70;
	dest.bytes[offsetx90 + 52] += 0x70;
	dest.bytes[offsetx90 + 56] += 0x70;
	offsetx90 += 64;
	dest.bytes[offsetx90] = DIRECTORY_MFT_IND + 7;
	dest.bytes[offsetx90 + 6] = 0x01;
	dest.bytes[offsetx90 + 8] = 0x70;
	offsetx90 += 16;
	for (int i = 0; i < filename_length; i++)
	{
		dest.bytes[offsetx90] = filename_bytes[i];
		offsetx90++;
	}
	dest.bytes[offsetx90] = 0xFF;
	dest.bytes[offsetx90 + 1] = 0xFF;
	dest.bytes[offsetx90 + 2] = 0xFF;
	dest.bytes[offsetx90 + 3] = 0xFF;
	// zapisanie rezultat�w na obraz dysku
	ofstream write_image(img_path, ios::binary | ios::out | ios::in);
	int offset = record_sector * SECTOR_SIZE;
	write_image.seekp(offset);
	write_image.write((char*)&file_bytes, sizeof(file_bytes));
	offset = MFT_sector + DIRECTORY_MFT_IND * SECTORS_PER_INDEX;
	offset *= SECTOR_SIZE;
	write_image.seekp(offset);
	write_image.write((char*)&dest.bytes, sizeof(dest.bytes));
	write_image.close();
	return 0;
}