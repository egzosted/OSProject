/*
	Autor: Michal Piekarski 175456
	Przedmiot: Oprogramowanie Systemowe
	Zadanie: Kopiowanie pliku .txt z systemu hosta na pen drive z systemem plików NTFS

	Za³o¿enie: Plik znajdzie sie w katalogu "Destination", VBS znajduje sie pod adresem 0x1000000.
			   Aby zmienic te wartosci nalezy zmienic numer indeksu w
			   tablicy MFT folderu docelowego (domyslnie Destination) poprzez zmianê stalej, a
			   do zmiany sektora VBS nalezy zmienic jego adres.

	Uruchomienie: Program bierze argumenty: 
		1. Sciezka do pliku tekstowego w systemie hosta
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
	unsigned char bytes[SECTOR_SIZE];
};

// Przeciazenie operatora >>, aby z wejscia pobierac caly sektor
istream& operator>>(std::istream& s, sector& sect) {
	s.read((char*)sect.bytes, sizeof(sect.bytes));
	return s;
}

/*
Funkcja zwraca numer sektora, w którym rozpoczyna sie tablica $MFT
arg1: sektor VBS, do odczytania numeru klastra $MFT
return: numer sektora
*/

int get_MFT_sector_no(sector vbs)
{
	char c_MFT_address[9];
	int ind = 0;
	int i_MFT_address = 0;
	// adres tablicy MFT to 8 bitowe pole o offsecie 48, które znajduje sie w VBS
	int MFT_offset = 48;
	// odczyt od najstarszego bajtu zgodnie z little endian
	for (int i = MFT_offset + 7; i >= MFT_offset; i--)
	{
		c_MFT_address[ind] = vbs.bytes[i];
		// przesuwamy o 8 bitów, aby wczytac nowa wartosc na najmlodszy bajt
		i_MFT_address <<= 8;
		int j = (uint8_t)c_MFT_address[ind];
		// alternatywa laczy poprzednie bajty z aktualnym
		i_MFT_address |= j;
		ind++;
	}
	int final_sector = i_MFT_address * 8 + vbs.sector_no;
	return final_sector;
}

/*
Funkcja zwraca dlugosc atrybutu filename, w którym rozpoczyna sie tablica $MFT
arg1: tablica charów, która przchowa atrybut 0x30 (filename), arg2 nazwa pliku do kopiowania
return: dlugosc atrybutu
*/

int create_file_name(unsigned char* bytes, string filename)
{
	const int attribute_header_size = 24;
	const int CREATION_TIME = 32;
	const int ALTER_TIME = 40;
	const int MFT_TIME = 48;
	const int READ_TIME = 56;
	bytes[0] = 0x30; // wartosc atrybutu
	bytes[14] = 0x03; // attribute Id
	bytes[20] = 0x18; // offset do atrybutu
	bytes[22] = 0x01; // indexed flag
	bytes[24] = DIRECTORY_MFT_IND; // numer indeksu katalogu nadrzednego w MFT
	bytes[30] = 0x04; // sequence number katalogu nadrzednego
	bytes[80] = 0x20;

	// znaczniki czasu
	// czas stworzenia
	bytes[CREATION_TIME] = 0xF1;
	bytes[CREATION_TIME + 1] = 0x90;
	bytes[CREATION_TIME + 2] = 0x12;
	bytes[CREATION_TIME + 3] = 0xC6;
	bytes[CREATION_TIME + 4] = 0x75;
	bytes[CREATION_TIME + 5] = 0xEB;
	bytes[CREATION_TIME + 6] = 0xD6;
	bytes[CREATION_TIME + 7] = 0x01;

	// czas modyfikacji
	bytes[ALTER_TIME] = 0xF1;
	bytes[ALTER_TIME + 1] = 0x90;
	bytes[ALTER_TIME + 2] = 0x12;
	bytes[ALTER_TIME + 3] = 0xC6;
	bytes[ALTER_TIME + 4] = 0x75;
	bytes[ALTER_TIME + 5] = 0xEB;
	bytes[ALTER_TIME + 6] = 0xD6;
	bytes[ALTER_TIME + 7] = 0x01;

	// czas zmiany w MFT
	bytes[MFT_TIME] = 0xF1;
	bytes[MFT_TIME + 1] = 0x90;
	bytes[MFT_TIME + 2] = 0x12;
	bytes[MFT_TIME + 3] = 0xC6;
	bytes[MFT_TIME + 4] = 0x75;
	bytes[MFT_TIME + 5] = 0xEB;
	bytes[MFT_TIME + 6] = 0xD6;
	bytes[MFT_TIME + 7] = 0x01;

	// czas odczytu
	bytes[READ_TIME] = 0xF1;
	bytes[READ_TIME + 1] = 0x90;
	bytes[READ_TIME + 2] = 0x12;
	bytes[READ_TIME + 3] = 0xC6;
	bytes[READ_TIME + 4] = 0x75;
	bytes[READ_TIME + 5] = 0xEB;
	bytes[READ_TIME + 6] = 0xD6;
	bytes[READ_TIME + 7] = 0x01;
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
	bytes[4] = length; // dlugosc wraz z naglówkiem
	bytes[16] = real_length - attribute_header_size; // dlugosc atrybutu
	return length;
}

/*
Funkcja zwraca dlugosc naglowka rekordu plikowego, w którym rozpoczyna sie tablica $MFT
arg1: tablica charów, która przchowany jest atrybut naglowek rekordu plikowego, arg2 nazwa pliku do kopiowania
return: dlugosc atrybutu
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
	bytes[6] = 0x03;
	// ??? $LogFile sequence number

	// sequence number
	bytes[16] = 0x01;
	// hard link count
	bytes[18] = 0x01;
	// offset to first attribute
	bytes[20] = 0x38;
	bytes[22] = 0x01;
	// real size of file record, needs to be changed
	bytes[24] = 0x00;
	bytes[25] = 0x04;
	// allocated size of file record
	bytes[28] = 0x00;
	bytes[29] = 0x04;
	// next attribute id
	bytes[40] = 0x05;
	// number of MFT record 
	bytes[44] = index;
	// update sequence number
	bytes[48] = 0x01;
	length = 56;
	return length;
}

int create_standard_attribute(unsigned char* bytes)
{
	int length = 96;
	const int CREATION_TIME = 24;
	const int ALTER_TIME = 32;
	const int MFT_TIME = 40;
	const int READ_TIME = 48;
	bytes[0] = 0x10;
	bytes[4] = length; // dlugoscwraz z naglówkiem
	bytes[20] = 0x18;

	// znaczniki czasu
	// czas stworzenia
	bytes[CREATION_TIME] = 0xF1;
	bytes[CREATION_TIME + 1] = 0x90;
	bytes[CREATION_TIME + 2] = 0x12;
	bytes[CREATION_TIME + 3] = 0xC6;
	bytes[CREATION_TIME + 4] = 0x75;
	bytes[CREATION_TIME + 5] = 0xEB;
	bytes[CREATION_TIME + 6] = 0xD6;
	bytes[CREATION_TIME + 7] = 0x01;

	// czas modyfikacji
	bytes[ALTER_TIME] = 0xF1;
	bytes[ALTER_TIME + 1] = 0x90;
	bytes[ALTER_TIME + 2] = 0x12;
	bytes[ALTER_TIME + 3] = 0xC6;
	bytes[ALTER_TIME + 4] = 0x75;
	bytes[ALTER_TIME + 5] = 0xEB;
	bytes[ALTER_TIME + 6] = 0xD6;
	bytes[ALTER_TIME + 7] = 0x01;

	// czas zmiany w MFT
	bytes[MFT_TIME] = 0x9D;
	bytes[MFT_TIME + 1] = 0xFD;
	bytes[MFT_TIME + 2] = 0x43;
	bytes[MFT_TIME + 3] = 0xC7;
	bytes[MFT_TIME + 4] = 0x75;
	bytes[MFT_TIME + 5] = 0xEB;
	bytes[MFT_TIME + 6] = 0xD6;
	bytes[MFT_TIME + 7] = 0x01;

	// czas odczytu
	bytes[READ_TIME] = 0xF1;
	bytes[READ_TIME + 1] = 0x90;
	bytes[READ_TIME + 2] = 0x12;
	bytes[READ_TIME + 3] = 0xC6;
	bytes[READ_TIME + 4] = 0x75;
	bytes[READ_TIME + 5] = 0xEB;
	bytes[READ_TIME + 6] = 0xD6;
	bytes[READ_TIME + 7] = 0x01;

	bytes[56] = 0x20; // DOS permissions
	bytes[76] = 0x0E; //security ID
	bytes[77] = 0x01;
	bytes[16] = length - 24; // dlugosc atrybutu
	return length;
}

int create_data_attribute(unsigned char* bytes)
{
	int length = 24;
	bytes[0] = 0x80; // kod atrybutu
	bytes[4] = length; // dlugosc wraz z naglówkiem
	bytes[10] = 0x18; // offset do nazwy
	bytes[14] = 0x01; // attribute id
	bytes[20] = 0x18; // offset do atrybutu
	bytes[16] = 0x00; // dlugosc atrybutu
	return length;
}

int create_object_id_attribute(unsigned char* bytes)
{
	const int GUID = 24;
	int length = 40;
	bytes[0] = 0x40; // kod atrybutu
	bytes[4] = length; // dlugosc wraz z naglówkiem
	bytes[10] = 0x18; // offset do nazwy
	bytes[14] = 0x04; // attribute id
	bytes[20] = 0x18; // offset do atrybutu
	bytes[16] = 0x10; // dlugosc atrybutu

	bytes[GUID] = 0x1A; 
	bytes[GUID + 1] = 0xA2;
	bytes[GUID + 2] = 0xF4;
	bytes[GUID + 3] = 0x3D;
	bytes[GUID + 4] = 0x2E;
	bytes[GUID + 5] = 0x3A;
	bytes[GUID + 6] = 0x41;
	bytes[GUID + 7] = 0x82;

	bytes[GUID + 8] = 0xAD;
	bytes[GUID + 9] = 0xAE;
	bytes[GUID + 10] = 0x82;
	bytes[GUID + 11] = 0xFD;
	bytes[GUID + 12] = 0x5A;
	bytes[GUID + 13] = 0x7D;
	bytes[GUID + 14] = 0x32;
	bytes[GUID + 15] = 0x5D;

	return length;
}

int main(int* argc, char** argv)
{
	// odczytanie z linii polecen sciezki do pliku tekstowego i obrazu dysku
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
	// pobranie sektora vbs dla partycji NTFS w folderze projektu (image.vhd) (dla innej struktury wystarczy zmienic vbs.address)
	sector vbs;
	vbs.address = 0x1000000;
	vbs.sector_no = vbs.address / SECTOR_SIZE;
	auto s = istream_iterator<sector>(disk_image);
	advance(s, vbs.sector_no); // przesuniecie iteratora obrazu dysku z poczatku na sektor vbs
	vbs = *s;
	vbs.address = 0x1000000;
	vbs.sector_no = vbs.address / SECTOR_SIZE;
	
	int MFT_sector = get_MFT_sector_no(vbs);

	cout << MFT_sector << endl;

	advance(s, MFT_sector - vbs.sector_no); // przesuniecie iteratora obrazu dysku z poczatku na sektor MFT
	sector mft;

	auto s_file = s;
	 //szukanie wolnego miejsca na nowy rekord plikowy
	sector record;
	 // odczytanie katalogu docelowego i znalezienie miejsca na plik
	advance(s_file, DIRECTORY_MFT_IND * SECTORS_PER_INDEX);
	sector dest = *s_file;
	int record_sector = MFT_sector + DIRECTORY_MFT_IND * SECTORS_PER_INDEX;
	while (true)
	{
		record = *s_file;
		if (record.bytes[0] == 0)
			break;
		advance(s_file, SECTORS_PER_INDEX);
		record_sector += SECTORS_PER_INDEX;
	}

	int record_index = (record_sector - MFT_sector) / 2;
	// utworzenie naglowka dla rekordu plikowego
	unsigned char header_bytes[512] = { 0 };
	int header_length = create_file_header(header_bytes, record_index); 

	//utworzenie atrybutu 0x10 dla rekordu plikowego
	unsigned char standard_bytes[512] = { 0 };
	int standard_length = create_standard_attribute(standard_bytes);

	// utworzenie atrybutu 0x30 dla rekordu plikowego
	unsigned char filename_bytes[512] = { 0 };
	int filename_length = create_file_name(filename_bytes, txt_path);

	// utworzenie atrybutu 0x40 dla rekordu plikowego
	unsigned char object_bytes[512] = { 0 };
	int object_length = create_object_id_attribute(object_bytes);

	// utworzenie atrybutu 0x80 dla rekordu plikowego
	unsigned char data_bytes[512] = { 0 };
	int data_length = create_data_attribute(data_bytes);

	// laczna dlugosc rekordu wynosi 292
	header_bytes[24] = 0x40;
	header_bytes[25] = 0x1;

	unsigned char file_bytes[1024] = { 0 };
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
	for (int i = 0; i < object_length; i++)
	{
		file_bytes[j] = object_bytes[i];
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

	int record_size = header_length + standard_length + filename_length + object_length + data_length + 4;
	disk_image.close();

	file_bytes[510] = 0x01;
	file_bytes[1022] = 0x01;
	// dołączenie węzła dla pliku w katalogie nadrzędnym
	int offsetx90 = 0;
	for (offsetx90; offsetx90 < 512; offsetx90 += 4)
	{
		if ((uint8_t)dest.bytes[offsetx90] == 0x90 && dest.bytes[offsetx90 + 1] == 0x00 && dest.bytes[offsetx90 + 2] == 0x00 && dest.bytes[offsetx90 + 3] == 0x00)
			break;
	}
	// manualnie zwiekszony rozmiar atrybutu
	dest.bytes[offsetx90 + 4] = 0x20;
	dest.bytes[offsetx90 + 5] = 0x01;
	dest.bytes[offsetx90 + 16] = 0x00;
	dest.bytes[offsetx90 + 17] = 0x01;
	dest.bytes[offsetx90 + 52] += 0x68; // zwiekszony rozmiar wezlow
	dest.bytes[offsetx90 + 56] += 0x68; // zwiekszony rozmiar pamieci zaalokowanej na wezly
	offsetx90 += 64;
	unsigned char copy[16] = { 0 };
	for (int i = 0; i < 16; i++)
		copy[i] = dest.bytes[offsetx90 + i];
	dest.bytes[offsetx90] = record_index;
	dest.bytes[offsetx90 + 6] = 0x01;
	dest.bytes[offsetx90 + 8] = 0x68;
	dest.bytes[offsetx90 + 10] = 0x52;
	offsetx90 += 16;
	for (int i = 24; i < filename_length; i++)
		dest.bytes[offsetx90++] = filename_bytes[i];

	dest.bytes[offsetx90 - 2] = 0x6F;
	dest.bytes[offsetx90 - 4] = 0x44;
	dest.bytes[offsetx90 - 6] = 0x20;
		
	for (int i = 0; i < 16; i++)
		dest.bytes[offsetx90++] = copy[i];
	dest.bytes[offsetx90] = 255;
	dest.bytes[offsetx90 + 1] = 255;
	dest.bytes[offsetx90 + 2] = 255;
	dest.bytes[offsetx90 + 3] = 255;
	offsetx90 += 4;
	cout << offsetx90 << endl;
	// aktualizacja wielkosci rekordu plikowego katalogu nadrzednego
	dest.bytes[24] = 0x50;
	dest.bytes[25] = 0x2;
	for (int i = 0; i < offsetx90; i++)
	{
		if (i % 16 == 0)
			cout << endl;
		cout << hex << (int)dest.bytes[i] << " ";
	}
	// zapisanie rezultatów na obraz dysku
	ofstream write_image(img_path, ios::binary | ios::out | ios::in);
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
	write_image.close();
	return 0;
}