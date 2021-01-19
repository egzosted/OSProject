/*
	Autor: Michal Piekarski 175456
	Przedmiot: Oprogramowanie Systemowe
	Zadanie: Kopiowanie pliku .txt z systemu hosta na partycję z systemem plików NTFS

	Zalozenie: Plik znajdzie sie w katalogu "Destination", VBS znajduje sie pod adresem 0x1000000.
			   Aby zmienic te wartosci nalezy zmienic numer indeksu w
			   tablicy MFT folderu docelowego (domyslnie Destination) poprzez zmiane stalej, a
			   do zmiany sektora VBS nalezy zmienic jego adres. W funkcjach tworzacych poszczegolne, stale
			   z przedrostkiem V odpowiadaja za wartosci, a bez przedrostka za offset.

	Do zmiany zawsze:
				- parent sequence
				- laczna dlugosc rekordu

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
const int DIRECTORY_MFT_IND = 38; // indeks katalogu docelowego w tablicy MFT
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
	const int V_PARENT_SEQUENCE = 0x02;
	const int ATTRIBUTE_TYPE = 0x00;
	const int V_ATTRIBUTE_TYPE = 0x30;
	const int ATTRIBUTE_LENGTH = 0x04;
	const int ATTRIBUTE_LENGTH_WO_HEADER = 0x10;
	const int ATTRIBUTE_ID = 0x0D;
	const int V_ATTRIBUTE_ID = 0x03;
	const int ATTRIBUTE_OFFSET = 0x14;
	const int V_ATTRIBUTE_OFFSET = 0x18;
	const int INDEXED_FLAG = 0x16;
	const int V_INDEXED_FLAG = 1;
	const int FILE_REFERENCE = 0x18;
	const int PARENT_SEQUENCE = 0x1E;
	const int FLAGS = 0x50;
	const int NOT_INDEXED = 0x20;
	const int FILENAME_LENGTH = 0x58;
	const int FILENAME = 0x5A;

	bytes[ATTRIBUTE_TYPE] = V_ATTRIBUTE_TYPE; // wartosc atrybutu
	bytes[ATTRIBUTE_ID] = V_ATTRIBUTE_ID; // attribute Id
	bytes[ATTRIBUTE_OFFSET] = V_ATTRIBUTE_OFFSET; // offset do atrybutu
	bytes[INDEXED_FLAG] = V_INDEXED_FLAG; // indexed flag
	bytes[FILE_REFERENCE] = DIRECTORY_MFT_IND; // numer indeksu katalogu nadrzednego w MFT
	bytes[PARENT_SEQUENCE] = V_PARENT_SEQUENCE; // sequence number katalogu nadrzednego
	bytes[FLAGS] = NOT_INDEXED;

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
	bytes[FILENAME_LENGTH] = filename.size();
	length += 2;
	for (int i = 0; i < filename.size(); i++)
	{
		bytes[FILENAME + 2 * i] = filename[i];
		length += 2;
	}
	real_length = length;
	if (length % 8 != 0)
	{
		while (length % 8 != 0)
			length++;
	}
	bytes[ATTRIBUTE_LENGTH] = length; // dlugosc wraz z naglówkiem
	bytes[ATTRIBUTE_LENGTH_WO_HEADER] = real_length - attribute_header_size; // dlugosc atrybutu
	return length;
}

/*
Funkcja zwraca dlugosc naglowka rekordu plikowego, w którym rozpoczyna sie tablica $MFT
arg1: tablica charów, która przchowany jest atrybut naglowek rekordu plikowego, arg2 nazwa pliku do kopiowania
return: dlugosc atrybutu
*/

int create_file_header(unsigned char* bytes, int index)
{
	const int MAGIC = 0x00;
	const int UPDATE_SEQUENCE_OFFSET = 0x04;
	const int V_UPDATE_SEQUENCE_OFFSET = 0x30;
	const int UPDATE_SEQUENCE_SIZE = 0x06;
	const int V_UPDATE_SEQUENCE_SIZE = 0x03;
	const int SEQUENCE_NUMBER = 0x10;
	const int V_SEQUENCE_NUMBER = 0x01;
	const int HARD_LINK_COUNT = 0x12;
	const int V_HARD_LINK_COUNT = 0x01;
	const int ATTRIBUTE_OFFSET = 0x14;
	const int V_ATTRIBUTE_OFFSET = 0x38;
	const int FLAGS = 0x16;
	const int IN_USE = 0x01;
	const int REAL_SIZE = 0x18;
	const int ALLOCATED_SIZE = 0x1C;
	const int NEXT_ATTRIBUTE_ID = 0x28;
	const int V_NEXT_ATTRIBUTE_ID = 0x05;
	const int HEADER_SIZE = 56;
	const int MFT_RECORD = 0x2C;
	const int UPDATE_SEQUENCE_NUMBER = 0x30;
	const int V_UPDATE_SEQUENCE_NUMBER = 0x01;

	int length = 0;
	// uzupelnienie magic number
	bytes[MAGIC] = 0x46;
	bytes[MAGIC + 1] = 0x49;
	bytes[MAGIC + 2] = 0x4C;
	bytes[MAGIC + 3] = 0x45;
	// offset do pola update sequence (pole 2 bitowe)
	bytes[UPDATE_SEQUENCE_OFFSET] = V_UPDATE_SEQUENCE_OFFSET;
	// update sequence size in word (2 bajty)
	bytes[UPDATE_SEQUENCE_SIZE] = V_UPDATE_SEQUENCE_SIZE;
	// ??? $LogFile sequence number

	// sequence number
	bytes[SEQUENCE_NUMBER] = V_SEQUENCE_NUMBER;
	// hard link count
	bytes[HARD_LINK_COUNT] = V_HARD_LINK_COUNT;
	// offset to first attribute
	bytes[ATTRIBUTE_OFFSET] = V_ATTRIBUTE_OFFSET;
	bytes[FLAGS] = IN_USE;
	// real size of file record, needs to be changed
	bytes[REAL_SIZE] = 0x00;
	bytes[REAL_SIZE + 1] = 0x04;
	// allocated size of file record
	bytes[ALLOCATED_SIZE] = 0x00;
	bytes[ALLOCATED_SIZE + 1] = 0x04;
	// next attribute id
	bytes[NEXT_ATTRIBUTE_ID] = V_NEXT_ATTRIBUTE_ID;
	// number of MFT record 
	bytes[MFT_RECORD] = index;
	// update sequence number
	bytes[UPDATE_SEQUENCE_NUMBER] = V_UPDATE_SEQUENCE_NUMBER;
	length = HEADER_SIZE;
	return length;
}

int create_standard_attribute(unsigned char* bytes)
{
	int length = 96;
	const int CREATION_TIME = 24;
	const int ALTER_TIME = 32;
	const int MFT_TIME = 40;
	const int READ_TIME = 48;
	const int ATTRIBUTE_TYPE = 0x00;
	const int V_ATTRIBUTE_TYPE = 0x10;
	const int ATTRIBUTE_LENGTH = 0x04;
	const int ATTRIBUTE_OFFSET = 0x14;
	const int V_ATTRIBUTE_OFFSET = 0x18;
	const int ATTRIBUTE_LENGTH_WO_HEADER = 0x10;
	const int SECURITY_ID = 76;
	const int DOS_PERMISSIONS = 56;
	const int ARCHIVE = 0x20;

	bytes[ATTRIBUTE_TYPE] = V_ATTRIBUTE_TYPE;
	bytes[ATTRIBUTE_LENGTH] = length; // dlugoscwraz z naglówkiem
	bytes[ATTRIBUTE_OFFSET] = V_ATTRIBUTE_OFFSET;

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

	bytes[DOS_PERMISSIONS] = ARCHIVE; // DOS permissions
	bytes[SECURITY_ID] = 0x0E; //security ID
	bytes[SECURITY_ID + 1] = 0x01;
	bytes[ATTRIBUTE_LENGTH_WO_HEADER] = length - 24; // dlugosc atrybutu
	return length;
}

int create_data_attribute(unsigned char* bytes)
{
	int length = 24;
	const int ATTRIBUTE_TYPE = 0x00;
	const int V_ATTRIBUTE_TYPE = 0x80;
	const int ATTRIBUTE_LENGTH = 0x04;
	const int ATTRIBUTE_LENGTH_WO_HEADER = 0x10;
	const int ATTRIBUTE_ID = 0x0D;
	const int V_ATTRIBUTE_ID = 0x01;
	const int ATTRIBUTE_OFFSET = 0x14;
	const int V_ATTRIBUTE_OFFSET = 0x18;
	const int ATTRIBUTE_OFFSET_NAME = 0x0A;
	const int V_ATTRIBUTE_OFFSET_NAME = 0x18;

	bytes[ATTRIBUTE_TYPE] = V_ATTRIBUTE_TYPE; // kod atrybutu
	bytes[ATTRIBUTE_LENGTH] = length; // dlugosc wraz z naglówkiem
	bytes[ATTRIBUTE_OFFSET] = V_ATTRIBUTE_OFFSET; // offset do nazwy
	bytes[ATTRIBUTE_ID] = V_ATTRIBUTE_ID; // attribute id
	bytes[ATTRIBUTE_OFFSET] = V_ATTRIBUTE_OFFSET; // offset do atrybutu
	bytes[ATTRIBUTE_LENGTH_WO_HEADER] = 0x00; // dlugosc atrybutu
	bytes[ATTRIBUTE_OFFSET_NAME] = V_ATTRIBUTE_OFFSET_NAME; // offset do nazwy
	return length;
}

int create_object_id_attribute(unsigned char* bytes)
{
	const int ATTRIBUTE_TYPE = 0x00;
	const int V_ATTRIBUTE_TYPE = 0x40;
	const int ATTRIBUTE_LENGTH = 0x04;
	const int ATTRIBUTE_LENGTH_WO_HEADER = 0x10;
	const int ATTRIBUTE_ID = 0x0D;
	const int V_ATTRIBUTE_ID = 0x04;
	const int ATTRIBUTE_OFFSET = 0x14;
	const int V_ATTRIBUTE_OFFSET = 0x18;
	const int INDEXED_FLAG = 0x16;
	const int V_INDEXED_FLAG = 1;
	const int GUID = 24;
	int length = 40;
	bytes[ATTRIBUTE_TYPE] = V_ATTRIBUTE_TYPE; // kod atrybutu
	bytes[ATTRIBUTE_LENGTH] = length; // dlugosc wraz z naglówkiem
	bytes[ATTRIBUTE_OFFSET] = V_ATTRIBUTE_OFFSET; // offset do nazwy
	bytes[ATTRIBUTE_ID] = V_ATTRIBUTE_ID; // attribute id
	bytes[ATTRIBUTE_LENGTH_WO_HEADER] = ATTRIBUTE_LENGTH_WO_HEADER; // dlugosc atrybutu

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

	cout << "Tablica MFT rozpoczyna sie od sektora numer: " << MFT_sector << endl;

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

	cout << "Rekord pliku zostanie zapisany na sektorze numer: " << record_sector << endl;

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


	// wpisanie lacznej dlugosci rekordu
	const int RECORD_SIZE = 24;
	header_bytes[RECORD_SIZE] = 0x50;
	header_bytes[RECORD_SIZE + 1] = 0x1;

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

	const int SEQUENCE_CHECK = 510;
	file_bytes[SEQUENCE_CHECK] = 0x01;
	file_bytes[SEQUENCE_CHECK + SECTOR_SIZE] = 0x01;

	// dołączenie węzła dla pliku w katalogie nadrzędnym
	int offsetx90 = 0;
	for (offsetx90; offsetx90 < 512; offsetx90 += 4)
	{
		if ((uint8_t)dest.bytes[offsetx90] == 0x90 && dest.bytes[offsetx90 + 1] == 0x00 && dest.bytes[offsetx90 + 2] == 0x00 && dest.bytes[offsetx90 + 3] == 0x00)
			break;
	}
	cout << offsetx90 << endl;
	// manualnie zwiekszony rozmiar atrybutu
	dest.bytes[offsetx90 + 4] = 0xB8;
	dest.bytes[offsetx90 + 16] = 0x98;
	dest.bytes[offsetx90 + 52] = 0x88; // zwiekszony rozmiar wezlow
	dest.bytes[offsetx90 + 56] = 0x88; // zwiekszony rozmiar pamieci zaalokowanej na wezly
	offsetx90 += 64;
	unsigned char copy[16] = { 0 };
	for (int i = 0; i < 16; i++) {
		copy[i] = dest.bytes[offsetx90 + i];
	}
	cout << endl;
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
	// aktualizacja wielkosci rekordu plikowego katalogu nadrzednego
	dest.bytes[24] = 0xA6;
	dest.bytes[25] = 0x2;
	for (int i = 0; i < 512; i++)
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