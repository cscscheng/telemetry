#include "fs.h"

unsigned char fs_init(unsigned char* buf) {
    // ustawienie wskazanego bufora na operacje I/O
    fs_buf = buf;
    return fs_is_medium_ok();
}

unsigned char fs_open(fs_file* file, unsigned char* name, unsigned char params) {
    
    // numer sektora
    unsigned long sector;

    // dane sektora
    fs_sector* sector_data = (fs_sector*) fs_buf;

    // inicjalizacja struktury file
    memset((void*) file, 0, sizeof(fs_file));

    // zako�cz nazw� znakiem NULL - ogranicz d�ugie �a�cuchy
    name[8] = 0;

    // skopiuj nazw� pliku do struktury file
    strcpy((char*)file->name, (char*)name);

    // szukaj podanego pliku w systemie
    sector = fs_find(name);

    // znaleziono plik
    if (sector) {

        //rs_dump((void*)fs_buf, 512);

        file->timestamp = sector_data->timestamp;
        file->sector    = sector;
        file->size      = sector_data->size;
        file->pos       = 0;

        return 1;
    }

    // nie tw�rz nowego pliku
    if (params & FS_DONT_CREATE) {
        return 0;
    }

    //
    // utw�rz nowy plik
    //

    // szukaj miejsca
    sector = fs_find_sector();

    // jest miejsce
    if (sector) {

        file->sector = sector;
        file->size   = 0;
        file->pos    = 0;

        // zapisz sektor na kart�
        memset((void*) sector_data, 0, 512);

        // ustaw dane
        sector_data->flag = FS_FLAG_FILE;
        strcpy((char*)(sector_data->name), (char*)file->name);
        sector_data->timestamp = 0UL;
        sector_data->size = 0;

        // zapis
        fs_write_sector(sector);
        //rs_dump((void*)fs_buf, 512);

        return 1;
    }

    // nie ma miejsca
    return 0;
}

// dopisuje dane z buf (o d�ugo�ci len) do ko�ca pliku file
unsigned int fs_write(fs_file* file, unsigned char* buf, unsigned int len) {

    // kontrola d�ugo�ci pliku
    if (FS_MAX_FILE_SIZE < (file->size + len) || !len ) {
        return 0;
    }

    // dane sektora
    fs_sector* sector_data = (fs_sector*) fs_buf;

    // odczytaj sektor
    fs_read_sector(file->sector);

    // kopiuj dane na koniec pliku
    memcpy( (void*)(sector_data->data) + sector_data->size, (void*)buf, len);

    // zwi�ksz informacj� o rozmiarze pliku
    sector_data->size += len;
    file->size += len;

    // zapisz na kart�
    fs_write_sector(file->sector);

    return len;
}

// oczytuje dane do buf (o d�ugo�ci len) z pliku file z pozycji file->pos
unsigned int fs_read(fs_file* file, unsigned char* buf, unsigned int len) {

    // kontrola po�o�enia wska�nika pliku
    if (file->size < (file->pos + len) || !len ) {
        return 0;
    }

    // dane sektora
    fs_sector* sector_data = (fs_sector*) fs_buf;

    // odczytaj sektor
    fs_read_sector(file->sector);

    // kopiuj dane
    memcpy((void*)buf, (void*)(sector_data->data) + file->pos, len);

    // zwi�ksz informacj� o wska�niku pliku
    file->pos += len;

    return len;
}

// zamknij plik
unsigned char fs_close(fs_file* file) {
    return 1;
}

// skasuj plik
unsigned char fs_delete(fs_file* file) {

    // zamazanie zawarto�ci pliku
    memset((void*) fs_buf, 0xff, 512);

    // zapis
    fs_write_sector(file->sector);

    return 1;
}

// szuka podanego pliku i zwraca numer zajmowanego przez niego sektora
unsigned long fs_find(unsigned char* name) {
    unsigned long sector;
    unsigned long sectors = fs_number_of_sectors();

    // zrzutuj dane sektora do struktury jego opisu
    fs_sector* sector_data = (fs_sector*) fs_buf;
    
    // sprawdzaj kolejne sektory
    for (sector = 1; sector < sectors; sector++) {
        fs_read_sector(sector);

        // szukaj tylko w�r�d sektor�w oznaczonych jako zawieraj�ce pliki
        if ( sector_data->flag != FS_FLAG_FILE )
            continue;

        // znaleziono plik?
        if ( strcmp((char*)(sector_data->name), (char*)name) == 0 )
            return sector;
    }
    
    return 0;
}

// szukaj pierwszego wolnego sektora
unsigned long fs_find_sector() {
    
    unsigned long sector;
    unsigned long sectors = fs_number_of_sectors();

    // zrzutuj dane sektora do struktury jego opisu
    fs_sector* sector_data = (fs_sector*) fs_buf;
    
    // sprawdzaj kolejne sektory
    for (sector = 1; sector < sectors; sector++) {
        fs_read_sector(sector);

        // znaleziono pusty sektor - nieustawiona flaga FS_FILE
        if ( sector_data->flag != FS_FLAG_FILE )
            return sector;
    }
    
    return 0;
}

// listuje pliki szukaj�c w systemie pocz�wszy od podanego sektora
unsigned long fs_list_files(fs_file* file, unsigned long sector) {
    
    // liczba sektor�w dost�pnych dla systemu plik�w
    unsigned long sectors = fs_number_of_sectors();
    
    // zrzutuj dane sektora do struktury jego opisu
    fs_sector* sector_data = (fs_sector*) fs_buf;

    // za ka�dym razem szukaj kolejnego sektora
    for (; sector < sectors; sector++) {
        fs_read_sector(sector);

        // znaleziono sektor z plikiem
        if ( sector_data->flag == FS_FLAG_FILE ) {
            // kopiuj nazw� pliku
            strcpy( (char*)file->name, (char*)(sector_data->name));

            //rs_send('>'); rs_text((char*)file->name); rs_newline();

            // pozosta�e dane o pliku
            file->timestamp = sector_data->timestamp;
            file->sector    = sector;
            file->size      = sector_data->size;
            file->pos       = 0;

            return ++sector;
        }
    }


    return 0;
}
