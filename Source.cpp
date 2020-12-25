﻿/*
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
const int DIRECTORY_MFT_IND = 43; // indeks katalogu docelowego w tablicy MFT
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
		int j = (uint8_t)c_MFT_address[ind];
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

int create_file_name(unsigned char* bytes, string filename)
{
	const int attribute_header_size = 24;
	bytes[0] = 0x30; // wartoœæ atrybutu
	bytes[14] = 0x03; // attribute Id
	bytes[20] = 0x18; // offset do atrybutu
	bytes[22] = 0x01; // indexed flag
	bytes[24] = DIRECTORY_MFT_IND; // numer indeksu katalogu nadrzêdnego w MFT
	bytes[30] = 0x02; // sequence number katalogu nadrzêdnego
	bytes[80] = 0x20;
	int length = 88;
	int real_length = 0;
	bytes[88] = filename.size();
	length += 2;
	for (int i = 0; i < filename.size(); i++)
	{
		bytes[90 + 2 * i] = filename[i];
		length += 2;
	}
	real_length = length;
	if (length % 8 != 0)
	{
		while (length % 8 != 0)
			length++;
	}
	bytes[4] = length; // d³ugoœæ wraz z nag³ówkiem
	bytes[16] = real_length - attribute_header_size; // d³ugoœæ atrybutu
	return length;
}

/*
Funkcja zwraca d³ugoœæ atrybutu filename, w którym rozpoczyna siê tablica $MFT
arg1: tablica charów, która przchowa atrybut 0x30 (filename), arg2 nazwa pliku do kopiowania
return: d³ugoœæ atrybutu
*/

int create_file_header(unsigned char* bytes, int index)
{
	int length = 0;
	// uzupe³nienie magic number
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
	bytes[25] = 0x04;
	// allocated size of file record
	bytes[28] = 0x00;
	bytes[29] = 0x04;
	// next attribute id
	bytes[40] = 0x05;
	// update sequence number
	bytes[44] = index;
	length = 56;
	return length;
}

int create_standard_attribute(unsigned char* bytes)
{
	int length = 96;
	bytes[0] = 0x10;
	bytes[4] = length; // d³ugoœæ wraz z nag³ówkiem
	bytes[20] = 0x18;
	bytes[56] = 0x20; // DOS permissions
	bytes[76] = 0x0C; //security ID
	bytes[77] = 0x01;
	bytes[16] = length - 24; // d³ugoœæ atrybutu
	return length;
}

int create_data_attribute(unsigned char* bytes)
{
	int length = 24;
	bytes[0] = 0x80; // kod atrybutu
	bytes[4] = length; // d³ugoœæ wraz z nag³ówkiem
	bytes[10] = 0x18; // offset do nazwy
	bytes[14] = 0x01; // attribute id
	bytes[20] = 0x18; // offset do atrybutu
	bytes[16] = 0x00; // d³ugoœæ atrybutu
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
	if (!disk_image)
	{
		cout << "Nie udalo sie otworzyc do odczytu" << endl;
	}
	else
	{
		cout << "Udalo sie otworzyc do odczytu" << endl;
	}
	// pobranie sektora vbs dla partycji NTFS w folderze projektu (ntfs.vhd) (dla innej struktury wystarczy zmieniæ vbs.address)
	sector vbs;
	vbs.address = 0x1000000;
	vbs.sector_no = vbs.address / SECTOR_SIZE;
	auto s = istream_iterator<sector>(disk_image);
	advance(s, vbs.sector_no); // przesuniêcie iteratora obrazu dysku z pocz¹tku na sektor vbs
	vbs = *s;
	vbs.address = 0x1000000;
	vbs.sector_no = vbs.address / SECTOR_SIZE;
	
	int MFT_sector = get_MFT_sector_no(vbs);

	advance(s, MFT_sector - vbs.sector_no); // przesuniêcie iteratora obrazu dysku z pocz¹tku na sektor MFT
	sector mft;

	// advance(s, DIRECTORY_MFT_IND * SECTORS_PER_INDEX); // przesuniêcie iteratora na folder Destination
	auto s_file = s;
	 //szukanie wolnego miejsca na nowy rekord plikowy
	sector record;
	 //faktyczne pliki użytkownika zaczynają się od 35. pliku
	advance(s_file, 35 * SECTORS_PER_INDEX);
	int record_sector = MFT_sector + 35 * SECTORS_PER_INDEX;
	while (true)
	{
		record = *s_file;
		if (record.bytes[0] == 0)
			break;
		advance(s_file, SECTORS_PER_INDEX);
		record_sector += SECTORS_PER_INDEX;
	}

	cout << record_sector << endl;
	int record_index = (record_sector - MFT_sector) / 2;
	// utworzenie nag³ówka dla rekordu plikowego
	unsigned char header_bytes[512] = { 0 };
	int header_length = create_file_header(header_bytes, record_index); 

	//utworzenie atrybutu 0x10 dla rekordu plikowego
	unsigned char standard_bytes[512] = { 0 };
	int standard_length = create_standard_attribute(standard_bytes);

	// utworzenie atrybutu 0x30 dla rekordu plikowego
	unsigned char filename_bytes[512] = { 0 };
	int filename_length = create_file_name(filename_bytes, txt_path);

	// utworzenie atrybutu 0x80 dla rekordu plikowego
	unsigned char data_bytes[512] = { 0 };
	int data_length = create_data_attribute(data_bytes);

	// laczna dlugosc rekordu wynosi 292
	header_bytes[24] = 0x24;
	header_bytes[25] = 0x1;

	unsigned char file_bytes[512] = { 0 };
	// sklejenie atrybutów w jeden sektor
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
	file_bytes[j] = 255;
	file_bytes[j + 1] = 255;
	file_bytes[j + 2] = 255;
	file_bytes[j + 3] = 255;

	int record_size = header_length + standard_length + filename_length + data_length + 4;
	//// do³¹czenie wêz³a dla pliku w katalogie nadrzêdnym
	//sector dest = *s;
	//disk_image.close();
	//int offsetx90 = 0;
	//for (offsetx90; offsetx90 < 512; offsetx90 += 4)
	//{
	//	if ((uint8_t)dest.bytes[offsetx90] == 0x90 && dest.bytes[offsetx90 + 1] == 0x00 && dest.bytes[offsetx90 + 2] == 0x00 && dest.bytes[offsetx90 + 3] == 0x00)
	//		break;
	//}
	//dest.bytes[offsetx90 + 4] += 0x70;
	//dest.bytes[offsetx90 + 16] += 0x70;
	//dest.bytes[offsetx90 + 52] += 0x70;
	//dest.bytes[offsetx90 + 56] += 0x70;
	//offsetx90 += 64;
	//dest.bytes[offsetx90] = DIRECTORY_MFT_IND + 5;
	//dest.bytes[offsetx90 + 6] = 0x01;
	//dest.bytes[offsetx90 + 8] = 0x70;
	//offsetx90 += 16;
	//for (int i = 0; i < filename_length; i++)
	//{
	//	dest.bytes[offsetx90] = filename_bytes[i];
	//	offsetx90++;
	//}
	//dest.bytes[offsetx90] = 255;
	//dest.bytes[offsetx90 + 1] = 255;
	//dest.bytes[offsetx90 + 2] = 255;
	//dest.bytes[offsetx90 + 3] = 255;

	//for (int i = 0; i < 512; i++)
	//{
	//	if (i % 16 == 0)
	//		cout << endl;
	//	cout << hex << (uint8_t)dest.bytes[i] << " ";
	//}
	//cout << endl;
	// zapisanie rezultatów na obraz dysku
	/*ofstream write_image(img_path, ios::binary | ios::out | ios::in);
	if (!write_image)
	{
		cout << "Nie udalo sie otworzyc do zapisu" << endl;
	}
	else
	{
		cout << "Udalo sie otworzyc do zapisu" << endl;
	}
	int offset = record_sector * SECTOR_SIZE;
	cout << offset << endl;
	write_image.seekp(offset);
	write_image.write((char*)&file_bytes, sizeof(file_bytes));
	offset = MFT_sector + DIRECTORY_MFT_IND * SECTORS_PER_INDEX;
	offset *= SECTOR_SIZE;
	cout << offset << endl;
	write_image.seekp(offset);
	write_image.write((char*)&dest.bytes, sizeof(dest.bytes));
	write_image.close();*/
	return 0;
}